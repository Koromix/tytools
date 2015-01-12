/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ty/process.h"

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

int ty_process_spawn(const char *path, const char *dir, const char * const args[],
                     const ty_descriptor desc[3], uint32_t flags, ty_descriptor *rdesc)
{
    assert(path && path[0]);
    assert(args && args[0]);

    const char *name;
    char *cmd = NULL;
    STARTUPINFO info = {0};
    PROCESS_INFORMATION proc = {0};
    DWORD ret;
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

    if (rdesc) {
        *rdesc = proc.hProcess;
        proc.hProcess = NULL;
    }

    r = 0;
cleanup:
    if (proc.hThread)
        CloseHandle(proc.hThread);
    if (proc.hProcess)
        CloseHandle(proc.hProcess);
    free(cmd);
    return r;
}

int ty_process_wait(ty_descriptor desc, int timeout)
{
    assert(desc);

    DWORD code, ret;
    int r;

    ret = WaitForSingleObject(desc, timeout < 0 ? INFINITE : (DWORD)timeout);
    switch (ret) {
    case WAIT_FAILED:
        r = ty_error(TY_ERROR_SYSTEM, "WaitForSingleObject() failed: %s",
                        ty_win32_strerror(0));
        goto cleanup;
    case WAIT_TIMEOUT:
        return 0;
    }
    assert(ret == WAIT_OBJECT_0);

    ret = (DWORD)GetExitCodeProcess(desc, &code);
    if (!ret) {
        r = ty_error(TY_ERROR_SYSTEM, "GetExitCodeProcess() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    switch (code) {
    case 0:
        r = TY_PROCESS_SUCCESS;
        break;
    case CONTROL_C_EXIT:
        r = TY_PROCESS_INTERRUPTED;
        break;
    default:
        r = TY_PROCESS_FAILURE;
        break;
    }
cleanup:
    CloseHandle(desc);
    return r;
}
