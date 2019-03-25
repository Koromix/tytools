/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef HS_LIBHS_H
#define HS_LIBHS_H

/* This file provides both the interface and the implementation.

   To instantiate the implementation,
        #define HS_IMPLEMENTATION
   in *ONE* source file, before #including this file.

   libhs depends on **a few OS-provided libraries** that you need to link:

   OS                  | Dependencies
   ------------------- | --------------------------------------------------------------------------------
   Windows (MSVC)      | Nothing to do, libhs uses `#pragma comment(lib)`
   Windows (MinGW-w64) | Link _user32, advapi32, setupapi and hid_ `-luser32 -ladvapi32 -lsetupapi -lhid`
   OSX (Clang)         | Link _CoreFoundation and IOKit_ `-framework CoreFoundation -framework IOKit`
   Linux (GCC)         | Link _libudev_ `-ludev`

   Other systems are not supported at the moment. */

#include "common.h"
#include "array.h"
#include "htable.h"
#include "device.h"
#include "hid.h"
#include "match.h"
#include "monitor.h"
#include "platform.h"
#include "serial.h"

#endif

#ifdef HS_IMPLEMENTATION
    #if defined(_MSC_VER)
        #pragma comment(lib, "user32.lib")
        #pragma comment(lib, "advapi32.lib")
        #pragma comment(lib, "setupapi.lib")
        #pragma comment(lib, "hid.lib")
    #endif

    #include "compat_priv.h"
    #include "common_priv.h"
    #include "device_priv.h"
    #include "match_priv.h"

    #include "common.c"
    #include "compat.c"
    #include "array.c"
    #include "device.c"
    #include "match.c"
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
