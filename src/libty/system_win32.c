/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <shlobj.h>
#include "system.h"

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
        case FILE_TYPE_PIPE: {
            return TY_DESCRIPTOR_MODE_FIFO;
        } break;
        case FILE_TYPE_CHAR: {
            if (GetConsoleMode(desc, &tmp)) {
                return TY_DESCRIPTOR_MODE_DEVICE | TY_DESCRIPTOR_MODE_TERMINAL;
            } else {
                return TY_DESCRIPTOR_MODE_DEVICE;
            }
        } break;
        case FILE_TYPE_DISK: {
            return TY_DESCRIPTOR_MODE_FILE;
        } break;
    }

    return 0;
}

ty_descriptor ty_standard_get_descriptor(ty_standard_stream std_stream)
{
    switch (std_stream) {
        case TY_STREAM_INPUT: { return GetStdHandle(STD_INPUT_HANDLE); } break;
        case TY_STREAM_OUTPUT: { return GetStdHandle(STD_OUTPUT_HANDLE); } break;
        case TY_STREAM_ERROR: { return GetStdHandle(STD_ERROR_HANDLE); } break;
    }

    assert(false);
    return NULL;
}

unsigned int ty_standard_get_paths(ty_standard_path std_path, const char *suffix,
                                   char (*rpaths)[TY_PATH_MAX_SIZE], unsigned int max_paths)
{
    assert(rpaths);

    unsigned int paths_count = 0;

    if (!max_paths)
        return 0;

#define ADD_SHELL_DIRECTORY(Id) \
        do { \
            if (paths_count < max_paths) { \
                if (SHGetFolderPath(NULL, (Id), NULL, SHGFP_TYPE_CURRENT, \
                                    rpaths[paths_count++]) != S_OK) \
                    goto overflow; \
            } \
        } while (false)

    switch (std_path) {
        case TY_PATH_EXECUTABLE_DIRECTORY: {
            DWORD len = GetModuleFileName(NULL, rpaths[0], TY_PATH_MAX_SIZE);
            if (len == TY_PATH_MAX_SIZE)
                goto overflow;
            while (len && !strchr(TY_PATH_SEPARATORS, rpaths[0][--len]))
                continue;
            rpaths[0][len] = 0;
            paths_count = 1;
        } break;

        case TY_PATH_CONFIG_DIRECTORY: {
            ADD_SHELL_DIRECTORY(CSIDL_APPDATA);
            ADD_SHELL_DIRECTORY(CSIDL_COMMON_APPDATA);
        } break;
    }

#undef ADD_SHELL_DIRECTORY

    if (suffix) {
        for (unsigned int i = 0; i < paths_count; i++) {
            size_t len, suffix_len;

            len = strlen(rpaths[i]);
            suffix_len = (size_t)snprintf(rpaths[i] + len, TY_PATH_MAX_SIZE - len, "/%s", suffix);
            if (suffix_len >= TY_PATH_MAX_SIZE - len)
                goto overflow;
        }
    }

    assert(paths_count);
    return paths_count;

overflow:
    ty_error(TY_ERROR_SYSTEM, "Ignoring truncated path in ty_standard_get_paths()");
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
        case WAIT_FAILED: {
            return ty_error(TY_ERROR_SYSTEM, "WaitForMultipleObjects() failed: %s",
                            ty_win32_strerror(0));
        } break;
        case WAIT_TIMEOUT: {
            return 0;
        } break;
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
