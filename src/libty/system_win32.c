/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "util.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <shlobj.h>
#include "ty/system.h"

typedef ULONGLONG WINAPI GetTickCount64_func(void);

static ULONGLONG WINAPI GetTickCount64_resolve(void);

static GetTickCount64_func *GetTickCount64_ = GetTickCount64_resolve;

static DWORD orig_console_mode;
static bool saved_console_mode;

char *ty_win32_strerror(DWORD err)
{
    static char buf[2048];
    char *ptr;
    DWORD ret;

    if (!err)
        err = GetLastError();

    ret = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf), NULL);

    if (ret) {
        ptr = buf + strlen(buf);
        // FormatMessage adds newlines, remove them
        while (ptr > buf && (ptr[-1] == '\n' || ptr[-1] == '\r'))
            ptr--;
        *ptr = 0;
    } else {
        strcpy(buf, "(unknown)");
    }

    return buf;
}

static ULONGLONG WINAPI GetTickCount64_fallback(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    BOOL success TY_POSSIBLY_UNUSED;

    if (!freq.QuadPart) {
        success = QueryPerformanceFrequency(&freq);
        assert(success);
    }
    success = QueryPerformanceCounter(&now);
    assert(success);

    return (ULONGLONG)now.QuadPart * 1000 / (ULONGLONG)freq.QuadPart;
}

static ULONGLONG WINAPI GetTickCount64_resolve(void)
{
    GetTickCount64_ = (GetTickCount64_func *)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetTickCount64");
    if (!GetTickCount64_)
        GetTickCount64_ = GetTickCount64_fallback;

    return GetTickCount64_();
}

uint64_t ty_millis(void)
{
    return GetTickCount64_();
}

void ty_delay(unsigned int ms)
{
    Sleep(ms);
}

bool ty_compare_paths(const char *path1, const char *path2)
{
    assert(path1);
    assert(path2);

    // This is mainly for COM ports, which exist as COMx files (with x < 10) and \\.\COMx files
    if (strncmp(path1, "\\\\.\\", 4) == 0 || strncmp(path1, "\\\\?\\", 4) == 0)
        path1 += 4;
    if (strncmp(path2, "\\\\.\\", 4) == 0 || strncmp(path2, "\\\\?\\", 4) == 0)
        path2 += 4;

    // Device nodes are not valid Win32 filesystem paths so a simple comparison is enough
    return strcasecmp(path1, path2) == 0;
}

unsigned int ty_descriptor_get_modes(ty_descriptor desc)
{
    DWORD tmp;
    switch (GetFileType(desc)) {
    case FILE_TYPE_PIPE:
        return TY_DESCRIPTOR_MODE_FIFO;
    case FILE_TYPE_CHAR:
        if (GetConsoleMode(desc, &tmp)) {
            return TY_DESCRIPTOR_MODE_DEVICE | TY_DESCRIPTOR_MODE_TERMINAL;
        } else {
            return TY_DESCRIPTOR_MODE_DEVICE;
        }
    case FILE_TYPE_DISK:
        return TY_DESCRIPTOR_MODE_FILE;
    }

    return 0;
}

ty_descriptor ty_standard_get_descriptor(ty_standard_stream std)
{
    switch (std) {
    case TY_STANDARD_INPUT:
        return GetStdHandle(STD_INPUT_HANDLE);
    case TY_STANDARD_OUTPUT:
        return GetStdHandle(STD_OUTPUT_HANDLE);
    case TY_STANDARD_ERROR:
        return GetStdHandle(STD_ERROR_HANDLE);
    }

    assert(false);
    return NULL;
}

int ty_poll(const ty_descriptor_set *set, int timeout)
{
    assert(set);
    assert(set->count);
    assert(set->count <= 64);

    DWORD ret = WaitForMultipleObjects((DWORD)set->count, set->desc, FALSE,
                                       timeout < 0 ? INFINITE : (DWORD)timeout);
    switch (ret) {
    case WAIT_FAILED:
        return ty_error(TY_ERROR_SYSTEM, "WaitForMultipleObjects() failed: %s",
                        ty_win32_strerror(0));
    case WAIT_TIMEOUT:
        return 0;
    }

    return set->id[ret - WAIT_OBJECT_0];
}

int ty_terminal_setup(int flags)
{
    HANDLE handle;
    DWORD mode;
    BOOL success;

    handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
        return ty_error(TY_ERROR_SYSTEM, "GetStdHandle(STD_INPUT_HANDLE) failed");

    success = GetConsoleMode(handle, &mode);
    if (!success) {
        if (GetLastError() == ERROR_INVALID_HANDLE)
            return ty_error(TY_ERROR_UNSUPPORTED, "Not a terminal");
        return ty_error(TY_ERROR_SYSTEM, "GetConsoleMode(STD_INPUT_HANDLE) failed: %s",
                        ty_win32_strerror(0));
    }

    if (!saved_console_mode) {
        orig_console_mode = mode;
        saved_console_mode = true;

        atexit(ty_terminal_restore);
    }

    mode |= ENABLE_PROCESSED_INPUT;

    mode &= (DWORD)~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    if (!(flags & TY_TERMINAL_RAW))
        mode |= ENABLE_LINE_INPUT;
    if (!(flags & TY_TERMINAL_SILENT))
        mode |= ENABLE_ECHO_INPUT;

    success = SetConsoleMode(handle, mode);
    if (!success)
        return ty_error(TY_ERROR_SYSTEM, "SetConsoleMode(STD_INPUT_HANDLE) failed: %s",
                        ty_win32_strerror(0));

    return 0;
}

void ty_terminal_restore(void)
{
    if (!saved_console_mode)
        return;

    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), orig_console_mode);
}
