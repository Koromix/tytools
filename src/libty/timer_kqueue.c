/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
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

    timer->fd = kqueue();
    if (timer->fd < 0) {
        r = ty_error(TY_ERROR_SYSTEM, "kqueue() failed: %s", strerror(errno));
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

    struct kevent kev[2];
    int kev_count = 0;
    static const struct timespec ts = {0};
    int r;

    if (value > 0) {
        EV_SET(&kev[kev_count++], 0, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, value, NULL);
        if (flags & TY_TIMER_ONESHOT)
            kev[0].flags |= EV_ONESHOT;
    } else {
        EV_SET(&kev[kev_count++], 0, EVFILT_TIMER, EV_DISABLE, 0, 0, NULL);
        if (!value)
            EV_SET(&kev[kev_count++], 1, EVFILT_USER, EV_ADD | EV_ONESHOT, NOTE_TRIGGER | NOTE_FFNOP, 0, NULL);
    }

    r = kevent(timer->fd, kev, kev_count, NULL, 0, &ts);
    if (r < 0 && errno != ENOENT)
        return ty_error(TY_ERROR_SYSTEM, "kevent() failed: %s", strerror(errno));

    return 0;
}

uint64_t ty_timer_rearm(ty_timer *timer)
{
    assert(timer);

    struct kevent kev;
    static const struct timespec ts = {0};
    int r;

    r = kevent(timer->fd, NULL, 0, &kev, 1, &ts);
    if (r <= 0)
        return 0;

    switch (kev.ident) {
        case 0: { return (uint64_t)kev.data; } break;
        case 1: { return 1; } break;
    }

    assert(false);
    __builtin_unreachable();
}
