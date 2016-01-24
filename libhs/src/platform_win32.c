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
#include "hs/platform.h"

typedef LONG NTAPI RtlGetVersion_func(OSVERSIONINFOW *info);
typedef ULONGLONG WINAPI GetTickCount64_func(void);

static ULONGLONG WINAPI GetTickCount64_fallback(void);

static GetTickCount64_func *GetTickCount64_;
static RtlGetVersion_func *RtlGetVersion_;

_HS_INIT()
{
    GetTickCount64_ = (GetTickCount64_func *)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetTickCount64");
    if (!GetTickCount64_)
        GetTickCount64_ = GetTickCount64_fallback;

    RtlGetVersion_ = (RtlGetVersion_func *)GetProcAddress(GetModuleHandle("ntdll.dll"), "RtlGetVersion");
    assert(RtlGetVersion_);
}

static ULONGLONG WINAPI GetTickCount64_fallback(void)
{
    static LARGE_INTEGER freq;

    LARGE_INTEGER now;
    BOOL ret _HS_POSSIBLY_UNUSED;

    if (!freq.QuadPart) {
        ret = QueryPerformanceFrequency(&freq);
        assert(ret);
    }

    ret = QueryPerformanceCounter(&now);
    assert(ret);

    return (ULONGLONG)now.QuadPart * 1000 / (ULONGLONG)freq.QuadPart;
}

uint64_t hs_millis(void)
{
    return GetTickCount64_();
}

int hs_poll(hs_poll_source *sources, unsigned int count, int timeout)
{
    assert(sources);
    assert(count);
    assert(count <= HS_POLL_MAX_SOURCES);

    HANDLE handles[HS_POLL_MAX_SOURCES];

    for (unsigned int i = 0; i < count; i++) {
        handles[i] = sources[i].desc;
        sources[i].ready = 0;
    }

    DWORD ret = WaitForMultipleObjects((DWORD)count, handles, FALSE,
                                       timeout < 0 ? INFINITE : (DWORD)timeout);
    if (ret == WAIT_FAILED)
        return hs_error(HS_ERROR_SYSTEM, "WaitForMultipleObjects() failed: %s",
                        hs_win32_strerror(0));

    for (unsigned int i = 0; i < count; i++)
        sources[i].ready = (i == ret);

    return ret < count;
}

const char *hs_win32_strerror(DWORD err)
{
    static _HS_THREAD_LOCAL char buf[256];
    char *ptr;
    DWORD r;

    if (!err)
        err = GetLastError();

    r = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf), NULL);

    if (r) {
        ptr = buf + strlen(buf);
        // FormatMessage adds newlines, remove them
        while (ptr > buf && (ptr[-1] == '\n' || ptr[-1] == '\r'))
            ptr--;
        *ptr = 0;
    } else {
        sprintf(buf, "Unknown error 0x%08lx", err);
    }

    return buf;
}

uint32_t hs_win32_version(void)
{
    OSVERSIONINFOW info;

    // Windows 8.1 broke GetVersionEx, so bypass the intermediary
    info.dwOSVersionInfoSize = sizeof(info);
    RtlGetVersion_(&info);

    return info.dwMajorVersion * 100 + info.dwMinorVersion;
}
