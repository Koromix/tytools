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

#ifndef TY_TIMER_H
#define TY_TIMER_H

#include "common.h"

struct ty_descriptor_set;

TY_C_BEGIN

typedef struct ty_timer ty_timer;

int ty_timer_new(ty_timer **rtimer);
void ty_timer_free(ty_timer *timer);

void ty_timer_get_descriptors(const ty_timer *timer, struct ty_descriptor_set *set, int id);

int ty_timer_set(ty_timer *timer, int value, unsigned int period);
uint64_t ty_timer_rearm(ty_timer *timer);

TY_C_END

#endif
