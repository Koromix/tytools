/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "ty/common.h"
#include "compat.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfgmgr32.h>
#include <dbt.h>
#include <devioctl.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <process.h>
#include <setupapi.h>
#include <usb.h>
#include <usbioctl.h>
#include <usbuser.h>
#include "ty/device.h"
#include "device_priv.h"
#include "ty/system.h"

struct tyd_monitor {
    TYD_MONITOR

    ty_list_head controllers;

    CRITICAL_SECTION mutex;
    int ret;
    ty_list_head notifications;
    HANDLE event;

    HANDLE thread;
    HANDLE hwnd;
};

struct tyd_handle {
    TYD_HANDLE

    HANDLE handle;
    struct _OVERLAPPED *ov;
    uint8_t *buf;
    DWORD pending_thread;

    uint8_t *ptr;
    ssize_t len;
};

struct device_type {
    char *prefix;
    const GUID *guid;
    tyd_device_type type;
};

struct usb_controller {
    ty_list_head list;

    uint8_t index;
    char *roothub_id;
};

struct device_notification {
    ty_list_head list;

    tyd_monitor_event event;
    char *key;
};

typedef BOOL WINAPI CancelIoEx_func(HANDLE hFile, LPOVERLAPPED lpOverlapped);

#ifdef __MINGW32__
// MinGW may miss these
__declspec(dllimport) void NTAPI HidD_GetHidGuid(LPGUID HidGuid);
__declspec(dllimport) BOOLEAN NTAPI HidD_GetSerialNumberString(HANDLE device, PVOID buffer, ULONG buffer_len);
__declspec(dllimport) BOOLEAN NTAPI HidD_GetPreparsedData(HANDLE HidDeviceObject, PHIDP_PREPARSED_DATA *PreparsedData);
__declspec(dllimport) BOOLEAN NTAPI HidD_FreePreparsedData(PHIDP_PREPARSED_DATA PreparsedData);
#endif

static CancelIoEx_func *CancelIoEx_;

#define MAX_USB_DEPTH 8
#define MONITOR_CLASS_NAME "tyd_monitor"

#define READ_BUFFER_SIZE 16384

static GUID hid_guid;
static const struct device_type device_types[] = {
    {"HID", &hid_guid, TYD_DEVICE_HID},
    {NULL}
};

static const struct _tyd_device_vtable win32_device_vtable;

TY_INIT()
{
    CancelIoEx_ = (CancelIoEx_func *)GetProcAddress(GetModuleHandle("kernel32.dll"), "CancelIoEx");
    return 0;
}

static void free_controller(struct usb_controller *controller)
{
    if (controller)
        free(controller->roothub_id);

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
        struct usb_controller *controller = ty_container_of(cur, struct usb_controller, list);

        if (strcmp(controller->roothub_id, id) == 0)
            return controller->index;
    }

    return 0;
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

static uint8_t find_device_port_vista(DEVINST inst)
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

static int build_location_string(uint8_t ports[], unsigned int depth, char **rpath)
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

static int wide_to_cstring(const wchar_t *wide, size_t size, char **rs)
{
    wchar_t *tmp = NULL;
    char *s = NULL;
    int len, r;

    tmp = calloc(1, size + sizeof(wchar_t));
    if (!tmp)
        return ty_error(TY_ERROR_MEMORY, NULL);

    memcpy(tmp, wide, size);

    len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, tmp, -1, NULL, 0, NULL, NULL);
    if (!len) {
        r = ty_error(TY_ERROR_PARSE, "Failed to convert UTF-16 string to local codepage: %s",
                     ty_win32_strerror(0));
        goto cleanup;
    }

    s = malloc((size_t)len);
    if (!s) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, tmp, -1, s, len, NULL, NULL);
    if (!len) {
        r = ty_error(TY_ERROR_PARSE, "Failed to convert UTF-16 string to local codepage: %s",
                     ty_win32_strerror(0));
        goto cleanup;
    }

    *rs = s;
    s = NULL;

    r = 0;
cleanup:
    free(s);
    free(tmp);
    return r;
}

static int get_port_driverkey(HANDLE hub, uint8_t port, char **rkey)
{
    DWORD len;
    USB_NODE_CONNECTION_INFORMATION_EX *node;
    USB_NODE_CONNECTION_DRIVERKEY_NAME pseudo = {0};
    USB_NODE_CONNECTION_DRIVERKEY_NAME *wide = NULL;
    BOOL success;
    int r;

    len = sizeof(node) + (sizeof(USB_PIPE_INFO) * 30);
    node = calloc(1, len);
    if (!node) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    node->ConnectionIndex = port;
    pseudo.ConnectionIndex = port;

    success = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, node, len,
                              node, len, &len, NULL);
    if (!success) {
        r = 0;
        goto cleanup;
    }

    if (node->ConnectionStatus != DeviceConnected) {
        r = 0;
        goto cleanup;
    }

    success = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME, &pseudo, sizeof(pseudo),
                              &pseudo, sizeof(pseudo), &len, NULL);
    if (!success) {
        r = 0;
        goto cleanup;
    }

    wide = calloc(1, pseudo.ActualLength);
    if (!wide) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    wide->ConnectionIndex = port;

    success = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME, wide, pseudo.ActualLength,
                              wide, pseudo.ActualLength, &len, NULL);
    if (!success) {
        r = 0;
        goto cleanup;
    }

    r = wide_to_cstring(wide->DriverKeyName, len - sizeof(pseudo) + 1, rkey);
    if (r < 0)
        goto cleanup;

    r = 1;
cleanup:
    free(wide);
    free(node);
    return r;
}

static int find_device_port_xp(const char *hub_id, const char *child_key)
{
    char *path = NULL;
    HANDLE h = NULL;
    USB_NODE_INFORMATION node;
    DWORD len;
    BOOL success;
    int r;

    r = build_device_path(hub_id, &GUID_DEVINTERFACE_USB_HUB, &path);
    if (r < 0)
        goto cleanup;

    h = CreateFile(path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (!h) {
        r = ty_error(TY_ERROR_SYSTEM, "Failed to open USB hub '%s': %s", path, ty_win32_strerror(0));
        goto cleanup;
    }

    success = DeviceIoControl(h, IOCTL_USB_GET_NODE_INFORMATION, NULL, 0, &node, sizeof(node),
                              &len, NULL);
    if (!success) {
        r = 0;
        goto cleanup;
    }

    for (uint8_t port = 1; port <= node.u.HubInformation.HubDescriptor.bNumberOfPorts; port++) {
        char *key = NULL;

        r = get_port_driverkey(h, port, &key);
        if (r < 0)
            goto cleanup;
        if (!r)
            continue;

        if (strcmp(key, child_key) == 0) {
            free(key);

            r = port;
            break;
        } else {
            free(key);
        }
    }

cleanup:
    if (h)
        CloseHandle(h);
    free(path);
    return r;
}

static int resolve_device_location(DEVINST inst, ty_list_head *controllers, char **rpath)
{
    DEVINST parent;
    char id[256];
    uint8_t ports[MAX_USB_DEPTH];
    unsigned int depth;
    CONFIGRET cret;
    int r;

    // skip nodes until we get to the USB ones
    parent = inst;
    do {
        inst = parent;

        cret = CM_Get_Device_ID(inst, id, sizeof(id), 0);
        if (cret != CR_SUCCESS)
            return 0;

        cret = CM_Get_Parent(&parent, inst, 0);
    } while (cret == CR_SUCCESS && strncmp(id, "USB\\", 4) != 0);
    if (cret != CR_SUCCESS)
        return 0;

    depth = 0;
    do {
        if (depth == MAX_USB_DEPTH) {
            ty_error(TY_ERROR_SYSTEM, "Excessive USB location depth");
            return 0;
        }

        cret = CM_Get_Device_ID(parent, id, sizeof(id), 0);
        if (cret != CR_SUCCESS)
            return 0;

        // Test for Vista, CancelIoEx() is needed elsewhere so no need for VerifyVersionInfo()
        if (CancelIoEx_) {
            r = find_device_port_vista(inst);
        } else {
            char child_key[256];
            DWORD len;

            len = sizeof(child_key);
            cret = CM_Get_DevNode_Registry_Property(inst, CM_DRP_DRIVER, NULL, child_key, &len, 0);
            if (cret != CR_SUCCESS)
                return 0;

            r = find_device_port_xp(id, child_key);
        }
        if (r < 0)
            return r;
        if (r)
            ports[depth++] = (uint8_t)r;

        if (strstr(id, "\\ROOT_HUB")) {
            if (!depth)
                return 0;

            ports[depth++] = find_controller_index(controllers, id);
            break;
        }

        inst = parent;
        cret = CM_Get_Parent(&parent, parent, 0);
    } while (cret == CR_SUCCESS);

    // The ports are in the wrong order
    for (unsigned int i = 0; i < depth / 2; i++) {
        uint8_t tmp = ports[i];
        ports[i] = ports[depth - i - 1];
        ports[depth - i - 1] = tmp;
    }

    r = build_location_string(ports, depth, rpath);
    if (r < 0)
        return r;

    return 1;
}

static int extract_device_info(DEVINST inst, tyd_device *dev)
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

    /* If the string is stored without a terminating NUL, the buffer won't have it either.
       Microsoft fixed it with RegGetValue(), but this function requires Vista. */
    if (buf[--len])
        buf[len + 1] = 0;

    // You need the \\.\ prefix to open COM ports beyond COM9
    r = asprintf(&node, "%s%s", len > 4 ? "\\\\.\\" : "", buf);
    if (r < 0)
        return ty_error(TY_ERROR_MEMORY, NULL);

    *rnode = node;
    return 1;
}

static int find_device_node(DEVINST inst, tyd_device *dev)
{
    int r;

    /* GUID_DEVINTERFACE_COMPORT only works for real COM ports... Haven't found any way to
       list virtual (USB) serial device interfaces, so instead list USB devices and consider
       them serial if registry key "PortName" is available (and use its value as device node). */
    if (strncmp(dev->key, "USB\\", 4) == 0) {
        r = get_device_comport(inst, &dev->path);
        if (r <= 0)
            return r;

        dev->type = TYD_DEVICE_SERIAL;
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

static int create_device(tyd_monitor *monitor, const char *id, DEVINST inst, uint8_t ports[], unsigned int depth)
{
    tyd_device *dev;
    CONFIGRET cret;
    int r;

    dev = calloc(1, sizeof(*dev));
    if (!dev) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }
    dev->refcount = 1;

    r = extract_device_id(id, &dev->key);
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

    /* ports is used to make the USB location string, but you can pass NULL. create_device() is
       called in two contexts:
       - when listing USB devices, we've already got the ports used to find the device so we pass
         it to this function
       - when a device is added, we know nothing so create_device() has to walk up the device tree
         to identify ports, see resolve_device_location() */
    if (ports) {
        r = build_location_string(ports, depth, &dev->location);
        if (r < 0)
            goto cleanup;
    } else {
        r = resolve_device_location(inst, &monitor->controllers, &dev->location);
        if (r <= 0)
            goto cleanup;
    }

    dev->vtable = &win32_device_vtable;

    r = _tyd_monitor_add(monitor, dev);
cleanup:
    tyd_device_unref(dev);
    return r;
}

static int recurse_devices(tyd_monitor *monitor, DEVINST inst, uint8_t ports[], unsigned int depth)
{
    char id[256];
    DEVINST child;
    CONFIGRET cret;
    int r;

    if (depth == MAX_USB_DEPTH) {
        ty_error(TY_ERROR_SYSTEM, "Excessive USB location depth");
        return 0;
    }

    cret = CM_Get_Device_ID(inst, id, sizeof(id), 0);
    if (cret != CR_SUCCESS)
        return 0;

    cret = CM_Get_Child(&child, inst, 0);
    // Leaf = actual device, so just try to create a device struct for it
    if (cret != CR_SUCCESS)
        return create_device(monitor, id, inst, ports, depth);

    do {
        // Test for Vista, CancelIoEx() is needed elsewhere so no need for VerifyVersionInfo()
        if (CancelIoEx_) {
            r = find_device_port_vista(child);
        } else {
            char key[256];
            DWORD len;

            len = sizeof(key);
            cret = CM_Get_DevNode_Registry_Property(child, CM_DRP_DRIVER, NULL, key, &len, 0);
            if (cret != CR_SUCCESS)
                return 0;

            r = find_device_port_xp(id, key);
        }
        if (r < 0)
            return r;

        ports[depth] = (uint8_t)r;
        r = recurse_devices(monitor, child, ports, depth + !!r);
        if (r < 0)
            return r;

        cret = CM_Get_Sibling(&child, child, 0);
    } while (cret == CR_SUCCESS);

    return 0;
}

static int browse_controller_tree(tyd_monitor *monitor, DEVINST inst, DWORD index)
{
    struct usb_controller *controller;
    DEVINST roothub_inst;
    char buf[256];
    uint8_t ports[MAX_USB_DEPTH];
    CONFIGRET cret;
    int r;

    controller = calloc(1, sizeof(*controller));
    if (!controller) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    // should we worry about having more than 255 controllers?
    controller->index = (uint8_t)(index + 1);

    cret = CM_Get_Child(&roothub_inst, inst, 0);
    if (cret != CR_SUCCESS) {
        r = 0;
        goto error;
    }

    cret = CM_Get_Device_ID(roothub_inst, buf, sizeof(buf), 0);
    if (cret != CR_SUCCESS || !strstr(buf, "\\ROOT_HUB")) {
        r = 0;
        goto error;
    }
    controller->roothub_id = strdup(buf);
    if (!controller->roothub_id) {
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

/* The principles here are simple, they're just hidden behind ugly Win32 APIs. Basically:
   - list USB controllers and assign them a controller ID (1, 2, etc.), this will be the first
     port number is the USB location string
   - for each controller, browse the device tree recursively. The port number for each hub/device
     comes from the device registry (Vista and later) or asking hubs about it (XP) */
static int list_devices(tyd_monitor *monitor)
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

static int post_device_event(tyd_monitor *monitor, tyd_monitor_event event, DEV_BROADCAST_DEVICEINTERFACE *data)
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
    tyd_monitor *monitor = (tyd_monitor *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    int r;

    switch (msg) {
    case WM_DEVICECHANGE:
        r = 0;
        switch (wparam) {
        case DBT_DEVICEARRIVAL:
            r = post_device_event(monitor, TYD_MONITOR_EVENT_ADDED, (DEV_BROADCAST_DEVICEINTERFACE *)lparam);
            break;
        case DBT_DEVICEREMOVECOMPLETE:
            r = post_device_event(monitor, TYD_MONITOR_EVENT_REMOVED, (DEV_BROADCAST_DEVICEINTERFACE *)lparam);
            break;
        }
        if (r < 0) {
            EnterCriticalSection(&monitor->mutex);

            monitor->ret = r;
            SetEvent(monitor->event);

            LeaveCriticalSection(&monitor->mutex);
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

    tyd_monitor *monitor = udata;

    WNDCLASSEX cls = {0};
    DEV_BROADCAST_DEVICEINTERFACE filter = {0};
    HDEVNOTIFY notify = NULL;
    MSG msg;
    ATOM atom;
    BOOL ret;
    int r;

    cls.cbSize = sizeof(cls);
    cls.hInstance = GetModuleHandle(NULL);
    cls.lpszClassName = MONITOR_CLASS_NAME;
    cls.lpfnWndProc = window_proc;

    atom = RegisterClassEx(&cls);
    if (!atom) {
        r = ty_error(TY_ERROR_SYSTEM, "RegisterClass() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    monitor->hwnd = CreateWindow(MONITOR_CLASS_NAME, MONITOR_CLASS_NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
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

    /* Our fake window is created and ready to receive device notifications,
       tyd_monitor_new() can go on. */
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
    UnregisterClass(MONITOR_CLASS_NAME, NULL);
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

/* Monitoring device changes on Windows involves a window to receive device notifications on the
   thread message queue. Unfortunately we can't poll on message queues so instead, we make a
   background thread to get device notifications, and tell us about it using Win32 events which
   we can poll. */
int tyd_monitor_new(tyd_monitor **rmonitor)
{
    assert(rmonitor);

    tyd_monitor *monitor;
    int r;

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

    r = _tyd_monitor_init(monitor);
    if (r < 0)
        goto error;

    r = list_devices(monitor);
    if (r < 0)
        goto error;

    /* We can't create our fake window here, because the messages would be posted to this thread's
       message queue and not to the monitoring thread. So instead, the background thread creates
       its own window and we wait for it to signal us before we continue. */
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
    tyd_monitor_free(monitor);
    return r;
}

void tyd_monitor_free(tyd_monitor *monitor)
{
    if (monitor) {
        _tyd_monitor_release(monitor);

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

void tyd_monitor_get_descriptors(const tyd_monitor *monitor, ty_descriptor_set *set, int id)
{
    assert(monitor);
    assert(set);

    ty_descriptor_set_add(set, monitor->event, id);
}

int tyd_monitor_refresh(tyd_monitor *monitor)
{
    assert(monitor);

    ty_list_head notifications;
    int r;

    EnterCriticalSection(&monitor->mutex);

    /* We don't want to keep the lock for too long, so move all notifications to our own list
       and let the background thread work and process Win32 events. */
    ty_list_replace(&monitor->notifications, &notifications);

    r = monitor->ret;
    monitor->ret = 0;

    LeaveCriticalSection(&monitor->mutex);

    if (r < 0)
        goto cleanup;

    ty_list_foreach(cur, &notifications) {
        struct device_notification *notification = ty_container_of(cur, struct device_notification, list);

        r = 0;
        switch (notification->event) {
        case TYD_MONITOR_EVENT_ADDED:
            r = create_device(monitor, notification->key, 0, NULL, 0);
            break;

        case TYD_MONITOR_EVENT_REMOVED:
            _tyd_monitor_remove(monitor, notification->key);
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

    /* If an error occurs, there maty be unprocessed notifications. We don't want to lose them so
       put everything back in front of the notification list. */
    ty_list_splice(&monitor->notifications, &notifications);
    if (ty_list_is_empty(&monitor->notifications))
        ResetEvent(monitor->event);

    LeaveCriticalSection(&monitor->mutex);
    return r;
}

static int start_async_read(tyd_handle *h)
{
    DWORD ret;

    ret = (DWORD)ReadFile(h->handle, h->buf, READ_BUFFER_SIZE, NULL, h->ov);
    if (!ret && GetLastError() != ERROR_IO_PENDING) {
        CancelIo(h->handle);
        return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->path);
    }

    h->pending_thread = GetCurrentThreadId();
    return 0;
}

static ssize_t finalize_async_read(tyd_handle *h, int timeout)
{
    DWORD len, ret;

    if (timeout > 0)
        WaitForSingleObject(h->ov->hEvent, (DWORD)timeout);

    ret = (DWORD)GetOverlappedResult(h->handle, h->ov, &len, timeout < 0);
    if (!ret) {
        if (GetLastError() == ERROR_IO_INCOMPLETE)
            return 0;

        h->pending_thread = 0;
        return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->path);
    }
    h->pending_thread = 0;

    return (ssize_t)len;
}

static int open_win32_device(tyd_device *dev, tyd_handle **rh)
{
    tyd_handle *h = NULL;
    COMMTIMEOUTS timeouts;
    int r;

    h = calloc(1, sizeof(*h));
    if (!h)
        return ty_error(TY_ERROR_MEMORY, NULL);
    h->dev = tyd_device_ref(dev);

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

    h->buf = malloc(READ_BUFFER_SIZE);
    if (!h->buf) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 1000;

    SetCommTimeouts(h->handle, &timeouts);

    if (dev->type == TYD_DEVICE_SERIAL)
        EscapeCommFunction(h->handle, SETDTR);

    start_async_read(h);

    *rh = h;
    return 0;

error:
    tyd_device_close(h);
    return r;
}

static void close_win32_device(tyd_handle *h);
static unsigned int __stdcall overlapped_cleanup_thread(void *udata)
{
    tyd_handle *h = udata;
    DWORD ret;

    /* Give up if nothing happens, even if it means a leak; we'll get rid of this when XP
       becomes irrelevant anyway. Hope this happens within my lifetime. */
    ret = WaitForSingleObject(h->ov->hEvent, 120000);
    if (ret != WAIT_OBJECT_0) {
        ty_error(TY_ERROR_SYSTEM, "Cannot stop asynchronous read request, leaking handle and memory");
        return 0;
    }

    h->pending_thread = 0;
    close_win32_device(h);

    return 0;
}

static void close_win32_device(tyd_handle *h)
{
    if (h) {
        tyd_device_unref(h->dev);
        h->dev = NULL;

        if (h->pending_thread) {
            if (CancelIoEx_) {
                CancelIoEx_(h->handle, NULL);
            } else if (h->pending_thread == GetCurrentThreadId()) {
                CancelIo(h->handle);
            } else {
                CloseHandle(h->handle);
                h->handle = NULL;

                /* CancelIoEx does not exist on XP, so instead we create a new thread to
                   cleanup when pending I/O stops. And if the thread cannot be created or
                   the kernel does not set h->ov->hEvent, just leaking seems better than a
                   potential segmentation fault. */
                HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, overlapped_cleanup_thread,
                                                       h, 0, NULL);
                if (thread)
                    CloseHandle(thread);

                return;
            }
        }

        if (h->handle)
            CloseHandle(h->handle);

        free(h->buf);
        if (h->ov && h->ov->hEvent)
            CloseHandle(h->ov->hEvent);
        free(h->ov);
    }

    free(h);
}

static void get_win32_descriptors(const tyd_handle *h, ty_descriptor_set *set, int id)
{
    ty_descriptor_set_add(set, h->ov->hEvent, id);
}

static const struct _tyd_device_vtable win32_device_vtable = {
    .open = open_win32_device,
    .close = close_win32_device,

    .get_descriptors = get_win32_descriptors
};

int tyd_hid_parse_descriptor(tyd_handle *h, tyd_hid_descriptor *desc)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_HID);
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

ssize_t tyd_hid_read(tyd_handle *h, uint8_t *buf, size_t size, int timeout)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_HID);
    assert(buf);
    assert(size);

    if (h->len < 0) {
        // Could be a transient error, try to restart it
        h->len = start_async_read(h);
        if (h->len < 0)
            return h->len;
    }

    h->len = finalize_async_read(h, timeout);
    if (h->len <= 0)
        return h->len;

    /* HID communication is message-based. So if the caller does not provide a big enough
       buffer, we can just discard the extra data, unlike for serial communication. */
    if (h->len) {
        if (h->buf[0]) {
            if (size > (size_t)h->len)
                size = (size_t)h->len;
            memcpy(buf, h->buf, size);
        } else {
            if (size > (size_t)--h->len)
                size = (size_t)h->len;
            memcpy(buf, h->buf + 1, size);
        }
    } else {
        size = 0;
    }

    ty_error_mask(TY_ERROR_IO);
    h->len = start_async_read(h);
    ty_error_unmask();

    return (ssize_t)size;
}

ssize_t tyd_hid_write(tyd_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_HID);
    assert(buf);

    if (size < 2)
        return 0;

    OVERLAPPED ov = {0};
    DWORD len;
    BOOL success;

    success = WriteFile(h->handle, buf, (DWORD)size, &len, &ov);
    if (!success) {
        if (GetLastError() != ERROR_IO_PENDING) {
            CancelIo(h->handle);
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
        }

        success = GetOverlappedResult(h->handle, &ov, &len, TRUE);
        if (!success)
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
    }

    return (ssize_t)len;
}

ssize_t tyd_hid_send_feature_report(tyd_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_HID);
    assert(buf);

    if (size < 2)
        return 0;

    // Timeout behavior?
    BOOL success = HidD_SetFeature(h->handle, (char *)buf, (DWORD)size);
    if (!success)
        return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);

    return (ssize_t)size;
}

int tyd_serial_set_attributes(tyd_handle *h, uint32_t rate, int flags)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_SERIAL);

    DCB dcb;
    BOOL success;

    dcb.DCBlength = sizeof(dcb);

    success = GetCommState(h->handle, &dcb);
    if (!success)
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
        break;
    }

    switch (flags & TYD_SERIAL_CSIZE_MASK) {
    case TYD_SERIAL_5BITS_CSIZE:
        dcb.ByteSize = 5;
        break;
    case TYD_SERIAL_6BITS_CSIZE:
        dcb.ByteSize = 6;
        break;
    case TYD_SERIAL_7BITS_CSIZE:
        dcb.ByteSize = 7;
        break;

    default:
        dcb.ByteSize = 8;
        break;
    }

    switch (flags & TYD_SERIAL_PARITY_MASK) {
    case 0:
        dcb.fParity = FALSE;
        dcb.Parity = NOPARITY;
        break;
    case TYD_SERIAL_ODD_PARITY:
        dcb.fParity = TRUE;
        dcb.Parity = ODDPARITY;
        break;
    case TYD_SERIAL_EVEN_PARITY:
        dcb.fParity = TRUE;
        dcb.Parity = EVENPARITY;
        break;

    default:
        assert(false);
        break;
    }

    dcb.StopBits = 0;
    if (flags & TYD_SERIAL_2BITS_STOP)
        dcb.StopBits = TWOSTOPBITS;

    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    switch (flags & TYD_SERIAL_FLOW_MASK) {
    case 0:
        break;
    case TYD_SERIAL_XONXOFF_FLOW:
        dcb.fOutX = TRUE;
        dcb.fInX = TRUE;
        break;
    case TYD_SERIAL_RTSCTS_FLOW:
        dcb.fOutxCtsFlow = TRUE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        break;

    default:
        assert(false);
        break;
    }

    success = SetCommState(h->handle, &dcb);
    if (!success)
        return ty_error(TY_ERROR_SYSTEM, "SetCommState() failed: %s", ty_win32_strerror(0));

    return 0;
}

ssize_t tyd_serial_read(tyd_handle *h, char *buf, size_t size, int timeout)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_SERIAL);
    assert(buf);
    assert(size);

    if (h->len < 0) {
        // Could be a transient error, try to restart it
        h->len = start_async_read(h);
        if (h->len < 0)
            return h->len;
    }

    /* Serial devices are stream-based. If we don't have any data yet, see if our asynchronous
       read request has returned anything. Then we can just give the user the data we have, until
       our buffer is empty. We can't just discard stuff, unlike what we do for long HID messages. */
    if (!h->len) {
        h->len = finalize_async_read(h, timeout);
        if (h->len < 0)
            return h->len;

        h->ptr = h->buf;
    }

    if (size > (size_t)h->len)
        size = (size_t)h->len;

    memcpy(buf, h->ptr, size);
    h->ptr += size;
    h->len -= (ssize_t)size;

    /* Our buffer has been fully read, start a new asynchonous request. I don't know how
       much latency this brings. Maybe double buffering would help, but not before any concrete
       benchmarking is done. */
    if (!h->len) {
        ty_error_mask(TY_ERROR_IO);
        h->len = start_async_read(h);
        ty_error_unmask();
    }

    return (ssize_t)size;
}

ssize_t tyd_serial_write(tyd_handle *h, const char *buf, ssize_t size)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_SERIAL);
    assert(buf);

    if (size < 0)
        size = (ssize_t)strlen(buf);
    if (!size)
        return 0;

    OVERLAPPED ov = {0};
    DWORD len;
    BOOL success;

    success = WriteFile(h->handle, buf, (DWORD)size, &len, &ov);
    if (!success) {
        if (GetLastError() != ERROR_IO_PENDING) {
            CancelIo(h->handle);
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
        }

        success = GetOverlappedResult(h->handle, &ov, &len, TRUE);
        if (!success)
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
    }

    return (ssize_t)len;
}
