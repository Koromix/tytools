/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_TASK_H
#define TY_TASK_H

#include "ty/common.h"

TY_C_BEGIN

typedef struct ty_pool ty_pool;
typedef struct ty_task ty_task;

typedef enum ty_task_status {
    TY_TASK_STATUS_READY,
    TY_TASK_STATUS_PENDING,
    TY_TASK_STATUS_RUNNING,
    TY_TASK_STATUS_FINISHED
} ty_task_status;

typedef struct ty_status_message {
    ty_task *task;
    ty_task_status status;
} ty_status_message;

typedef void ty_task_cleanup_func(void *ptr);

TY_PUBLIC int ty_pool_new(ty_pool **rpool);
TY_PUBLIC void ty_pool_free(ty_pool *pool);

TY_PUBLIC int ty_pool_get_default(ty_pool **rpool);

TY_PUBLIC ty_task *ty_task_ref(ty_task *task);
TY_PUBLIC void ty_task_unref(ty_task *task);

TY_PUBLIC void ty_task_set_cleanup(ty_task *task, ty_task_cleanup_func *f, void *ptr);
TY_PUBLIC void ty_task_set_callback(ty_task *task, ty_message_func *f, void *udata);
TY_PUBLIC void ty_task_set_pool(ty_task *task, ty_pool *pool);

TY_PUBLIC int ty_task_start(ty_task *task);
TY_PUBLIC int ty_task_wait(ty_task *task, ty_task_status status, int timeout);
TY_PUBLIC int ty_task_join(ty_task *task);

TY_PUBLIC ty_task_status ty_task_get_status(ty_task *task);

TY_PUBLIC int ty_task_get_return_value(ty_task *task);
TY_PUBLIC void *ty_task_get_result(ty_task *task);
TY_PUBLIC void *ty_task_steal_result(ty_task *task, ty_task_cleanup_func **rf);

TY_C_END

#endif
