/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_TASK_H
#define TY_TASK_H

#include "common.h"
#include "thread.h"

TY_C_BEGIN

struct ty_board;
struct ty_firmware;

typedef struct ty_pool ty_pool;

typedef struct ty_task {
    unsigned int refcount;

    char *name;
    ty_task_status status;
    ty_pool *pool;

    ty_message_func *user_callback;
    void *user_callback_udata;
    void (*user_cleanup)(void *udata);
    void *user_cleanup_udata;

    int ret;
    void *result;
    void (*result_cleanup)(void *result);

    int (*task_run)(struct ty_task *task);
    void (*task_finalize)(struct ty_task *task);

    ty_mutex mutex;
    ty_cond cond;

    union {
        struct {
            struct ty_board *board;
            struct ty_firmware **fws;
            unsigned int fws_count;
            int flags;
        } upload;

        struct {
            struct ty_board *board;
            char *buf;
            size_t size;
        } send;

        struct {
            struct ty_board *board;
            FILE *fp;
            size_t size;
            char *filename;
        } send_file;

        struct {
            struct ty_board *board;
        } reset;

        struct {
            struct ty_board *board;
        } reboot;
    } u;
} ty_task;

TY_PUBLIC int ty_pool_new(ty_pool **rpool);
TY_PUBLIC void ty_pool_free(ty_pool *pool);

TY_PUBLIC int ty_pool_set_max_threads(ty_pool *pool, unsigned int max);
TY_PUBLIC unsigned int ty_pool_get_max_threads(ty_pool *pool);
TY_PUBLIC void ty_pool_set_idle_timeout(ty_pool *pool, int timeout);
TY_PUBLIC int ty_pool_get_idle_timeout(ty_pool *pool);

TY_PUBLIC int ty_pool_get_default(ty_pool **rpool);

TY_PUBLIC int ty_task_new(const char *name, int (*run)(ty_task *task), ty_task **rtask);

TY_PUBLIC ty_task *ty_task_ref(ty_task *task);
TY_PUBLIC void ty_task_unref(ty_task *task);

TY_PUBLIC int ty_task_start(ty_task *task);
TY_PUBLIC int ty_task_wait(ty_task *task, ty_task_status status, int timeout);
TY_PUBLIC int ty_task_join(ty_task *task);

TY_PUBLIC ty_task *ty_task_get_current(void);

TY_C_END

#endif
