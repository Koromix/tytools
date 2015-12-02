/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct echo_context {
    HANDLE in;
    HANDLE out;
};

#define UNUSED(arg) ((void)(arg))

static DWORD WINAPI echo_thread(void *udata)
{
    struct echo_context *ctx = udata;

    while (true) {
        uint8_t buf[1024];
        DWORD len;
        BOOL success;

        success = ReadFile(ctx->in, buf, sizeof(buf), &len, NULL);
        if (!success)
            return 0;

        success = WriteFile(ctx->out, buf, len, &len, NULL);
        if (!success)
            return 0;
    }

    return 0;
}

typedef enum echo_direction {
    ECHO_IN = 0,
    ECHO_OUT = 1
} echo_direction;

/* If this function succeeds, resources will be leaked when the thread ends but
   it is supposed to keep running to the end anyway. */
static HANDLE start_echo_thread(HANDLE desc, echo_direction dir)
{
    struct echo_context *ctx;
    HANDLE pipe[2] = {NULL}, thread;
    BOOL success;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        goto error;

    success = CreatePipe(&pipe[0], &pipe[1], NULL, 0);
    if (!success)
        goto error;
    success = DuplicateHandle(GetCurrentProcess(), pipe[dir], GetCurrentProcess(), &pipe[dir], 0,
                              TRUE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS);
    if (!success)
        goto error;

    switch (dir) {
    case ECHO_IN:
        ctx->in = desc;
        ctx->out = pipe[1];
        break;
    case ECHO_OUT:
        ctx->in = pipe[0];
        ctx->out = desc;
        break;
    }

    thread = CreateThread(NULL, 0, echo_thread, ctx, 0, NULL);
    if (!thread)
        goto error;
    CloseHandle(thread);

    return pipe[dir];

error:
    if (pipe[0])
        CloseHandle(pipe[0]);
    if (pipe[1])
        CloseHandle(pipe[1]);
    free(ctx);
    return NULL;
}

static bool setup_pipes(void)
{
    HANDLE handles[3] = {NULL};
    char buf[128];

#define START_ECHO_THREAD(n, nstd, dir) \
        do { \
            handles[n] = start_echo_thread(GetStdHandle(nstd), (dir)); \
            if (!handles[n]) \
                return false; \
        } while (false)

    /* You cannot use asynchronous I/O or the Wait functions for anonymous pipes on Windows,
       do not ask me why but that is sad. */
    START_ECHO_THREAD(0, STD_INPUT_HANDLE, ECHO_IN);
    START_ECHO_THREAD(1, STD_OUTPUT_HANDLE, ECHO_OUT);
    START_ECHO_THREAD(2, STD_ERROR_HANDLE, ECHO_OUT);

#undef START_ECHO_THREAD

    sprintf(buf, "_TYQT_BRIDGE=%"PRIxPTR":%"PRIxPTR":%"PRIxPTR, (uintptr_t)handles[0],
            (uintptr_t)handles[1], (uintptr_t)handles[2]);
    _putenv(buf);

    return true;
}

static bool execute_tyqt(LPSTR cmdline, int show, DWORD *ret)
{
    char path[MAX_PATH + 1], *ptr;
    STARTUPINFO startup;
    PROCESS_INFORMATION proc;
    BOOL success;

    GetModuleFileName(NULL, path, sizeof(path));
    path[MAX_PATH] = 0;
    ptr = strrchr(path, '\\');
    if (!ptr)
        return false;
    strncpy(ptr, "\\tyqt.exe", (size_t)(path + sizeof(path) - ptr));
    path[MAX_PATH] = 0;

    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.wShowWindow = (WORD)(show & ~SW_SHOWDEFAULT);
    startup.dwFlags |= STARTF_USESHOWWINDOW;

    success = CreateProcess(path, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &proc);
    if (!success)
        return false;
    CloseHandle(proc.hThread);

    WaitForSingleObject(proc.hProcess, INFINITE);
    if (ret)
        GetExitCodeProcess(proc.hProcess, ret);
    CloseHandle(proc.hProcess);

    return true;
}

int CALLBACK WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    UNUSED(inst);
    UNUSED(prev);
    UNUSED(cmdline);

    DWORD ret;

    if (!setup_pipes())
        goto error;
    if (!execute_tyqt(GetCommandLine(), show, &ret))
        goto error;

    return (int)ret;

error:
    // No kidding
    fprintf(stderr, "tyqtc failed\n");
    return 2;
}
