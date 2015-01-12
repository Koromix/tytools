/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_TIMER_H
#define TY_TIMER_H

#include "common.h"

struct ty_descriptor_set;

TY_C_BEGIN

typedef struct ty_timer ty_timer;

enum {
    TY_TIMER_ONESHOT = 1
};

TY_PUBLIC int ty_timer_new(ty_timer **rtimer);
TY_PUBLIC void ty_timer_free(ty_timer *timer);

TY_PUBLIC void ty_timer_get_descriptors(const ty_timer *timer, struct ty_descriptor_set *set, int id);

TY_PUBLIC int ty_timer_set(ty_timer *timer, int value, uint16_t flags);
TY_PUBLIC uint64_t ty_timer_rearm(ty_timer *timer);

TY_C_END

#endif
