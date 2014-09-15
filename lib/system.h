/**
 * ty, command-line program to manage Teensy devices
 * http://github.com/Koromix/ty
 * Copyright (C) 2014 Niels Martignène
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TY_SYSTEM_H
#define TY_SYSTEM_H

#include "common.h"
#include <sys/types.h>
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

TY_C_BEGIN

typedef enum ty_file_type {
    TY_FILE_UNKNOWN,

    TY_FILE_REGULAR,
    TY_FILE_DIRECTORY,
    TY_FILE_LINK,

    // Device, socket, pipe, etc.
    TY_FILE_SPECIAL
} ty_file_type;

typedef struct ty_file_info {
    ty_file_type type;
    off_t size;
    uint64_t mtime;

#ifndef _WIN32
    dev_t dev;
    ino_t ino;
#endif
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
    size_t count;
    ty_descriptor desc[64];
    int id[64];
} ty_descriptor_set;

typedef struct ty_timer ty_timer;

enum {
    TY_TERMINAL_RAW = 0x1,
    TY_TERMINAL_SILENT = 0x2
};

#ifdef _WIN32
typedef enum ty_win32_version {
    TY_WIN32_XP,
    TY_WIN32_VISTA,
    TY_WIN32_SEVEN
} ty_win32_version;
#endif

#ifdef _WIN32
char *ty_win32_strerror(unsigned long err);
bool ty_win32_test_version(ty_win32_version version);
#endif

uint64_t ty_millis(void);
void ty_delay(unsigned int ms);

int ty_stat(const char *path, ty_file_info *info, bool follow_symlink);

int ty_timer_new(ty_timer **rtimer);
void ty_timer_free(ty_timer *timer);

void ty_timer_get_descriptors(ty_timer *timer, ty_descriptor_set *set, int id);

int ty_timer_set(ty_timer *timer, int value, unsigned int period);
uint64_t ty_timer_rearm(ty_timer *timer);

void ty_descriptor_set_clear(ty_descriptor_set *set);
void ty_descriptor_set_add(ty_descriptor_set *set, ty_descriptor desc, int id);

int ty_poll(const ty_descriptor_set *set, int timeout);

int ty_terminal_change(uint32_t flags);

TY_C_END

#endif
