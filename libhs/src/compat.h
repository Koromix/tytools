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

#include "util.h"
#include <stdarg.h>

#ifdef HAVE_CONFIG_H
    #include "config.h"
#else
    /* This file is used when building with qmake, otherwise CMake detects
       these features. */
    #if defined(__MINGW32__)
        /* #undef HAVE_STPCPY */
        #define HAVE_ASPRINTF
    #elif defined(_MSC_VER)
        /* #undef HAVE_STPCPY */
        /* #undef HAVE_ASPRINTF */
    #elif defined(__APPLE__)
        #define HAVE_STPCPY
        #define HAVE_ASPRINTF
    #elif defined(__linux__)
        #define HAVE_STPCPY
        #define HAVE_ASPRINTF
    #else
        #error "Unknown platform, build with CMake instead"
    #endif
#endif

char *strrpbrk(const char *s, const char *accept);

#ifndef HAVE_STPCPY
char *stpcpy(char *dest, const char *src);
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...) HS_PRINTF_FORMAT(2, 3);
int vasprintf(char **strp, const char *fmt, va_list ap);
#endif

#endif
