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
#include "device_priv.h"
#include "platform.h"
#include "serial.h"

int hs_serial_set_config(hs_port *port, const hs_serial_config *config)
{
    assert(port);
    assert(config);

    DCB dcb;
    BOOL success;

    dcb.DCBlength = sizeof(dcb);
    success = GetCommState(port->u.handle.h, &dcb);
    if (!success)
        return hs_error(HS_ERROR_SYSTEM, "GetCommState() failed on '%s': %s", port->dev->path,
                        hs_win32_strerror(0));

    switch (config->baudrate) {
        case 0: {} break;

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
        case 230400: {
            dcb.BaudRate = config->baudrate;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Unsupported baud rate value: %u", config->baudrate);
        } break;
    }

    switch (config->databits) {
        case 0: {} break;

        case 5:
        case 6:
        case 7:
        case 8: {
            dcb.ByteSize = (BYTE)config->databits;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid data bits setting: %u", config->databits);
        } break;
    }

    switch (config->stopbits) {
        case 0: {} break;

        case 1: { dcb.StopBits = ONESTOPBIT; } break;
        case 2: { dcb.StopBits = TWOSTOPBITS; } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid stop bits setting: %u", config->stopbits);
        } break;
    }

    switch (config->parity) {
        case 0: {} break;

        case HS_SERIAL_CONFIG_PARITY_OFF: {
            dcb.fParity = FALSE;
            dcb.Parity = NOPARITY;
        } break;
        case HS_SERIAL_CONFIG_PARITY_EVEN: {
            dcb.fParity = TRUE;
            dcb.Parity = EVENPARITY;
        } break;
        case HS_SERIAL_CONFIG_PARITY_ODD: {
            dcb.fParity = TRUE;
            dcb.Parity = ODDPARITY;
        } break;
        case HS_SERIAL_CONFIG_PARITY_MARK: {
            dcb.fParity = TRUE;
            dcb.Parity = MARKPARITY;
        } break;
        case HS_SERIAL_CONFIG_PARITY_SPACE: {
            dcb.fParity = TRUE;
            dcb.Parity = SPACEPARITY;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid parity setting: %d", config->parity);
        } break;
    }

    switch (config->rts) {
        case 0: {} break;

        case HS_SERIAL_CONFIG_RTS_OFF: {
            dcb.fRtsControl = RTS_CONTROL_DISABLE;
            dcb.fOutxCtsFlow = FALSE;
        } break;
        case HS_SERIAL_CONFIG_RTS_ON: {
            dcb.fRtsControl = RTS_CONTROL_ENABLE;
            dcb.fOutxCtsFlow = FALSE;
        } break;
        case HS_SERIAL_CONFIG_RTS_FLOW: {
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
            dcb.fOutxCtsFlow = TRUE;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid RTS setting: %d", config->rts);
        } break;
    }

    switch (config->dtr) {
        case 0: {} break;

        case HS_SERIAL_CONFIG_DTR_OFF: {
            dcb.fDtrControl = DTR_CONTROL_DISABLE;
            dcb.fOutxDsrFlow = FALSE;
        } break;
        case HS_SERIAL_CONFIG_DTR_ON: {
            dcb.fDtrControl = DTR_CONTROL_ENABLE;
            dcb.fOutxDsrFlow = FALSE;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid DTR setting: %d", config->dtr);
        } break;
    }

    switch (config->xonxoff) {
        case 0: {} break;

        case HS_SERIAL_CONFIG_XONXOFF_OFF: {
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
        } break;
        case HS_SERIAL_CONFIG_XONXOFF_IN: {
            dcb.fOutX = FALSE;
            dcb.fInX = TRUE;
        } break;
        case HS_SERIAL_CONFIG_XONXOFF_OUT: {
            dcb.fOutX = TRUE;
            dcb.fInX = FALSE;
        } break;
        case HS_SERIAL_CONFIG_XONXOFF_INOUT: {
            dcb.fOutX = TRUE;
            dcb.fInX = TRUE;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid XON/XOFF setting: %d", config->xonxoff);
        } break;
    }

    success = SetCommState(port->u.handle.h, &dcb);
    if (!success)
        return hs_error(HS_ERROR_SYSTEM, "SetCommState() failed on '%s': %s",
                        port->dev->path, hs_win32_strerror(0));

    return 0;
}

int hs_serial_get_config(hs_port *port, hs_serial_config *config)
{
    assert(port);
    assert(config);

    DCB dcb;
    BOOL success;

    dcb.DCBlength = sizeof(dcb);
    success = GetCommState(port->u.handle.h, &dcb);
    if (!success)
        return hs_error(HS_ERROR_SYSTEM, "GetCommState() failed on '%s': %s", port->dev->path,
                        hs_win32_strerror(0));

    /* 0 is the INVALID value for all parameters, we keep that value if we can't interpret
       a DCB setting (only a cross-platform subset of it is exposed in hs_serial_config). */
    memset(config, 0, sizeof(*config));

    config->baudrate = dcb.BaudRate;
    config->databits = dcb.ByteSize;

    // There is also ONE5STOPBITS, ignore it for now (and ever, probably)
    switch (dcb.StopBits) {
        case ONESTOPBIT: { config->stopbits = 1; } break;
        case TWOSTOPBITS: { config->stopbits = 2; } break;
    }

    if (dcb.fParity) {
        switch (dcb.Parity) {
            case NOPARITY: { config->parity = HS_SERIAL_CONFIG_PARITY_OFF; } break;
            case EVENPARITY: { config->parity = HS_SERIAL_CONFIG_PARITY_EVEN; } break;
            case ODDPARITY: { config->parity = HS_SERIAL_CONFIG_PARITY_ODD; } break;
            case MARKPARITY: { config->parity = HS_SERIAL_CONFIG_PARITY_MARK; } break;
            case SPACEPARITY: { config->parity = HS_SERIAL_CONFIG_PARITY_SPACE; } break;
        }
    } else {
        config->parity = HS_SERIAL_CONFIG_PARITY_OFF;
    }

    switch (dcb.fRtsControl) {
        case RTS_CONTROL_DISABLE: { config->rts = HS_SERIAL_CONFIG_RTS_OFF; } break;
        case RTS_CONTROL_ENABLE: { config->rts = HS_SERIAL_CONFIG_RTS_ON; } break;
        case RTS_CONTROL_HANDSHAKE: { config->rts = HS_SERIAL_CONFIG_RTS_FLOW; } break;
    }

    switch (dcb.fDtrControl) {
        case DTR_CONTROL_DISABLE: { config->dtr = HS_SERIAL_CONFIG_DTR_OFF; } break;
        case DTR_CONTROL_ENABLE: { config->dtr = HS_SERIAL_CONFIG_DTR_ON; } break;
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

ssize_t hs_serial_read(hs_port *port, uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_SERIAL);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    if (port->u.handle.read_status < 0) {
        // Could be a transient error, try to restart it
        _hs_win32_start_async_read(port);
        if (port->u.handle.read_status < 0)
            return port->u.handle.read_status;
    }

    /* Serial devices are stream-based. If we don't have any data yet, see if our asynchronous
       read request has returned anything. Then we can just give the user the data we have, until
       our buffer is empty. We can't just discard stuff, unlike what we do for long HID messages. */
    if (!port->u.handle.read_len) {
        _hs_win32_finalize_async_read(port, timeout);
        if (port->u.handle.read_status <= 0)
            return port->u.handle.read_status;
    }

    if (size > port->u.handle.read_len)
        size = port->u.handle.read_len;
    memcpy(buf, port->u.handle.read_ptr, size);
    port->u.handle.read_ptr += size;
    port->u.handle.read_len -= size;

    /* Our buffer has been fully read, start a new asynchonous request. I don't know how
       much latency this brings. Maybe double buffering would help, but not before any concrete
       benchmarking is done. */
    if (!port->u.handle.read_len) {
        hs_error_mask(HS_ERROR_IO);
        _hs_win32_start_async_read(port);
        hs_error_unmask();
    }

    return (ssize_t)size;
}

ssize_t hs_serial_write(hs_port *port, const uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_SERIAL);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);
    
    if (!size)
        return 0;

    return _hs_win32_write_sync(port, buf, size, timeout);
}
