/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ty/timer.h"
#include "ty/system.h"

struct ty_timer {
    CRITICAL_SECTION mutex;

    HANDLE h;

    HANDLE event;
    uint64_t ticks;
};

static HANDLE timer_queue;

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

void ty_timer_get_descriptors(const ty_timer *timer, ty_descriptor_set *set, int id)
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

int ty_timer_set(ty_timer *timer, int value, uint16_t flags)
{
    assert(timer);

    if (timer->h) {
        // INVALID_HANDLE_VALUE = wait for any running callback to complete (NULL does not wait)
        DeleteTimerQueueTimer(timer_queue, timer->h, INVALID_HANDLE_VALUE);
        timer->h = NULL;
    }

    ty_timer_rearm(timer);

    if (value >= 0) {
        DWORD period = 0;
        BOOL ret;

        if (!(flags & TY_TIMER_ONESHOT))
            period = (DWORD)value;

        if (!value)
            value = 1;

        ret = CreateTimerQueueTimer(&timer->h, timer_queue, timer_callback, timer, (DWORD)value, period, 0);
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
