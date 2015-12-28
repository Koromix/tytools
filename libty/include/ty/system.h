/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_SYSTEM_H
#define TY_SYSTEM_H

#include "common.h"
#include <sys/types.h>

TY_C_BEGIN

#ifdef _WIN32
    #define TY_PATH_SEPARATORS "\\/"
#else
    #define TY_PATH_SEPARATORS "/"
#endif

#ifdef _WIN32
typedef void *ty_descriptor; // HANDLE
#else
typedef int ty_descriptor;
#endif

#ifdef _WIN32
    #define TY_DESCRIPTOR_STDIN (_ty_win32_descriptors[0])
    #define TY_DESCRIPTOR_STDOUT (_ty_win32_descriptors[1])
    #define TY_DESCRIPTOR_STDERR (_ty_win32_descriptors[2])
#else
    #define TY_DESCRIPTOR_STDIN 0
    #define TY_DESCRIPTOR_STDOUT 1
    #define TY_DESCRIPTOR_STDERR 2
#endif

enum {
    TY_DESCRIPTOR_MODE_FIFO = 1,
    TY_DESCRIPTOR_MODE_DEVICE = 2,
    TY_DESCRIPTOR_MODE_TERMINAL = 4,
    TY_DESCRIPTOR_MODE_FILE = 8
};

typedef struct ty_descriptor_set {
    unsigned int count;
    ty_descriptor desc[64];
    int id[64];
} ty_descriptor_set;

enum {
    TY_TERMINAL_RAW = 0x1,
    TY_TERMINAL_SILENT = 0x2
};

#ifdef _WIN32
TY_PUBLIC extern void *_ty_win32_descriptors[3]; // HANDLE
#endif

#ifdef _WIN32
TY_PUBLIC char *ty_win32_strerror(unsigned long err);
#endif

TY_PUBLIC uint64_t ty_millis(void);
TY_PUBLIC void ty_delay(unsigned int ms);

TY_PUBLIC int ty_adjust_timeout(int timeout, uint64_t start);

TY_PUBLIC void ty_descriptor_set_clear(ty_descriptor_set *set);
TY_PUBLIC void ty_descriptor_set_add(ty_descriptor_set *set, ty_descriptor desc, int id);
TY_PUBLIC void ty_descriptor_set_remove(ty_descriptor_set *set, int id);

TY_PUBLIC unsigned int ty_descriptor_get_modes(ty_descriptor desc);

TY_PUBLIC int ty_poll(const ty_descriptor_set *set, int timeout);

TY_PUBLIC bool ty_compare_paths(const char *path1, const char *path2);

TY_PUBLIC int ty_terminal_setup(int flags);
TY_PUBLIC void ty_terminal_restore(void);

TY_C_END

#endif
