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

#include "common.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "system.h"

int make_directory(const char *path, mode_t mode, bool ignore_exists)
{
#ifdef _WIN32
    TY_UNUSED(mode);
#endif

    int r;

#ifdef _WIN32
    r = mkdir(path);
#else
    r = mkdir(path, mode);
#endif
    if (r < 0) {
        switch (errno) {
        case EEXIST:
            if (ignore_exists)
                return 0;
            return ty_error(TY_ERROR_EXISTS, "Directory '%s' already exists", path);
        case EACCES:
            return ty_error(TY_ERROR_ACCESS, "Permission denied to create '%s'", path);
        case ENOSPC:
            return ty_error(TY_ERROR_IO, "Failed to create directory '%s' because disk is full",
                            path);
        case ENOENT:
            return ty_error(TY_ERROR_NOT_FOUND, "Part of '%s' path does not exist", path);
        case ENOTDIR:
            return ty_error(TY_ERROR_NOT_FOUND, "Part of '%s' is not a directory", path);
        }
        return ty_error(TY_ERROR_SYSTEM, "mkdir('%s') failed: %s", path, strerror(errno));
    }

    return 1;
}

int ty_mkdir(const char *path, mode_t mode, uint16_t flags)
{
    assert(path && path[0]);

    char *copy, *ptr;
    int r;

    copy = strdup(path);
    if (!copy)
        return ty_error(TY_ERROR_MEMORY, NULL);

    ptr = copy + strlen(copy);
    while (ptr > copy && strchr(TY_PATH_SEPARATORS, ptr[-1]))
        ptr--;
    if (ptr == copy) {
        r = 0;
        goto cleanup;
    }
    *ptr = 0;

    if (flags & TY_MKDIR_OMIT_LAST) {
        ptr = strrpbrk(copy, TY_PATH_SEPARATORS);
        while (ptr > copy && strchr(TY_PATH_SEPARATORS, ptr[-1]))
            ptr--;
        if (ptr <= copy) {
            r = 0;
            goto cleanup;
        }
        *ptr = 0;
    }

    if (flags & TY_MKDIR_MAKE_PARENTS) {
        ptr = copy;
        while ((ptr = strpbrk(ptr + 1, TY_PATH_SEPARATORS))) {
            *ptr = 0;
            r = make_directory(copy, mode, true);
            if (r < 0)
                goto cleanup;
            *ptr = *TY_PATH_SEPARATORS;
        }
    }

    r = make_directory(copy, mode, flags & TY_MKDIR_IGNORE_EXISTS);

cleanup:
    free(copy);
    return r;
}

int ty_stat(const char *path, ty_file_info *info, bool follow_symlink)
{
#ifdef _WIN32
    TY_UNUSED(follow_symlink);
#endif

    assert(path && path[0]);
    assert(info);

    struct stat st;
    int r;

#ifdef _WIN32
    r = stat(path, &st);
#else
    if (follow_symlink) {
        r = stat(path, &st);
    } else {
        r = lstat(path, &st);
    }
#endif
    if (r < 0) {
        switch (errno) {
        case EACCES:
            return ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", path);
        case EIO:
            return ty_error(TY_ERROR_IO, "I/O error while stating '%s'", path);
        case ENOENT:
            return ty_error(TY_ERROR_NOT_FOUND, "Path '%s' does not exist", path);
        case ENOTDIR:
            return ty_error(TY_ERROR_NOT_FOUND, "Part of '%s' is not a directory", path);
        }
        return ty_error(TY_ERROR_SYSTEM, "Failed to stat '%s': %s", path, strerror(errno));
    }

    if (S_ISDIR(st.st_mode)) {
        info->type = TY_FILE_DIRECTORY;
    } else if (S_ISREG(st.st_mode)) {
        info->type = TY_FILE_REGULAR;
#ifdef S_ISLNK
    } else if (S_ISLNK(st.st_mode)) {
        info->type = TY_FILE_LINK;
#endif
    } else {
        info->type = TY_FILE_SPECIAL;
    }

    info->size = st.st_size;
#ifdef st_mtime
    info->mtime = (uint64_t)st.st_mtim.tv_sec * 1000 + (uint64_t)st.st_mtim.tv_nsec / 1000000;
#else
    info->mtime = (uint64_t)st.st_mtime * 1000;
#endif

#ifndef _WIN32
    info->dev = st.st_dev;
    info->ino = st.st_ino;
#endif

    return 0;
}
