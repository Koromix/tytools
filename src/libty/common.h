/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_COMMON_H
#define TY_COMMON_H

#ifdef _WIN32
    #include <malloc.h>
#else
    #include <alloca.h>
#endif
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libhs/common.h"
#include "config.h"

_HS_BEGIN_C

/* I really don't know where to put that and it is useful as a generic
   "how long to show error message" constant. */
#define TY_SHOW_ERROR_TIMEOUT 5000

struct ty_task;

typedef enum ty_err {
    TY_ERROR_MEMORY        = -1,
    TY_ERROR_PARAM         = -2,
    TY_ERROR_UNSUPPORTED   = -3,
    TY_ERROR_NOT_FOUND     = -4,
    TY_ERROR_EXISTS        = -5,
    TY_ERROR_ACCESS        = -6,
    TY_ERROR_BUSY          = -7,
    TY_ERROR_IO            = -8,
    TY_ERROR_MODE          = -9,
    TY_ERROR_TIMEOUT       = -10,
    TY_ERROR_RANGE         = -11,
    TY_ERROR_SYSTEM        = -12,
    TY_ERROR_PARSE         = -13,
    TY_ERROR_OTHER         = -14
} ty_err;

typedef enum ty_message_type {
    TY_MESSAGE_LOG,
    TY_MESSAGE_PROGRESS,
    TY_MESSAGE_STATUS
} ty_message_type;

typedef enum ty_log_level {
    TY_LOG_ERROR,
    TY_LOG_WARNING,
    TY_LOG_INFO,
    TY_LOG_DEBUG
} ty_log_level;

typedef enum ty_task_status {
    TY_TASK_STATUS_READY,
    TY_TASK_STATUS_PENDING,
    TY_TASK_STATUS_RUNNING,
    TY_TASK_STATUS_FINISHED
} ty_task_status;

typedef struct ty_message_data {
    const char *ctx;
    struct ty_task *task;

    ty_message_type type;
    union {
        struct {
            ty_log_level level;
            int err;
            const char *msg;
        } log;
        struct {
            const char *action;
            uint64_t value;
            uint64_t max;
        } progress;
        struct {
            ty_task_status status;
        } task;
    } u;
} ty_message_data;

typedef void ty_message_func(const ty_message_data *msg, void *udata);

extern int ty_config_verbosity;

const char *ty_version_string(void);

void ty_message_default_handler(const ty_message_data *msg, void *udata);
void ty_message_redirect(ty_message_func *f, void *udata);

void ty_error_mask(ty_err err);
void ty_error_unmask(void);
bool ty_error_is_masked(int err);

const char *ty_error_last_message(void);

void ty_message(ty_message_data *msg);
void ty_log(ty_log_level level, const char *fmt, ...) _HS_PRINTF_FORMAT(2, 3);
int ty_error(ty_err err, const char *fmt, ...) _HS_PRINTF_FORMAT(2, 3);
void ty_progress(const char *action, uint64_t value, uint64_t max);

int ty_libhs_translate_error(int err);
void ty_libhs_log_handler(hs_log_level level, int err, const char *log, void *udata);

void _ty_refcount_increase(unsigned int *rrefcount);
unsigned int _ty_refcount_decrease(unsigned int *rrefcount);

_HS_END_C

#endif
