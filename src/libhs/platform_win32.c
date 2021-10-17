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

typedef LONG NTAPI RtlGetVersion_func(OSVERSIONINFOW *info);

uint64_t hs_millis(void)
{
    return GetTickCount64();
}

void hs_delay(unsigned int ms)
{
    Sleep(ms);
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

    r = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
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
    static uint32_t version;

    if (!version) {
        OSVERSIONINFOW info;

        // Windows 8.1 broke GetVersionEx, so bypass the intermediary
        info.dwOSVersionInfoSize = sizeof(info);

        RtlGetVersion_func *RtlGetVersion =
            (RtlGetVersion_func *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
        RtlGetVersion(&info);

        version = info.dwMajorVersion * 100 + info.dwMinorVersion;
    }

    return version;
}
