/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_TIMER_H
#define TY_TIMER_H

#include "common.h"

TY_C_BEGIN

struct ty_descriptor_set;

typedef struct ty_timer ty_timer;

enum {
    TY_TIMER_ONESHOT = 1
};

TY_PUBLIC int ty_timer_new(ty_timer **rtimer);
TY_PUBLIC void ty_timer_free(ty_timer *timer);

TY_PUBLIC void ty_timer_get_descriptors(const ty_timer *timer, struct ty_descriptor_set *set, int id);

TY_PUBLIC int ty_timer_set(ty_timer *timer, int value, int flags);
TY_PUBLIC uint64_t ty_timer_rearm(ty_timer *timer);

TY_C_END

#endif
