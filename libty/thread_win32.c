/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "ty/common.h"
#include "compat.h"
#include <process.h>
#include "ty/system.h"
#include "ty/thread.h"

typedef void WINAPI InitializeConditionVariable_func(CONDITION_VARIABLE *cv);
typedef BOOL WINAPI SleepConditionVariableCS_func(CONDITION_VARIABLE *cv, CRITICAL_SECTION *cs, DWORD timeout);
typedef VOID WINAPI WakeConditionVariable_func(CONDITION_VARIABLE *cv);
typedef VOID WINAPI WakeAllConditionVariable_func(CONDITION_VARIABLE *cv);

static InitializeConditionVariable_func *InitializeConditionVariable_;
static SleepConditionVariableCS_func *SleepConditionVariableCS_;
static WakeConditionVariable_func *WakeConditionVariable_;
static WakeAllConditionVariable_func *WakeAllConditionVariable_;

TY_INIT()
{
    HANDLE h;

    h = GetModuleHandle("kernel32.dll");
    assert(h);

    // Condition Variables appeared on Vista, emulate them on Windows XP
    InitializeConditionVariable_ = (InitializeConditionVariable_func *)GetProcAddress(h, "InitializeConditionVariable");
    if (InitializeConditionVariable_) {
        SleepConditionVariableCS_ = (SleepConditionVariableCS_func *)GetProcAddress(h, "SleepConditionVariableCS");
        WakeConditionVariable_ = (WakeConditionVariable_func *)GetProcAddress(h, "WakeConditionVariable");
        WakeAllConditionVariable_ = (WakeAllConditionVariable_func *)GetProcAddress(h, "WakeAllConditionVariable");
    }
}

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
        r = ty_error(TY_ERROR_SYSTEM, "CreateEvent() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    thread->h = (HANDLE)_beginthreadex(NULL, 0, thread_proc, &ctx, 0, NULL);
    if (!thread->h) {
        r = ty_error(TY_ERROR_SYSTEM, "_beginthreadex() failed: %s", ty_win32_strerror(0));
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
    // FIXME: assert? ignore?
    assert(thread->h);

    union { DWORD dw; int i; } code;
    DWORD ret TY_POSSIBLY_UNUSED;

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

int ty_mutex_init(ty_mutex *mutex, ty_mutex_type type)
{
    TY_UNUSED(type);

    InitializeCriticalSection(&mutex->mutex);
    mutex->init = true;

    return 0;
}

void ty_mutex_release(ty_mutex *mutex)
{
    if (!mutex->init)
        return;

    DeleteCriticalSection(&mutex->mutex);
    mutex->init = false;
}

void ty_mutex_lock(ty_mutex *mutex)
{
    EnterCriticalSection(&mutex->mutex);
}

void ty_mutex_unlock(ty_mutex *mutex)
{
    LeaveCriticalSection(&mutex->mutex);
}

int ty_cond_init(ty_cond *cond)
{
    if (InitializeConditionVariable_) {
        InitializeConditionVariable_(&cond->cv);
    } else {
        memset(cond, 0, sizeof(*cond));

        cond->xp.ev = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!cond->xp.ev)
            return ty_error(TY_ERROR_SYSTEM, "CreateEvent() failed: %s", ty_win32_strerror(0));

        InitializeCriticalSection(&cond->xp.mutex);
    }
    cond->init = true;

    return 0;
}

void ty_cond_release(ty_cond *cond)
{
    if (!cond->init)
        return;

    // Apparently, there is no need for a DeleteConditionVariable() on Windows >= Vista
    if (!InitializeConditionVariable_) {
        DeleteCriticalSection(&cond->xp.mutex);
        CloseHandle(cond->xp.ev);
    }
    cond->init = false;
}

void ty_cond_signal(ty_cond *cond)
{
    if (InitializeConditionVariable_) {
        WakeConditionVariable_(&cond->cv);
    } else {
        EnterCriticalSection(&cond->xp.mutex);

        if (cond->xp.wakeup < cond->xp.waiting)
            cond->xp.wakeup++;
        SetEvent(cond->xp.ev);

        LeaveCriticalSection(&cond->xp.mutex);
    }
}

void ty_cond_broadcast(ty_cond *cond)
{
    if (InitializeConditionVariable_) {
        WakeAllConditionVariable_(&cond->cv);
    } else {
        EnterCriticalSection(&cond->xp.mutex);

        cond->xp.wakeup = cond->xp.waiting;
        SetEvent(cond->xp.ev);

        LeaveCriticalSection(&cond->xp.mutex);
    }
}

// Not sure if the fallback code is correct or not, let's hope so for now
bool ty_cond_wait(ty_cond *cond, ty_mutex *mutex, int timeout)
{
    if (InitializeConditionVariable_) {
        return SleepConditionVariableCS_(&cond->cv, &mutex->mutex, timeout >= 0 ? (DWORD)timeout : INFINITE);
    } else {
        uint64_t start;
        bool signaled;
        DWORD wret;

        while (true) {
            EnterCriticalSection(&cond->xp.mutex);
            if (!cond->xp.wakeup)
                break;
            LeaveCriticalSection(&cond->xp.mutex);
        }

        cond->xp.waiting++;

        LeaveCriticalSection(&cond->xp.mutex);
        LeaveCriticalSection(&mutex->mutex);

        start = ty_millis();
restart:
        wret = WaitForSingleObject(cond->xp.ev, timeout >= 0 ? (DWORD)ty_adjust_timeout(timeout, start) : INFINITE);
        assert(wret == WAIT_OBJECT_0 || wret == WAIT_TIMEOUT);

        EnterCriticalSection(&cond->xp.mutex);

        if (cond->xp.wakeup) {
            if (!--cond->xp.wakeup)
                ResetEvent(cond->xp.ev);
            signaled = true;
        } else if (wret == WAIT_TIMEOUT) {
            signaled = false;
        } else {
            LeaveCriticalSection(&cond->xp.mutex);
            goto restart;
        }
        cond->xp.waiting--;

        LeaveCriticalSection(&cond->xp.mutex);

        EnterCriticalSection(&mutex->mutex);
        return signaled;
    }
}
