/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef _HS_COMPAT_H
#define _HS_COMPAT_H

#include "common_priv.h"
#include <stdarg.h>

#ifdef _HS_HAVE_CONFIG_H
    #include "config.h"
#else
    /* This file is used when building with qmake, otherwise CMake detects
       these features. */
    #if defined(_GNU_SOURCE) || _POSIX_C_SOURCE >= 200809L
        #define _HS_HAVE_STPCPY
    #endif
    #ifdef _GNU_SOURCE
        #define _HS_HAVE_ASPRINTF
    #endif
#endif

#ifdef _HS_HAVE_STPCPY
    #define _hs_stpcpy stpcpy
#else
char *_hs_stpcpy(char *dest, const char *src);
#endif

#ifdef _HS_HAVE_ASPRINTF
    #define _hs_asprintf asprintf
    #define _hs_vasprintf vasprintf
#else
int _hs_asprintf(char **strp, const char *fmt, ...) HS_PRINTF_FORMAT(2, 3);
int _hs_vasprintf(char **strp, const char *fmt, va_list ap);
#endif

#endif
