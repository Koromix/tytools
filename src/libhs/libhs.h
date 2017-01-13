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

#ifndef HS_LIBHS_H
#define HS_LIBHS_H

/* This file provides both the interface and the implementation.
   To instantiate the implementation,
        #define HS_IMPLEMENTATION
   in *ONE* source file, before #including this file. */

#include "common.h"
#include "htable.h"
#include "list.h"
#include "device.h"
#include "hid.h"
#include "match.h"
#include "monitor.h"
#include "platform.h"
#include "serial.h"

#ifdef HS_IMPLEMENTATION
    #include "compat_priv.h"
    #include "common_priv.h"
    #include "device_priv.h"
    #include "filter_priv.h"

    #include "common.c"
    #include "compat.c"
    #include "device.c"
    #include "filter.c"
    #include "htable.c"
    #include "monitor_common.c"
    #include "platform.c"

    #if defined(_WIN32)
        #include "device_win32.c"
        #include "hid_win32.c"
        #include "monitor_win32.c"
        #include "platform_win32.c"
        #include "serial_win32.c"
    #elif defined(__APPLE__)
        #include "device_posix.c"
        #include "hid_darwin.c"
        #include "monitor_darwin.c"
        #include "platform_darwin.c"
        #include "serial_posix.c"
    #elif defined(__linux__)
        #include "device_posix.c"
        #include "hid_linux.c"
        #include "monitor_linux.c"
        #include "platform_posix.c"
        #include "serial_posix.c"
    #else
        #error "Platform not supported"
    #endif
#endif

#endif
