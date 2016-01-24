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
#include <process.h>
#include "device_win32_priv.h"
#include "hs/platform.h"

typedef BOOL WINAPI CancelIoEx_func(HANDLE hFile, LPOVERLAPPED lpOverlapped);

static CancelIoEx_func *CancelIoEx_;

#define READ_BUFFER_SIZE 16384

const struct _hs_device_vtable _hs_win32_device_vtable;

_HS_INIT()
{
    HMODULE h = LoadLibrary("kernel32.dll");
    assert(h);

    CancelIoEx_ = (CancelIoEx_func *)GetProcAddress(h, "CancelIoEx");
}

static int open_win32_device(hs_device *dev, hs_handle **rh)
{
    hs_handle *h = NULL;
    COMMTIMEOUTS timeouts;
    int r;

    h = calloc(1, sizeof(*h));
    if (!h) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    h->dev = hs_device_ref(dev);

    h->handle = CreateFile(dev->path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (h->handle == INVALID_HANDLE_VALUE) {
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

    h->ov = calloc(1, sizeof(*h->ov));
    if (!h->ov) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }

    h->ov->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!h->ov->hEvent) {
        r = hs_error(HS_ERROR_SYSTEM, "CreateEvent() failed: %s", hs_win32_strerror(0));
        goto error;
    }

    h->buf = malloc(READ_BUFFER_SIZE);
    if (!h->buf) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }

    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 1000;

    SetCommTimeouts(h->handle, &timeouts);

    if (dev->type == HS_DEVICE_TYPE_SERIAL)
        EscapeCommFunction(h->handle, SETDTR);

    _hs_win32_start_async_read(h);

    *rh = h;
    return 0;

error:
    hs_handle_close(h);
    return r;
}

static void close_win32_device(hs_handle *h);
static unsigned int __stdcall overlapped_cleanup_thread(void *udata)
{
    hs_handle *h = udata;
    DWORD ret;

    /* Give up if nothing happens, even if it means a leak; we'll get rid of this when XP
       becomes irrelevant anyway. Hope this happens within my lifetime. */
    ret = WaitForSingleObject(h->ov->hEvent, 120000);
    if (ret != WAIT_OBJECT_0) {
        hs_log(HS_LOG_WARNING, "Cannot stop asynchronous read request, leaking handle");
        return 0;
    }

    h->pending_thread = 0;
    close_win32_device(h);

    return 0;
}

static void close_win32_device(hs_handle *h)
{
    if (h) {
        hs_device_unref(h->dev);
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
                HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, overlapped_cleanup_thread, h, 0, NULL);
                if (thread) {
                    hs_log(HS_LOG_WARNING, "Cannot stop asynchronous read request, leaking handle");
                    CloseHandle(thread);
                }
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

static hs_descriptor get_win32_descriptor(const hs_handle *h)
{
    return h->ov->hEvent;
}

const struct _hs_device_vtable _hs_win32_device_vtable = {
    .open = open_win32_device,
    .close = close_win32_device,

    .get_descriptor = get_win32_descriptor
};

int _hs_win32_start_async_read(hs_handle *h)
{
    DWORD ret;

    ret = (DWORD)ReadFile(h->handle, h->buf, READ_BUFFER_SIZE, NULL, h->ov);
    if (!ret && GetLastError() != ERROR_IO_PENDING) {
        CancelIo(h->handle);
        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s'", h->dev->path);
    }

    h->pending_thread = GetCurrentThreadId();
    return 0;
}

ssize_t _hs_win32_finalize_async_read(hs_handle *h, int timeout)
{
    DWORD len, ret;

    if (timeout > 0)
        WaitForSingleObject(h->ov->hEvent, (DWORD)timeout);

    ret = (DWORD)GetOverlappedResult(h->handle, h->ov, &len, timeout < 0);
    if (!ret) {
        if (GetLastError() == ERROR_IO_INCOMPLETE)
            return 0;

        h->pending_thread = 0;
        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s'", h->dev->path);
    }
    h->pending_thread = 0;

    return (ssize_t)len;
}
