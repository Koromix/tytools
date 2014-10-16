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

#ifndef TY_COMMON_H
#define TY_COMMON_H

#define TY_VERSION "1.0~alpha"

// Keep assertions ;)
#undef NDEBUG

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
    #define TY_FUNC_NORETURN __attribute__((__noreturn__))

    #define TY_INIT(name) \
        __attribute__((constructor)) \
        void name(void)

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

TY_C_BEGIN

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
    TY_ERROR_RANGE         = -10,
    TY_ERROR_SYSTEM        = -11,
    TY_ERROR_PARSE         = -12,
    TY_ERROR_FIRMWARE      = -13,
    TY_ERROR_OTHER         = -14
} ty_err;

typedef void ty_error_func(ty_err err, const char *msg, void *udata);

void ty_error_redirect(ty_error_func *f, void *udata);
void ty_error_mask(ty_err err);
void ty_error_unmask(void);

int ty_error(ty_err err, const char *fmt, ...) TY_PRINTF_FORMAT(2, 3);

TY_C_END

#endif
