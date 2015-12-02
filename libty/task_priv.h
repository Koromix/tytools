/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_TASK_PRIV_H
#define TY_TASK_PRIV_H

#include "ty/common.h"
#include "list.h"
#include "ty/task.h"
#include "ty/thread.h"

TY_C_BEGIN

#define TY_TASK \
    unsigned int refcount; \
    \
    ty_list_head list; \
    \
    ty_task_status status; \
    int ret; \
    ty_mutex mutex; \
    ty_cond cond; \
    \
    const struct _ty_task_vtable *vtable; \
    \
    ty_pool *pool; \
    \
    ty_message_func *callback; \
    void *callback_udata; \
    \
    ty_task_cleanup_func *cleanup; \
    void *cleanup_udata;

struct _ty_task_vtable {
    int (*run)(ty_task *task);
    void (*cleanup)(ty_task *task);
};

int _ty_task_new(size_t size, const struct _ty_task_vtable *vtable, ty_task **rtask);

TY_C_END

#endif
