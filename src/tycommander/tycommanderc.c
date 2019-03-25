/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#define _CRT_RAND_S
#include <stdlib.h>
#include <string.h>

struct echo_context {
    HANDLE pipe;

    HANDLE in;
    HANDLE out;
};

#define UNUSED(arg) ((void)(arg))
#define COUNTOF(a) (sizeof(a) / sizeof(*(a)))

static DWORD WINAPI echo_thread(void *udata)
{
    struct echo_context *ctx = udata;
    BOOL success;

    success = ConnectNamedPipe(ctx->pipe, NULL);
    if (!success)
        return 0;

    while (true) {
        uint8_t buf[1024];
        DWORD len;

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
    ECHO_IN,
    ECHO_OUT
} echo_direction;

static inline bool handle_is_valid(HANDLE h)
{
    return h && h != INVALID_HANDLE_VALUE;
}

/* If this function succeeds, resources will be leaked when the thread ends but it
   is supposed to run until the end anyway. */
static bool start_echo_thread(HANDLE desc, echo_direction dir, char *path, size_t path_size)
{
    struct echo_context *ctx;
    HANDLE thread;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        goto error;

    for (unsigned int i = 0; i < 8 && !handle_is_valid(ctx->pipe); i++) {
        unsigned int rnd;

        rand_s(&rnd);
        snprintf(path, path_size, "\\\\.\\pipe\\tycommanderc-pipe-%04x", rnd);

        ctx->pipe = CreateNamedPipe(path, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, 512, 512, 0, NULL);
    }
    if (!handle_is_valid(ctx->pipe))
        goto error;

    switch (dir) {
        case ECHO_IN: {
            ctx->in = desc;
            ctx->out = ctx->pipe;
        } break;

        case ECHO_OUT: {
            ctx->in = ctx->pipe;
            ctx->out = desc;
        } break;
    }

    thread = CreateThread(NULL, 0, echo_thread, ctx, 0, NULL);
    if (!thread)
        goto error;
    CloseHandle(thread);

    return true;

error:
    if (ctx && handle_is_valid(ctx->pipe))
        CloseHandle(ctx->pipe);
    free(ctx);
    return false;
}

static bool setup_pipes(void)
{
    char paths[3][256], env[1024];

#define START_ECHO_THREAD(n, nstd, dir) \
        do { \
            bool ret = start_echo_thread(GetStdHandle(nstd), (dir), paths[n], sizeof(paths[n])); \
            if (!ret) \
                return false; \
        } while (false)

    /* You cannot use asynchronous I/O or the Wait functions for console I/O on Windows,
       do not ask me why but that is sad. It is possible for Named Pipes though. */
    START_ECHO_THREAD(0, STD_INPUT_HANDLE, ECHO_IN);
    START_ECHO_THREAD(1, STD_OUTPUT_HANDLE, ECHO_OUT);
    START_ECHO_THREAD(2, STD_ERROR_HANDLE, ECHO_OUT);

#undef START_ECHO_THREAD

    snprintf(env, sizeof(env), "_TYCOMMANDERC_PIPES=%s:%s:%s", paths[0], paths[1], paths[2]);
    _putenv(env);

    return true;
}

static bool execute_tycommander(LPSTR cmdline, const STARTUPINFO *si, DWORD *rret)
{
    char path[MAX_PATH + 1], *ptr;
    PROCESS_INFORMATION proc;
    BOOL success;

    GetModuleFileName(NULL, path, sizeof(path));
    path[MAX_PATH] = 0;
    ptr = strrchr(path, '\\');
    if (!ptr)
        return false;
    strncpy(ptr, "\\tycommander.exe", (size_t)(path + sizeof(path) - ptr));
    path[MAX_PATH] = 0;

    success = CreateProcess(path, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, (STARTUPINFO *)si, &proc);
    if (!success)
        return false;
    CloseHandle(proc.hThread);

    AllowSetForegroundWindow(GetProcessId(proc.hProcess));

    WaitForSingleObject(proc.hProcess, INFINITE);
    if (rret)
        GetExitCodeProcess(proc.hProcess, rret);
    CloseHandle(proc.hProcess);

    return true;
}

int main(void)
{
    STARTUPINFO si;
    DWORD ret;

    si.cb = sizeof(si);
    GetStartupInfo(&si);

    if (!setup_pipes())
        goto error;
    if (!execute_tycommander(GetCommandLine(), &si, &ret))
        goto error;

    // Small delay to avoid dropping unread output/error data
    Sleep(50);

    return (int)ret;

error:
    // No kidding
    fprintf(stderr, "TyCommanderC failed\n");
    return 2;
}
