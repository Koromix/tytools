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
#include <process.h>
#include "system.h"
#include "thread.h"

typedef void WINAPI InitializeConditionVariable_func(CONDITION_VARIABLE *cv);
typedef BOOL WINAPI SleepConditionVariableCS_func(CONDITION_VARIABLE *cv, CRITICAL_SECTION *cs, DWORD timeout);
typedef VOID WINAPI WakeConditionVariable_func(CONDITION_VARIABLE *cv);
typedef VOID WINAPI WakeAllConditionVariable_func(CONDITION_VARIABLE *cv);

static WakeConditionVariable_func *WakeConditionVariable_;
static WakeAllConditionVariable_func *WakeAllConditionVariable_;
static SleepConditionVariableCS_func *SleepConditionVariableCS_;

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

    thread->h = (HANDLE)_beginthreadex(NULL, 0, thread_proc, &ctx, 0,
                                       (unsigned int *)&thread->thread_id);
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

static void WINAPI WakeConditionVariable_fallback(CONDITION_VARIABLE *cv)
{
    ty_cond *cond = (ty_cond *)cv;

    EnterCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);

    if (cond->xp.wakeup < cond->xp.waiting)
        cond->xp.wakeup++;
    SetEvent(cond->xp.ev);

    LeaveCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);
}

static void WINAPI WakeAllConditionVariable_fallback(CONDITION_VARIABLE *cv)
{
    ty_cond *cond = (ty_cond *)cv;

    EnterCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);

    cond->xp.wakeup = cond->xp.waiting;
    SetEvent(cond->xp.ev);

    LeaveCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);
}

static DWORD adjust_timeout_win32(DWORD timeout, uint64_t start)
{
    if (timeout == INFINITE)
        return INFINITE;

    uint64_t now = ty_millis();

    if (now > start + timeout)
        return 0;
    return (DWORD)(start + timeout - now);
}

// Not sure if the fallback code is correct or not, let's hope so for now
static BOOL WINAPI SleepConditionVariableCS_fallback(CONDITION_VARIABLE *cv,
                                                     CRITICAL_SECTION *mutex, DWORD timeout)
{
    ty_cond *cond = (ty_cond *)cv;
    uint64_t start;
    bool signaled;
    DWORD wret;

    while (true) {
        EnterCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);
        if (!cond->xp.wakeup)
            break;
        LeaveCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);
    }

    cond->xp.waiting++;

    LeaveCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);
    LeaveCriticalSection(mutex);

    start = ty_millis();
restart:
    wret = WaitForSingleObject(cond->xp.ev, adjust_timeout_win32(timeout, start));
    assert(wret == WAIT_OBJECT_0 || wret == WAIT_TIMEOUT);

    EnterCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);

    if (cond->xp.wakeup) {
        if (!--cond->xp.wakeup)
            ResetEvent(cond->xp.ev);
        signaled = true;
    } else if (wret == WAIT_TIMEOUT) {
        signaled = false;
    } else {
        LeaveCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);
        goto restart;
    }
    cond->xp.waiting--;

    LeaveCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);

    EnterCriticalSection(mutex);
    return signaled;
}

int ty_cond_init(ty_cond *cond)
{
    static bool init;
    static InitializeConditionVariable_func *InitializeConditionVariable_;

    if (!init) {
        HANDLE kernel32 = GetModuleHandle("kernel32.dll");

        // Condition Variables appeared on Vista, emulate them on Windows XP
        InitializeConditionVariable_ = (InitializeConditionVariable_func *)GetProcAddress(kernel32, "InitializeConditionVariable");
        if (InitializeConditionVariable_) {
            WakeConditionVariable_ = (WakeConditionVariable_func *)GetProcAddress(kernel32, "WakeConditionVariable");
            WakeAllConditionVariable_ = (WakeAllConditionVariable_func *)GetProcAddress(kernel32, "WakeAllConditionVariable");
            SleepConditionVariableCS_ = (SleepConditionVariableCS_func *)GetProcAddress(kernel32, "SleepConditionVariableCS");
        } else {
            WakeConditionVariable_ = WakeConditionVariable_fallback;
            WakeAllConditionVariable_ = WakeAllConditionVariable_fallback;
            SleepConditionVariableCS_ = SleepConditionVariableCS_fallback;
        }

        init = true;
    }

    if (InitializeConditionVariable_) {
        InitializeConditionVariable_((CONDITION_VARIABLE *)&cond->cv);
    } else {
        memset(cond, 0, sizeof(*cond));
        cond->xp.ev = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!cond->xp.ev)
            return ty_error(TY_ERROR_SYSTEM, "CreateEvent() failed: %s", ty_win32_strerror(0));
        InitializeCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);
    }
    cond->init = true;

    return 0;
}

void ty_cond_release(ty_cond *cond)
{
    if (!cond->init)
        return;

    // Apparently, there is no need for a DeleteConditionVariable() on Windows >= Vista
    if (!WakeConditionVariable_) {
        DeleteCriticalSection((CRITICAL_SECTION *)&cond->xp.mutex);
        CloseHandle(cond->xp.ev);
    }
    cond->init = false;
}

void ty_cond_signal(ty_cond *cond)
{
    WakeConditionVariable_((CONDITION_VARIABLE *)&cond->cv);
}

void ty_cond_broadcast(ty_cond *cond)
{
    WakeAllConditionVariable_((CONDITION_VARIABLE *)&cond->cv);
}

bool ty_cond_wait(ty_cond *cond, ty_mutex *mutex, int timeout)
{
    return SleepConditionVariableCS_((CONDITION_VARIABLE *)&cond->cv,
                                     (CRITICAL_SECTION *)&mutex->mutex,
                                     timeout >= 0 ? (DWORD)timeout : INFINITE);
}
