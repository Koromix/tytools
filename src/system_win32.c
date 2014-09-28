/**
 * ty, command-line program to manage Teensy devices
 * http://github.com/Koromix/ty
 * Copyright (C) 2014 Niels Martignène
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ty/common.h"
#include "compat.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <shlobj.h>
#include "ty/system.h"

struct ty_timer {
    CRITICAL_SECTION mutex;

    HANDLE h;

    HANDLE event;
    uint64_t ticks;
};

typedef ULONGLONG WINAPI GetTickCount64_func(void);
typedef WINBOOL WINAPI GetFileInformationByHandleEx_func(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass, LPVOID lpFileInformation, DWORD dwBufferSize);

static ULONGLONG WINAPI GetTickCount64_fallback(void);

static GetTickCount64_func *GetTickCount64_;
static GetFileInformationByHandleEx_func *GetFileInformationByHandleEx_;

static const uint64_t delta_epoch = 11644473600000;

static HANDLE timer_queue;
static DWORD orig_mode;

HANDLE _ty_win32_descriptors[3];

TY_INIT(init_win32)
{
    HANDLE h = GetModuleHandle("kernel32.dll");
    assert(h);

    GetTickCount64_ = (GetTickCount64_func *)GetProcAddress(h, "GetTickCount64");
    if (!GetTickCount64_)
        GetTickCount64_ = GetTickCount64_fallback;

    GetFileInformationByHandleEx_ = (GetFileInformationByHandleEx_func *)GetProcAddress(h, "GetFileInformationByHandleEx");

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

bool ty_win32_test_version(ty_win32_version version)
{
    OSVERSIONINFOEX info = {0};
    DWORDLONG cond = 0;

    switch (version) {
    case TY_WIN32_XP:
        info.dwMajorVersion = 5;
        info.dwMinorVersion = 1;
        break;
    case TY_WIN32_VISTA:
        info.dwMajorVersion = 6;
        break;
    case TY_WIN32_SEVEN:
        info.dwMajorVersion = 6;
        info.dwMinorVersion = 1;
        break;
    case TY_WIN32_EIGHT:
        info.dwMajorVersion = 6;
        info.dwMinorVersion = 2;
        break;
    }

    VER_SET_CONDITION(cond, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(cond, VER_MINORVERSION, VER_GREATER_EQUAL);

    return VerifyVersionInfo(&info, VER_MAJORVERSION | VER_MINORVERSION, cond);
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

    info->volume = attr.dwVolumeSerialNumber;
    if (ty_win32_test_version(TY_WIN32_EIGHT)) {
        FILE_ID_INFO id;
        r = GetFileInformationByHandleEx_(h, FileIdInfo, &id, sizeof(id));
        if (!r)
            return ty_error(TY_ERROR_SYSTEM, "GetFileInformationByHandleEx('%s') failed: %s", path, ty_win32_strerror(0));

        memcpy(info->fileindex, &id, 16);
    } else {
        memcpy(&info->fileindex[8], &attr.nFileIndexHigh, 4);
        memcpy(&info->fileindex[12], &attr.nFileIndexLow, 4);
    }

    info->flags = 0;
    if (attr.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
        info->flags |= TY_FILE_HIDDEN;

    return 0;
}

bool ty_file_unique(const ty_file_info *info1, const ty_file_info *info2)
{
    assert(info1);
    assert(info2);

    return info1->volume == info2->volume && memcmp(info1->fileindex, info2->fileindex, sizeof(info1->fileindex)) == 0;
}

int ty_realpath(const char *path, const char *base, char **rpath)
{
    assert(path && path[0]);

    char *tmp = NULL, *real = NULL;
    int r;

    if (base && !ty_path_is_absolute(path)) {
        r = asprintf(&tmp, "%s\\%s", base, path);
        if (r < 0)
            goto cleanup;

        path = tmp;
    }

    real = _fullpath(NULL, path, 0);
    if (!real) {
        if (errno == ENOMEM) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
        } else {
            r = ty_error(TY_ERROR_SYSTEM, "_fullpath('%s') failed: %s", path, strerror(errno));
        }
        goto cleanup;
    }

    r = _access(real, 0);
    if (r < 0) {
        switch (errno) {
        case EACCES:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", path);
            break;
        case EIO:
            r = ty_error(TY_ERROR_IO, "I/O error while resolving path '%s'", path);
            break;
        case ENOENT:
            r = ty_error(TY_ERROR_NOT_FOUND, "Path '%s' does not exist", path);
            break;

        default:
            r = ty_error(TY_ERROR_SYSTEM, "_access('%s') failed: %s", real, strerror(errno));
            break;
        }
        goto cleanup;
    }

    if (rpath) {
        *rpath = real;
        real = NULL;
    }

    r = 0;
cleanup:
    free(real);
    free(tmp);
    return r;
}

int ty_delete(const char *path, bool tolerant)
{
    assert(path && path[0]);

    int r;

    if (GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY) {
        r = _rmdir(path);
    } else {
        r = _unlink(path);
    }
    if (r < 0) {
        switch (errno) {
        case EACCES:
            return ty_error(TY_ERROR_ACCESS, "Permission denied to delete '%s'", path);
        case ENOENT:
            if (tolerant)
                return 0;
            return ty_error(TY_ERROR_NOT_FOUND, "Path '%s' does not exist", path);
        case ENOTEMPTY:
            return ty_error(TY_ERROR_EXISTS, "Cannot remove non-empty directory '%s", path);
        }
        return ty_error(TY_ERROR_SYSTEM, "remove('%s') failed: %s", path, strerror(errno));
    }

    return 0;
}

// FIXME: look at how msvcrt tokenizes the command line, and fix this
int append_argument(char **cmd, const char *arg)
{
    size_t size;
    bool quote = false;
    char *ptr, *tmp;

    // Account for separating space and NUL byte
    size = strlen(arg) + 2;
    if (*cmd)
        size += strlen(*cmd);
    // Add quotation marks if there are spaces
    if (strchr(arg, ' ')) {
        quote = true;
        size += 2;
    }
    // How many backslashes will we add?
    ptr = (char *)arg - 1;
    while ((ptr = strpbrk(ptr + 1, "\\\"")))
        size++;

    tmp = realloc(*cmd, size);
    if (!tmp)
        return ty_error(TY_ERROR_MEMORY, NULL);
    if (*cmd) {
        ptr = tmp + strlen(*cmd);
    } else {
        ptr = tmp;
    }
    *cmd = tmp;

    if (quote)
        *ptr++ = '"';
    while (*arg) {
        if (strchr("\\\"", *arg))
            *ptr++ = '\\';
        *ptr++ = *arg++;
    }
    if (quote)
        *ptr++ = '"';
    strcpy(ptr, " ");

    return 0;
}

int ty_spawn(const char *path, const char *dir, const char * const *args,
             const ty_descriptor desc[3], int *rcode, uint32_t flags)
{
    assert(path && path[0]);
    assert(args && args[0]);

    const char *name;
    char *cmd = NULL;
    STARTUPINFO info = {0};
    PROCESS_INFORMATION proc = {0};
    DWORD code, ret;
    int r;

    if (flags & TY_SPAWN_PATH) {
        r = append_argument(&cmd, path);
        if (r < 0)
            goto cleanup;
        name = NULL;
        args++;
    } else {
        name = path;
    }

    while (*args) {
        r = append_argument(&cmd, *args++);
        if (r < 0)
            goto cleanup;
    }

    info.cb = sizeof(info);
    if (desc) {
        info.dwFlags |= STARTF_USESTDHANDLES;

        for (size_t i = 0; i < 3; i++) {
            HANDLE h = desc[i];

            if (!h) {
                h = CreateFile("\\\\.\\NUL",  GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
                if (!h) {
                    switch (GetLastError()) {
                    case ERROR_ACCESS_DENIED:
                        r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", path);
                        break;
                    case ERROR_NOT_READY:
                        r = ty_error(TY_ERROR_IO, "I/O error while stating '%s'", path);
                        break;
                    case ERROR_FILE_NOT_FOUND:
                        r = ty_error(TY_ERROR_NOT_FOUND, "Path '%s' does not exist", path);
                        break;
                    case ERROR_PATH_NOT_FOUND:
                        r = ty_error(TY_ERROR_NOT_FOUND, "Part of '%s' is not a directory", path);
                        break;
                    default:
                        r = ty_error(TY_ERROR_SYSTEM, "GetFileAttributesEx('%s') failed: %s", path, ty_win32_strerror(0));
                        break;
                    }
                    goto cleanup;
                }
            }

            switch (i) {
            case 0:
                info.hStdInput = h;
                break;
            case 1:
                info.hStdOutput = h;
                break;
            case 2:
                info.hStdError = h;
                break;
            }
        }
    }

    ret = (DWORD)CreateProcess(name, cmd, NULL, NULL, FALSE, 0, NULL, dir, &info, &proc);
    if (!ret) {
        switch (GetLastError()) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            r = ty_error(TY_ERROR_NOT_FOUND, "Executable '%s' not found", path);
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            r = ty_error(TY_ERROR_MEMORY, NULL);
            break;
        case ERROR_ACCESS_DENIED:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied to execute '%s'", path);
            break;
        case ERROR_BAD_LENGTH:
            r = ty_error(TY_ERROR_PARAM, "Path '%s' is invalid", path);
            break;
        case ERROR_BAD_EXE_FORMAT:
            r = ty_error(TY_ERROR_PARAM, "Not a valid executable: '%s'", path);
            break;

        default:
            r = ty_error(TY_ERROR_SYSTEM, "CreateProcess('%s') failed: %s", path,
                         ty_win32_strerror(0));
            break;
        }
        goto cleanup;
    }

    if (flags & TY_SPAWN_ASYNC) {
        r = 0;
        goto cleanup;
    }

    ret = WaitForSingleObject(proc.hProcess, INFINITE);
    if (ret == WAIT_FAILED) {
        r = ty_error(TY_ERROR_SYSTEM, "WaitForSingleObject() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }
    assert(ret == WAIT_OBJECT_0);

    ret = (DWORD)GetExitCodeProcess(proc.hProcess, &code);
    if (!ret) {
        r = ty_error(TY_ERROR_SYSTEM, "GetExitCodeProcess() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    if (code == CONTROL_C_EXIT)
        ExitProcess(CONTROL_C_EXIT);

    if (rcode)
        *rcode = (int)code;

    r = 0;
cleanup:
    if (proc.hThread)
        CloseHandle(proc.hThread);
    if (proc.hProcess)
        CloseHandle(proc.hProcess);
    free(cmd);
    return r;
}

static void free_timer_queue(void)
{
    DeleteTimerQueue(timer_queue);
}

int ty_timer_new(ty_timer **rtimer)
{
    assert(rtimer);

    ty_timer *timer;
    int r;

    if (!timer_queue) {
        timer_queue = CreateTimerQueue();
        if (!timer_queue)
            return ty_error(TY_ERROR_SYSTEM, "CreateTimerQueue() failed: %s", ty_win32_strerror(0));

        atexit(free_timer_queue);
    }

    timer = calloc(1, sizeof(*timer));
    if (!timer)
        return ty_error(TY_ERROR_MEMORY, NULL);

    // Must be done first because ty_timer_free can't check if mutex is initialized
    // (without an annoying mutex_initialized variable anyway).
    InitializeCriticalSection(&timer->mutex);

    timer->event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!timer->event) {
        r = ty_error(TY_ERROR_SYSTEM, "CreateEvent() failed: %s", ty_win32_strerror(0));
        goto error;
    }

    *rtimer = timer;
    return 0;

error:
    ty_timer_free(timer);
    return r;
}

void ty_timer_free(ty_timer *timer)
{
    if (timer) {
        if (timer->h)
            DeleteTimerQueueTimer(timer_queue, timer->h, NULL);

        if (timer->event)
            CloseHandle(timer->event);
        DeleteCriticalSection(&timer->mutex);
    }

    free(timer);
}

void ty_timer_get_descriptors(ty_timer *timer, ty_descriptor_set *set, int id)
{
    assert(timer);
    assert(set);

    ty_descriptor_set_add(set, timer->event, id);
}

static void __stdcall timer_callback(void *udata, BOOLEAN timer_or_wait)
{
    TY_UNUSED(timer_or_wait);

    ty_timer *timer = udata;

    EnterCriticalSection(&timer->mutex);

    timer->ticks++;
    SetEvent(timer->event);

    LeaveCriticalSection(&timer->mutex);
}

int ty_timer_set(ty_timer *timer, int value, unsigned int period)
{
    assert(timer);

    if (timer->h) {
        DeleteTimerQueueTimer(timer_queue, timer->h, NULL);
        timer->h = NULL;
    }

    ty_timer_rearm(timer);

    if (!value)
        value = 1;
    if (value > 0) {
        BOOL ret = CreateTimerQueueTimer(&timer->h, timer_queue, timer_callback, timer, (DWORD)value, period, 0);
        if (!ret)
            return ty_error(TY_ERROR_SYSTEM, "CreateTimerQueueTimer() failed: %s", ty_win32_strerror(0));
    }

    return 0;
}

uint64_t ty_timer_rearm(ty_timer *timer)
{
    assert(timer);

    uint64_t ticks;

    EnterCriticalSection(&timer->mutex);

    ticks = timer->ticks;

    timer->ticks = 0;
    ResetEvent(timer->event);

    LeaveCriticalSection(&timer->mutex);

    return ticks;
}

int ty_poll(const ty_descriptor_set *set, int timeout)
{
    assert(set);
    assert(set->count);
    assert(set->count <= 64);

    DWORD ret = WaitForMultipleObjects(set->count, set->desc, FALSE,
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

static void restore_terminal(void)
{
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), orig_mode);
}

int ty_terminal_change(uint32_t flags)
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

    static bool saved = false;
    if (!saved) {
        orig_mode = mode;
        saved = true;

        atexit(restore_terminal);
    }

    mode = ENABLE_PROCESSED_INPUT;
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
