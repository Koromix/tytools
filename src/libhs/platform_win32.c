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
#include "platform.h"

typedef ULONGLONG WINAPI GetTickCount64_func(void);
typedef LONG NTAPI RtlGetVersion_func(OSVERSIONINFOW *info);

static ULONGLONG WINAPI GetTickCount64_resolve(void);
static GetTickCount64_func *GetTickCount64_ = GetTickCount64_resolve;
static LONG NTAPI RtlGetVersion_resolve(OSVERSIONINFOW *info);
static RtlGetVersion_func *RtlGetVersion_ = RtlGetVersion_resolve;

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

static ULONGLONG WINAPI GetTickCount64_resolve(void)
{
    GetTickCount64_ = (GetTickCount64_func *)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetTickCount64");
    if (!GetTickCount64_)
        GetTickCount64_ = GetTickCount64_fallback;

    return GetTickCount64_();
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

static LONG NTAPI RtlGetVersion_resolve(OSVERSIONINFOW *info)
{
    RtlGetVersion_ = (RtlGetVersion_func *)GetProcAddress(GetModuleHandle("ntdll.dll"),
                                                          "RtlGetVersion");
    return RtlGetVersion_(info);
}

uint32_t hs_win32_version(void)
{
    static uint32_t version;

    if (!version) {
        OSVERSIONINFOW info;

        // Windows 8.1 broke GetVersionEx, so bypass the intermediary
        info.dwOSVersionInfoSize = sizeof(info);
        RtlGetVersion_(&info);

        version = info.dwMajorVersion * 100 + info.dwMinorVersion;
    }

    return version;
}
