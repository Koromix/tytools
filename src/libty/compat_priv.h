/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_COMPAT_PRIV_H
#define TY_COMPAT_PRIV_H

#include "common_priv.h"
#include <stdarg.h>

TY_C_BEGIN

#ifdef _TY_HAVE_CONFIG_H
    #include "config.h"
#else
    /* This file is used when building with qmake, otherwise CMake detects
       these features. */
    #ifdef _GNU_SOURCE
        #define _TY_HAVE_ASPRINTF
    #endif
    #ifdef __APPLE__
        #define _TY_HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP
    #endif
#endif

#ifndef _TY_HAVE_ASPRINTF
int _ty_asprintf(char **strp, const char *fmt, ...) TY_PRINTF_FORMAT(2, 3);
#define asprintf _ty_asprintf
int _ty_vasprintf(char **strp, const char *fmt, va_list ap);
#define vasprintf _ty_vasprintf
#endif

TY_C_END

#endif
