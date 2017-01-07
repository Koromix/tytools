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

#include "common_priv.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <hidsdi.h>
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
