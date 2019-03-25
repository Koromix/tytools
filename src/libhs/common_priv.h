/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef _HS_COMMON_PRIV_H
#define _HS_COMMON_PRIV_H

#include "common.h"
#include "compat_priv.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
    #define _HS_POSSIBLY_UNUSED __attribute__((__unused__))
    #define _HS_THREAD_LOCAL __thread
    #define _HS_ALIGN_OF(type)  __alignof__(type)
#elif defined(_MSC_VER)
    #define _HS_POSSIBLY_UNUSED
    #define _HS_THREAD_LOCAL __declspec(thread)
    #define _HS_ALIGN_OF(type) __alignof(type)

    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
#endif

#define _HS_UNUSED(arg) ((void)(arg))

#define _HS_COUNTOF(a) (sizeof(a) / sizeof(*(a)))

#define _HS_ALIGN_SIZE(size, align) (((size) + (align) - 1) / (align) * (align))
#define _HS_ALIGN_SIZE_FOR_TYPE(size, type) _HS_ALIGN_SIZE((size), sizeof(type))

#endif
