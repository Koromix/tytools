/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifdef _WIN32

#include "common.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include "system.h"
#include "thread.h"

struct thread_context {
    ty_thread *thread;

    ty_thread_func *f;
    void *udata;

    HANDLE ev;
};

static unsigned int __stdcall thread_proc(void *udata)
{
    struct thread_context ctx = *(struct thread_context *)udata;
    union { DWORD dw; int i; } code;

    SetEvent(ctx.ev);

    code.i = (*ctx.f)(ctx.udata);
    return code.dw;
}

int ty_thread_create(ty_thread *thread, ty_thread_func *f, void *udata)
{
    struct thread_context ctx;
    int r;

    ctx.thread = thread;
    ctx.f = f;
    ctx.udata = udata;

    ctx.ev = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ctx.ev) {
        r = ty_error(TY_ERROR_SYSTEM, "CreateEvent() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    thread->h = (HANDLE)_beginthreadex(NULL, 0, thread_proc, &ctx, 0,
                                       (unsigned int *)&thread->thread_id);
    if (!thread->h) {
        r = ty_error(TY_ERROR_SYSTEM, "_beginthreadex() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    WaitForSingleObject(ctx.ev, INFINITE);

    r = 0;
cleanup:
    if (ctx.ev)
        CloseHandle(ctx.ev);
    return r;
}

int ty_thread_join(ty_thread *thread)
{
    assert(thread->h);

    union { DWORD dw; int i; } code;
    DWORD ret _HS_POSSIBLY_UNUSED;

    ret = WaitForSingleObject(thread->h, INFINITE);
    assert(ret == WAIT_OBJECT_0);
    GetExitCodeThread(thread->h, &code.dw);

    CloseHandle(thread->h);
    thread->h = NULL;

    return code.i;
}

void ty_thread_detach(ty_thread *thread)
{
    if (!thread->h)
        return;

    CloseHandle(thread->h);
    thread->h = NULL;
}

ty_thread_id ty_thread_get_self_id()
{
    return GetCurrentThreadId();
}

int ty_mutex_init(ty_mutex *mutex)
{
    InitializeCriticalSection((CRITICAL_SECTION *)&mutex->mutex);
    mutex->init = true;

    return 0;
}

void ty_mutex_release(ty_mutex *mutex)
{
    if (!mutex->init)
        return;

    DeleteCriticalSection((CRITICAL_SECTION *)&mutex->mutex);
    mutex->init = false;
}

void ty_mutex_lock(ty_mutex *mutex)
{
    EnterCriticalSection((CRITICAL_SECTION *)&mutex->mutex);
}

void ty_mutex_unlock(ty_mutex *mutex)
{
    LeaveCriticalSection((CRITICAL_SECTION *)&mutex->mutex);
}

int ty_cond_init(ty_cond *cond)
{
    InitializeConditionVariable((CONDITION_VARIABLE *)&cond->cv);
    cond->init = true;

    return 0;
}

void ty_cond_release(ty_cond *cond)
{
    // Apparently, there is no need for DeleteConditionVariable()
    cond->init = false;
}

void ty_cond_signal(ty_cond *cond)
{
    WakeConditionVariable((CONDITION_VARIABLE *)&cond->cv);
}

void ty_cond_broadcast(ty_cond *cond)
{
    WakeAllConditionVariable((CONDITION_VARIABLE *)&cond->cv);
}

bool ty_cond_wait(ty_cond *cond, ty_mutex *mutex, int timeout)
{
    return SleepConditionVariableCS((CONDITION_VARIABLE *)&cond->cv,
                                    (CRITICAL_SECTION *)&mutex->mutex,
                                    timeout >= 0 ? (DWORD)timeout : INFINITE);
}

#endif
