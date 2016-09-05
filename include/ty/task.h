/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_TASK_H
#define TY_TASK_H

#include "common.h"

TY_C_BEGIN

typedef struct ty_pool ty_pool;
typedef struct ty_task ty_task;

typedef void ty_task_cleanup_func(void *ptr);

TY_PUBLIC int ty_pool_new(ty_pool **rpool);
TY_PUBLIC void ty_pool_free(ty_pool *pool);

TY_PUBLIC int ty_pool_set_max_threads(ty_pool *pool, unsigned int max);
TY_PUBLIC unsigned int ty_pool_get_max_threads(ty_pool *pool);
TY_PUBLIC void ty_pool_set_idle_timeout(ty_pool *pool, int timeout);
TY_PUBLIC int ty_pool_get_idle_timeout(ty_pool *pool);

TY_PUBLIC int ty_pool_get_default(ty_pool **rpool);

TY_PUBLIC ty_task *ty_task_ref(ty_task *task);
TY_PUBLIC void ty_task_unref(ty_task *task);

TY_PUBLIC void ty_task_set_cleanup(ty_task *task, ty_task_cleanup_func *f, void *ptr);
TY_PUBLIC void ty_task_set_callback(ty_task *task, ty_message_func *f, void *udata);
TY_PUBLIC void ty_task_set_pool(ty_task *task, ty_pool *pool);

TY_PUBLIC int ty_task_start(ty_task *task);
TY_PUBLIC int ty_task_wait(ty_task *task, ty_task_status status, int timeout);
TY_PUBLIC int ty_task_join(ty_task *task);

TY_PUBLIC const char *ty_task_get_name(ty_task *task);
TY_PUBLIC ty_task_status ty_task_get_status(ty_task *task);

TY_PUBLIC int ty_task_get_return_value(ty_task *task);
TY_PUBLIC void *ty_task_get_result(ty_task *task);
TY_PUBLIC void *ty_task_steal_result(ty_task *task, ty_task_cleanup_func **rf);

TY_C_END

#endif
