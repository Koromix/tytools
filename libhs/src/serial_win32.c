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
#include "device_win32_priv.h"
#include "hs/platform.h"
#include "hs/serial.h"

int hs_serial_set_attributes(hs_handle *h, uint32_t rate, int flags)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_SERIAL);

    DCB dcb;
    BOOL success;

    dcb.DCBlength = sizeof(dcb);

    success = GetCommState(h->handle, &dcb);
    if (!success)
        return hs_error(HS_ERROR_SYSTEM, "GetCommState() failed: %s", hs_win32_strerror(0));

    switch (rate) {
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

    switch (flags & HS_SERIAL_MASK_CSIZE) {
    case HS_SERIAL_CSIZE_5BITS:
        dcb.ByteSize = 5;
        break;
    case HS_SERIAL_CSIZE_6BITS:
        dcb.ByteSize = 6;
        break;
    case HS_SERIAL_CSIZE_7BITS:
        dcb.ByteSize = 7;
        break;

    default:
        dcb.ByteSize = 8;
        break;
    }

    switch (flags & HS_SERIAL_MASK_PARITY) {
    case 0:
        dcb.fParity = FALSE;
        dcb.Parity = NOPARITY;
        break;
    case HS_SERIAL_PARITY_ODD:
        dcb.fParity = TRUE;
        dcb.Parity = ODDPARITY;
        break;
    case HS_SERIAL_PARITY_EVEN:
        dcb.fParity = TRUE;
        dcb.Parity = EVENPARITY;
        break;

    default:
        assert(false);
        break;
    }

    dcb.StopBits = 0;
    if (flags & HS_SERIAL_STOP_2BITS)
        dcb.StopBits = TWOSTOPBITS;

    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    switch (flags & HS_SERIAL_MASK_FLOW) {
    case 0:
        break;
    case HS_SERIAL_FLOW_XONXOFF:
        dcb.fOutX = TRUE;
        dcb.fInX = TRUE;
        break;
    case HS_SERIAL_FLOW_RTSCTS:
        dcb.fOutxCtsFlow = TRUE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        break;

    default:
        assert(false);
        break;
    }

    success = SetCommState(h->handle, &dcb);
    if (!success)
        return hs_error(HS_ERROR_SYSTEM, "SetCommState() failed: %s", hs_win32_strerror(0));

    return 0;
}

int hs_serial_set_config(hs_handle *h, const hs_serial_config *config)
{
    assert(h);
    assert(config);

    DCB dcb;
    BOOL success;

    dcb.DCBlength = sizeof(dcb);
    success = GetCommState(h->handle, &dcb);
    if (!success)
        return hs_error(HS_ERROR_SYSTEM, "GetCommState() failed on '%s': %s", h->dev->path,
                        hs_win32_strerror(0));

    switch (config->baudrate) {
    case 0:
        break;
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
    case 230400:
        dcb.BaudRate = config->baudrate;
        break;
    default:
        return hs_error(HS_ERROR_SYSTEM, "Unsupported baud rate value: %u", config->baudrate);
    }

    switch (config->databits) {
    case 0:
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        dcb.ByteSize = (BYTE)config->databits;
        break;
    default:
        return hs_error(HS_ERROR_SYSTEM, "Invalid data bits setting: %u", config->databits);
    }

    switch (config->stopbits) {
    case 0:
        break;
    case 1:
        dcb.StopBits = ONESTOPBIT;
        break;
    case 2:
        dcb.StopBits = TWOSTOPBITS;
        break;
    default:
        return hs_error(HS_ERROR_SYSTEM, "Invalid stop bits setting: %u", config->stopbits);
    }

    switch (config->parity) {
    case 0:
        break;
    case HS_SERIAL_CONFIG_PARITY_OFF:
        dcb.fParity = FALSE;
        dcb.Parity = NOPARITY;
        break;
    case HS_SERIAL_CONFIG_PARITY_EVEN:
        dcb.fParity = TRUE;
        dcb.Parity = EVENPARITY;
        break;
    case HS_SERIAL_CONFIG_PARITY_ODD:
        dcb.fParity = TRUE;
        dcb.Parity = ODDPARITY;
        break;
    case HS_SERIAL_CONFIG_PARITY_MARK:
        dcb.fParity = TRUE;
        dcb.Parity = MARKPARITY;
        break;
    case HS_SERIAL_CONFIG_PARITY_SPACE:
        dcb.fParity = TRUE;
        dcb.Parity = SPACEPARITY;
        break;
    default:
        return hs_error(HS_ERROR_SYSTEM, "Invalid parity setting: %d", config->parity);
    }

    switch (config->rts) {
    case 0:
        break;
    case HS_SERIAL_CONFIG_RTS_OFF:
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.fOutxCtsFlow = FALSE;
        break;
    case HS_SERIAL_CONFIG_RTS_ON:
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        dcb.fOutxCtsFlow = FALSE;
        break;
    case HS_SERIAL_CONFIG_RTS_FLOW:
        dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
        dcb.fOutxCtsFlow = TRUE;
        break;
    default:
        return hs_error(HS_ERROR_SYSTEM, "Invalid RTS setting: %d", config->rts);
    }

    switch (config->dtr) {
    case 0:
        break;
    case HS_SERIAL_CONFIG_DTR_OFF:
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        dcb.fOutxDsrFlow = FALSE;
        break;
    case HS_SERIAL_CONFIG_DTR_ON:
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fOutxDsrFlow = FALSE;
        break;
    default:
        return hs_error(HS_ERROR_SYSTEM, "Invalid DTR setting: %d", config->dtr);
    }

    switch (config->xonxoff) {
    case 0:
        break;
    case HS_SERIAL_CONFIG_XONXOFF_OFF:
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
        break;
    case HS_SERIAL_CONFIG_XONXOFF_IN:
        dcb.fOutX = FALSE;
        dcb.fInX = TRUE;
        break;
    case HS_SERIAL_CONFIG_XONXOFF_OUT:
        dcb.fOutX = TRUE;
        dcb.fInX = FALSE;
        break;
    case HS_SERIAL_CONFIG_XONXOFF_INOUT:
        dcb.fOutX = TRUE;
        dcb.fInX = TRUE;
        break;
    default:
        return hs_error(HS_ERROR_SYSTEM, "Invalid XON/XOFF setting: %d", config->xonxoff);
    }

    success = SetCommState(h->handle, &dcb);
    if (!success)
        return hs_error(HS_ERROR_SYSTEM, "SetCommState() failed on '%s': %s",
                        h->dev->path, hs_win32_strerror(0));

    return 0;
}

int hs_serial_get_config(hs_handle *h, hs_serial_config *config)
{
    assert(h);
    assert(config);

    DCB dcb;
    BOOL success;

    dcb.DCBlength = sizeof(dcb);
    success = GetCommState(h->handle, &dcb);
    if (!success)
        return hs_error(HS_ERROR_SYSTEM, "GetCommState() failed on '%s': %s", h->dev->path,
                        hs_win32_strerror(0));

    /* 0 is the INVALID value for all parameters, we keep that value if we can't interpret
       a DCB setting (only a cross-platform subset of it is exposed in hs_serial_config). */
    memset(config, 0, sizeof(*config));

    config->baudrate = dcb.BaudRate;
    config->databits = dcb.ByteSize;

    // There is also ONE5STOPBITS, ignore it for now (and ever, probably)
    switch (dcb.StopBits) {
    case ONESTOPBIT:
        config->stopbits = 1;
        break;
    case TWOSTOPBITS:
        config->stopbits = 2;
        break;
    }

    if (dcb.fParity) {
        switch (dcb.Parity) {
        case NOPARITY:
            config->parity = HS_SERIAL_CONFIG_PARITY_OFF;
            break;
        case EVENPARITY:
            config->parity = HS_SERIAL_CONFIG_PARITY_EVEN;
            break;
        case ODDPARITY:
            config->parity = HS_SERIAL_CONFIG_PARITY_ODD;
            break;
        case MARKPARITY:
            config->parity = HS_SERIAL_CONFIG_PARITY_MARK;
            break;
        case SPACEPARITY:
            config->parity = HS_SERIAL_CONFIG_PARITY_SPACE;
            break;
        }
    } else {
        config->parity = HS_SERIAL_CONFIG_PARITY_OFF;
    }

    switch (dcb.fRtsControl) {
    case RTS_CONTROL_DISABLE:
        config->rts = HS_SERIAL_CONFIG_RTS_OFF;
        break;
    case RTS_CONTROL_ENABLE:
        config->rts = HS_SERIAL_CONFIG_RTS_ON;
        break;
    case RTS_CONTROL_HANDSHAKE:
        config->rts = HS_SERIAL_CONFIG_RTS_FLOW;
        break;
    }

    switch (dcb.fDtrControl) {
    case DTR_CONTROL_DISABLE:
        config->dtr = HS_SERIAL_CONFIG_DTR_OFF;
        break;
    case DTR_CONTROL_ENABLE:
        config->dtr = HS_SERIAL_CONFIG_DTR_ON;
        break;
    }

    if (dcb.fInX && dcb.fOutX) {
        config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_INOUT;
    } else if (dcb.fInX) {
        config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_IN;
    } else if (dcb.fOutX) {
        config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_OUT;
    } else {
        config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_OFF;
    }

    return 0;
}

ssize_t hs_serial_read(hs_handle *h, uint8_t *buf, size_t size, int timeout)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_SERIAL);
    assert(h->mode & HS_HANDLE_MODE_READ);
    assert(buf);
    assert(size);

    if (h->status < 0) {
        // Could be a transient error, try to restart it
        _hs_win32_start_async_read(h);
        if (h->status < 0)
            return h->status;
    }

    /* Serial devices are stream-based. If we don't have any data yet, see if our asynchronous
       read request has returned anything. Then we can just give the user the data we have, until
       our buffer is empty. We can't just discard stuff, unlike what we do for long HID messages. */
    if (!h->len) {
        _hs_win32_finalize_async_read(h, timeout);
        if (h->status <= 0)
            return h->status;
    }

    if (size > h->len)
        size = h->len;
    memcpy(buf, h->ptr, size);
    h->ptr += size;
    h->len -= size;

    /* Our buffer has been fully read, start a new asynchonous request. I don't know how
       much latency this brings. Maybe double buffering would help, but not before any concrete
       benchmarking is done. */
    if (!h->len) {
        hs_error_mask(HS_ERROR_IO);
        _hs_win32_start_async_read(h);
        hs_error_unmask();
    }

    return (ssize_t)size;
}

ssize_t hs_serial_write(hs_handle *h, const uint8_t *buf, ssize_t size)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_SERIAL);
    assert(h->mode & HS_HANDLE_MODE_WRITE);
    assert(buf);
    
    if (!size)
        return 0;

    OVERLAPPED ov = {0};
    DWORD len;
    BOOL success;

    success = WriteFile(h->handle, buf, (DWORD)size, NULL, &ov);
    if (!success && GetLastError() != ERROR_IO_PENDING) {
        CancelIo(h->handle);
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
    }

    success = GetOverlappedResult(h->handle, &ov, &len, TRUE);
    if (!success)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);

    return (ssize_t)len;
}
