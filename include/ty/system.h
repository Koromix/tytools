/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_SYSTEM_H
#define TY_SYSTEM_H

#include "common.h"
#include <sys/types.h>

TY_C_BEGIN

#ifdef _WIN32
extern void *_ty_win32_descriptors[3]; // HANDLE
#endif

typedef enum ty_file_type {
    TY_FILE_REGULAR,
    TY_FILE_DIRECTORY,
    TY_FILE_LINK,

    // Device, socket, pipe, etc.
    TY_FILE_SPECIAL
} ty_file_type;

enum {
    TY_FILE_HIDDEN = 1
};

typedef struct ty_file_info {
    ty_file_type type;
    uint64_t size;
    uint64_t mtime;

#ifdef _WIN32
    uint32_t volume;
    uint8_t fileindex[16];
#else
    dev_t dev;
    ino_t ino;
#endif

    uint16_t flags;
} ty_file_info;

#ifdef _WIN32
#define TY_PATH_SEPARATORS "\\/"
#else
#define TY_PATH_SEPARATORS "/"
#endif

enum {
    TY_MKDIR_PARENTS  = 1,
    TY_MKDIR_PERMISSIVE = 2
};

enum {
    TY_WALK_FOLLOW = 1,
    TY_WALK_HIDDEN  = 2
};

typedef struct ty_walk_history {
    struct ty_walk_history *prev;

    ty_file_info info;

    size_t relative;
    size_t base;
    size_t level;

    struct _ty_walk_context *ctx;
} ty_walk_history;

typedef int ty_walk_func(const char *path, ty_walk_history *history, void *udata);

#ifdef _WIN32

typedef void *ty_descriptor; // HANDLE

#define TY_INVALID_DESCRIPTOR (NULL)
#define TY_STDIN_DESCRIPTOR (_ty_win32_descriptors[0])
#define TY_STDOUT_DESCRIPTOR (_ty_win32_descriptors[1])
#define TY_STDERR_DESCRIPTOR (_ty_win32_descriptors[2])

#else

typedef int ty_descriptor;

#define TY_INVALID_DESCRIPTOR (-1)
#define TY_STDIN_DESCRIPTOR (0)
#define TY_STDOUT_DESCRIPTOR (1)
#define TY_STDERR_DESCRIPTOR (2)

#endif

typedef struct ty_descriptor_set {
    size_t count;
    ty_descriptor desc[64];
    int id[64];
} ty_descriptor_set;

enum {
    TY_TERMINAL_RAW = 0x1,
    TY_TERMINAL_SILENT = 0x2
};

#ifdef _WIN32

typedef enum ty_win32_version {
    TY_WIN32_XP,
    TY_WIN32_VISTA,
    TY_WIN32_SEVEN,
    TY_WIN32_EIGHT
} ty_win32_version;

TY_PUBLIC char *ty_win32_strerror(unsigned long err);
TY_PUBLIC bool ty_win32_test_version(ty_win32_version version);

#endif

TY_PUBLIC uint64_t ty_millis(void);
TY_PUBLIC void ty_delay(unsigned int ms);

TY_PUBLIC int ty_adjust_timeout(int timeout, uint64_t start);

TY_PUBLIC bool ty_path_is_absolute(const char *path);

TY_PUBLIC int ty_path_split(const char *path, char **rdirectory, char **rname);
TY_PUBLIC const char *ty_path_ext(const char *path);

TY_PUBLIC int ty_realpath(const char *path, const char *base, char **rpath);

TY_PUBLIC int ty_stat(const char *path, ty_file_info *info, bool follow);
TY_PUBLIC bool ty_file_unique(const ty_file_info *info1, const ty_file_info *info2);

TY_PUBLIC int ty_mkdir(const char *path, mode_t mode, uint16_t flags);
TY_PUBLIC int ty_delete(const char *path, bool tolerant);

TY_PUBLIC int ty_walk(const char *path, ty_walk_history *history, ty_walk_func *f, void *udata, uint32_t flags);

TY_PUBLIC void ty_descriptor_set_clear(ty_descriptor_set *set);
TY_PUBLIC void ty_descriptor_set_add(ty_descriptor_set *set, ty_descriptor desc, int id);
TY_PUBLIC void ty_descriptor_set_remove(ty_descriptor_set *set, int id);

TY_PUBLIC int ty_poll(const ty_descriptor_set *set, int timeout);

TY_PUBLIC int ty_terminal_change(uint32_t flags);

TY_C_END

#endif
