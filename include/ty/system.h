/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_SYSTEM_H
#define TY_SYSTEM_H

#include "common.h"
#include <sys/types.h>

TY_C_BEGIN

typedef enum ty_file_type {
    TY_FILE_REGULAR,
    TY_FILE_DIRECTORY,
    TY_FILE_LINK,

    // Device, socket, pipe, etc.
    TY_FILE_SPECIAL
} ty_file_type;

typedef struct ty_file_info {
    ty_file_type type;
    uint64_t size;
    uint64_t mtime;
} ty_file_info;

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
TY_PUBLIC char *ty_win32_strerror(unsigned long err);
#endif

TY_PUBLIC uint64_t ty_millis(void);
TY_PUBLIC void ty_delay(unsigned int ms);

TY_PUBLIC int ty_adjust_timeout(int timeout, uint64_t start);

TY_PUBLIC int ty_stat(const char *path, ty_file_info *info, bool follow);

TY_PUBLIC void ty_descriptor_set_clear(ty_descriptor_set *set);
TY_PUBLIC void ty_descriptor_set_add(ty_descriptor_set *set, ty_descriptor desc, int id);
TY_PUBLIC void ty_descriptor_set_remove(ty_descriptor_set *set, int id);

TY_PUBLIC int ty_poll(const ty_descriptor_set *set, int timeout);

TY_PUBLIC int ty_terminal_setup(int flags);
TY_PUBLIC void ty_terminal_restore(void);

TY_C_END

#endif
