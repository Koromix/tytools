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
