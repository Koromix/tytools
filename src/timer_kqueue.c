/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
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

    timer->fd = kqueue();
    if (timer->fd < 0) {
        if (errno == ENOMEM)
            return ty_error(TY_ERROR_MEMORY, NULL);
        return ty_error(TY_ERROR_SYSTEM, "kqueue() failed: %s", strerror(errno));
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

    struct kevent kev;
    const struct timespec ts = {0};
    int r;

    if (value >= 0) {
        if (!value)
            value = 1;

        EV_SET(&kev, 1, EVFILT_TIMER, EV_ADD, 0, value, NULL);
        if (flags & TY_TIMER_ONESHOT)
            kev.flags |= EV_ONESHOT;
    } else {
        EV_SET(&kev, 1, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
    }

    r = kevent(timer->fd, &kev, 1, NULL, 0, &ts);
    if (r < 0) {
        if (errno == ENOMEM)
            return ty_error(TY_ERROR_MEMORY, NULL);
        return ty_error(TY_ERROR_SYSTEM, "kevent() failed: %s", strerror(errno));
    }

    return 0;
}

uint64_t ty_timer_rearm(ty_timer *timer)
{
    assert(timer);

    struct kevent kev;
    const struct timespec ts = {0};
    int r;

    r = kevent(timer->fd, NULL, 0, &kev, 1, &ts);
    if (r <= 0)
        return 0;
    assert(kev.ident == 1);

    return (uint64_t)kev.data;
}
