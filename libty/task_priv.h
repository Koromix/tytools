/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
