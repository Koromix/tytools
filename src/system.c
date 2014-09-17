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
#include "compat.h"
#include <sys/stat.h>
#include <unistd.h>
#include "system.h"

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

void ty_descriptor_set_clear(ty_descriptor_set *set)
{
    assert(set);

    set->count = 0;
}

void ty_descriptor_set_add(ty_descriptor_set *set, ty_descriptor desc, int id)
{
    assert(set);
    assert(set->count < TY_COUNTOF(set->desc));
#ifdef _WIN32
    assert(desc);
#else
    assert(desc >= 0);
#endif

    set->desc[set->count] = desc;
    set->id[set->count] = id;

    set->count++;
}

void ty_descriptor_set_remove(ty_descriptor_set *set, int id)
{
    assert(set);

    size_t delta = 0;
    for (size_t i = 0; i < set->count; i++) {
        set->desc[i - delta] = set->desc[i];
        set->id[i - delta] = set->id[i];

        if (set->id[i] == id)
            delta++;
    }

    set->count -= delta;
}
