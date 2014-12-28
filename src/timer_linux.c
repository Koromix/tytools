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

int ty_timer_set(ty_timer *timer, int value, uint16_t flags)
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
