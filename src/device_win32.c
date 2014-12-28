/**
 * ty, command-line program to manage Teensy devices
 * http://github.com/Koromix/ty
 * Copyright (C) 2014 Niels Martignène
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ty/common.h"
#include "compat.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfgmgr32.h>
#include <dbt.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <process.h>
#include <setupapi.h>
#include <usb.h>
#include <usbuser.h>
#include "ty/device.h"
#include "device_priv.h"
#include "ty/system.h"

struct ty_device_monitor {
    struct ty_device_monitor_;

    ty_list_head controllers;

    CRITICAL_SECTION mutex;
    int ret;
    ty_list_head notifications;
    HANDLE event;

    HANDLE thread;
    HANDLE hwnd;
};

struct ty_handle {
    ty_device *dev;

    bool block;
    HANDLE handle;
    struct _OVERLAPPED *ov;
    uint8_t *buf;

    uint8_t *ptr;
    ssize_t len;
};

struct device_type {
    char *prefix;
    const GUID *guid;
    ty_device_type type;
};

struct usb_controller {
    ty_list_head list;

    uint8_t index;
    char *id;
};

struct device_notification {
    ty_list_head list;

    ty_device_event event;
    char *key;
};

#ifdef __MINGW32__
// MinGW may miss these
__declspec(dllimport) void NTAPI HidD_GetHidGuid(LPGUID HidGuid);
__declspec(dllimport) BOOLEAN NTAPI HidD_GetSerialNumberString(HANDLE device, PVOID buffer, ULONG buffer_len);
__declspec(dllimport) BOOLEAN NTAPI HidD_GetPreparsedData(HANDLE HidDeviceObject, PHIDP_PREPARSED_DATA *PreparsedData);
__declspec(dllimport) BOOLEAN NTAPI HidD_FreePreparsedData(PHIDP_PREPARSED_DATA PreparsedData);
#endif

static const char *monitor_class_name = "ty_device_monitor";
static const size_t read_buffer_size = 1024;

static GUID hid_guid;
static const struct device_type device_types[] = {
    {"HID", &hid_guid, TY_DEVICE_HID},
    {NULL}
};

static void free_controller(struct usb_controller *controller)
{
    if (controller)
        free(controller->id);

    free(controller);
}

static void free_notification(struct device_notification *notification)
{
    if (notification)
        free(notification->key);

    free(notification);
}

static uint8_t find_controller_index(ty_list_head *controllers, const char *id)
{
    ty_list_foreach(cur, controllers) {
        struct usb_controller *usb_controller = ty_container_of(cur, struct usb_controller, list);

        if (strcmp(usb_controller->id, id) == 0)
            return usb_controller->index;
    }

    return 0;
}

static uint8_t find_device_port(DEVINST inst)
{
    char buf[256];
    DWORD len;
    CONFIGRET cret;
    uint8_t port;

    len = sizeof(buf);
    cret = CM_Get_DevNode_Registry_Property(inst, CM_DRP_LOCATION_INFORMATION, NULL, buf, &len, 0);
    if (cret != CR_SUCCESS)
        return 0;

    port = 0;
    sscanf(buf, "Port_#%04hhu", &port);

    return port;
}

static int build_location_string(uint8_t ports[], size_t depth, char **rpath)
{
    char buf[256];
    char *ptr;
    size_t size;
    char *path;
    int r;

    ptr = buf;
    size = sizeof(buf);

    strcpy(buf, "usb");
    ptr += strlen(buf);
    size -= (size_t)(ptr - buf);

    for (size_t i = 0; i < depth; i++) {
        r = snprintf(ptr, size, "-%hhu", ports[i]);
        assert(r >= 2 && (size_t)r < size);

        ptr += r;
        size -= (size_t)r;
    }

    path = strdup(buf);
    if (!path)
        return ty_error(TY_ERROR_MEMORY, NULL);

    *rpath = path;
    return 0;
}

static int resolve_device_location(DEVINST inst, ty_list_head *controllers, char **rpath)
{
    char buf[256];
    uint8_t ports[16];
    size_t depth;
    int r;

    depth = 0;

    CONFIGRET cret;
    do {
        assert(depth < TY_COUNTOF(ports));

        cret = CM_Get_Device_ID(inst, buf, sizeof(buf), 0);
        if (cret != CR_SUCCESS)
            return 0;

        if (depth && strncmp(buf, "USB\\", 4) != 0) {
            ports[depth++] = find_controller_index(controllers, buf);
            break;
        }

        ports[depth] = find_device_port(inst);
        if (ports[depth])
            depth++;

        cret = CM_Get_Parent(&inst, inst, 0);
    } while (cret == CR_SUCCESS);
    if (cret != CR_SUCCESS)
        return 0;

    for (size_t i = 0; i < depth / 2; i++) {
        uint8_t tmp = ports[i];

        ports[i] = ports[depth - i - 1];
        ports[depth - i - 1] = tmp;
    }

    r = build_location_string(ports, depth, rpath);
    if (r < 0)
        return r;

    return 1;
}

static int extract_device_info(DEVINST inst, ty_device *dev)
{
    char buf[256];
    ULONG type, len;
    DWORD capabilities;
    CONFIGRET cret;
    int r;

    do {
        cret = CM_Get_Device_ID(inst, buf, sizeof(buf), 0);
        if (cret != CR_SUCCESS)
            return 0;

        if (strncmp(buf, "USB\\", 4) == 0)
            break;

        cret = CM_Get_Parent(&inst, inst, 0);
    } while (cret == CR_SUCCESS);
    if (cret != CR_SUCCESS)
        return 0;

    dev->iface = 0;
    r = sscanf(buf, "USB\\VID_%04hx&PID_%04hx&MI_%02hhu", &dev->vid, &dev->pid, &dev->iface);
    if (r < 2)
        return 0;

    // We need the USB device for the serial number, not the interface
    if (r == 3) {
        cret = CM_Get_Parent(&inst, inst, 0);
        if (cret != CR_SUCCESS)
            return 1;

        cret = CM_Get_Device_ID(inst, buf, sizeof(buf), 0);
        if (cret != CR_SUCCESS)
            return 1;

        if (strncmp(buf, "USB\\", 4) != 0)
            return 1;
    }

    len = sizeof(capabilities);
    cret = CM_Get_DevNode_Registry_Property(inst, CM_DRP_CAPABILITIES, &type, &capabilities, &len, 0);
    if (cret != CR_SUCCESS)
        return 1;

    if (capabilities & CM_DEVCAP_UNIQUEID) {
        char *ptr = strrchr(buf, '\\');
        if (ptr) {
            dev->serial = strdup(ptr + 1);
            if (!dev->serial)
                return ty_error(TY_ERROR_MEMORY, NULL);
        }
    }

    return 1;
}

static int get_device_comport(DEVINST inst, char **rnode)
{
    HKEY key;
    char buf[32];
    DWORD type, len;
    char *node;
    CONFIGRET cret;
    LONG ret;
    int r;

    cret = CM_Open_DevNode_Key(inst, KEY_READ, 0, RegDisposition_OpenExisting, &key, CM_REGISTRY_HARDWARE);
    if (cret != CR_SUCCESS)
        return 0;

    len = (DWORD)sizeof(buf);
    ret = RegQueryValueEx(key, "PortName", NULL, &type, (BYTE *)buf, &len);
    RegCloseKey(key);
    if (ret != ERROR_SUCCESS)
        return 0;

    r = asprintf(&node, "\\\\.\\%s", buf);
    if (r < 0)
        return ty_error(TY_ERROR_MEMORY, NULL);

    *rnode = node;
    return 1;
}

static int build_device_path(const char *id, const GUID *guid, char **rpath)
{
    char *path, *ptr;

    path = malloc(4 + strlen(id) + 41);
    if (!path)
        return ty_error(TY_ERROR_MEMORY, NULL);

    ptr = stpcpy(path, "\\\\.\\");
    while (*id) {
        if (*id == '\\') {
            *ptr++ = '#';
            id++;
        } else {
            *ptr++ = *id++;
        }
    }

    sprintf(ptr, "#{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
            guid->Data1, guid->Data2, guid->Data3, guid->Data4[0], guid->Data4[1],
            guid->Data4[2], guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);

    *rpath = path;
    return 0;
}

static int find_device_node(DEVINST inst, ty_device *dev)
{
    int r;

    // GUID_DEVINTERFACE_COMPORT only works for real COM ports... Haven't found any way to
    // list virtual (USB) serial device interfaces, so instead list USB devices and consider
    // them serial if registry key "PortName" is available (and use its value as device node).
    if (strncmp(dev->key, "USB\\", 4) == 0) {
        r = get_device_comport(inst, &dev->path);
        if (r <= 0)
            return r;

        dev->type = TY_DEVICE_SERIAL;
        return 1;
    }

    for (const struct device_type *type = device_types; type->prefix; type++) {
        if (strncmp(type->prefix, dev->key, strlen(type->prefix)) == 0) {
            r = build_device_path(dev->key, type->guid, &dev->path);
            if (r < 0)
                return r;

            dev->type = type->type;
            return 1;
        }
    }

    return 0;
}

static int extract_device_id(const char *key, char **rid)
{
    char *id, *ptr;

    if (strncmp(key, "\\\\?\\", 4) == 0
            || strncmp(key, "\\\\.\\", 4) == 0
            || strncmp(key, "##.#", 4) == 0
            || strncmp(key, "##?#", 4) == 0)
        key += 4;

    id = strdup(key);
    if (!id)
        return ty_error(TY_ERROR_MEMORY, NULL);

    ptr = strrpbrk(id, "\\#");
    if (ptr && ptr[1] == '{')
        *ptr = 0;

    for (ptr = id; *ptr; ptr++) {
        *ptr = (char)toupper(*ptr);
        if (*ptr == '#')
            *ptr = '\\';
    }

    *rid = id;
    return 0;
}

static int create_device(ty_device_monitor *monitor, const char *key, DEVINST inst, uint8_t ports[], size_t depth)
{
    ty_device *dev;
    CONFIGRET cret;
    int r;

    dev = calloc(1, sizeof(*dev));
    if (!dev) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }
    dev->refcount = 1;

    if (!key) {
        char buf[256];

        cret = CM_Get_Device_ID(inst, buf, sizeof(buf), 0);
        if (cret != CR_SUCCESS) {
            r = 0;
            goto cleanup;
        }

        r = extract_device_id(buf, &dev->key);
    } else {
        r = extract_device_id(key, &dev->key);
    }
    if (r < 0)
        return r;

    if (!inst) {
        cret = CM_Locate_DevNode(&inst, dev->key, CM_LOCATE_DEVNODE_NORMAL);
        if (cret != CR_SUCCESS) {
            r = 0;
            goto cleanup;
        }
    }

    cret = CM_Locate_DevNode(&inst, dev->key, CM_LOCATE_DEVNODE_NORMAL);
    if (cret != CR_SUCCESS) {
        r = 0;
        goto cleanup;
    }

    r = extract_device_info(inst, dev);
    if (r < 0)
        goto cleanup;

    r = find_device_node(inst, dev);
    if (r <= 0)
        goto cleanup;

    if (ports) {
        r = build_location_string(ports, depth, &dev->location);
        if (r < 0)
            goto cleanup;
    } else {
        r = resolve_device_location(inst, &monitor->controllers, &dev->location);
        if (r <= 0)
            goto cleanup;
    }

    r = _ty_device_monitor_add(monitor, dev);
cleanup:
    ty_device_unref(dev);
    return r;
}

static int recurse_devices(ty_device_monitor *monitor, DEVINST inst, uint8_t ports[], size_t depth)
{
    uint8_t port;
    DEVINST child;
    CONFIGRET cret;
    int r;

    port = find_device_port(inst);
    if (port) {
        assert(depth < 16);
        ports[depth++] = port;
    }

    cret = CM_Get_Child(&child, inst, 0);
    if (cret != CR_SUCCESS)
        return create_device(monitor, NULL, inst, ports, depth);

    do {
        r = recurse_devices(monitor, child, ports, depth);
        if (r < 0)
            return r;

        cret = CM_Get_Sibling(&child, child, 0);
    } while (cret == CR_SUCCESS);

    return 0;
}

static int browse_controller_tree(ty_device_monitor *monitor, DEVINST inst, DWORD index)
{
    struct usb_controller *controller;
    char buf[256];
    uint8_t ports[16];
    CONFIGRET cret;
    int r;

    controller = calloc(1, sizeof(*controller));
    if (!controller) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    // should we worry about having more than 255 controllers?
    controller->index = (uint8_t)(index + 1);

    cret = CM_Get_Device_ID(inst, buf, sizeof(buf), 0);
    if (cret != CR_SUCCESS) {
        r = 0;
        goto error;
    }
    controller->id = strdup(buf);
    if (!controller->id) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    ports[0] = controller->index;
    r = recurse_devices(monitor, inst, ports, 1);
    if (r < 0)
        goto error;

    ty_list_add_tail(&monitor->controllers, &controller->list);

    return 0;

error:
    free_controller(controller);
    return r;
}

static int list_devices(ty_device_monitor *monitor)
{
    HDEVINFO set;
    SP_DEVINFO_DATA dev;
    int r;

    if (!hid_guid.Data4[0])
        HidD_GetHidGuid(&hid_guid);

    ty_list_foreach(cur, &monitor->controllers) {
        struct usb_controller *controller = ty_container_of(cur, struct usb_controller, list);

        ty_list_remove(&controller->list);
        free(controller);
    }

    set = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_HOST_CONTROLLER, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (!set) {
        r = ty_error(TY_ERROR_SYSTEM, "SetupDiGetClassDevs() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    dev.cbSize = sizeof(dev);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dev); i++) {
        r = browse_controller_tree(monitor, dev.DevInst, i);
        if (r < 0)
            goto cleanup;
    }

    r = 0;
cleanup:
    SetupDiDestroyDeviceInfoList(set);
    return 0;
}

static int post_device_event(ty_device_monitor *monitor, ty_device_event event, DEV_BROADCAST_DEVICEINTERFACE *data)
{
    struct device_notification *notification;
    int r;

    notification = calloc(1, sizeof(*notification));
    if (!notification)
        return ty_error(TY_ERROR_MEMORY, NULL);

    notification->event = event;
    r = extract_device_id(data->dbcc_name, &notification->key);
    if (r < 0)
        goto error;

    EnterCriticalSection(&monitor->mutex);

    ty_list_add_tail(&monitor->notifications, &notification->list);
    SetEvent(monitor->event);

    LeaveCriticalSection(&monitor->mutex);

    return 0;

error:
    free_notification(notification);
    return r;
}

static LRESULT __stdcall window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ty_device_monitor *monitor = GetWindowLongPtr(hwnd, GWLP_USERDATA);

    int r;

    switch (msg) {
    case WM_DEVICECHANGE:
        r = 0;
        switch (wparam) {
        case DBT_DEVICEARRIVAL:
            r = post_device_event(monitor, TY_DEVICE_EVENT_ADDED, (DEV_BROADCAST_DEVICEINTERFACE *)lparam);
            break;
        case DBT_DEVICEREMOVECOMPLETE:
            r = post_device_event(monitor, TY_DEVICE_EVENT_REMOVED, (DEV_BROADCAST_DEVICEINTERFACE *)lparam);
            break;
        }
        if (r < 0) {
            monitor->ret = r;
            SetEvent(monitor->event);
        }

        break;

    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static unsigned int __stdcall monitor_thread(void *udata)
{
    TY_UNUSED(udata);

    ty_device_monitor *monitor = udata;

    WNDCLASSEX cls = {0};
    DEV_BROADCAST_DEVICEINTERFACE filter = {0};
    HDEVNOTIFY notify = NULL;
    MSG msg;
    ATOM atom;
    BOOL ret;
    int r;

    cls.cbSize = sizeof(cls);
    cls.hInstance = GetModuleHandle(NULL);
    cls.lpszClassName = monitor_class_name;
    cls.lpfnWndProc = window_proc;

    atom = RegisterClassEx(&cls);
    if (!atom) {
        r = ty_error(TY_ERROR_SYSTEM, "RegisterClass() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    monitor->hwnd = CreateWindow(monitor_class_name, monitor_class_name, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    if (!monitor->hwnd) {
        r = ty_error(TY_ERROR_SYSTEM, "CreateWindow() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    SetLastError(0);
    SetWindowLongPtr(monitor->hwnd, GWLP_USERDATA, (LONG_PTR)monitor);
    if (GetLastError()) {
        r = ty_error(TY_ERROR_SYSTEM, "SetWindowLongPtr() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    notify = RegisterDeviceNotification(monitor->hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    if (!notify) {
        r = ty_error(TY_ERROR_SYSTEM, "RegisterDeviceNotification() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    SetEvent(monitor->event);

    while((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if(ret < 0) {
            r = ty_error(TY_ERROR_SYSTEM, "GetMessage() failed: %s", ty_win32_strerror(0));
            goto cleanup;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    r = 0;
cleanup:
    if (notify)
        UnregisterDeviceNotification(notify);
    if (monitor->hwnd)
        DestroyWindow(monitor->hwnd);
    UnregisterClass(monitor_class_name, NULL);
    if (r < 0) {
        monitor->ret = r;
        SetEvent(monitor->event);
    }
    return 0;
}

static int wait_event(HANDLE event)
{
    DWORD ret;

    ret = WaitForSingleObject(event, INFINITE);
    if (ret != WAIT_OBJECT_0)
        return ty_error(TY_ERROR_SYSTEM, "WaitForSingleObject() failed: %s", ty_win32_strerror(0));

    return 0;
}

int ty_device_monitor_new(ty_device_monitor **rmonitor)
{
    assert(rmonitor);

    ty_device_monitor *monitor;
    int r;

    if (!ty_win32_test_version(TY_WIN32_VISTA))
        return ty_error(TY_ERROR_UNSUPPORTED, "Device monitor requires at least Windows Vista to work");

    monitor = calloc(1, sizeof(*monitor));
    if (!monitor)
        return ty_error(TY_ERROR_MEMORY, NULL);

    ty_list_init(&monitor->controllers);

    ty_list_init(&monitor->notifications);
    InitializeCriticalSection(&monitor->mutex);
    monitor->event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!monitor->event) {
        r = ty_error(TY_ERROR_SYSTEM, "CreateEvent() failed: %s", ty_win32_strerror(0));
        goto error;
    }

    r = _ty_device_monitor_init(monitor);
    if (r < 0)
        goto error;

    r = list_devices(monitor);
    if (r < 0)
        goto error;

    monitor->thread = (HANDLE)_beginthreadex(NULL, 0, monitor_thread, monitor, 0, NULL);
    if (!monitor->thread)
        return ty_error(TY_ERROR_SYSTEM, "_beginthreadex() failed: %s", ty_win32_strerror(0));

    r = wait_event(monitor->event);
    if (r < 0)
        goto error;
    if (monitor->ret < 0) {
        r = monitor->ret;
        goto error;
    }

    ResetEvent(monitor->event);

    *rmonitor = monitor;
    return 0;

error:
    ty_device_monitor_free(monitor);
    return r;
}

void ty_device_monitor_free(ty_device_monitor *monitor)
{
    if (monitor) {
        _ty_device_monitor_release(monitor);

        if (monitor->thread) {
            if (monitor->hwnd) {
                PostMessage(monitor->hwnd, WM_CLOSE, 0, 0);
                WaitForSingleObject(monitor->thread, INFINITE);
            }
            CloseHandle(monitor->thread);
        }

        ty_list_foreach(cur, &monitor->controllers) {
            struct usb_controller *controller = ty_container_of(cur, struct usb_controller, list);
            free_controller(controller);
        }

        DeleteCriticalSection(&monitor->mutex);
        if (monitor->event)
            CloseHandle(monitor->event);

        ty_list_foreach(cur, &monitor->notifications) {
            struct device_notification *notification = ty_container_of(cur, struct device_notification, list);
            free_notification(notification);
        }
    }

    free(monitor);
}

void ty_device_monitor_get_descriptors(const ty_device_monitor *monitor, ty_descriptor_set *set, int id)
{
    assert(monitor);
    assert(set);

    ty_descriptor_set_add(set, monitor->event, id);
}

int ty_device_monitor_refresh(ty_device_monitor *monitor)
{
    assert(monitor);

    ty_list_head notifications;
    int r;

    EnterCriticalSection(&monitor->mutex);

    ty_list_replace(&monitor->notifications, &notifications);
    r = monitor->ret;

    LeaveCriticalSection(&monitor->mutex);

    if (r < 0)
        goto cleanup;

    ty_list_foreach(cur, &notifications) {
        struct device_notification *notification = ty_container_of(cur, struct device_notification, list);

        r = 0;
        switch (notification->event) {
        case TY_DEVICE_EVENT_ADDED:
            r = create_device(monitor, notification->key, 0, NULL, 0);
            break;

        case TY_DEVICE_EVENT_REMOVED:
            _ty_device_monitor_remove(monitor, notification->key);
            r = 0;
            break;
        }

        ty_list_remove(&notification->list);
        free_notification(notification);

        if (r < 0)
            goto cleanup;
    }

    r = 0;
cleanup:
    EnterCriticalSection(&monitor->mutex);

    ty_list_splice(&monitor->notifications, &notifications);
    if (ty_list_empty(&monitor->notifications))
        ResetEvent(monitor->event);

    LeaveCriticalSection(&monitor->mutex);
    return r;
}

int ty_device_open(ty_device *dev, bool block, ty_handle **rh)
{
    assert(rh);
    assert(dev);

    ty_handle *h = NULL;
    DWORD len;
    COMMTIMEOUTS timeouts;
    int r;

    h = calloc(1, sizeof(*h));
    if (!h)
        return ty_error(TY_ERROR_MEMORY, NULL);

    h->handle = CreateFile(dev->path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (h->handle == INVALID_HANDLE_VALUE) {
        switch (GetLastError()) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            r = ty_error(TY_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            r = ty_error(TY_ERROR_MEMORY, NULL);
            break;
        case ERROR_ACCESS_DENIED:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied for device '%s'", dev->path);
            break;

        default:
            r = ty_error(TY_ERROR_SYSTEM, "CreateFile('%s') failed: %s", dev->path,
                         ty_win32_strerror(0));
            break;
        }
        goto error;
    }

    h->ov = calloc(1, sizeof(*h->ov));
    if (!h->ov) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    h->ov->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!h->ov->hEvent) {
        r = ty_error(TY_ERROR_SYSTEM, "CreateEvent() failed: %s", ty_win32_strerror(0));
        goto error;
    }

    h->buf = malloc(read_buffer_size);
    if (!h->buf) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    h->block = block;

    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 1000;

    SetCommTimeouts(h->handle, &timeouts);

    r = ReadFile(h->handle, h->buf, read_buffer_size, &len, h->ov);
    if (!r && GetLastError() != ERROR_IO_PENDING) {
        r = ty_error(TY_ERROR_SYSTEM, "ReadFile() failed: %s", ty_win32_strerror(0));
        goto error;
    }

    h->dev = ty_device_ref(dev);

    *rh = h;
    return 0;

error:
    ty_device_close(h);
    return r;
}

void ty_device_close(ty_handle *h)
{
    if (h) {
        if (h->handle)
            CloseHandle(h->handle);
        if (h->ov && h->ov->hEvent)
            CloseHandle(h->ov->hEvent);
        free(h->ov);
        free(h->buf);
        ty_device_unref(h->dev);
    }

    free(h);
}

void ty_device_get_descriptors(const ty_handle *h, ty_descriptor_set *set, int id)
{
    assert(h);
    assert(set);

    ty_descriptor_set_add(set, h->ov->hEvent, id);
}

int ty_hid_parse_descriptor(ty_handle *h, ty_hid_descriptor *desc)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_HID);
    assert(desc);

    // semi-hidden Hungarian pointers? Really , Microsoft?
    PHIDP_PREPARSED_DATA pp;
    HIDP_CAPS caps;
    LONG ret;

    ret = HidD_GetPreparsedData(h->handle, &pp);
    if (!ret)
        return ty_error(TY_ERROR_SYSTEM, "HidD_GetPreparsedData() failed");

    // NTSTATUS and BOOL are both defined as LONG
    ret = HidP_GetCaps(pp, &caps);
    HidD_FreePreparsedData(pp);
    if (ret != HIDP_STATUS_SUCCESS)
        return ty_error(TY_ERROR_PARSE, "Invalid HID descriptor");

    desc->usage = caps.Usage;
    desc->usage_page = caps.UsagePage;

    return 0;
}

ssize_t ty_hid_read(ty_handle *h, uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_HID);
    assert(buf);
    assert(size);

    DWORD len, ret;

    ret = (DWORD)GetOverlappedResult(h->handle, h->ov, &len, h->block);
    if (!ret) {
        if (GetLastError() == ERROR_IO_PENDING)
            return 0;
        return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->path);
    }

    if (len) {
        if (h->buf[0]) {
            if (size > len)
                size = (size_t)len;
            memcpy(buf, h->buf, size);
        } else {
            if (size > --len)
                size = (size_t)len;
            memcpy(buf, h->buf + 1, size);
        }
    } else {
        size = 0;
    }

    ResetEvent(h->ov->hEvent);
    ret = (DWORD)ReadFile(h->handle, h->buf, read_buffer_size, NULL, h->ov);
    if (!ret && GetLastError() != ERROR_IO_PENDING) {
        CancelIo(h->handle);
        return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->path);
    }

    return (ssize_t)size;
}

ssize_t ty_hid_write(ty_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_HID);
    assert(buf);

    if (size < 2)
        return 0;

    OVERLAPPED ov = {0};
    DWORD len;
    BOOL r;

    r = WriteFile(h->handle, buf, (DWORD)size, &len, &ov);
    if (!r) {
        if (GetLastError() != ERROR_IO_PENDING) {
            CancelIo(h->handle);
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
        }

        r = GetOverlappedResult(h->handle, &ov, &len, TRUE);
        if (!r)
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
    }

    return (ssize_t)len;
}

int ty_hid_send_feature_report(ty_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_HID);
    assert(buf);

    if (size < 2)
        return 0;

    // Timeout behavior?
    BOOL r = HidD_SetFeature(h->handle, (char *)buf, (DWORD)size);
    if (!r)
        return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);

    return 1;
}

int ty_serial_set_control(ty_handle *h, uint32_t rate, uint16_t flags)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_SERIAL);

    DCB dcb;
    BOOL r;

    dcb.DCBlength = sizeof(dcb);

    r = GetCommState(h->handle, &dcb);
    if (!r)
        return ty_error(TY_ERROR_SYSTEM, "GetCommState() failed: %s", ty_win32_strerror(0));

    switch (rate) {
    case 0:
    case 50:
    case 75:
    case 110:
    case 134:
    case 150:
    case 200:
    case 300:
    case 600:
    case 1200:
    case 1800:
    case 2400:
    case 4800:
    case 9600:
    case 19200:
    case 38400:
    case 57600:
    case 115200:
        dcb.BaudRate = rate;
        break;

    default:
        assert(false);
    }

    switch (flags & TY_SERIAL_CSIZE_MASK) {
    case TY_SERIAL_5BITS_CSIZE:
        dcb.ByteSize = 5;
        break;
    case TY_SERIAL_6BITS_CSIZE:
        dcb.ByteSize = 6;
        break;
    case TY_SERIAL_7BITS_CSIZE:
        dcb.ByteSize = 7;
        break;

    default:
        dcb.ByteSize = 8;
        break;
    }

    switch (flags & TY_SERIAL_PARITY_MASK) {
    case 0:
        dcb.fParity = FALSE;
        dcb.Parity = NOPARITY;
        break;
    case TY_SERIAL_ODD_PARITY:
        dcb.fParity = TRUE;
        dcb.Parity = ODDPARITY;
        break;
    case TY_SERIAL_EVEN_PARITY:
        dcb.fParity = TRUE;
        dcb.Parity = EVENPARITY;
        break;

    default:
        assert(false);
    }

    dcb.StopBits = 0;
    if (flags & TY_SERIAL_2BITS_STOP)
        dcb.StopBits = TWOSTOPBITS;

    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    switch (flags & TY_SERIAL_FLOW_MASK) {
    case 0:
        break;
    case TY_SERIAL_XONXOFF_FLOW:
        dcb.fOutX = TRUE;
        dcb.fInX = TRUE;
        break;
    case TY_SERIAL_RTSCTS_FLOW:
        dcb.fOutxCtsFlow = TRUE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        break;

    default:
        assert(false);
    }

    r = SetCommState(h->handle, &dcb);
    if (!r)
        return ty_error(TY_ERROR_SYSTEM, "SetCommState() failed: %s", ty_win32_strerror(0));

    return 0;
}

ssize_t ty_serial_read(ty_handle *h, char *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_SERIAL);
    assert(buf);
    assert(size);

    DWORD len, ret;

    if (h->len < 0) {
        h->len = 0;

        // Could be a transient error, try to restart it
        ResetEvent(h->ov->hEvent);
        ret = (DWORD)ReadFile(h->handle, h->buf, read_buffer_size, NULL, h->ov);
        if (!ret && GetLastError() != ERROR_IO_PENDING) {
            CancelIo(h->handle);
            h->len = -1;
        }

        return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->path);
    }

    if (!h->len) {
        ret = (DWORD)GetOverlappedResult(h->handle, h->ov, &len, h->block);
        if (!ret) {
            if (GetLastError() == ERROR_IO_PENDING)
                return 0;
            return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->path);
        }

        h->ptr = h->buf;
        h->len = (ssize_t)len;
    }

    if (size > (size_t)h->len)
        size = (size_t)h->len;

    memcpy(buf, h->ptr, size);
    h->ptr += size;
    h->len -= (ssize_t)size;

    if (!h->len) {
        ResetEvent(h->ov->hEvent);
        ret = (DWORD)ReadFile(h->handle, h->buf, read_buffer_size, NULL, h->ov);
        if (!ret && GetLastError() != ERROR_IO_PENDING) {
            CancelIo(h->handle);
            h->len = -1;
        }
    }

    return (ssize_t)size;
}

ssize_t ty_serial_write(ty_handle *h, const char *buf, ssize_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_SERIAL);
    assert(buf);

    if (size < 0)
        size = (ssize_t)strlen(buf);
    if (!size)
        return 0;

    OVERLAPPED ov = {0};
    DWORD len;
    BOOL r;

    r = WriteFile(h->handle, buf, (DWORD)size, &len, &ov);
    if (!r) {
        if (GetLastError() != ERROR_IO_PENDING) {
            CancelIo(h->handle);
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
        }

        r = GetOverlappedResult(h->handle, &ov, &len, TRUE);
        if (!r)
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
    }

    return (ssize_t)len;
}
