/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

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
#include "hs/common.h"

#ifdef __cplusplus
    #define TY_C_BEGIN extern "C" {
    #define TY_C_END }
#else
    #define TY_C_BEGIN
    #define TY_C_END
#endif

TY_C_BEGIN

#if defined(__GNUC__)
    #define TY_PUBLIC __attribute__((__visibility__("default")))
    #define TY_POSSIBLY_UNUSED __attribute__((__unused__))
    #ifdef __MINGW_PRINTF_FORMAT
        #define TY_PRINTF_FORMAT(fmt, first) __attribute__((__format__(__MINGW_PRINTF_FORMAT, fmt, first)))
    #else
        #define TY_PRINTF_FORMAT(fmt, first) __attribute__((__format__(__printf__, fmt, first)))
    #endif

    #define TY_THREAD_LOCAL __thread

    #ifdef __APPLE__
        #define TY_INIT() \
            static int TY_UNIQUE_ID(init_)(void); \
            static int (*TY_UNIQUE_ID(init_ptr_))(void) __attribute((__section__("__DATA,TY_INIT"),__used__)) \
                = &TY_UNIQUE_ID(init_); \
            static int TY_UNIQUE_ID(init_)(void)
        #define TY_RELEASE() \
            static void TY_UNIQUE_ID(release_)(void); \
            static void (*TY_UNIQUE_ID(release_ptr_))(void) __attribute((__section__("__DATA,TY_RELEASE"),__used__)) \
                = &TY_UNIQUE_ID(release_); \
            static void TY_UNIQUE_ID(release_)(void)
    #else
        #define TY_INIT() \
            static int TY_UNIQUE_ID(init_)(void); \
            static int (*TY_UNIQUE_ID(init_ptr_))(void) __attribute((__section__("TY_INIT"),__used__)) \
                = &TY_UNIQUE_ID(init_); \
            static int TY_UNIQUE_ID(init_)(void)
        #define TY_RELEASE() \
            static void TY_UNIQUE_ID(release_)(void); \
            static void (*TY_UNIQUE_ID(release_ptr_))(void) __attribute((__section__("TY_RELEASE"),__used__)) \
                = &TY_UNIQUE_ID(release_); \
            static void TY_UNIQUE_ID(release_)(void)
    #endif
#elif _MSC_VER >= 1900
    #if defined(TY_STATIC)
        #define TY_PUBLIC
    #elif defined(TY_UTIL_H)
        #define TY_PUBLIC __declspec(dllexport)
    #else
        #define TY_PUBLIC __declspec(dllimport)
    #endif
    #define TY_POSSIBLY_UNUSED
    #define TY_PRINTF_FORMAT(fmt, first)

    #define TY_THREAD_LOCAL __declspec(thread)

    #define TY_INIT() \
        static int TY_UNIQUE_ID(init_)(void); \
        __pragma(section(".TY_INIT$u", read)) \
        __declspec(allocate(".TY_INIT$u")) int (*TY_UNIQUE_ID(init_ptr_))(void) = TY_UNIQUE_ID(init_); \
        static int __cdecl TY_UNIQUE_ID(init_)(void)
    #define TY_RELEASE() \
        static void __cdecl TY_UNIQUE_ID(release_)(void); \
        __pragma(section(".TY_RELEASE$u", read)) \
        __declspec(allocate(".TY_RELEASE$u")) void (*TY_UNIQUE_ID(release_ptr_))(void) = TY_UNIQUE_ID(release_); \
        static void __cdecl TY_UNIQUE_ID(release_)(void)

    // HAVE_SSIZE_T is used this way by other projects
    #ifndef HAVE_SSIZE_T
        #define HAVE_SSIZE_T
        #ifdef _WIN64
typedef __int64 ssize_t;
        #else
typedef long ssize_t;
        #endif
    #endif

    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
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
    TY_LOG_ERROR,
    TY_LOG_WARNING,
    TY_LOG_INFO,
    TY_LOG_DEBUG
} ty_log_level;

typedef struct ty_log_message {
    ty_log_level level;
    int err;
    const char *msg;
} ty_log_message;

typedef struct ty_progress_message {
    const char *action;

    unsigned int value;
    unsigned int max;
} ty_progress_message;

typedef void ty_message_func(struct ty_task *task, ty_message_type type, const void *data, void *udata);

TY_PUBLIC extern ty_log_level ty_config_verbosity;

TY_PUBLIC int ty_init(void);
TY_PUBLIC void ty_release(void);

TY_PUBLIC void ty_message_default_handler(struct ty_task *task, ty_message_type type, const void *data, void *udata);
TY_PUBLIC void ty_message_redirect(ty_message_func *f, void *udata);

TY_PUBLIC void ty_error_mask(ty_err err);
TY_PUBLIC void ty_error_unmask(void);
TY_PUBLIC bool ty_error_is_masked(int err);

TY_PUBLIC const char *ty_error_last_message(void);

TY_PUBLIC void ty_log(ty_log_level level, const char *fmt, ...) TY_PRINTF_FORMAT(2, 3);
TY_PUBLIC int ty_error(ty_err err, const char *fmt, ...) TY_PRINTF_FORMAT(2, 3);
TY_PUBLIC void ty_progress(const char *action, unsigned int value, unsigned int max);

TY_PUBLIC int ty_libhs_translate_error(int err);
TY_PUBLIC void ty_libhs_log_handler(hs_log_level level, int err, const char *log, void *udata);

TY_C_END

#endif
