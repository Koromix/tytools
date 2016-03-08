/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_UTIL_H
#define TY_UTIL_H

#ifdef HAVE_CONFIG_H
    #include "config.h"
#else
    /* This file is used when building with qmake, otherwise CMake detects
       these features. */
    #if defined(_WIN32)
        #define HAVE_ASPRINTF
    #elif defined(__APPLE__)
        #define HAVE_ASPRINTF
        #define HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP
    #elif defined(__linux__)
        #define HAVE_ASPRINTF
        /* #undef HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP */
    #else
        #error "Unknown platform, build with CMake instead"
    #endif
#endif

#include "ty/common.h"
#include "compat.h"

void _ty_message(struct ty_task *task, ty_message_type type, const void *data);

int _ty_libhs_translate_error(int err);

void _ty_refcount_increase(unsigned int *rrefcount);
unsigned int _ty_refcount_decrease(unsigned int *rrefcount);

#endif
