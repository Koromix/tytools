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

#include "common.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfgmgr32.h>
#include <devioctl.h>
#include <fcntl.h>
#include <io.h>
#include <hidpi.h>
#include <hidsdi.h>
#include <initguid.h>
#include <setupapi.h>
#include <usb.h>
#include <usbioctl.h>
#include <usbiodef.h>
#include <usbuser.h>
#include <winioctl.h>
#include "device.h"
#include "system.h"

struct usb_map {
    ty_device **devices;
    size_t count;
};

struct usb_context {
    struct usb_map *map;

    uint8_t ports[16];
    size_t depth;
};

struct didev_aggregate {
    const GUID* guid;
    HDEVINFO set;
    DWORD i;

    SP_DEVINFO_DATA dev;
    SP_DEVICE_INTERFACE_DATA iface;
    SP_DEVICE_INTERFACE_DETAIL_DATA *detail;
};

struct list_context {
    ty_device_walker *f;
    void *udata;

    ty_device_type type;
};

typedef int enumerate_func(struct didev_aggregate *agg, struct usb_map *map, void *udata);

#ifdef __MINGW32__
// MinGW may miss these
HIDAPI void NTAPI HidD_GetHidGuid(LPGUID HidGuid);
HIDAPI BOOLEAN NTAPI HidD_GetSerialNumberString(HANDLE device, PVOID buffer, ULONG buffer_len);
HIDAPI BOOLEAN NTAPI HidD_GetPreparsedData(HANDLE HidDeviceObject, PHIDP_PREPARSED_DATA *PreparsedData);
HIDAPI BOOLEAN NTAPI HidD_FreePreparsedData(PHIDP_PREPARSED_DATA PreparsedData);
#endif

static const size_t read_buffer_size = 1024;

static int map_add(struct usb_map *map, ty_device *dev)
{
    if (map->count % 32 == 0) {
        ty_device **tmp = realloc(map->devices, (map->count + 32) * sizeof(*tmp));
        if (!tmp)
            return ty_error(TY_ERROR_MEMORY, NULL);
        map->devices = tmp;
    }

    map->devices[map->count++] = ty_device_ref(dev);

    return 0;
}

static void map_free(struct usb_map *map)
{
    for (size_t i = 0; i < map->count; i++)
        ty_device_unref(map->devices[i]);

    free(map->devices);
}

static int enumerate(const GUID *guid, enumerate_func *f, struct usb_map *map, void *udata)
{
    struct didev_aggregate agg = {0};
    DWORD len;
    BOOL success;
    int r;

    agg.set = SetupDiGetClassDevs(guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (!agg.set)
        return ty_error(TY_ERROR_SYSTEM, "SetupDiGetClassDevs() failed: %s",
                        ty_win32_strerror(0));

    agg.dev.cbSize = sizeof(agg.dev);
    agg.iface.cbSize = sizeof(agg.iface);

    for (agg.i = 0; SetupDiEnumDeviceInfo(agg.set, agg.i, &agg.dev); agg.i++) {
        success = SetupDiEnumDeviceInterfaces(agg.set, NULL, guid, agg.i, &agg.iface);
        if (!success)
            return ty_error(TY_ERROR_SYSTEM, "SetupDiEnumDeviceInterfaces() failed: %s",
                            ty_win32_strerror(0));

        success = SetupDiGetDeviceInterfaceDetail(agg.set, &agg.iface, NULL, 0, &len, NULL);
        if (!success && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return ty_error(TY_ERROR_SYSTEM, "SetupDiGetDeviceInterfaceDetail() failed: %s",
                            ty_win32_strerror(0));

        agg.detail = malloc(len);
        if (!agg.detail)
            return ty_error(TY_ERROR_MEMORY, NULL);
        agg.detail->cbSize = sizeof(*agg.detail);

        success = SetupDiGetDeviceInterfaceDetail(agg.set, &agg.iface, agg.detail, len, NULL, NULL);
        if (!success) {
            r = ty_error(TY_ERROR_SYSTEM, "SetupDiGetDeviceInterfaceDetail() failed: %s",
                         ty_win32_strerror(0));
            goto cleanup;
        }

        r = (*f)(&agg, map, udata);
        if (r <= 0)
            goto cleanup;

        free(agg.detail);
        agg.detail = NULL;
    }

    r = 1;
cleanup:
    free(agg.detail);
    SetupDiDestroyDeviceInfoList(agg.set);
    return r;
}

static int wide_to_cstring(wchar_t *wide, size_t size, char **rs)
{
    wchar_t *tmp = NULL;
    char *s = NULL;
    int len, r;

    tmp = calloc(1, size + sizeof(wchar_t));
    if (!tmp)
        return ty_error(TY_ERROR_MEMORY, NULL);

    memcpy(tmp, wide, size);

    if (ty_win32_test_version(TY_WIN32_VISTA)) {
        len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, tmp, -1, NULL, 0, NULL, NULL);
    } else {
        len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, tmp, -1, NULL, 0, NULL, NULL);
    }
    if (!len) {
        r = ty_error(TY_ERROR_PARSE, "Failed to convert UTF-16 string to UTF-8: %s",
                     ty_win32_strerror(0));
        goto cleanup;
    }

    s = malloc((size_t)len);
    if (!s) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    if (ty_win32_test_version(TY_WIN32_VISTA)) {
        len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, tmp, -1, s, len, NULL, NULL);
    } else {
        len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, tmp, -1, s, len, NULL, NULL);
    }
    if (!len) {
        r = ty_error(TY_ERROR_PARSE, "Failed to convert UTF-16 string to UTF-8: %s",
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

// read_hub needs this
static int enumerate_hub(const char *path, struct usb_context *ctx);

static int read_hub(HANDLE h, USB_NODE_CONNECTION_INFORMATION_EX *node,
                    struct usb_context *ctx)
{
    USB_NODE_CONNECTION_NAME pseudo = {0};
    USB_NODE_CONNECTION_NAME *wide;
    DWORD len;
    char *name;
    BOOL success;
    int r;

    pseudo.ConnectionIndex = node->ConnectionIndex;

    success = DeviceIoControl(h, IOCTL_USB_GET_NODE_CONNECTION_NAME, &pseudo, sizeof(pseudo),
                              &pseudo, sizeof(pseudo), &len, NULL);
    if (!success)
        return ty_error(TY_ERROR_IO, "DeviceIoControl() failed: %s", ty_win32_strerror(0));

    wide = calloc(1, pseudo.ActualLength);
    if (!wide)
        return ty_error(TY_ERROR_MEMORY, NULL);

    wide->ConnectionIndex = node->ConnectionIndex;

    success = DeviceIoControl(h, IOCTL_USB_GET_NODE_CONNECTION_NAME, wide, pseudo.ActualLength,
                              wide, pseudo.ActualLength, &len, NULL);
    if (!success) {
        r = ty_error(TY_ERROR_IO, "DeviceIoControl() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    r = wide_to_cstring(wide->NodeName, len - sizeof(pseudo) + 1, &name);
    if (r < 0)
        goto cleanup;

    r = enumerate_hub(name, ctx);
    free(name);
    if (r < 0)
        goto cleanup;

    r = 0;
cleanup:
    free(wide);
    return r;
}

static int get_string_descriptor(HANDLE h, USB_NODE_CONNECTION_INFORMATION_EX *node, uint8_t i,
                                 char **rs)
{
    struct {
        USB_DESCRIPTOR_REQUEST req;
        struct {
            UCHAR bLength;
            UCHAR bDescriptorType;
            WCHAR bString[MAXIMUM_USB_STRING_LENGTH];
        } desc;
    } string = {{0}};
    DWORD len = 0;
    char *s;
    BOOL success;
    int r;

    string.req.ConnectionIndex = node->ConnectionIndex;
    string.req.SetupPacket.wValue = (USHORT)((USB_STRING_DESCRIPTOR_TYPE << 8) | i);
    string.req.SetupPacket.wIndex = 0x409;
    string.req.SetupPacket.wLength = sizeof(string.desc);

    success = DeviceIoControl(h, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, &string, sizeof(string),
                              &string, sizeof(string), &len, NULL);
    if (!success)
        return ty_error(TY_ERROR_IO, "DeviceIoControl() failed: %s", ty_win32_strerror(0));

    if (len < 2 || string.desc.bDescriptorType != USB_STRING_DESCRIPTOR_TYPE ||
            string.desc.bLength != len - sizeof(string.req) || string.desc.bLength % 2 != 0)
        return ty_error(TY_ERROR_IO, "Failed to retrieve string descriptor, got incorrect data");

    r = wide_to_cstring(string.desc.bString, len - sizeof(USB_DESCRIPTOR_REQUEST), &s);
    if (r < 0)
        return r;

    *rs = s;
    return 0;
}

static int make_string_location(uint8_t ports[], size_t depth, char **rpath)
{
    char buf[128];
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

static int add_device(HANDLE h, USB_NODE_CONNECTION_INFORMATION_EX *node,
                      struct usb_context *ctx)
{
    ty_device *dev;
    USB_NODE_CONNECTION_DRIVERKEY_NAME pseudo = {0};
    USB_NODE_CONNECTION_DRIVERKEY_NAME *wide = NULL;
    DWORD len;
    BOOL success;
    int r;

    dev = calloc(1, sizeof(*dev));
    if (!dev)
        return ty_error(TY_ERROR_MEMORY, NULL);
    dev->refcount = 1;

    pseudo.ConnectionIndex = node->ConnectionIndex;

    success = DeviceIoControl(h, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
                              &pseudo, sizeof(pseudo), &pseudo, sizeof(pseudo), &len, NULL);
    if (!success) {
        r = ty_error(TY_ERROR_IO, "DeviceIoControl() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    wide = calloc(1, pseudo.ActualLength);
    if (!wide) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    wide->ConnectionIndex = node->ConnectionIndex;

    success = DeviceIoControl(h, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
                              wide, pseudo.ActualLength, wide, pseudo.ActualLength, &len, NULL);
    if (!success) {
        r = ty_error(TY_ERROR_IO, "DeviceIoControl() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    r = wide_to_cstring(wide->DriverKeyName, len - sizeof(pseudo) + 1, &dev->key);
    if (r < 0)
        goto cleanup;

    r = make_string_location(ctx->ports, ctx->depth, &dev->path);
    if (r < 0)
        goto cleanup;

    dev->vid = node->DeviceDescriptor.idVendor;
    dev->pid = node->DeviceDescriptor.idProduct;

    if (node->DeviceDescriptor.iSerialNumber) {
        r = get_string_descriptor(h, node, node->DeviceDescriptor.iSerialNumber, &dev->serial);
        if (r < 0)
            goto cleanup;
    }

    r = map_add(ctx->map, dev);
    if (r < 0)
       goto cleanup;

    r = 0;
cleanup:
    ty_device_unref(dev);
    free(wide);
    return r;
}

static int read_port(HANDLE h, uint8_t port, struct usb_context *ctx)
{
    assert(ctx->depth < TY_COUNTOF(ctx->ports));

    USB_NODE_CONNECTION_INFORMATION_EX *node;
    DWORD len;
    BOOL success;
    int r;

    len = sizeof(node) + (sizeof(USB_PIPE_INFO) * 30);
    node = calloc(1, len);
    if (!node)
        return ty_error(TY_ERROR_MEMORY, NULL);

    node->ConnectionIndex = port;

    success = DeviceIoControl(h, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, node, len,
                              node, len, &len, NULL);
    if (!success) {
        r = ty_error(TY_ERROR_IO, "DeviceIoControl() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    if (node->ConnectionStatus != DeviceConnected) {
        r = 0;
        goto cleanup;
    }

    ctx->ports[ctx->depth - 1] = port;
    if (node->DeviceIsHub) {
        r = read_hub(h, node, ctx);
    } else {
        r = add_device(h, node, ctx);
    }
    if (r < 0)
        goto cleanup;

    r = 0;
cleanup:
    free(node);
    return r;
}

static int enumerate_hub(const char *name, struct usb_context *ctx)
{
    char *path;
    HANDLE h = NULL;
    USB_NODE_INFORMATION node;
    DWORD len;
    BOOL success;
    int r;

    ctx->depth++;

    r = asprintf(&path, "\\\\.\\%s", name);
    if (r < 0) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }
    h = CreateFile(path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    free(path);
    if (!h) {
        r = ty_error(TY_ERROR_SYSTEM, "Failed to open USB hub device: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    success = DeviceIoControl(h, IOCTL_USB_GET_NODE_INFORMATION, NULL, 0, &node, sizeof(node),
                              &len, NULL);
    if (!success) {
        r = ty_error(TY_ERROR_IO, "DeviceIoControl() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }
    assert(node.NodeType == UsbHub);

    for (uint8_t i = 1; i <= node.u.HubInformation.HubDescriptor.bNumberOfPorts; i++) {
        r = read_port(h, i, ctx);
        if (r < 0 && r != TY_ERROR_IO)
            goto cleanup;
    }

    r = 0;
cleanup:
    if (h)
        CloseHandle(h);
    ctx->depth--;
    return r;
}

static int read_controller(struct didev_aggregate *agg, struct usb_map *map, void *udata)
{
    TY_UNUSED(udata);

    struct usb_context ctx;
    HANDLE h = NULL;
    DWORD len;
    USB_ROOT_HUB_NAME pseudo = {0};
    USB_ROOT_HUB_NAME *wide = NULL;
    char *name;
    BOOL success;
    int r;

    ctx.map = map;
    ctx.ports[0] = (uint8_t)(agg->i + 1);
    ctx.depth = 1;

    h = CreateFile(agg->detail->DevicePath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                   0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        r = ty_error(TY_ERROR_SYSTEM, "Failed to open USB host controller: %s",
                     ty_win32_strerror(0));
        goto cleanup;
    }

    success = DeviceIoControl(h, IOCTL_USB_GET_ROOT_HUB_NAME, NULL, 0, &pseudo, sizeof(pseudo), &len,
                        NULL);
    if (!success) {
        r = ty_error(TY_ERROR_IO, "DeviceIoControl() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    wide = calloc(1, pseudo.ActualLength);
    if (!wide) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    success = DeviceIoControl(h, IOCTL_USB_GET_ROOT_HUB_NAME, NULL, 0, wide, pseudo.ActualLength, &len,
                        NULL);
    if (!success) {
        r = ty_error(TY_ERROR_IO, "DeviceIoControl() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    r = wide_to_cstring(wide->RootHubName, len - sizeof(pseudo) + 1, &name);
    if (r < 0)
        goto cleanup;
    r = enumerate_hub(name, &ctx);
    free(name);
    if (r < 0)
        goto cleanup;

    r = 1;
cleanup:
    free(wide);
    if (h)
        CloseHandle(h);
    return r;
}

static int get_device_id(DEVINST inst, char **rid)
{
    DWORD len;
    char *id = NULL;
    CONFIGRET cret;
    int r;

    cret = CM_Get_Device_ID_Size(&len, inst, 0);
    if (cret != CR_SUCCESS)
        return ty_error(TY_ERROR_SYSTEM, "CM_Get_Device_ID_Size() failed");

    // NULL terminator is not accounted for by CM_Get_Device_ID_Size()
    id = malloc(++len);
    if (!id)
        return ty_error(TY_ERROR_MEMORY, NULL);

    cret = CM_Get_Device_ID(inst, id, len, 0);
    if (cret != CR_SUCCESS) {
        r = ty_error(TY_ERROR_SYSTEM, "CM_Get_Device_ID() failed");
        goto error;
    }

    *rid = id;
    return 0;

error:
    free(id);
    return r;
}

static int update_device_details(struct didev_aggregate *agg, struct usb_map *map, void *udata)
{
    ty_device_type type = *(ty_device_type *)udata;

    char *key = NULL, *id = NULL;
    DWORD len;
    BOOL success;
    int r;

    success = SetupDiGetDeviceRegistryProperty(agg->set, &agg->dev, SPDRP_DRIVER, NULL, NULL, 0,
                                               &len);
    if (!success && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        r = ty_error(TY_ERROR_SYSTEM, "SetupDiGetDeviceRegistryProperty() failed: %s",
                     ty_win32_strerror(0));
        goto cleanup;
    }

    key = malloc(len);
    if (!key) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    success = SetupDiGetDeviceRegistryProperty(agg->set, &agg->dev, SPDRP_DRIVER, NULL, (BYTE *)key,
                                               len, NULL);
    if (!success) {
        r = ty_error(TY_ERROR_SYSTEM, "SetupDiGetDeviceRegistryProperty() failed: %s",
                     ty_win32_strerror(0));
        goto cleanup;
    }

    r = get_device_id(agg->dev.DevInst, &id);
    if (r < 0)
        goto cleanup;

    // If a new device is enumerated by Windows between enumerate_devices() and here,
    // we won't find anything. But that's okay, users will just have to enumerate
    // again to see it.
    for (size_t i = 0; i < map->count; i++) {
        ty_device *dev = map->devices[i];
        if (!dev->id && strcmp(dev->key, key) == 0) {
            dev->type = type;
            dev->id = strdup(id);
            if (!dev->id) {
                r = ty_error(TY_ERROR_MEMORY, NULL);
                goto cleanup;
            }
        }
    }

    r = 1;
cleanup:
    free(id);
    free(key);
    return r;
}

static int find_serial_node(struct didev_aggregate* agg, char **rnode)
{
    HKEY key;
    char buf[32];
    DWORD type, len;
    char *node;
    LONG ret;
    int r;

    key = SetupDiOpenDevRegKey(agg->set, &agg->dev, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
    if (key == INVALID_HANDLE_VALUE)
        return ty_error(TY_ERROR_SYSTEM, "SetupDiOpenDevRegKey() failed: %s",
                        ty_win32_strerror(0));

    len = (DWORD)sizeof(buf);
    ret = RegQueryValueEx(key, "PortName", NULL, &type, (BYTE *)buf, &len);
    if (ret != ERROR_SUCCESS) {
        if (ret == ERROR_FILE_NOT_FOUND) {
            r = 0;
        } else {
            r = ty_error(TY_ERROR_SYSTEM, "RegQueryValueEx() failed: %s",
                         ty_win32_strerror((DWORD)ret));
        }
        goto cleanup;
    }
    node = strdup(buf);
    if (!node)
        return ty_error(TY_ERROR_MEMORY, NULL);

    *rnode = node;

    r = 1;
cleanup:
    RegCloseKey(key);
    return r;
}

static int trigger_device(ty_device *dev, struct didev_aggregate *agg, uint8_t iface,
                          struct list_context *ctx)
{
    int r;

    r = ty_device_dup(dev, &dev);
    if (r < 0)
        return r;

    free(dev->node);

    switch (ctx->type) {
    case TY_DEVICE_SERIAL:
        r = find_serial_node(agg, &dev->node);
        if (r < 0)
            goto cleanup;
        if (!r) {
            r = 1;
            goto cleanup;
        }
        break;

    default:
        dev->node = strdup(agg->detail->DevicePath);
        if (!dev->node) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto cleanup;
        }
        break;
    }

    dev->iface = iface;

    r = (*ctx->f)(dev, ctx->udata);
    if (r <= 0)
        goto cleanup;

    r = 1;
cleanup:
    ty_device_unref(dev);
    return r;
}

static int find_device_and_trigger(struct didev_aggregate *agg, struct usb_map *map, void *udata)
{
    struct list_context *ctx = udata;

    char *ptr;
    uint8_t iface = 0;
    char *id = NULL;
    int r;

    ptr = strstr(agg->detail->DevicePath, "&mi_");
    if (ptr)
        sscanf(ptr, "&mi_%hhu", &iface);

    DEVINST inst = agg->dev.DevInst;
    do {
        r = get_device_id(inst, &id);
        if (r < 0)
            goto cleanup;

        for (size_t i = 0; i < map->count; i++) {
            ty_device *dev = map->devices[i];
            if (dev->id && strcmp(dev->id, id) == 0) {
                if (iface < dev->iface) {
                    r = 1;
                    goto cleanup;
                }

                // Don't trigger anything for this interface again
                dev->iface = (uint8_t)(iface + 1);

                r = trigger_device(dev, agg, iface, ctx);
                goto cleanup;
            }
        }

        free(id);
        id = NULL;
    } while (CM_Get_Parent(&inst, inst, 0) == CR_SUCCESS);

    r = 1;
cleanup:
    free(id);
    return r;
}

int ty_device_list(ty_device_type type, ty_device_walker *f, void *udata)
{
    static GUID hid_guid;

    const GUID* guid = NULL;
    struct usb_map map = {0};
    struct list_context ctx;
    int r;

    if (!hid_guid.Data4[0])
        HidD_GetHidGuid(&hid_guid);

    switch (type) {
    case TY_DEVICE_HID:
        guid = &hid_guid;
        break;
    case TY_DEVICE_SERIAL:
        // GUID_DEVINTERFACE_COMPORT only works for real COM ports... Haven't found any way to
        // list virtual (USB) serial devices, so instead list USB devices and consider them
        // as serial if registry key "PortName" is available (and use its value as device node).
        guid = &GUID_DEVINTERFACE_USB_DEVICE;
        break;
    };
    assert(guid);

    ctx.f = f;
    ctx.udata = udata;
    ctx.type = type;

    // enumerate devices by recursing through host controllers and hubs, which seems
    // to be the best/easiest way to calculate each device's location on Windows.
    ty_error_mask(TY_ERROR_IO);
    r = enumerate(&GUID_CLASS_USB_HOST_CONTROLLER, read_controller, &map, NULL);
    ty_error_unmask();
    if (r < 0 && r != TY_ERROR_IO)
        goto cleanup;

    // use the SetupAPI to list USB devices and find their instance ID, and use the
    // driver key to map them to the devices found previously by enumerate_devices.
    r = enumerate(&GUID_CLASS_USB_DEVICE, update_device_details, &map, &type);
    if (r < 0)
        goto cleanup;

    // now we have to resolve specific device interfaces, and trigger them when found
    r = enumerate(guid, find_device_and_trigger, &map, &ctx);

cleanup:
    map_free(&map);
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

    h->handle = CreateFile(dev->node, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (h->handle == INVALID_HANDLE_VALUE) {
        switch (GetLastError()) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            r = ty_error(TY_ERROR_NOT_FOUND, "Device '%s' not found", dev->node);
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            r = ty_error(TY_ERROR_MEMORY, NULL);
            break;
        case ERROR_ACCESS_DENIED:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied for device '%s'", dev->node);
            break;

        default:
            r = ty_error(TY_ERROR_SYSTEM, "CreateFile('%s') failed: %s", dev->node,
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
    h->ptr = h->buf;

    h->block = block;

    r = ReadFile(h->handle, h->buf, read_buffer_size, &len, h->ov);
    if (!r && GetLastError() != ERROR_IO_PENDING) {
        r = ty_error(TY_ERROR_SYSTEM, "ReadFile() failed: %s", ty_win32_strerror(0));
        goto error;
    }

    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 1000;

    SetCommTimeouts(h->handle, &timeouts);

    h->dev = ty_device_ref(dev);

    *rh = h;
    return 0;

error:
    ty_device_close(h);
    return r;
}

void ty_device_close(ty_handle *h)
{
    assert(h);

    if (h->handle)
        CloseHandle(h->handle);
    if (h->ov && h->ov->hEvent)
        CloseHandle(h->ov->hEvent);
    free(h->ov);
    free(h->buf);
    ty_device_unref(h->dev);

    free(h);
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
        return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->node);
    }

    if (len) {
        if (h->ptr[0]) {
            if (size > len)
                size = (size_t)len;
            memcpy(buf, h->ptr, size);
        } else {
            if (size > --len)
                size = (size_t)len;
            memcpy(buf, h->ptr + 1, size);
        }
    } else {
        size = 0;
    }

    ResetEvent(h->ov->hEvent);
    ret = (DWORD)ReadFile(h->handle, h->buf, read_buffer_size, NULL, h->ov);
    if (!ret && GetLastError() != ERROR_IO_PENDING) {
        CancelIo(h->handle);
        return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->node);
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
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->node);
        }

        r = GetOverlappedResult(h->handle, &ov, &len, TRUE);
        if (!r)
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->node);
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
        return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->node);

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

    printf("read\n");
    if (!h->len) {
        ret = (DWORD)GetOverlappedResult(h->handle, h->ov, &len, h->block);
        if (!ret) {
            if (GetLastError() == ERROR_IO_PENDING)
                return 0;
            return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->node);
        }

        h->ptr = h->buf;
        h->len = (size_t)len;

        ResetEvent(h->ov->hEvent);
        ret = (DWORD)ReadFile(h->handle, h->buf, read_buffer_size, NULL, h->ov);
        if (!ret && GetLastError() != ERROR_IO_PENDING) {
            CancelIo(h->handle);
            return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->node);
        }
    }

    if (size > h->len)
        size = h->len;

    memcpy(buf, h->ptr, size);
    h->ptr += size;
    h->len -= size;

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
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->node);
        }

        r = GetOverlappedResult(h->handle, &ov, &len, TRUE);
        if (!r)
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->node);
    }

    return (ssize_t)len;
}
