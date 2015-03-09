/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include "ty/timer.h"
#include "ty/system.h"

struct ty_timer {
    int fd;
};

int ty_timer_new(ty_timer **rtimer)
{
    assert(rtimer);

    ty_timer *timer;
    int r;

    timer = calloc(1, sizeof(*timer));
    if (!timer) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    timer->fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (timer->fd < 0) {
        if (errno == ENOMEM)
            return ty_error(TY_ERROR_MEMORY, NULL);
        return ty_error(TY_ERROR_SYSTEM, "timerfd_create() failed: %s", strerror(errno));
    }

    *rtimer = timer;
    return 0;

error:
    ty_timer_free(timer);
    return r;
}

void ty_timer_free(ty_timer *timer)
{
    if (timer)
        close(timer->fd);

    free(timer);
}

void ty_timer_get_descriptors(const ty_timer *timer, ty_descriptor_set *set, int id)
{
    assert(timer);
    assert(set);

    ty_descriptor_set_add(set, timer->fd, id);
}

int ty_timer_set(ty_timer *timer, int value, int flags)
{
    assert(timer);

    struct itimerspec ispec = {{0}};
    int r;

    if (value > 0) {
        ispec.it_value.tv_sec = (int)value / 1000;
        ispec.it_value.tv_nsec = (int)((value % 1000) * 1000000);

        if (!(flags & TY_TIMER_ONESHOT))
            ispec.it_interval = ispec.it_value;
    } else if (!value) {
        ispec.it_value.tv_nsec = 1;
    }

    r = timerfd_settime(timer->fd, 0, &ispec, NULL);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "timerfd_settime() failed: %s", strerror(errno));

    return 0;
}

uint64_t ty_timer_rearm(ty_timer *timer)
{
    assert(timer);

    uint64_t ticks;
    ssize_t r;

    r = read(timer->fd, &ticks, sizeof(ticks));
    if (r <= 0)
        return 0;

    return ticks;
}
