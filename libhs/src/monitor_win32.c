/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "util.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfgmgr32.h>
#include <dbt.h>
#include <devioctl.h>
#include <hidsdi.h>
#include <initguid.h>
#include <process.h>
#include <setupapi.h>
#include <usb.h>
#include <usbiodef.h>
#include <usbioctl.h>
#include <usbuser.h>
#include <wchar.h>
#include "device_priv.h"
#include "filter.h"
#include "list.h"
#include "monitor_priv.h"
#include "hs/platform.h"

struct hs_monitor {
    _HS_MONITOR

    CRITICAL_SECTION mutex;
    int ret;
    _hs_list_head notifications;
    HANDLE event;

    HANDLE thread;
    HANDLE hwnd;
};

struct setup_class {
    const char *name;
    hs_device_type type;
};

struct usb_controller {
    _hs_list_head list;

    uint8_t index;
    char roothub_id[];
};

enum device_event {
    DEVICE_EVENT_ADDED,
    DEVICE_EVENT_REMOVED
};

struct device_notification {
    _hs_list_head list;

    enum device_event event;
    char *key;
};

#if defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 4
__declspec(dllimport) BOOLEAN NTAPI HidD_GetSerialNumberString(HANDLE HidDeviceObject,
                                                               PVOID Buffer, ULONG BufferLength);
#endif

extern const struct _hs_device_vtable _hs_win32_device_vtable;

#define MAX_USB_DEPTH 8
#define MONITOR_CLASS_NAME "hs_monitor"

static const struct setup_class setup_classes[] = {
    {"Ports",    HS_DEVICE_TYPE_SERIAL},
    {"HIDClass", HS_DEVICE_TYPE_HID}
};

static GUID hid_guid;

static CRITICAL_SECTION controllers_lock;
static _HS_LIST(controllers);

_HS_INIT()
{
    HidD_GetHidGuid(&hid_guid);
    InitializeCriticalSection(&controllers_lock);
}

_HS_EXIT()
{
    DeleteCriticalSection(&controllers_lock);
}

static void free_notification(struct device_notification *notification)
{
    if (notification)
        free(notification->key);

    free(notification);
}

static uint8_t find_controller(const char *id)
{
    _hs_list_foreach(cur, &controllers) {
        struct usb_controller *controller = _hs_container_of(cur, struct usb_controller, list);

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
        return hs_error(HS_ERROR_MEMORY, NULL);

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
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_DEBUG, "No location information on this device node");
        return 0;
    }

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
        return hs_error(HS_ERROR_MEMORY, NULL);

    *rpath = path;
    return 0;
}

static int wide_to_cstring(const wchar_t *wide, size_t size, char **rs)
{
    wchar_t *tmp = NULL;
    char *s = NULL;
    int len, r;

    tmp = calloc(1, size + sizeof(wchar_t));
    if (!tmp) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    memcpy(tmp, wide, size);

    len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, tmp, -1, NULL, 0, NULL, NULL);
    if (!len) {
        r = hs_error(HS_ERROR_SYSTEM, "Failed to convert UTF-16 string to local codepage: %s",
                     hs_win32_strerror(0));
        goto cleanup;
    }

    s = malloc((size_t)len);
    if (!s) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, tmp, -1, s, len, NULL, NULL);
    if (!len) {
        r = hs_error(HS_ERROR_SYSTEM, "Failed to convert UTF-16 string to local codepage: %s",
                     hs_win32_strerror(0));
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
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    node->ConnectionIndex = port;
    pseudo.ConnectionIndex = port;

    success = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, node, len,
                              node, len, &len, NULL);
    if (!success) {
        hs_log(HS_LOG_WARNING, "DeviceIoControl(IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX) failed");
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
        hs_log(HS_LOG_WARNING, "DeviceIoControl(IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME) failed");
        r = 0;
        goto cleanup;
    }

    wide = calloc(1, pseudo.ActualLength);
    if (!wide) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    wide->ConnectionIndex = port;

    success = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME, wide, pseudo.ActualLength,
                              wide, pseudo.ActualLength, &len, NULL);
    if (!success) {
        hs_log(HS_LOG_WARNING, "DeviceIoControl(IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME) failed");
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
        r = hs_error(HS_ERROR_SYSTEM, "Failed to open USB hub '%s': %s", path, hs_win32_strerror(0));
        goto cleanup;
    }

    hs_log(HS_LOG_DEBUG, "Asking HUB at '%s' for port information (XP code path)", path);
    success = DeviceIoControl(h, IOCTL_USB_GET_NODE_INFORMATION, NULL, 0, &node, sizeof(node),
                              &len, NULL);
    if (!success) {
        hs_log(HS_LOG_DEBUG, "DeviceIoControl(IOCTL_USB_GET_NODE_INFORMATION) failed");
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

static int resolve_device_location(DEVINST inst, uint8_t ports[])
{
    DEVINST parent;
    char id[256];
    unsigned int depth;
    CONFIGRET cret;
    int r;

    // skip nodes until we get to the USB ones
    parent = inst;
    do {
        inst = parent;

        cret = CM_Get_Device_ID(inst, id, sizeof(id), 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_WARNING, "CM_Get_Device_ID() failed: 0x%lx", cret);
            return 0;
        }
        hs_log(HS_LOG_DEBUG, "Going through device parents to find USB node: '%s'", id);

        cret = CM_Get_Parent(&parent, inst, 0);
    } while (cret == CR_SUCCESS && strncmp(id, "USB\\", 4) != 0);
    if (cret != CR_SUCCESS)
        return 0;

    depth = 0;
    do {
        hs_log(HS_LOG_DEBUG, "Going through device parents to resolve USB location: '%s'", id);

        if (depth == MAX_USB_DEPTH) {
            hs_log(HS_LOG_WARNING, "Excessive USB location depth, ignoring device");
            return 0;
        }

        cret = CM_Get_Device_ID(parent, id, sizeof(id), 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_WARNING, "CM_Get_Device_ID() failed: 0x%lx", cret);
            return 0;
        }

        // Test for Vista, CancelIoEx() is needed elsewhere so no need for VerifyVersionInfo()
        if (hs_win32_version() >= HS_WIN32_VERSION_VISTA) {
            r = find_device_port_vista(inst);
        } else {
            char child_key[256];
            DWORD len;

            len = sizeof(child_key);
            cret = CM_Get_DevNode_Registry_Property(inst, CM_DRP_DRIVER, NULL, child_key, &len, 0);
            if (cret != CR_SUCCESS) {
                hs_log(HS_LOG_WARNING, "Failed to get device driver key: 0x%lx", cret);
                return 0;
            }

            r = find_device_port_xp(id, child_key);
        }
        if (r < 0)
            return r;
        if (r) {
            ports[depth++] = (uint8_t)r;
            hs_log(HS_LOG_DEBUG, "Found port number: %d", r);
        }

        if (strstr(id, "\\ROOT_HUB")) {
            if (!depth)
                return 0;

            ports[depth] = find_controller(id);
            if (!ports[depth]) {
                hs_log(HS_LOG_WARNING, "Unknown USB host controller '%s'", id);
                return 0;
            }
            depth++;

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

    return (int)depth;
}

static int read_hid_properties(hs_device *dev, const USB_DEVICE_DESCRIPTOR *desc)
{
    HANDLE h = NULL;
    wchar_t wbuf[256];
    BOOL success;
    int r;

    h = CreateFile(dev->path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (!h) {
        hs_log(HS_LOG_WARNING, "Cannot open HID device '%s': %s", dev->path, hs_win32_strerror(0));
        r = 0;
        goto cleanup;
    }

#define READ_HID_PROPERTY(index, func, dest) \
        if (index) { \
            success = func(h, wbuf, sizeof(wbuf)); \
            if (success) { \
                wbuf[_HS_COUNTOF(wbuf) - 1] = 0; \
                r = wide_to_cstring(wbuf, wcslen(wbuf) * sizeof(wchar_t), (dest)); \
                if (r < 0) \
                    goto cleanup; \
            } else { \
                hs_log(HS_LOG_WARNING, "Function %s() failed despite non-zero string index", #func); \
            } \
        }

    READ_HID_PROPERTY(desc->iManufacturer, HidD_GetManufacturerString, &dev->manufacturer);
    READ_HID_PROPERTY(desc->iProduct, HidD_GetProductString, &dev->product);
    READ_HID_PROPERTY(desc->iSerialNumber, HidD_GetSerialNumberString, &dev->serial);

#undef READ_HID_PROPERTY

    r = 1;
cleanup:
    if (h)
        CloseHandle(h);
    return r;
}

static int get_string_descriptor(HANDLE h, uint8_t port, uint8_t index, char **rs)
{
    // A bit ugly, but using USB_DESCRIPTOR_REQUEST directly triggers a C2229 on MSVC
    struct {
        // USB_DESCRIPTOR_REQUEST
        struct {
            ULONG  ConnectionIndex;
            struct {
                UCHAR bmRequest;
                UCHAR bRequest;
                USHORT wValue;
                USHORT wIndex;
                USHORT wLength;
            } SetupPacket;
        } req;

        // Filled by DeviceIoControl
        struct {
            UCHAR bLength;
            UCHAR bDescriptorType;
            WCHAR bString[MAXIMUM_USB_STRING_LENGTH];
        } desc;
    } string;
    DWORD len = 0;
    char *s;
    BOOL success;
    int r;

    memset(&string, 0, sizeof(string));
    string.req.ConnectionIndex = port;
    string.req.SetupPacket.wValue = (USHORT)((USB_STRING_DESCRIPTOR_TYPE << 8) | index);
    string.req.SetupPacket.wIndex = 0x409;
    string.req.SetupPacket.wLength = sizeof(string.desc);

    success = DeviceIoControl(h, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, &string, sizeof(string),
                              &string, sizeof(string), &len, NULL);
    if (!success) {
        hs_log(HS_LOG_WARNING, "DeviceIoControl(IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION) failed: %s",
               hs_win32_strerror(0));
        return 0;
    }

    if (len < 2 || string.desc.bDescriptorType != USB_STRING_DESCRIPTOR_TYPE ||
            string.desc.bLength != len - sizeof(string.req) || string.desc.bLength % 2 != 0) {
        hs_log(HS_LOG_WARNING, "Malformed or corrupt string descriptor");
        return 0;
    }

    r = wide_to_cstring(string.desc.bString, len - sizeof(USB_DESCRIPTOR_REQUEST), &s);
    if (r < 0)
        return r;

    *rs = s;
    return 0;
}

static int read_device_properties(hs_device *dev, DEVINST inst, uint8_t port)
{
    char buf[256];
    char *path = NULL;
    HANDLE h = NULL;
    DWORD len;
    USB_NODE_CONNECTION_INFORMATION_EX *node = NULL;
    CONFIGRET cret;
    BOOL success;
    int r;

    // Get the device handle corresponding to the USB device or interface
    do {
        cret = CM_Get_Device_ID(inst, buf, sizeof(buf), 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_WARNING, "CM_Get_Device_ID() failed: 0x%lx", cret);
            return 0;
        }

        if (strncmp(buf, "USB\\", 4) == 0)
            break;

        cret = CM_Get_Parent(&inst, inst, 0);
    } while (cret == CR_SUCCESS);
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_WARNING, "CM_Get_Parent() failed: 0x%lx", cret);
        r = 0;
        goto cleanup;
    }

    dev->iface = 0;
    r = sscanf(buf, "USB\\VID_%04hx&PID_%04hx&MI_%02hhu", &dev->vid, &dev->pid, &dev->iface);
    if (r < 2) {
        hs_log(HS_LOG_WARNING, "Failed to parse USB properties from '%s'", buf);
        r = 0;
        goto cleanup;
    }

    // Now we need the device handle for the USB hub where the device is plugged
    if (r == 3) {
        cret = CM_Get_Parent(&inst, inst, 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_WARNING, "CM_Get_Parent() failed: 0x%lx", cret);
            r = 0;
            goto cleanup;
        }
    }
    cret = CM_Get_Parent(&inst, inst, 0);
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_WARNING, "CM_Get_Parent() failed: 0x%lx", cret);
        r = 0;
        goto cleanup;
    }
    cret = CM_Get_Device_ID(inst, buf, sizeof(buf), 0);
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_WARNING, "CM_Get_Device_ID() failed: 0x%lx", cret);
        r = 0;
        goto cleanup;
    }

    r = build_device_path(buf, &GUID_DEVINTERFACE_USB_HUB, &path);
    if (r < 0)
        goto cleanup;

    h = CreateFile(path, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (!h) {
        hs_log(HS_LOG_DEBUG, "Cannot open parent hub device at '%s', ignoring device properties for '%s'",
               path, dev->key);
        r = 1;
        goto cleanup;
    }

    len = sizeof(node) + (sizeof(USB_PIPE_INFO) * 30);
    node = calloc(1, len);
    if (!node) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    node->ConnectionIndex = port;
    success = DeviceIoControl(h, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, node, len,
                              node, len, &len, NULL);
    if (!success) {
        hs_log(HS_LOG_DEBUG, "Failed to interrogate hub device at '%s' for device '%s'", path,
               dev->key);
        r = 1;
        goto cleanup;
    }

    /* Descriptor requests to USB devices underlying HID devices fail most (all?) of the time,
       so we need a different technique here. We still need the device descriptor because the
       HidD_GetXString() functions sometime return garbage (at least on XP) when the string
       index is 0. */
    if (dev->type == HS_DEVICE_TYPE_HID) {
        r = read_hid_properties(dev, &node->DeviceDescriptor);
        goto cleanup;
    }

#define READ_STRING_DESCRIPTOR(index, var) \
        if (index) { \
            r = get_string_descriptor(h, port, (index), (var)); \
            if (r < 0) \
                goto cleanup; \
        }

    READ_STRING_DESCRIPTOR(node->DeviceDescriptor.iManufacturer, &dev->manufacturer);
    READ_STRING_DESCRIPTOR(node->DeviceDescriptor.iProduct, &dev->product);
    READ_STRING_DESCRIPTOR(node->DeviceDescriptor.iSerialNumber, &dev->serial);

#undef READ_STRING_DESCRIPTOR

    r = 1;
cleanup:
    free(node);
    if (h)
        CloseHandle(h);
    free(path);
    return r;
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
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_WARNING, "CM_Open_DevNode_Key() failed: 0x%lu", cret);
        return 0;
    }

    len = (DWORD)sizeof(buf) - 1;
    ret = RegQueryValueEx(key, "PortName", NULL, &type, (BYTE *)buf, &len);
    RegCloseKey(key);
    if (ret != ERROR_SUCCESS) {
        if (ret != ERROR_FILE_NOT_FOUND)
            hs_log(HS_LOG_WARNING, "RegQueryValue() failed: %ld", ret);
        return 0;
    }

    /* If the string is stored without a terminating NUL, the buffer won't have it either.
       Microsoft fixed it with RegGetValue(), but this function requires Vista. */
    if (buf[--len])
        buf[len + 1] = 0;

    // You need the \\.\ prefix to open COM ports beyond COM9
    r = asprintf(&node, "%s%s", len > 4 ? "\\\\.\\" : "", buf);
    if (r < 0)
        return hs_error(HS_ERROR_MEMORY, NULL);

    *rnode = node;
    return 1;
}

static int find_device_node(DEVINST inst, hs_device *dev)
{
    int r;

    /* GUID_DEVINTERFACE_COMPORT only works for real COM ports... Haven't found any way to
       list virtual (USB) serial device interfaces, so instead list USB devices and consider
       them serial if registry key "PortName" is available (and use its value as device node). */
    if (strncmp(dev->key, "USB\\", 4) == 0) {
        r = get_device_comport(inst, &dev->path);
        if (!r) {
            hs_log(HS_LOG_DEBUG, "Device '%s' has no 'PortName' registry property", dev->key);
            return r;
        }
        if (r < 0)
            return r;

        dev->type = HS_DEVICE_TYPE_SERIAL;
    } else if (strncmp(dev->key, "HID\\", 4) == 0) {
        r = build_device_path(dev->key, &hid_guid, &dev->path);
        if (r < 0)
            return r;

        dev->type = HS_DEVICE_TYPE_HID;
    } else {
        hs_log(HS_LOG_DEBUG, "Unknown device type for '%s'", dev->key);
        return 0;
    }

    return 1;
}

static int process_win32_device(DEVINST inst, const char *id, hs_device **rdev)
{
    hs_device *dev;
    uint8_t ports[MAX_USB_DEPTH];
    unsigned int depth;
    int r;

    dev = calloc(1, sizeof(*dev));
    if (!dev) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }
    dev->refcount = 1;
    dev->state = HS_DEVICE_STATUS_ONLINE;

    if (id) {
        dev->key = strdup(id);
    } else {
        char buf[256];
        CONFIGRET cret;

        cret = CM_Get_Device_ID(inst, buf, sizeof(buf), 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_WARNING, "CM_Get_Device_ID() failed: 0x%lx", cret);
            r = 0;
            goto cleanup;
        }

        dev->key = strdup(buf);
    }
    if (!dev->key) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    // HID devices can have multiple collections for each interface, ignore them
    if (strncmp(dev->key, "HID\\", 4) == 0) {
        const char *ptr = strstr(dev->key, "&COL");
        if (ptr && strncmp(ptr, "&COL01\\",  7) != 0) {
            hs_log(HS_LOG_DEBUG, "Ignoring duplicate HID collection device '%s'", dev->key);
            r = 0;
            goto cleanup;
        }
    }

    hs_log(HS_LOG_DEBUG, "Examining device node '%s'", dev->key);

    r = find_device_node(inst, dev);
    if (r <= 0)
        goto cleanup;

    r = resolve_device_location(inst, ports);
    if (r <= 0)
        goto cleanup;
    depth = (unsigned int)r;

    r = read_device_properties(dev, inst, ports[depth - 1]);
    if (r <= 0)
        goto cleanup;

    r = build_location_string(ports, depth, &dev->location);
    if (r < 0)
        goto cleanup;

    dev->vtable = &_hs_win32_device_vtable;

    *rdev = dev;
    dev = NULL;
    r = 1;

cleanup:
    hs_device_unref(dev);
    return r;
}

static int add_controller(DEVINST inst, uint8_t index)
{
    DEVINST roothub_inst;
    char roothub_id[512];
    struct usb_controller *controller;
    CONFIGRET cret;

    cret = CM_Get_Child(&roothub_inst, inst, 0);
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_WARNING, "Found USB Host controller without a root hub");
        return 0;
    }

    cret = CM_Get_Device_ID(roothub_inst, roothub_id, sizeof(roothub_id), 0);
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_WARNING, "CM_Get_Device_ID() failed: 0x%lx", cret);
        return 0;
    }
    if (!strstr(roothub_id, "\\ROOT_HUB")) {
        hs_log(HS_LOG_WARNING, "Unsupported root HUB at '%s'", roothub_id);
        return 0;
    }

    controller = malloc(sizeof(*controller) + strlen(roothub_id) + 1);
    if (!controller)
        return hs_error(HS_ERROR_MEMORY, NULL);

    controller->index = index;
    strcpy(controller->roothub_id, roothub_id);

    hs_log(HS_LOG_DEBUG, "Found USB root hub '%s' with ID %"PRIu8, controller->roothub_id,
           controller->index);
    _hs_list_add_tail(&controllers, &controller->list);

    return 1;
}

static int populate_controllers(void)
{
    HDEVINFO set = NULL;
    SP_DEVINFO_DATA info;
    int r;

    EnterCriticalSection(&controllers_lock);

    if (!_hs_list_is_empty(&controllers)) {
        r = 0;
        goto cleanup;
    }

    set = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_HOST_CONTROLLER, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (!set) {
        r = hs_error(HS_ERROR_SYSTEM, "SetupDiGetClassDevs() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    info.cbSize = sizeof(info);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &info); i++) {
        if (i + 1 == UINT8_MAX) {
            hs_log(HS_LOG_WARNING, "Reached maximum controller ID %d, ignoring", UINT8_MAX);
            break;
        }

        r = add_controller(info.DevInst, (uint8_t)(i + 1));
        if (r < 0)
            goto cleanup;
    }

    r = 0;
cleanup:
    LeaveCriticalSection(&controllers_lock);
    if (set)
        SetupDiDestroyDeviceInfoList(set);
    return r;
}

static int enumerate_setup_class(const GUID *guid, const _hs_filter *filter, hs_enumerate_func *f,
                                 void *udata)
{
    HDEVINFO set = NULL;
    SP_DEVINFO_DATA info;
    hs_device *dev;
    int r;

    set = SetupDiGetClassDevs(guid, NULL, NULL, DIGCF_PRESENT);
    if (!set) {
        r = hs_error(HS_ERROR_SYSTEM, "SetupDiGetClassDevs() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    info.cbSize = sizeof(info);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &info); i++) {
        r = process_win32_device(info.DevInst, NULL, &dev);
        if (r < 0)
            goto cleanup;
        if (!r)
            continue;

        if (_hs_filter_match_device(filter, dev)) {
            r = (*f)(dev, udata);
            hs_device_unref(dev);
            if (r)
                goto cleanup;
        } else {
            hs_device_unref(dev);
        }
    }

    r = 0;
cleanup:
    if (set)
        SetupDiDestroyDeviceInfoList(set);
    return r;
}

int enumerate(_hs_filter *filter, hs_enumerate_func *f, void *udata)
{
    int r;

    r = populate_controllers();
    if (r < 0)
        return r;

    for (unsigned int i = 0; i < _HS_COUNTOF(setup_classes); i++) {
        if (_hs_filter_has_type(filter, setup_classes[i].type)) {
            GUID guids[8];
            DWORD guids_count;
            BOOL success;

            success = SetupDiClassGuidsFromName(setup_classes[i].name, guids, _HS_COUNTOF(guids),
                                                &guids_count);
            if (!success)
                return hs_error(HS_ERROR_SYSTEM, "SetupDiClassGuidsFromName('%s') failed: %s",
                                setup_classes[i].name, hs_win32_strerror(0));

            for (unsigned int j = 0; j < guids_count; j++) {
                r = enumerate_setup_class(&guids[j], filter, f, udata);
                if (r)
                    return r;
            }
        }
    }

    return 0;
}

int hs_enumerate(const hs_match *matches, unsigned int count, hs_enumerate_func *f, void *udata)
{
    assert(f);

    _hs_filter filter = {0};
    int r;

    r = _hs_filter_init(&filter, matches, count);
    if (r < 0)
        return r;

    r = enumerate(&filter, f, udata);

    _hs_filter_release(&filter);
    return r;
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
        return hs_error(HS_ERROR_MEMORY, NULL);

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

static int post_device_event(hs_monitor *monitor, enum device_event event, DEV_BROADCAST_DEVICEINTERFACE *data)
{
    struct device_notification *notification;
    int r;

    notification = calloc(1, sizeof(*notification));
    if (!notification)
        return hs_error(HS_ERROR_MEMORY, NULL);

    notification->event = event;
    r = extract_device_id(data->dbcc_name, &notification->key);
    if (r < 0)
        goto error;

    EnterCriticalSection(&monitor->mutex);

    _hs_list_add_tail(&monitor->notifications, &notification->list);
    SetEvent(monitor->event);

    LeaveCriticalSection(&monitor->mutex);

    return 0;

error:
    free_notification(notification);
    return r;
}

static LRESULT __stdcall window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    hs_monitor *monitor = (hs_monitor *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    int r;

    switch (msg) {
    case WM_DEVICECHANGE:
        r = 0;
        switch (wparam) {
        case DBT_DEVICEARRIVAL:
            r = post_device_event(monitor, DEVICE_EVENT_ADDED, (DEV_BROADCAST_DEVICEINTERFACE *)lparam);
            break;
        case DBT_DEVICEREMOVECOMPLETE:
            r = post_device_event(monitor, DEVICE_EVENT_REMOVED, (DEV_BROADCAST_DEVICEINTERFACE *)lparam);
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
    _HS_UNUSED(udata);

    hs_monitor *monitor = udata;

    WNDCLASSEX cls = {0};
    DEV_BROADCAST_DEVICEINTERFACE filter = {0};
    HDEVNOTIFY notify_handle = NULL;
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
        r = hs_error(HS_ERROR_SYSTEM, "RegisterClass() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    monitor->hwnd = CreateWindow(MONITOR_CLASS_NAME, MONITOR_CLASS_NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    if (!monitor->hwnd) {
        r = hs_error(HS_ERROR_SYSTEM, "CreateWindow() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    SetLastError(0);
    SetWindowLongPtr(monitor->hwnd, GWLP_USERDATA, (LONG_PTR)monitor);
    if (GetLastError()) {
        r = hs_error(HS_ERROR_SYSTEM, "SetWindowLongPtr() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    /* We monitor everything because I cannot find an interface class to detect
       serial devices within an IAD, and RegisterDeviceNotification() does not
       support device setup class filtering. */
    notify_handle = RegisterDeviceNotification(monitor->hwnd, &filter,
                                               DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    if (!notify_handle) {
        r = hs_error(HS_ERROR_SYSTEM, "RegisterDeviceNotification() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    /* Our fake window is created and ready to receive device notifications,
       hs_monitor_new() can go on. */
    SetEvent(monitor->event);

    while((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if(ret < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "GetMessage() failed: %s", hs_win32_strerror(0));
            goto cleanup;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    r = 0;
cleanup:
    if (notify_handle)
        UnregisterDeviceNotification(notify_handle);
    if (monitor->hwnd)
        DestroyWindow(monitor->hwnd);
    UnregisterClass(MONITOR_CLASS_NAME, NULL);
    if (r < 0) {
        monitor->ret = r;
        SetEvent(monitor->event);
    }
    return 0;
}

static int monitor_enumerate_callback(hs_device *dev, void *udata)
{
    return _hs_monitor_add(udata, dev, NULL, NULL);
}

/* Monitoring device changes on Windows involves a window to receive device notifications on the
   thread message queue. Unfortunately we can't poll on message queues so instead, we make a
   background thread to get device notifications, and tell us about it using Win32 events which
   we can poll. */
int hs_monitor_new(const hs_match *matches, unsigned int count, hs_monitor **rmonitor)
{
    assert(rmonitor);

    hs_monitor *monitor;
    int r;

    monitor = calloc(1, sizeof(*monitor));
    if (!monitor) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }

    r = _hs_monitor_init(monitor, matches, count);
    if (r < 0)
        goto error;

    InitializeCriticalSection(&monitor->mutex);
    monitor->event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!monitor->event) {
        r = hs_error(HS_ERROR_SYSTEM, "CreateEvent() failed: %s", hs_win32_strerror(0));
        goto error;
    }

    _hs_list_init(&monitor->notifications);

    *rmonitor = monitor;
    return 0;

error:
    hs_monitor_free(monitor);
    return r;
}

void hs_monitor_free(hs_monitor *monitor)
{
    if (monitor) {
        hs_monitor_stop(monitor);

        DeleteCriticalSection(&monitor->mutex);
        if (monitor->event)
            CloseHandle(monitor->event);

        _hs_monitor_release(monitor);
    }

    free(monitor);
}

hs_descriptor hs_monitor_get_descriptor(const hs_monitor *monitor)
{
    assert(monitor);
    return monitor->event;
}

int hs_monitor_start(hs_monitor *monitor)
{
    assert(monitor);
    assert(!monitor->thread);

    int r;

    if (monitor->thread)
        return 0;

    /* We can't create our fake window here, because the messages would be posted to this thread's
       message queue and not to the monitoring thread. So instead, the background thread creates
       its own window and we wait for it to signal us before we continue. */
    monitor->thread = (HANDLE)_beginthreadex(NULL, 0, monitor_thread, monitor, 0, NULL);
    if (!monitor->thread) {
        r = hs_error(HS_ERROR_SYSTEM, "_beginthreadex() failed: %s", hs_win32_strerror(0));
        goto error;
    }

    WaitForSingleObject(monitor->event, INFINITE);
    if (monitor->ret < 0) {
        r = monitor->ret;
        goto error;
    }
    ResetEvent(monitor->event);

    r = enumerate(&monitor->filter, monitor_enumerate_callback, monitor);
    if (r < 0)
        goto error;

    return 0;

error:
    hs_monitor_stop(monitor);
    return r;
}

void hs_monitor_stop(hs_monitor *monitor)
{
    assert(monitor);

    if (!monitor->thread)
        return;

    _hs_monitor_clear(monitor);

    if (monitor->hwnd) {
        PostMessage(monitor->hwnd, WM_CLOSE, 0, 0);
        WaitForSingleObject(monitor->thread, INFINITE);
    }
    CloseHandle(monitor->thread);
    monitor->thread = NULL;

    _hs_list_foreach(cur, &monitor->notifications) {
        struct device_notification *notification = _hs_container_of(cur, struct device_notification, list);
        free_notification(notification);
    }
}

static int process_arrival_notification(hs_monitor *monitor, const char *key, hs_enumerate_func *f,
                                        void *udata)
{
    DEVINST inst;
    hs_device *dev = NULL;
    CONFIGRET cret;
    int r;

    cret = CM_Locate_DevNode(&inst, (DEVINSTID)key, CM_LOCATE_DEVNODE_NORMAL);
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_WARNING, "Device node '%s' does not exist: 0x%lx", dev->key, cret);
        return 0;
    }

    r = process_win32_device(inst, key, &dev);
    if (r <= 0)
        return r;

    r = _hs_monitor_add(monitor, dev, f, udata);
    hs_device_unref(dev);

    return r;
}

int hs_monitor_refresh(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    assert(monitor);

    _HS_LIST(notifications);
    int r;

    if (!monitor->thread)
        return 0;

    EnterCriticalSection(&monitor->mutex);

    /* We don't want to keep the lock for too long, so move all notifications to our own list
       and let the background thread work and process Win32 events. */
    _hs_list_splice(&notifications, &monitor->notifications);

    r = monitor->ret;
    monitor->ret = 0;

    LeaveCriticalSection(&monitor->mutex);

    if (r < 0)
        goto cleanup;

    _hs_list_foreach(cur, &notifications) {
        struct device_notification *notification = _hs_container_of(cur, struct device_notification, list);

        switch (notification->event) {
        case DEVICE_EVENT_ADDED:
            hs_log(HS_LOG_DEBUG, "Received arrival notification for device '%s'", notification->key);
            r = process_arrival_notification(monitor, notification->key, f, udata);
            break;

        case DEVICE_EVENT_REMOVED:
            hs_log(HS_LOG_DEBUG, "Received removal notification for device '%s'", notification->key);
            _hs_monitor_remove(monitor, notification->key, f, udata);
            r = 0;
            break;
        }

        _hs_list_remove(&notification->list);
        free_notification(notification);

        if (r)
            goto cleanup;
    }

    r = 0;
cleanup:
    EnterCriticalSection(&monitor->mutex);

    /* If an error occurs, there maty be unprocessed notifications. We don't want to lose them so
       put everything back in front of the notification list. */
    _hs_list_splice(&monitor->notifications, &notifications);
    if (_hs_list_is_empty(&monitor->notifications))
        ResetEvent(monitor->event);

    LeaveCriticalSection(&monitor->mutex);
    return r;
}
