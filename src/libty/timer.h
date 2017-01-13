/* Teensy Tools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/teensytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

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
