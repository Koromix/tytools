/* libhs - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

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
    #ifdef _MSC_VER
        #pragma comment(lib, "setupapi.lib")
        #pragma comment(lib, "hid.lib")
    #endif

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
