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
HS_BEGIN_C
    #include <hidsdi.h>
HS_END_C
#include <winioctl.h>
#include "device_priv.h"
#include "hid.h"
#include "platform.h"

// Copied from hidclass.h in the MinGW-w64 headers
#define HID_OUT_CTL_CODE(id) \
    CTL_CODE(FILE_DEVICE_KEYBOARD, (id), METHOD_OUT_DIRECT, FILE_ANY_ACCESS)
#define IOCTL_HID_GET_FEATURE HID_OUT_CTL_CODE(100)

ssize_t hs_hid_read(hs_port *port, uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    if (port->u.handle.read_status < 0) {
        // Could be a transient error, try to restart it
        _hs_win32_start_async_read(port);
        if (port->u.handle.read_status < 0)
            return port->u.handle.read_status;
    }

    _hs_win32_finalize_async_read(port, timeout);
    if (port->u.handle.read_status <= 0)
        return port->u.handle.read_status;

    /* HID communication is message-based. So if the caller does not provide a big enough
       buffer, we can just discard the extra data, unlike for serial communication. */
    if (port->u.handle.read_len) {
        if (size > port->u.handle.read_len)
            size = port->u.handle.read_len;
        memcpy(buf, port->u.handle.read_buf, size);
    } else {
        size = 0;
    }

    hs_error_mask(HS_ERROR_IO);
    _hs_win32_start_async_read(port);
    hs_error_unmask();

    return (ssize_t)size;
}

ssize_t hs_hid_write(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    if (size < 2)
        return 0;

    ssize_t r = _hs_win32_write_sync(port, buf, size, 5000);
    if (!r)
        return hs_error(HS_ERROR_IO, "Timed out while writing to '%s'", port->dev->path);

    return r;
}

ssize_t hs_hid_get_feature_report(hs_port *port, uint8_t report_id, uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    OVERLAPPED ov = {0};
    DWORD len;
    BOOL success;

    buf[0] = report_id;
    len = (DWORD)size;

    success = DeviceIoControl(port->u.handle.h, IOCTL_HID_GET_FEATURE, buf, (DWORD)size, buf,
                              (DWORD)size, NULL, &ov);
    if (!success && GetLastError() != ERROR_IO_PENDING) {
        CancelIo(port->u.handle.h);
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->dev->path);
    }

    success = GetOverlappedResult(port->u.handle.h, &ov, &len, TRUE);
    if (!success)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->dev->path);

    /* Apparently the length returned by the IOCTL_HID_GET_FEATURE ioctl does not account
       for the report ID byte. */
    return (ssize_t)len + 1;
}

ssize_t hs_hid_send_feature_report(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    if (size < 2)
        return 0;

    // Timeout behavior?
    BOOL success = HidD_SetFeature(port->u.handle.h, (char *)buf, (DWORD)size);
    if (!success)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->dev->path);

    return (ssize_t)size;
}
