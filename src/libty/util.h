/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_UTIL_H
#define TY_UTIL_H

#include "ty/common.h"
#include "compat.h"

void _ty_message(struct ty_task *task, ty_message_type type, const void *data);

void _ty_refcount_increase(unsigned int *rrefcount);
unsigned int _ty_refcount_decrease(unsigned int *rrefcount);

#endif
