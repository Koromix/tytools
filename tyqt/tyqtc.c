/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(arg) ((void)(arg))

static bool get_tyqt_path(char *path, size_t size)
{
    char *ptr;

    GetModuleFileName(NULL, path, size);
    path[size - 1] = 0;
    ptr = strrchr(path, '\\');
    if (!ptr)
        return false;
    strncpy(ptr, "\\tyqt.exe", (size_t)(path + size - ptr));
    path[size - 1] = 0;

    return true;
}

static bool execute_and_wait(const char *path, LPSTR cmdline, const STARTUPINFO *si, DWORD *ret)
{
    PROCESS_INFORMATION proc;
    BOOL success;

    success = CreateProcess(path, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, (STARTUPINFO *)si,
                            &proc);
    if (!success)
        return false;
    CloseHandle(proc.hThread);

    WaitForSingleObject(proc.hProcess, INFINITE);
    if (ret)
        GetExitCodeProcess(proc.hProcess, ret);
    CloseHandle(proc.hProcess);

    return true;
}

int main(void)
{
    STARTUPINFO si;
    char path[MAX_PATH + 1];
    DWORD ret;

    si.cb = sizeof(si);
    GetStartupInfo(&si);
    if (!get_tyqt_path(path, sizeof(path)))
        goto error;

    _putenv("_TYQTC=1");
    if (!execute_and_wait(path, GetCommandLine(), &si, &ret))
        goto error;

    return (int)ret;

error:
    // No kidding
    fprintf(stderr, "tyqtc failed\n");
    return 2;
}
