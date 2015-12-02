/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_COMMON_H
#define TY_COMMON_H

// Avoid msvcrt's limited versions of printf/scanf functions
#define __USE_MINGW_ANSI_STDIO 1

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef __cplusplus
    #define TY_C_BEGIN extern "C" {
    #define TY_C_END }
#else
    #define TY_C_BEGIN
    #define TY_C_END
#endif

#ifdef __GNUC__
    #define TY_PUBLIC __attribute__((__visibility__("default")))
    #define TY_NORETURN __attribute__((__noreturn__))

    #define TY_INIT() \
        __attribute__((constructor)) \
        static void TY_UNIQUE_ID(init_)(void)
    #define TY_EXIT() \
        __attribute__((destructor)) \
        static void TY_UNIQUE_ID(exit_)(void)

    #define TY_WARNING_DISABLE_SIGN_CONVERSION \
        _Pragma("GCC diagnostic push"); \
        _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"");
    #define TY_WARNING_RESTORE \
        _Pragma("GCC diagnostic pop");

    #ifdef __MINGW_PRINTF_FORMAT
        #define TY_PRINTF_FORMAT(fmt, first) __attribute__((__format__(__MINGW_PRINTF_FORMAT, fmt, first)))
    #else
        #define TY_PRINTF_FORMAT(fmt, first) __attribute__((__format__(__printf__, fmt, first)))
    #endif
#else
    #error "This compiler is not supported"
#endif

#define TY_UNUSED(arg) ((void)(arg))

#define TY_COUNTOF(a) (sizeof(a) / sizeof(*(a)))

#define TY_MIN(a,b) ((a) < (b) ? (a) : (b))
#define TY_MAX(a,b) ((a) > (b) ? (a) : (b))

#define TY_CONCAT_HELPER(a, b) a ## b
#define TY_CONCAT(a, b) TY_CONCAT_HELPER(a, b)

#define TY_UNIQUE_ID(prefix) TY_CONCAT(prefix, __LINE__)

#define ty_container_of(head, type, member) \
    ((type *)((char *)(head) - (size_t)(&((type *)0)->member)))

#define ty_member_sizeof(type, member) sizeof(((type *)0)->member)

TY_C_BEGIN

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
    TY_ERROR_FIRMWARE      = -14,
    TY_ERROR_OTHER         = -15
} ty_err;

typedef enum ty_message_type {
    TY_MESSAGE_LOG,

    TY_MESSAGE_PROGRESS,
    TY_MESSAGE_STATUS
} ty_message_type;

typedef enum ty_log_level {
    TY_LOG_DEBUG = -1,
    TY_LOG_INFO,
    TY_LOG_WARNING,
    TY_LOG_ERROR,
} ty_log_level;

typedef struct ty_log_message {
    ty_log_level level;
    const char *msg;
} ty_log_message;

typedef struct ty_progress_message {
    const char *action;

    unsigned int value;
    unsigned int max;
} ty_progress_message;

typedef void ty_message_func(struct ty_task *task, ty_message_type type, const void *data, void *udata);

TY_PUBLIC extern bool ty_config_experimental;

TY_PUBLIC void ty_message_default_handler(struct ty_task *task, ty_message_type type, const void *data, void *udata);
TY_PUBLIC void ty_message_redirect(ty_message_func *f, void *udata);

TY_PUBLIC void ty_error_mask(ty_err err);
TY_PUBLIC void ty_error_unmask(void);

TY_PUBLIC const char *ty_error_last_message(void);

TY_PUBLIC void ty_log(ty_log_level level, const char *fmt, ...) TY_PRINTF_FORMAT(2, 3);
TY_PUBLIC int ty_error(ty_err err, const char *fmt, ...) TY_PRINTF_FORMAT(2, 3);

TY_PUBLIC void ty_progress(const char *action, unsigned int value, unsigned int max);

void _ty_message(struct ty_task *task, ty_message_type type, void *data);

TY_C_END

#endif
