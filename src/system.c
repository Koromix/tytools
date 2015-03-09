/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "ty/system.h"

#ifdef HAVE_FSTATAT
int _ty_statat(int fd, const char *path, ty_file_info *info, bool follow);
#endif

int ty_adjust_timeout(int timeout, uint64_t start)
{
    if (timeout < 0)
        return -1;

    uint64_t now = ty_millis();

    if (now > start + (uint64_t)timeout)
        return 0;
    return (int)(start + (uint64_t)timeout - now);
}

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

    size_t count = 0;
    for (size_t i = 0; i < set->count; i++) {
        if (set->id[i] != id) {
            set->desc[count] = set->desc[i];
            set->id[count] = set->id[i];

            count++;
        }
    }

    set->count = count;
}
