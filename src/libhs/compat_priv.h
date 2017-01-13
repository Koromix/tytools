/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

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
