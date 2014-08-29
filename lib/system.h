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

enum {
    TY_MKDIR_MAKE_PARENTS  = 1,
    TY_MKDIR_IGNORE_EXISTS = 2,
    TY_MKDIR_OMIT_LAST     = 4
};

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

int ty_mkdir(const char *path, mode_t mode, uint16_t flags);

int ty_stat(const char *path, ty_file_info *info, bool follow_symlink);

int ty_find_config(char **rpath, const char *filename);
int ty_user_config(char **rpath, const char *filename, bool make_parents);

int ty_terminal_change(uint32_t flags);

TY_C_END

#endif
