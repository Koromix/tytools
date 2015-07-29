/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <shlobj.h>
#include "ty/system.h"

typedef ULONGLONG WINAPI GetTickCount64_func(void);

static ULONGLONG WINAPI GetTickCount64_fallback(void);

HANDLE _ty_win32_descriptors[3];

static GetTickCount64_func *GetTickCount64_;

static DWORD orig_console_mode;
static bool saved_console_mode;

TY_INIT()
{
    HANDLE h = GetModuleHandle("kernel32.dll");
    assert(h);

    GetTickCount64_ = (GetTickCount64_func *)GetProcAddress(h, "GetTickCount64");
    if (!GetTickCount64_)
        GetTickCount64_ = GetTickCount64_fallback;

    _ty_win32_descriptors[0] = GetStdHandle(STD_INPUT_HANDLE);
    _ty_win32_descriptors[1] = GetStdHandle(STD_OUTPUT_HANDLE);
    _ty_win32_descriptors[2] = GetStdHandle(STD_ERROR_HANDLE);
}

char *ty_win32_strerror(DWORD err)
{
    static char buf[2048];
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
        strcpy(buf, "(unknown)");
    }

    return buf;
}

static ULONGLONG WINAPI GetTickCount64_fallback(void)
{
    static LARGE_INTEGER freq;

    LARGE_INTEGER now;
    BOOL ret;

    if (!freq.QuadPart) {
        ret = QueryPerformanceFrequency(&freq);
        assert(ret);
    }

    ret = QueryPerformanceCounter(&now);
    assert(ret);

    return (ULONGLONG)now.QuadPart * 1000 / (ULONGLONG)freq.QuadPart;
}

uint64_t ty_millis(void)
{
    return GetTickCount64_();
}

void ty_delay(unsigned int ms)
{
    Sleep(ms);
}

static uint64_t filetime_to_unix_time(FILETIME *ft)
{
    uint64_t time = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;

    return time / 10000000 - 11644473600ull;
}

int ty_stat(const char *path, ty_file_info *info, bool follow)
{
    TY_UNUSED(follow);

    assert(path && path[0]);
    assert(info);

    HANDLE h;
    BY_HANDLE_FILE_INFORMATION attr;
    int r;

    // FIXME: check error handling
    h = CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        switch (GetLastError()) {
        case ERROR_ACCESS_DENIED:
            return ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", path);
        case ERROR_NOT_READY:
            return ty_error(TY_ERROR_IO, "I/O error while stating '%s'", path);
        case ERROR_FILE_NOT_FOUND:
            return ty_error(TY_ERROR_NOT_FOUND, "Path '%s' does not exist", path);
        case ERROR_PATH_NOT_FOUND:
            return ty_error(TY_ERROR_NOT_FOUND, "Part of '%s' is not a directory", path);
        }
        // Let's lie a little, error will be clearer this way
        return ty_error(TY_ERROR_SYSTEM, "GetFileAttributesEx('%s') failed: %s", path, ty_win32_strerror(0));
    }

    r = GetFileInformationByHandle(h, &attr);
    if (!r)
        return ty_error(TY_ERROR_SYSTEM, "GetFileInformationByHandle('%s') failed: %s", path, ty_win32_strerror(0));

    if (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        info->type = TY_FILE_DIRECTORY;
    } else if (attr.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) {
        info->type = TY_FILE_SPECIAL;
    } else {
        info->type = TY_FILE_REGULAR;
    }

    info->size = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
    info->mtime = filetime_to_unix_time(&attr.ftLastWriteTime);

    return 0;
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

bool ty_terminal_available(ty_descriptor desc)
{
    DWORD mode;

    if (!desc)
        return false;

    return GetConsoleMode(desc, &mode);
}

int ty_terminal_setup(int flags)
{
    HANDLE handle;
    DWORD mode;
    BOOL r;

    handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
        return ty_error(TY_ERROR_SYSTEM, "GetStdHandle(STD_INPUT_HANDLE) failed");

    r = GetConsoleMode(handle, &mode);
    if (!r) {
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

    r = SetConsoleMode(handle, mode);
    if (!r)
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
