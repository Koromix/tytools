/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include "timer.h"
#include "system.h"

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
        r = ty_error(TY_ERROR_SYSTEM, "timerfd_create() failed: %s", strerror(errno));
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

    struct itimerspec ispec = {0};
    int tfd_flags = 0;
    int r;

    if (value > 0) {
        ispec.it_value.tv_sec = (int)value / 1000;
        ispec.it_value.tv_nsec = (int)((value % 1000) * 1000000);

        if (!(flags & TY_TIMER_ONESHOT))
            ispec.it_interval = ispec.it_value;
    } else if (!value) {
        tfd_flags |= TFD_TIMER_ABSTIME;
        ispec.it_value.tv_nsec = 1;
    }

    r = timerfd_settime(timer->fd, tfd_flags, &ispec, NULL);
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
