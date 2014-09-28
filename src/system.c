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

#include "ty/common.h"
#include "compat.h"
#include <unistd.h>
#include "ty/system.h"

bool ty_path_is_absolute(const char *path)
{
    assert(path);

#ifdef _WIN32
    if (((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) && path[1] == ':')
        path += 2;
#endif

    return strchr(TY_PATH_SEPARATORS, path[0]);
}

int ty_path_split(const char *path, char **rdirectory, char **rname)
{
    assert(path && path[0]);
    assert(rdirectory || rname);

    const char *path_;
    const char *end, *base;
    char *directory = NULL, *name = NULL;
    int r;

    path_ = path;
#ifdef _WIN32
    if (((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) && path[1] == ':')
        path_ += 2;
#endif

    end = path_ + strlen(path_);
    while (end > path_ + 1 && strchr(TY_PATH_SEPARATORS, end[-1]))
        end--;

    base = end;
    while (base > path_ && !strchr(TY_PATH_SEPARATORS, base[-1]))
        base--;

    if (rname) {
        name = strndup(base, (size_t)(end - base));
        if (!name) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto error;
        }
    }

    if (rdirectory) {
        if (base > path) {
            while (base > path_ + 1 && strchr(TY_PATH_SEPARATORS, base[-1]))
                base--;

            directory = strndup(path, (size_t)(base - path));
        } else {
            directory = strdup(".");
        }
        if (!directory) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto error;
        }

        *rdirectory = directory;
    }

    if (rname)
        *rname = name;

    return 0;

error:
    free(name);
    free(directory);
    return r;
}

const char *ty_path_ext(const char *path)
{
    assert(path);

    const char *ext = strrchr(path, '.');
    if (!ext)
        return "";

    if (strpbrk(ext, TY_PATH_SEPARATORS))
        return "";

    return ext;
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
