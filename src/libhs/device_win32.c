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
#include <process.h>
#include "device_priv.h"
#include "platform.h"

typedef BOOL WINAPI CancelIoEx_func(HANDLE hFile, LPOVERLAPPED lpOverlapped);

static BOOL WINAPI CancelIoEx_resolve(HANDLE hFile, LPOVERLAPPED lpOverlapped);
static CancelIoEx_func *CancelIoEx_ = CancelIoEx_resolve;

#define READ_BUFFER_SIZE 16384

static int open_win32_device(hs_device *dev, hs_port_mode mode, hs_port **rport)
{
    hs_port *port = NULL;
    DWORD access;
    int r;

    port = calloc(1, sizeof(*port));
    if (!port) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    port->dev = hs_device_ref(dev);
    port->mode = mode;

    switch (mode) {
    case HS_PORT_MODE_READ:
        access = GENERIC_READ;
        break;
    case HS_PORT_MODE_WRITE:
        access = GENERIC_WRITE;
        break;
    case HS_PORT_MODE_RW:
        access = GENERIC_READ | GENERIC_WRITE;
        break;
    }

    port->u.handle.h = CreateFile(dev->path, access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (port->u.handle.h == INVALID_HANDLE_VALUE) {
        switch (GetLastError()) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            r = hs_error(HS_ERROR_MEMORY, NULL);
            break;
        case ERROR_ACCESS_DENIED:
            r = hs_error(HS_ERROR_ACCESS, "Permission denied for device '%s'", dev->path);
            break;

        default:
            r = hs_error(HS_ERROR_SYSTEM, "CreateFile('%s') failed: %s", dev->path,
                         hs_win32_strerror(0));
            break;
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
            r = hs_error(HS_ERROR_SYSTEM, "GetCommState() failed on '%s': %s", port->dev->path,
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
                         port->dev->path, hs_win32_strerror(0));
            goto error;
        }
        success = SetCommTimeouts(port->u.handle.h, &timeouts);
        if (!success) {
            r = hs_error(HS_ERROR_SYSTEM, "SetCommTimeouts() failed on '%s': %s",
                         port->dev->path, hs_win32_strerror(0));
            goto error;
        }
        success = PurgeComm(port->u.handle.h, PURGE_RXCLEAR);
        if (!success) {
            r = hs_error(HS_ERROR_SYSTEM, "PurgeComm(PURGE_RXCLEAR) failed on '%s': %s",
                         port->dev->path, hs_win32_strerror(0));
            goto error;
        }
    }

    if (mode & HS_PORT_MODE_READ) {
        port->u.handle.read_ov = calloc(1, sizeof(*port->u.handle.read_ov));
        if (!port->u.handle.read_ov) {
            r = hs_error(HS_ERROR_MEMORY, NULL);
            goto error;
        }

        port->u.handle.read_ov->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!port->u.handle.read_ov->hEvent) {
            r = hs_error(HS_ERROR_SYSTEM, "CreateEvent() failed: %s", hs_win32_strerror(0));
            goto error;
        }

        port->u.handle.read_buf = malloc(READ_BUFFER_SIZE);
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

    if (mode & HS_PORT_MODE_WRITE) {
        port->u.handle.write_event = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!port->u.handle.write_event) {
            r = hs_error(HS_ERROR_SYSTEM, "CreateEvent() failed: %s", hs_win32_strerror(0));
            goto error;
        }

        /* We need to cancel IO for writes without interfering with pending read requests,
           we can use CancelIoEx() on Vista or CancelIo() on a duplicate handle on XP. */
        if ((mode & HS_PORT_MODE_READ) && hs_win32_version() < HS_WIN32_VERSION_VISTA) {
            BOOL success;

            hs_log(HS_LOG_DEBUG, "Using duplicate handle to write to '%s'", dev->path);
            success = DuplicateHandle(GetCurrentProcess(), port->u.handle.h, GetCurrentProcess(),
                                      &port->u.handle.write_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
            if (!success) {
                r = hs_error(HS_ERROR_SYSTEM, "DuplicateHandle() failed: %s", hs_win32_strerror(0));
                port->u.handle.write_handle = NULL;
                goto error;
            }
        } else {
            port->u.handle.write_handle = port->u.handle.h;
        }
    }

    *rport = port;
    return 0;

error:
    hs_port_close(port);
    return r;
}

static void close_win32_device(hs_port *port);
static unsigned int __stdcall overlapped_cleanup_thread(void *udata)
{
    hs_port *port = udata;
    DWORD ret;

    /* Give up if nothing happens, even if it means a leak; we'll get rid of this when XP
       becomes irrelevant anyway. Hope this happens within my lifetime. */
    ret = WaitForSingleObject(port->u.handle.read_ov->hEvent, 120000);
    if (ret != WAIT_OBJECT_0) {
        hs_log(HS_LOG_WARNING, "Cannot stop asynchronous read request, leaking handle");
        return 0;
    }

    port->u.handle.read_pending_thread = 0;
    close_win32_device(port);

    return 0;
}

static BOOL WINAPI CancelIoEx_resolve(HANDLE hFile, LPOVERLAPPED lpOverlapped)
{
    CancelIoEx_ = (CancelIoEx_func *)GetProcAddress(LoadLibrary("kernel32.dll"), "CancelIoEx");
    return CancelIoEx_(hFile, lpOverlapped);
}

static void close_win32_device(hs_port *port)
{
    if (port) {
        hs_device_unref(port->dev);
        port->dev = NULL;

        if (port->u.handle.read_pending_thread) {
            if (hs_win32_version() >= HS_WIN32_VERSION_VISTA) {
                CancelIoEx_(port->u.handle.h, NULL);
                WaitForSingleObject(port->u.handle.read_ov->hEvent, INFINITE);
            } else if (port->u.handle.read_pending_thread == GetCurrentThreadId()) {
                CancelIo(port->u.handle.h);
                WaitForSingleObject(port->u.handle.read_ov->hEvent, INFINITE);
            } else {
                CloseHandle(port->u.handle.h);
                port->u.handle.h = NULL;

                /* CancelIoEx does not exist on XP, so instead we create a new thread to
                   cleanup when pending I/O stops. And if the thread cannot be created or
                   the kernel does not set port->ov->hEvent, just leaking seems better than a
                   potential segmentation fault. */
                HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, overlapped_cleanup_thread, port, 0, NULL);
                if (thread) {
                    hs_log(HS_LOG_WARNING, "Cannot stop asynchronous read request, leaking handle");
                    CloseHandle(thread);
                }
                return;
            }
        }

        if (port->u.handle.write_handle && port->u.handle.write_handle != port->u.handle.h)
            CloseHandle(port->u.handle.write_handle);
        if (port->u.handle.h)
            CloseHandle(port->u.handle.h);

        if (port->u.handle.write_event)
            CloseHandle(port->u.handle.write_event);
        free(port->u.handle.read_buf);
        if (port->u.handle.read_ov && port->u.handle.read_ov->hEvent)
            CloseHandle(port->u.handle.read_ov->hEvent);
        free(port->u.handle.read_ov);
    }

    free(port);
}

static hs_handle get_win32_handle(const hs_port *port)
{
    return port->u.handle.read_ov->hEvent;
}

const struct _hs_device_vtable _hs_win32_device_vtable = {
    .open = open_win32_device,
    .close = close_win32_device,

    .get_poll_handle = get_win32_handle
};

// Call only when port->status != 0, otherwise you will leak kernel memory
void _hs_win32_start_async_read(hs_port *port)
{
    DWORD ret;

    ret = (DWORD)ReadFile(port->u.handle.h, port->u.handle.read_buf, READ_BUFFER_SIZE, NULL,
                          port->u.handle.read_ov);
    if (!ret && GetLastError() != ERROR_IO_PENDING) {
        CancelIo(port->u.handle.h);

        port->u.handle.read_status = hs_error(HS_ERROR_IO, "I/O error while reading from '%s'",
                                           port->dev->path);
        return;
    }

    port->u.handle.read_pending_thread = GetCurrentThreadId();
    port->u.handle.read_status = 0;
}

void _hs_win32_finalize_async_read(hs_port *port, int timeout)
{
    DWORD len, ret;

    if (timeout > 0)
        WaitForSingleObject(port->u.handle.read_ov->hEvent, (DWORD)timeout);

    ret = (DWORD)GetOverlappedResult(port->u.handle.h, port->u.handle.read_ov, &len, timeout < 0);
    if (!ret) {
        if (GetLastError() == ERROR_IO_INCOMPLETE) {
            port->u.handle.read_status = 0;
            return;
        }

        port->u.handle.read_pending_thread = 0;
        port->u.handle.read_status = hs_error(HS_ERROR_IO, "I/O error while reading from '%s'",
                                           port->dev->path);
        return;
    }

    port->u.handle.read_len = (size_t)len;
    port->u.handle.read_ptr = port->u.handle.read_buf;

    port->u.handle.read_pending_thread = 0;
    port->u.handle.read_status = 1;
}

ssize_t _hs_win32_write_sync(hs_port *port, const uint8_t *buf, size_t size, int timeout)
{
    OVERLAPPED ov = {0};
    DWORD len;
    BOOL success;

    ov.hEvent = port->u.handle.write_event;
    success = WriteFile(port->u.handle.write_handle, buf, (DWORD)size, NULL, &ov);
    if (!success && GetLastError() != ERROR_IO_PENDING)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->dev->path);

    if (timeout > 0)
        WaitForSingleObject(ov.hEvent, (DWORD)timeout);

    success = GetOverlappedResult(port->u.handle.write_handle, &ov, &len, timeout < 0);
    if (!success) {
        if (GetLastError() == ERROR_IO_INCOMPLETE) {
            if (port->u.handle.write_handle != port->u.handle.h) {
                CancelIo(port->u.handle.write_handle);
            } else {
                CancelIoEx_(port->u.handle.write_handle, &ov);
            }

            success = GetOverlappedResult(port->u.handle.write_handle, &ov, &len, TRUE);
            if (!success)
                len = 0;
        } else {
            return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->dev->path);
        }
    }

    return (ssize_t)len;
}
