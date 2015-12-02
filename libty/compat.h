/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_COMPAT_H
#define TY_COMPAT_H

#include "ty/common.h"
#include <stdarg.h>

#ifdef HAVE_CONFIG_H
    #include "config.h"
#else
    /* This file is used when building with qmake, otherwise CMake detects
       these features. */
    #if defined(_WIN32)
        /* #undef HAVE_STPCPY */
        /* #undef HAVE_STPNCPY */
        /* #undef HAVE_STRNDUP */
        /* #undef HAVE_MEMRCHR */
        #define HAVE_ASPRINTF
        /* #undef HAVE_GETDELIM */
        /* #undef HAVE_GETLINE */
        /* #undef HAVE_FSTATAT */
        /* #undef HAVE_PIPE2 */
    #elif defined(__APPLE__)
        #define HAVE_STPCPY
        #define HAVE_STPNCPY
        #define HAVE_STRNDUP
        /* #undef HAVE_MEMRCHR */
        #define HAVE_ASPRINTF
        #define HAVE_GETDELIM
        #define HAVE_GETLINE
        #define HAVE_FSTATAT
        /* #undef HAVE_PIPE2 */
        #define HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP

        /* #undef HAVE_STAT_MTIM */
        #define HAVE_STAT_MTIMESPEC
    #elif defined(__linux__)
        #define HAVE_STPCPY
        #define HAVE_STPNCPY
        #define HAVE_STRNDUP
        #define HAVE_MEMRCHR
        #define HAVE_ASPRINTF
        #define HAVE_GETDELIM
        #define HAVE_GETLINE
        #define HAVE_FSTATAT
        #define HAVE_PIPE2
        /* #undef HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP */

        #define HAVE_STAT_MTIM
        /* #undef HAVE_STAT_MTIMESPEC */
    #else
        #error "Unknown platform, build with CMake instead"
    #endif
#endif

TY_C_BEGIN

char *strrpbrk(const char *s, const char *accept);

#ifndef HAVE_STPCPY
char *stpcpy(char *dest, const char *src);
#endif

#ifndef HAVE_STPNCPY
char *stpncpy(char *dest, const char *src, size_t n);
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif

#ifndef HAVE_MEMRCHR
void *memrchr(const void *s, int c, size_t n);
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...) TY_PRINTF_FORMAT(2, 3);
int vasprintf(char **strp, const char *fmt, va_list ap);
#endif

#ifndef HAVE_GETDELIM
ssize_t getdelim(char **rbuf, size_t *rsize, int delim, FILE *fp);
#endif

#ifndef HAVE_GETLINE
ssize_t getline(char **rbuf, size_t *rsize, FILE *fp);
#endif

TY_C_END

#endif
