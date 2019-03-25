/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfgmgr32.h>
#include <dbt.h>
#include <devioctl.h>
HS_BEGIN_C
    #include <hidsdi.h>
    #include <hidpi.h>
HS_END_C
#include <initguid.h>
#include <process.h>
#include <setupapi.h>
#include <usb.h>
#include <usbiodef.h>
#include <usbioctl.h>
#include <usbuser.h>
#include <wchar.h>
#include "array.h"
#include "device_priv.h"
#include "match_priv.h"
#include "monitor_priv.h"
#include "platform.h"

enum event_type {
    DEVICE_EVENT_ADDED,
    DEVICE_EVENT_REMOVED
};

struct event {
    enum event_type type;
    char device_key[256];
};

typedef _HS_ARRAY(struct event) event_array;

struct hs_monitor {
    _hs_match_helper match_helper;
    _hs_htable devices;

    HANDLE thread;
    HWND thread_hwnd;

    HANDLE thread_event;
    CRITICAL_SECTION events_lock;
    event_array events;
    event_array thread_events;
    event_array refresh_events;
    int thread_ret;
};

struct setup_class {
    const char *name;
    hs_device_type type;
};

struct device_cursor {
    DEVINST inst;
    char id[256];
};

enum device_cursor_relative {
    DEVINST_RELATIVE_PARENT,
    DEVINST_RELATIVE_SIBLING,
    DEVINST_RELATIVE_CHILD
};

#if defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 4
__declspec(dllimport) BOOLEAN NTAPI HidD_GetSerialNumberString(HANDLE HidDeviceObject,
                                                               PVOID Buffer, ULONG BufferLength);
__declspec(dllimport) BOOLEAN NTAPI HidD_GetPreparsedData(HANDLE HidDeviceObject,
                                                          PHIDP_PREPARSED_DATA *PreparsedData);
__declspec(dllimport) BOOLEAN NTAPI HidD_FreePreparsedData(PHIDP_PREPARSED_DATA PreparsedData);
#endif

#define MAX_USB_DEPTH 8
#define MONITOR_CLASS_NAME "hs_monitor"

static const struct setup_class setup_classes[] = {
    {"Ports",    HS_DEVICE_TYPE_SERIAL},
    {"HIDClass", HS_DEVICE_TYPE_HID}
};

static volatile LONG controllers_lock_setup;
static CRITICAL_SECTION controllers_lock;
static char *controllers[32];
static unsigned int controllers_count;

static bool get_device_cursor(DEVINST inst, struct device_cursor *new_cursor)
{
    CONFIGRET cret;

    new_cursor->inst = inst;
    cret = CM_Get_Device_ID(inst, new_cursor->id, sizeof(new_cursor->id), 0);
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_WARNING, "CM_Get_Device_ID() failed for instance 0x%lx: 0x%lx", inst, cret);
        return false;
    }

    return true;
}

static bool get_device_cursor_relative(struct device_cursor *cursor,
                                       enum device_cursor_relative relative,
                                       struct device_cursor *new_cursor)
{
    DEVINST new_inst;
    CONFIGRET cret = 0xFFFFFFFF;

    switch (relative) {
        case DEVINST_RELATIVE_PARENT: {
            cret = CM_Get_Parent(&new_inst, cursor->inst, 0);
            if (cret != CR_SUCCESS) {
                hs_log(HS_LOG_DEBUG, "Cannot get parent of device '%s': 0x%lx", cursor->id, cret);
                return false;
            }
        } break;

        case DEVINST_RELATIVE_CHILD: {
            cret = CM_Get_Child(&new_inst, cursor->inst, 0);
            if (cret != CR_SUCCESS) {
                hs_log(HS_LOG_DEBUG, "Cannot get child of device '%s': 0x%lx", cursor->id, cret);
                return false;
            }
        } break;

        case DEVINST_RELATIVE_SIBLING: {
            cret = CM_Get_Sibling(&new_inst, cursor->inst, 0);
            if (cret != CR_SUCCESS) {
                hs_log(HS_LOG_DEBUG, "Cannot get sibling of device '%s': 0x%lx", cursor->id, cret);
                return false;
            }
        } break;
    }
    assert(cret != 0xFFFFFFFF);

    return get_device_cursor(new_inst, new_cursor);
}

static bool move_device_cursor(struct device_cursor *cursor, enum device_cursor_relative relative)
{
    return get_device_cursor_relative(cursor, relative, cursor);
}

static uint8_t find_controller(const char *id)
{
    for (unsigned int i = 0; i < controllers_count; i++) {
        if (strcmp(controllers[i], id) == 0)
            return (uint8_t)(i + 1);
    }

    return 0;
}

static int build_device_path(const char *id, const GUID *guid, char **rpath)
{
    char *path, *ptr;

    path = (char *)malloc(4 + strlen(id) + 41);
    if (!path)
        return hs_error(HS_ERROR_MEMORY, NULL);

    ptr = _hs_stpcpy(path, "\\\\.\\");
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

    tmp = (wchar_t *)calloc(1, size + sizeof(wchar_t));
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

    s = (char *)malloc((size_t)len);
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
    node = (USB_NODE_CONNECTION_INFORMATION_EX *)calloc(1, len);
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

    wide = (USB_NODE_CONNECTION_DRIVERKEY_NAME *)calloc(1, pseudo.ActualLength);
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

static int find_device_port_ioctl(const char *hub_id, const char *child_key)
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
    if (h == INVALID_HANDLE_VALUE) {
        hs_log(HS_LOG_DEBUG, "Failed to open USB hub '%s': %s", path, hs_win32_strerror(0));
        r = 0;
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

static bool is_root_usb_controller(const char *id)
{
    static const char *const root_needles[] = {
        "ROOT_HUB",
        "VMUSB\\HUB" // Microsoft Virtual PC
    };

    for (size_t i = 0; i < _HS_COUNTOF(root_needles); i++) {
        if (strstr(id, root_needles[i]))
            return true;
    }
    return false;
}

static int resolve_usb_location_ioctl(struct device_cursor usb_cursor, uint8_t ports[],
                                      struct device_cursor *roothub_cursor)
{
    unsigned int depth;

    depth = 0;
    do {
        struct device_cursor parent_cursor;
        char child_key[256];
        DWORD child_key_len;
        CONFIGRET cret;
        int r;

        if (!get_device_cursor_relative(&usb_cursor, DEVINST_RELATIVE_PARENT, &parent_cursor)) {
            return 0;
        }

        child_key_len = sizeof(child_key);
        cret = CM_Get_DevNode_Registry_Property(usb_cursor.inst, CM_DRP_DRIVER, NULL,
                                                child_key, &child_key_len, 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_WARNING, "Failed to get device driver key: 0x%lx", cret);
            return 0;
        }
        r = find_device_port_ioctl(parent_cursor.id, child_key);
        if (r <= 0)
            return r;
        ports[depth] = (uint8_t)r;
        hs_log(HS_LOG_DEBUG, "Found port number of '%s': %u", usb_cursor.id, ports[depth]);
        depth++;

        // We need place for the root hub index
        if (depth == MAX_USB_DEPTH) {
            hs_log(HS_LOG_WARNING, "Excessive USB location depth, ignoring device");
            return 0;
        }

        usb_cursor = parent_cursor;
    } while (!is_root_usb_controller(usb_cursor.id));

    *roothub_cursor = usb_cursor;
    return (int)depth;
}

static int resolve_usb_location_cfgmgr(struct device_cursor usb_cursor, uint8_t ports[],
                                       struct device_cursor *roothub_cursor)
{
    unsigned int depth;

    depth = 0;
    do {
        char location_buf[256];
        DWORD location_len;
        unsigned int location_port;
        CONFIGRET cret;

        // Extract port from CM_DRP_LOCATION_INFORMATION (Vista and later versions)
        location_len = sizeof(location_buf);
        cret = CM_Get_DevNode_Registry_Property(usb_cursor.inst, CM_DRP_LOCATION_INFORMATION,
                                                NULL, location_buf, &location_len, 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_DEBUG, "No location information on this device node");
            return 0;
        }
        location_port = 0;
        sscanf(location_buf, "Port_#%04u", &location_port);
        if (!location_port)
            return 0;
        hs_log(HS_LOG_DEBUG, "Found port number of '%s': %u", usb_cursor.id, location_port);
        ports[depth++] = (uint8_t)location_port;

        // We need place for the root hub index
        if (depth == MAX_USB_DEPTH) {
            hs_log(HS_LOG_WARNING, "Excessive USB location depth, ignoring device");
            return 0;
        }

        if (!move_device_cursor(&usb_cursor, DEVINST_RELATIVE_PARENT)) {
            return 0;
        }
    } while (!is_root_usb_controller(usb_cursor.id));

    *roothub_cursor = usb_cursor;
    return (int)depth;
}

static int find_device_location(DEVINST inst, uint8_t ports[])
{
    struct device_cursor dev_cursor;
    struct device_cursor usb_cursor;
    struct device_cursor roothub_cursor;
    int depth;

    if (!get_device_cursor(inst, &dev_cursor)) {
        return 0;
    }

    // Find the USB device instance
    usb_cursor = dev_cursor;
    while (strncmp(usb_cursor.id, "USB\\", 4) != 0 || strstr(usb_cursor.id, "&MI_")) {
        if (!move_device_cursor(&usb_cursor, DEVINST_RELATIVE_PARENT)) {
            return 0;
        }
    }

    /* Browse the USB tree to resolve USB ports. Try the CfgMgr method first, only
       available on Windows Vista and later versions. It may also fail with third-party
       USB controller drivers, typically USB 3.0 host controllers before Windows 10. */
    depth = 0;
    if (!getenv("LIBHS_WIN32_FORCE_XP_LOCATION_CODE")) {
        depth = resolve_usb_location_cfgmgr(usb_cursor, ports, &roothub_cursor);
        if (depth < 0)
            return depth;
    }
    if (!depth) {
        hs_log(HS_LOG_DEBUG, "Using legacy XP code for location of '%s'", dev_cursor.id);
        depth = resolve_usb_location_ioctl(usb_cursor, ports, &roothub_cursor);
        if (depth < 0)
            return depth;
    }
    if (!depth) {
        hs_log(HS_LOG_DEBUG, "Cannot resolve USB location for '%s'", dev_cursor.id);
        return 0;
    }

    // Resolve the USB controller ID
    ports[depth] = find_controller(roothub_cursor.id);
    if (!ports[depth]) {
        hs_log(HS_LOG_WARNING, "Unknown USB host controller '%s'", roothub_cursor.id);
        return 0;
    }
    hs_log(HS_LOG_DEBUG, "Found controller ID for '%s': %u", roothub_cursor.id, ports[depth]);
    depth++;

    // The ports are in the wrong order
    for (int i = 0; i < depth / 2; i++) {
        uint8_t tmp = ports[i];
        ports[i] = ports[depth - i - 1];
        ports[depth - i - 1] = tmp;
    }

    return depth;
}

static int read_hid_properties(hs_device *dev, const USB_DEVICE_DESCRIPTOR *desc)
{
    HANDLE h = NULL;
    int r;

    h = CreateFile(dev->path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        hs_log(HS_LOG_WARNING, "Cannot open HID device '%s': %s", dev->path, hs_win32_strerror(0));
        r = 0;
        goto cleanup;
    }

#define READ_HID_PROPERTY(index, func, dest) \
        if (index) { \
            wchar_t wbuf[256]; \
            BOOL bret; \
            \
            bret = func(h, wbuf, sizeof(wbuf)); \
            if (bret) { \
                wbuf[_HS_COUNTOF(wbuf) - 1] = 0; \
                r = wide_to_cstring(wbuf, wcslen(wbuf) * sizeof(wchar_t), (dest)); \
                if (r < 0) \
                    goto cleanup; \
            } else { \
                hs_log(HS_LOG_WARNING, "Function %s() failed despite non-zero string index", #func); \
            } \
        }

    READ_HID_PROPERTY(desc->iManufacturer, HidD_GetManufacturerString, &dev->manufacturer_string);
    READ_HID_PROPERTY(desc->iProduct, HidD_GetProductString, &dev->product_string);
    READ_HID_PROPERTY(desc->iSerialNumber, HidD_GetSerialNumberString, &dev->serial_number_string);

#undef READ_HID_PROPERTY

    {
        // semi-hidden Hungarian pointers? Really , Microsoft?
        PHIDP_PREPARSED_DATA pp;
        HIDP_CAPS caps;
        LONG lret;

        lret = HidD_GetPreparsedData(h, &pp);
        if (!lret) {
            hs_log(HS_LOG_WARNING, "HidD_GetPreparsedData() failed on '%s", dev->path);
            r = 0;
            goto cleanup;
        }
        lret = HidP_GetCaps(pp, &caps);
        HidD_FreePreparsedData(pp);
        if (lret != HIDP_STATUS_SUCCESS) {
            hs_log(HS_LOG_WARNING, "Invalid HID descriptor from '%s", dev->path);
            r = 0;
            goto cleanup;
        }

        dev->u.hid.usage_page = caps.UsagePage;
        dev->u.hid.usage = caps.Usage;
        dev->u.hid.input_report_len = caps.InputReportByteLength;
    }

    r = 1;
cleanup:
    if (h)
        CloseHandle(h);
    return r;
}

static int get_string_descriptor(HANDLE hub, uint8_t port, uint8_t index, char **rs)
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
    } rq;
    DWORD desc_len = 0;
    char *s;
    BOOL success;
    int r;

    memset(&rq, 0, sizeof(rq));
    rq.req.ConnectionIndex = port;
    rq.req.SetupPacket.wValue = (USHORT)((USB_STRING_DESCRIPTOR_TYPE << 8) | index);
    rq.req.SetupPacket.wIndex = 0x409;
    rq.req.SetupPacket.wLength = sizeof(rq.desc);

    success = DeviceIoControl(hub, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, &rq,
                              sizeof(rq), &rq, sizeof(rq), &desc_len, NULL);
    if (!success || desc_len < 2 || rq.desc.bDescriptorType != USB_STRING_DESCRIPTOR_TYPE ||
            rq.desc.bLength != desc_len - sizeof(rq.req) || rq.desc.bLength % 2 != 0) {
        hs_log(HS_LOG_DEBUG, "Invalid string descriptor %u", index);
        return 0;
    }

    r = wide_to_cstring(rq.desc.bString, desc_len - sizeof(USB_DESCRIPTOR_REQUEST), &s);
    if (r < 0)
        return r;

    *rs = s;
    return 0;
}

static int read_device_properties(hs_device *dev, DEVINST inst, uint8_t port)
{
    char buf[256];
    unsigned int vid, pid, iface_number;
    char *path = NULL;
    HANDLE hub = NULL;
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

    /* h and hh type modifier characters are not known to msvcrt, and MinGW issues warnings
       if we try to use them. Use temporary unsigned int variables to get around that. */
    iface_number = 0;
    r = sscanf(buf, "USB\\VID_%04x&PID_%04x&MI_%02u", &vid, &pid, &iface_number);
    if (r < 2) {
        hs_log(HS_LOG_WARNING, "Failed to parse USB properties from '%s'", buf);
        r = 0;
        goto cleanup;
    }
    dev->vid = (uint16_t)vid;
    dev->pid = (uint16_t)pid;
    dev->iface_number = (uint8_t)iface_number;

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

    hub = CreateFile(path, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hub == INVALID_HANDLE_VALUE) {
        hs_log(HS_LOG_DEBUG, "Cannot open parent hub device at '%s', ignoring device properties for '%s'",
               path, dev->key);
        r = 1;
        goto cleanup;
    }

    len = sizeof(node) + (sizeof(USB_PIPE_INFO) * 30);
    node = (USB_NODE_CONNECTION_INFORMATION_EX *)calloc(1, len);
    if (!node) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    node->ConnectionIndex = port;
    success = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, node, len,
                              node, len, &len, NULL);
    if (!success) {
        hs_log(HS_LOG_DEBUG, "Failed to interrogate hub device at '%s' for device '%s'", path,
               dev->key);
        r = 1;
        goto cleanup;
    }

    // Get additional properties
    dev->bcd_device = node->DeviceDescriptor.bcdDevice;

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
            r = get_string_descriptor(hub, port, (index), (var)); \
            if (r < 0) \
                goto cleanup; \
        }

    READ_STRING_DESCRIPTOR(node->DeviceDescriptor.iManufacturer, &dev->manufacturer_string);
    READ_STRING_DESCRIPTOR(node->DeviceDescriptor.iProduct, &dev->product_string);
    READ_STRING_DESCRIPTOR(node->DeviceDescriptor.iSerialNumber, &dev->serial_number_string);

#undef READ_STRING_DESCRIPTOR

    r = 1;
cleanup:
    free(node);
    if (hub)
        CloseHandle(hub);
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
    r = _hs_asprintf(&node, "%s%s", len > 4 ? "\\\\.\\" : "", buf);
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
    if (strncmp(dev->key, "USB\\", 4) == 0 || strncmp(dev->key, "FTDIBUS\\", 8) == 0) {
        r = get_device_comport(inst, &dev->path);
        if (!r) {
            hs_log(HS_LOG_DEBUG, "Device '%s' has no 'PortName' registry property", dev->key);
            return r;
        }
        if (r < 0)
            return r;

        dev->type = HS_DEVICE_TYPE_SERIAL;
    } else if (strncmp(dev->key, "HID\\", 4) == 0) {
        static GUID hid_interface_guid;
        if (!hid_interface_guid.Data1)
            HidD_GetHidGuid(&hid_interface_guid);

        r = build_device_path(dev->key, &hid_interface_guid, &dev->path);
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

    dev = (hs_device *)calloc(1, sizeof(*dev));
    if (!dev) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }
    dev->refcount = 1;
    dev->status = HS_DEVICE_STATUS_ONLINE;

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

    r = find_device_location(inst, ports);
    if (r <= 0)
        goto cleanup;
    depth = (unsigned int)r;

    r = read_device_properties(dev, inst, ports[depth - 1]);
    if (r <= 0)
        goto cleanup;

    r = build_location_string(ports, depth, &dev->location);
    if (r < 0)
        goto cleanup;

    *rdev = dev;
    dev = NULL;
    r = 1;

cleanup:
    hs_device_unref(dev);
    return r;
}

static void free_controllers(void)
{
    for (unsigned int i = 0; i < controllers_count; i++)
        free(controllers[i]);
    DeleteCriticalSection(&controllers_lock);
}

static int populate_controllers(void)
{
    HDEVINFO set = NULL;
    SP_DEVINFO_DATA info;
    int r;

    if (controllers_count)
        return 0;

    if (controllers_lock_setup != 2) {
        if (!InterlockedCompareExchange(&controllers_lock_setup, 1, 0)) {
            InitializeCriticalSection(&controllers_lock);
            atexit(free_controllers);
            controllers_lock_setup = 2;
        } else {
            while (controllers_lock_setup != 2)
                continue;
        }
    }

    EnterCriticalSection(&controllers_lock);

    if (controllers_count) {
        r = 0;
        goto cleanup;
    }

    hs_log(HS_LOG_DEBUG, "Listing USB host controllers and root hubs");

    set = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_HOST_CONTROLLER, NULL, NULL,
                              DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (!set) {
        r = hs_error(HS_ERROR_SYSTEM, "SetupDiGetClassDevs() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    info.cbSize = sizeof(info);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &info); i++) {
        DEVINST roothub_inst;
        char roothub_id[256];
        CONFIGRET cret;

        if (controllers_count == _HS_COUNTOF(controllers)) {
            hs_log(HS_LOG_WARNING, "Reached maximum controller ID %d, ignoring", UINT8_MAX);
            break;
        }

        cret = CM_Get_Child(&roothub_inst, info.DevInst, 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_WARNING, "Found USB Host controller without a root hub");
            continue;
        }
        cret = CM_Get_Device_ID(roothub_inst, roothub_id, sizeof(roothub_id), 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_WARNING, "CM_Get_Device_ID() failed: 0x%lx", cret);
            continue;
        }
        if (!is_root_usb_controller(roothub_id)) {
            hs_log(HS_LOG_WARNING, "Expected root hub device at '%s'", roothub_id);
            continue;
        }

        controllers[controllers_count] = strdup(roothub_id);
        if (!controllers[controllers_count]) {
            r = hs_error(HS_ERROR_MEMORY, NULL);
            goto cleanup;
        }
        hs_log(HS_LOG_DEBUG, "Found root USB hub '%s' with ID %u", roothub_id, controllers_count);
        controllers_count++;
    }

    r = 0;
cleanup:
    LeaveCriticalSection(&controllers_lock);
    if (set)
        SetupDiDestroyDeviceInfoList(set);
    return r;
}

static int enumerate_setup_class(const GUID *guid, const _hs_match_helper *match_helper,
                                 hs_enumerate_func *f, void *udata)
{
    HDEVINFO set = NULL;
    SP_DEVINFO_DATA info;
    hs_device *dev = NULL;
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

        if (_hs_match_helper_match(match_helper, dev, &dev->match_udata)) {
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

int enumerate(_hs_match_helper *match_helper, hs_enumerate_func *f, void *udata)
{
    int r;

    r = populate_controllers();
    if (r < 0)
        return r;

    for (unsigned int i = 0; i < _HS_COUNTOF(setup_classes); i++) {
        if (_hs_match_helper_has_type(match_helper, setup_classes[i].type)) {
            GUID guids[8];
            DWORD guids_count;
            BOOL success;

            success = SetupDiClassGuidsFromName(setup_classes[i].name, guids, _HS_COUNTOF(guids),
                                                &guids_count);
            if (!success)
                return hs_error(HS_ERROR_SYSTEM, "SetupDiClassGuidsFromName('%s') failed: %s",
                                setup_classes[i].name, hs_win32_strerror(0));

            for (unsigned int j = 0; j < guids_count; j++) {
                r = enumerate_setup_class(&guids[j], match_helper, f, udata);
                if (r)
                    return r;
            }
        }
    }

    return 0;
}

struct enumerate_enumerate_context {
    hs_enumerate_func *f;
    void *udata;
};

static int enumerate_enumerate_callback(hs_device *dev, void *udata)
{
    struct enumerate_enumerate_context *ctx = (struct enumerate_enumerate_context *)udata;

    _hs_device_log(dev, "Enumerate");
    return (*ctx->f)(dev, ctx->udata);
}

int hs_enumerate(const hs_match_spec *matches, unsigned int count, hs_enumerate_func *f, void *udata)
{
    assert(f);

    _hs_match_helper match_helper = {0};
    struct enumerate_enumerate_context ctx;
    int r;

    r = _hs_match_helper_init(&match_helper, matches, count);
    if (r < 0)
        return r;

    ctx.f = f;
    ctx.udata = udata;

    r = enumerate(&match_helper, enumerate_enumerate_callback, &ctx);

    _hs_match_helper_release(&match_helper);
    return r;
}

static int post_event(hs_monitor *monitor, enum event_type event_type,
                      DEV_BROADCAST_DEVICEINTERFACE *msg)
{
    const char *id;
    size_t id_len;
    struct event *event;
    UINT_PTR timer;
    int r;

    if (msg->dbcc_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
        return 0;

    /* Extract the device instance ID part.
       - in: \\?\USB#Vid_2341&Pid_0042#85336303532351101252#{a5dcbf10-6530-11d2-901f-00c04fb951ed}
       - out: USB#Vid_2341&Pid_0042#85336303532351101252
       You may notice that paths from RegisterDeviceNotification() seem to start with '\\?\',
       which according to MSDN is the file namespace, not the device namespace '\\.\'. Oh well. */
    id = msg->dbcc_name;
    if (strncmp(id, "\\\\?\\", 4) == 0
            || strncmp(id, "\\\\.\\", 4) == 0
            || strncmp(id, "##.#", 4) == 0
            || strncmp(id, "##?#", 4) == 0)
        id += 4;
    id_len = strlen(id);
    if (id_len >= 39 && id[id_len - 39] == '#' && id[id_len - 38] == '{' && id[id_len - 1] == '}')
        id_len -= 39;

    if (id_len >= sizeof(event->device_key)) {
        hs_log(HS_LOG_WARNING, "Device instance ID string '%s' is too long, ignoring", id);
        return 0;
    }
    r = _hs_array_grow(&monitor->thread_events, 1);
    if (r < 0)
        return r;
    event = &monitor->thread_events.values[monitor->thread_events.count];
    event->type = event_type;
    memcpy(event->device_key, id, id_len);
    event->device_key[id_len] = 0;
    monitor->thread_events.count++;

    /* Normalize device instance ID, uppercase and replace '#' with '\'. Could not do it on msg,
       Windows may not like it. Maybe, not sure so don't try. */
    for (char *ptr = event->device_key; *ptr; ptr++) {
        if (*ptr == '#') {
            *ptr = '\\';
        } else if (*ptr >= 97 && *ptr <= 122) {
            *ptr = (char)(*ptr - 32);
        }
    }

    timer = SetTimer(monitor->thread_hwnd, 1, 100, NULL);
    if (!timer)
        return hs_error(HS_ERROR_SYSTEM, "SetTimer() failed: %s", hs_win32_strerror(0));

    return 0;
}

static LRESULT __stdcall window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    hs_monitor *monitor = (hs_monitor *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    int r;

    switch (msg) {
        case WM_DEVICECHANGE: {
            r = 0;
            switch (wparam) {
                case DBT_DEVICEARRIVAL: {
                    r = post_event(monitor, DEVICE_EVENT_ADDED,
                                          (DEV_BROADCAST_DEVICEINTERFACE *)lparam);
                } break;

                case DBT_DEVICEREMOVECOMPLETE: {
                    r = post_event(monitor, DEVICE_EVENT_REMOVED,
                                          (DEV_BROADCAST_DEVICEINTERFACE *)lparam);
                } break;
            }
            if (r < 0) {
                EnterCriticalSection(&monitor->events_lock);
                monitor->thread_ret = r;
                SetEvent(monitor->thread_event);
                LeaveCriticalSection(&monitor->events_lock);
            }
        } break;

        case WM_TIMER: {
            if (CMP_WaitNoPendingInstallEvents(0) == WAIT_OBJECT_0) {
                KillTimer(hwnd, 1);

                EnterCriticalSection(&monitor->events_lock);
                r = _hs_array_grow(&monitor->events, monitor->thread_events.count);
                if (r < 0) {
                    monitor->thread_ret = r;
                } else {
                    memcpy(monitor->events.values + monitor->events.count,
                           monitor->thread_events.values,
                           monitor->thread_events.count * sizeof(*monitor->thread_events.values));
                    monitor->events.count += monitor->thread_events.count;
                    _hs_array_release(&monitor->thread_events);
                }
                SetEvent(monitor->thread_event);
                LeaveCriticalSection(&monitor->events_lock);
            }
        } break;

        case WM_CLOSE: {
            PostQuitMessage(0);
        } break;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void unregister_monitor_class(void)
{
    UnregisterClass(MONITOR_CLASS_NAME, GetModuleHandle(NULL));
}

static unsigned int __stdcall monitor_thread(void *udata)
{
    _HS_UNUSED(udata);

    hs_monitor *monitor = (hs_monitor *)udata;

    WNDCLASSEX cls = {0};
    ATOM cls_atom;
    DEV_BROADCAST_DEVICEINTERFACE filter = {0};
    HDEVNOTIFY notify_handle = NULL;
    MSG msg;
    int r;

    cls.cbSize = sizeof(cls);
    cls.hInstance = GetModuleHandle(NULL);
    cls.lpszClassName = MONITOR_CLASS_NAME;
    cls.lpfnWndProc = window_proc;

    /* If this fails, CreateWindow() will fail too so we can ignore errors here. This
       also takes care of any failure that may result from the class already existing. */
    cls_atom = RegisterClassEx(&cls);
    if (cls_atom)
        atexit(unregister_monitor_class);

    monitor->thread_hwnd = CreateWindow(MONITOR_CLASS_NAME, MONITOR_CLASS_NAME, 0, 0, 0, 0, 0,
                                        HWND_MESSAGE, NULL, NULL, NULL);
    if (!monitor->thread_hwnd) {
        r = hs_error(HS_ERROR_SYSTEM, "CreateWindow() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    SetLastError(0);
    SetWindowLongPtr(monitor->thread_hwnd, GWLP_USERDATA, (LONG_PTR)monitor);
    if (GetLastError()) {
        r = hs_error(HS_ERROR_SYSTEM, "SetWindowLongPtr() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    /* We monitor everything because I cannot find an interface class to detect
       serial devices within an IAD, and RegisterDeviceNotification() does not
       support device setup class filtering. */
    notify_handle = RegisterDeviceNotification(monitor->thread_hwnd, &filter,
                                               DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    if (!notify_handle) {
        r = hs_error(HS_ERROR_SYSTEM, "RegisterDeviceNotification() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    /* Our fake window is created and ready to receive device notifications,
       hs_monitor_new() can go on. */
    SetEvent(monitor->thread_event);

    /* As it turns out, GetMessage() cannot fail if the parameters are correct.
       https://blogs.msdn.microsoft.com/oldnewthing/20130322-00/?p=4873/ */
    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    r = 0;
cleanup:
    if (notify_handle)
        UnregisterDeviceNotification(notify_handle);
    if (monitor->thread_hwnd)
        DestroyWindow(monitor->thread_hwnd);
    if (r < 0) {
        monitor->thread_ret = r;
        SetEvent(monitor->thread_event);
    }
    return 0;
}

static int monitor_enumerate_callback(hs_device *dev, void *udata)
{
    hs_monitor *monitor = (hs_monitor *)udata;
    return _hs_monitor_add(&monitor->devices, dev, NULL, NULL);
}

/* Monitoring device changes on Windows involves a window to receive device notifications on the
   thread message queue. Unfortunately we can't poll on message queues so instead, we make a
   background thread to get device notifications, and tell us about it using Win32 events which
   we can poll. */
int hs_monitor_new(const hs_match_spec *matches, unsigned int count, hs_monitor **rmonitor)
{
    assert(rmonitor);

    hs_monitor *monitor;
    int r;

    monitor = (hs_monitor *)calloc(1, sizeof(*monitor));
    if (!monitor) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }

    r = _hs_match_helper_init(&monitor->match_helper, matches, count);
    if (r < 0)
        goto error;

    r = _hs_htable_init(&monitor->devices, 64);
    if (r < 0)
        goto error;

    InitializeCriticalSection(&monitor->events_lock);
    monitor->thread_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!monitor->thread_event) {
        r = hs_error(HS_ERROR_SYSTEM, "CreateEvent() failed: %s", hs_win32_strerror(0));
        goto error;
    }

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

        DeleteCriticalSection(&monitor->events_lock);
        if (monitor->thread_event)
            CloseHandle(monitor->thread_event);

        _hs_monitor_clear_devices(&monitor->devices);
        _hs_htable_release(&monitor->devices);
        _hs_match_helper_release(&monitor->match_helper);
    }

    free(monitor);
}

hs_handle hs_monitor_get_poll_handle(const hs_monitor *monitor)
{
    assert(monitor);
    return monitor->thread_event;
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

    WaitForSingleObject(monitor->thread_event, INFINITE);
    if (monitor->thread_ret < 0) {
        r = monitor->thread_ret;
        goto error;
    }
    ResetEvent(monitor->thread_event);

    r = enumerate(&monitor->match_helper, monitor_enumerate_callback, monitor);
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

    _hs_monitor_clear_devices(&monitor->devices);

    if (monitor->thread_hwnd) {
        PostMessage(monitor->thread_hwnd, WM_CLOSE, 0, 0);
        WaitForSingleObject(monitor->thread, INFINITE);
    }
    CloseHandle(monitor->thread);
    monitor->thread = NULL;

    _hs_array_release(&monitor->events);
    _hs_array_release(&monitor->thread_events);
    _hs_array_release(&monitor->refresh_events);
}

static int process_arrival_event(hs_monitor *monitor, const char *key, hs_enumerate_func *f,
                                 void *udata)
{
    DEVINST inst;
    hs_device *dev = NULL;
    CONFIGRET cret;
    int r;

    cret = CM_Locate_DevNode(&inst, (DEVINSTID)key, CM_LOCATE_DEVNODE_NORMAL);
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_DEBUG, "Device node '%s' does not exist: 0x%lx", key, cret);
        return 0;
    }

    r = process_win32_device(inst, key, &dev);
    if (r <= 0)
        return r;

    r = _hs_match_helper_match(&monitor->match_helper, dev, &dev->match_udata);
    if (r)
        r = _hs_monitor_add(&monitor->devices, dev, f, udata);
    hs_device_unref(dev);

    return r;
}

int hs_monitor_refresh(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    assert(monitor);

    unsigned int event_idx = 0;
    int r;

    if (!monitor->thread)
        return 0;

    if (!monitor->refresh_events.count) {
        /* We don't want to keep the lock for too long, so move all device events to our
           own array and let the background thread work and process Win32 events. */
        EnterCriticalSection(&monitor->events_lock);
        monitor->refresh_events = monitor->events;
        memset(&monitor->events, 0, sizeof(monitor->events));
        r = monitor->thread_ret;
        monitor->thread_ret = 0;
        LeaveCriticalSection(&monitor->events_lock);

        if (r < 0)
            goto cleanup;
    }

    for (; event_idx < monitor->refresh_events.count; event_idx++) {
        struct event *event = &monitor->refresh_events.values[event_idx];

        switch (event->type) {
            case DEVICE_EVENT_ADDED: {
                hs_log(HS_LOG_DEBUG, "Received arrival notification for device '%s'",
                       event->device_key);
                r = process_arrival_event(monitor, event->device_key, f, udata);
                if (r)
                    goto cleanup;
            } break;

            case DEVICE_EVENT_REMOVED: {
                hs_log(HS_LOG_DEBUG, "Received removal notification for device '%s'",
                       event->device_key);
                _hs_monitor_remove(&monitor->devices, event->device_key, f, udata);
            } break;
        }
    }

    r = 0;
cleanup:
    /* If an error occurs, there may be unprocessed notifications. Keep them in
       monitor->refresh_events for the next time this function is called. */
    _hs_array_remove(&monitor->refresh_events, 0, event_idx);
    EnterCriticalSection(&monitor->events_lock);
    if (!monitor->refresh_events.count && !monitor->events.count)
        ResetEvent(monitor->thread_event);
    LeaveCriticalSection(&monitor->events_lock);
    return r;
}

int hs_monitor_list(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    return _hs_monitor_list(&monitor->devices, f, udata);
}
