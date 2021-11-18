/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifdef _WIN32

#include "common_priv.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include "device_priv.h"
#include "platform.h"

#define READ_BUFFER_SIZE 16384

int _hs_open_file_port(hs_device *dev, hs_port_mode mode, hs_port **rport)
{
    hs_port *port = NULL;
    DWORD access;
    int r;

    port = (hs_port *)calloc(1, sizeof(*port));
    if (!port) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    port->type = dev->type;

    port->mode = mode;
    port->path = dev->path;
    port->dev = hs_device_ref(dev);

    access = UINT32_MAX;
    switch (mode) {
        case HS_PORT_MODE_READ: { access = GENERIC_READ; } break;
        case HS_PORT_MODE_WRITE: { access = GENERIC_WRITE; } break;
        case HS_PORT_MODE_RW: { access = GENERIC_READ | GENERIC_WRITE; } break;
    }
    assert(access != UINT32_MAX);

    port->u.handle.h = CreateFileA(dev->path, access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (port->u.handle.h == INVALID_HANDLE_VALUE) {
        switch (GetLastError()) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND: {
                r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
            } break;
            case ERROR_NOT_ENOUGH_MEMORY:
            case ERROR_OUTOFMEMORY: {
                r = hs_error(HS_ERROR_MEMORY, NULL);
            } break;
            case ERROR_ACCESS_DENIED: {
                r = hs_error(HS_ERROR_ACCESS, "Permission denied for device '%s'", dev->path);
            } break;

            default: {
                r = hs_error(HS_ERROR_SYSTEM, "CreateFile('%s') failed: %s", dev->path,
                             hs_win32_strerror(0));
            } break;
        }
        goto error;
    }

    if (dev->type == HS_DEVICE_TYPE_SERIAL) {
        DCB dcb;
        COMMTIMEOUTS timeouts;
        BOOL success;

        dcb.DCBlength = sizeof(dcb);
        success = GetCommState(port->u.handle.h, &dcb);
        if (!success) {
            r = hs_error(HS_ERROR_SYSTEM, "GetCommState() failed on '%s': %s", port->path,
                         hs_win32_strerror(0));
            goto error;
        }

        /* Sane config, inspired by libserialport, and with DTR pin on by default for
           consistency with UNIX platforms. */
        dcb.fBinary = TRUE;
        dcb.fAbortOnError = FALSE;
        dcb.fErrorChar = FALSE;
        dcb.fNull = FALSE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fDsrSensitivity = FALSE;

        /* See SERIAL_TIMEOUTS documentation on MSDN, this basically means "Terminate read request
           when there is at least one byte available". You still need a total timeout in that mode
           so use 0xFFFFFFFE (using 0xFFFFFFFF for all read timeouts is not allowed). Using
           WaitCommEvent() instead would probably be a good idea, I'll look into that later. */
        timeouts.ReadIntervalTimeout = ULONG_MAX;
        timeouts.ReadTotalTimeoutMultiplier = ULONG_MAX;
        timeouts.ReadTotalTimeoutConstant = ULONG_MAX - 1;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 5000;

        success = SetCommState(port->u.handle.h, &dcb);
        if (!success) {
            r = hs_error(HS_ERROR_SYSTEM, "SetCommState() failed on '%s': %s",
                         port->path, hs_win32_strerror(0));
            goto error;
        }
        success = SetCommTimeouts(port->u.handle.h, &timeouts);
        if (!success) {
            r = hs_error(HS_ERROR_SYSTEM, "SetCommTimeouts() failed on '%s': %s",
                         port->path, hs_win32_strerror(0));
            goto error;
        }
        success = PurgeComm(port->u.handle.h, PURGE_RXCLEAR);
        if (!success) {
            r = hs_error(HS_ERROR_SYSTEM, "PurgeComm(PURGE_RXCLEAR) failed on '%s': %s",
                         port->path, hs_win32_strerror(0));
            goto error;
        }
    }

    if (mode & HS_PORT_MODE_READ) {
        port->u.handle.read_ov = (OVERLAPPED *)calloc(1, sizeof(*port->u.handle.read_ov));
        if (!port->u.handle.read_ov) {
            r = hs_error(HS_ERROR_MEMORY, NULL);
            goto error;
        }

        port->u.handle.read_ov->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!port->u.handle.read_ov->hEvent) {
            r = hs_error(HS_ERROR_SYSTEM, "CreateEvent() failed: %s", hs_win32_strerror(0));
            goto error;
        }

        if (dev->type == HS_DEVICE_TYPE_HID) {
            port->u.handle.read_buf_size = dev->u.hid.max_input_len + 1;
        } else {
            port->u.handle.read_buf_size = READ_BUFFER_SIZE;
        }

        if (port->u.handle.read_buf_size) {
            port->u.handle.read_buf = (uint8_t *)malloc(port->u.handle.read_buf_size);
            if (!port->u.handle.read_buf) {
                r = hs_error(HS_ERROR_MEMORY, NULL);
                goto error;
            }

            _hs_win32_start_async_read(port);
            if (port->u.handle.read_status < 0) {
                r = port->u.handle.read_status;
                goto error;
            }
        }
    }

    if (mode & HS_PORT_MODE_WRITE) {
        port->u.handle.write_event = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!port->u.handle.write_event) {
            r = hs_error(HS_ERROR_SYSTEM, "CreateEvent() failed: %s", hs_win32_strerror(0));
            goto error;
        }
    }

    *rport = port;
    return 0;

error:
    hs_port_close(port);
    return r;
}

void _hs_close_file_port(hs_port *port)
{
    if (port) {
        hs_device_unref(port->dev);
        port->dev = NULL;

        if (CancelIoEx(port->u.handle.h, NULL))
            WaitForSingleObject(port->u.handle.read_ov->hEvent, INFINITE);
        if (port->u.handle.h)
            CloseHandle(port->u.handle.h);

        free(port->u.handle.read_buf);
        if (port->u.handle.read_ov && port->u.handle.read_ov->hEvent)
            CloseHandle(port->u.handle.read_ov->hEvent);
        free(port->u.handle.read_ov);

        if (port->u.handle.write_event)
            CloseHandle(port->u.handle.write_event);
    }

    free(port);
}

hs_handle _hs_get_file_port_poll_handle(const hs_port *port)
{
    return port->u.handle.read_ov->hEvent;
}

// Call only when port->status != 0, otherwise you will leak kernel memory
void _hs_win32_start_async_read(hs_port *port)
{
    DWORD ret;

    ret = (DWORD)ReadFile(port->u.handle.h, port->u.handle.read_buf,
                          (DWORD)port->u.handle.read_buf_size, NULL, port->u.handle.read_ov);
    if (!ret && GetLastError() != ERROR_IO_PENDING) {
        CancelIo(port->u.handle.h);

        port->u.handle.read_status = hs_error(HS_ERROR_IO, "I/O error while reading from '%s'",
                                           port->path);
        return;
    }

    port->u.handle.read_status = 0;
}

void _hs_win32_finalize_async_read(hs_port *port, int timeout)
{
    DWORD len, ret;

    if (!port->u.handle.read_buf_size)
        return;

    if (timeout > 0)
        WaitForSingleObject(port->u.handle.read_ov->hEvent, (DWORD)timeout);
    ret = (DWORD)GetOverlappedResult(port->u.handle.h, port->u.handle.read_ov, &len, timeout < 0);
    if (!ret) {
        if (GetLastError() == ERROR_IO_INCOMPLETE) {
            port->u.handle.read_status = 0;
            return;
        }

        port->u.handle.read_status = hs_error(HS_ERROR_IO, "I/O error while reading from '%s'",
                                              port->path);
        return;
    }

    port->u.handle.read_len = (size_t)len;
    port->u.handle.read_ptr = port->u.handle.read_buf;

    port->u.handle.read_status = 1;
}

ssize_t _hs_win32_write_sync(hs_port *port, const uint8_t *buf, size_t size, int timeout)
{
    OVERLAPPED ov = {0};
    DWORD len;
    BOOL success;

    ov.hEvent = port->u.handle.write_event;
    success = WriteFile(port->u.handle.h, buf, (DWORD)size, NULL, &ov);
    if (!success && GetLastError() != ERROR_IO_PENDING)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->path);

    if (timeout > 0)
        WaitForSingleObject(ov.hEvent, (DWORD)timeout);

    success = GetOverlappedResult(port->u.handle.h, &ov, &len, timeout < 0);
    if (!success) {
        if (GetLastError() == ERROR_IO_INCOMPLETE) {
            CancelIoEx(port->u.handle.h, &ov);

            success = GetOverlappedResult(port->u.handle.h, &ov, &len, TRUE);
            if (!success)
                len = 0;
        } else {
            return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->path);
        }
    }

    return (ssize_t)len;
}

#endif
