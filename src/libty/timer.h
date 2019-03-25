/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

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

int ty_timer_new(ty_timer **rtimer);
void ty_timer_free(ty_timer *timer);

void ty_timer_get_descriptors(const ty_timer *timer, struct ty_descriptor_set *set, int id);

int ty_timer_set(ty_timer *timer, int value, int flags);
uint64_t ty_timer_rearm(ty_timer *timer);

TY_C_END

#endif
