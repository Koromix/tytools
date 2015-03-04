/*
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
    HANDLE h;

    CRITICAL_SECTION mutex;
    HANDLE event;

    bool enabled;
    bool oneshot;

    uint64_t ticks;
};

int ty_timer_new(ty_timer **rtimer)
{
    assert(rtimer);

    ty_timer *timer;
    int r;

    timer = calloc(1, sizeof(*timer));
    if (!timer)
        return ty_error(TY_ERROR_MEMORY, NULL);

    /* Must be done first because ty_timer_free can't check if mutex is initialized
       (without an annoying mutex_initialized variable anyway). */
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
        // INVALID_HANDLE_VALUE = wait for any running callback to complete (NULL does not wait)
        if (timer->h)
            DeleteTimerQueueTimer(NULL, timer->h, INVALID_HANDLE_VALUE);

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

    if (!timer->enabled)
        goto cleanup;

    timer->ticks++;
    SetEvent(timer->event);

    if (timer->oneshot)
        timer->enabled = false;

cleanup:
    LeaveCriticalSection(&timer->mutex);
}

int ty_timer_set(ty_timer *timer, int value, uint16_t flags)
{
    assert(timer);

    DWORD due, period;
    BOOL ret;
    int r;

    EnterCriticalSection(&timer->mutex);

    if (value > 0) {
        due = (DWORD)value;

        if (flags & TY_TIMER_ONESHOT) {
            /* ChangeTimerQueueTimer() fails on expired one-shot timers so make a periodic
               timer and ignore subsequent events (one every 49.7 days). */
            period = 0xFFFFFFFE;
            timer->oneshot = true;
        } else {
            period = due;
            timer->oneshot = false;
        }

        timer->enabled = true;

        if (!timer->h) {
            ret = CreateTimerQueueTimer(&timer->h, NULL, timer_callback, timer, due, period, 0);
            if (!ret) {
                r = ty_error(TY_ERROR_SYSTEM, "CreateTimerQueueTimer() failed: %s", ty_win32_strerror(0));
                goto cleanup;
            }

            r = 0;
            goto cleanup;
        }
    } else {
        if (!value) {
            timer->ticks = 1;
            SetEvent(timer->event);
        }

        if (!timer->h) {
            r = 0;
            goto cleanup;
        }

        due = 0xFFFFFFFE;
        period = 0xFFFFFFFE;

        timer->enabled = false;
    }

    ret = ChangeTimerQueueTimer(NULL, timer->h, due, period);
    if (!ret) {
        r = ty_error(TY_ERROR_SYSTEM, "ChangeTimerQueueTimer() failed: %s", ty_win32_strerror(0));
        goto cleanup;
    }

    r = 0;
cleanup:
    LeaveCriticalSection(&timer->mutex);
    return r;
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
