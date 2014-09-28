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
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "ty/system.h"

#ifdef __unix__
int _ty_statat(int fd, const char *path, ty_file_info *info, bool follow);
#endif

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

int make_directory(const char *path, mode_t mode, bool permissive)
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
            if (permissive)
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

    return 0;
}

int ty_mkdir(const char *path, mode_t mode, uint16_t flags)
{
    assert(path && path[0]);

    char *parent = NULL;
    int r;

    if (flags & TY_MKDIR_PARENTS) {
        r = ty_path_split(path, &parent, NULL);
        if (r < 0)
            return r;

        char *ptr = parent;
        do {
            ptr = strpbrk(ptr + 1, TY_PATH_SEPARATORS);

            if (ptr) {
                *ptr = 0;
                r = make_directory(parent, mode, true);
                *ptr = *TY_PATH_SEPARATORS;
            } else {
                r = make_directory(parent, mode, true);
            }
            if (r < 0)
                goto cleanup;
        } while (ptr);
    }

    r = make_directory(path, mode, flags & TY_MKDIR_PERMISSIVE);
cleanup:
    free(parent);
    return r;
}

struct _ty_walk_context {
    ty_walk_func *f;
    void *udata;

    uint32_t flags;
};

int ty_walk(const char *path, ty_walk_history *history, ty_walk_func *f, void *udata, uint32_t flags)
{
    assert(path);
    assert(f || history);

    struct _ty_walk_context *ctx = NULL;
    ty_walk_history newhistory;
    DIR *dp = NULL;
#ifdef __unix__
    int fd;
#endif
    char *filename = NULL;
    int r;

    if (!history) {
        // No, it's not evil
        ctx = alloca(sizeof(*ctx));

        ctx->f = f;
        ctx->udata = udata;
        ctx->flags = flags;

        history = alloca(sizeof(*history));
        history->prev = NULL;

        history->relative = strlen(path) + 1;
        history->level = 0;

        r = ty_stat(path, &history->info, true);
        if (r < 0)
            goto cleanup;
    } else {
        if (history->info.type != TY_FILE_DIRECTORY) {
            r = 0;
            goto cleanup;
        }

        ctx = history->ctx;
    }

    newhistory.prev = history;
    newhistory.ctx = ctx;

    newhistory.relative = history->relative;
    newhistory.base = strlen(path) + 1;
    newhistory.level = history->level + 1;

    dp = opendir(path);
    if (!dp) {
        switch (errno) {
        case ENOMEM:
            r = ty_error(TY_ERROR_MEMORY, NULL);
            break;
        case EACCES:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", path);
            break;
        case ENOENT:
            if (!history->prev) {
                r = ty_error(TY_ERROR_NOT_FOUND, "Directory '%s' does not exist", path);
            } else {
                r = 0;
            }
            break;
        case ENOTDIR:
            if (!history->prev) {
                r = ty_error(TY_ERROR_NOT_FOUND, "Part of '%s' is not a directory", path);
            } else {
                r = 0;
            }
            break;
        default:
            r = ty_error(TY_ERROR_SYSTEM, "opendir('%s') failed: %s", path, strerror(errno));
            break;
        }
        goto cleanup;
    }
#ifdef __unix__
    fd = dirfd(dp);
#endif

    struct dirent *ent;
    while ((ent = readdir(dp))) {
        // Redundant with the TY_FILE_HIDDEN check further down, but this avoids many useless stats
        if (!(ctx->flags & TY_WALK_HIDDEN) && ent->d_name[0] == '.')
            continue;

        // Don't follow '.' and '..'
        if (ent->d_name[0] == '.' && (!ent->d_name[1] || (ent->d_name[1] == '.' && !ent->d_name[2])))
            continue;

        r = asprintf(&filename, "%s%c%s", path, *TY_PATH_SEPARATORS, ent->d_name);
        if (r < 0) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto cleanup;
        }

        ty_error_mask(TY_ERROR_NOT_FOUND);
#ifdef __unix__
        r = _ty_statat(fd, ent->d_name, &newhistory.info, ctx->flags & TY_WALK_FOLLOW);
#else
        r = ty_stat(filename, &newhistory.info, ctx->flags & TY_WALK_FOLLOW);
#endif
        ty_error_unmask();
        if (r < 0) {
            if (r == TY_ERROR_NOT_FOUND || r == TY_ERROR_ACCESS)
                goto next;
            goto cleanup;
        }

        if (!(ctx->flags & TY_WALK_HIDDEN) && newhistory.info.flags & TY_FILE_HIDDEN)
            continue;

        if (newhistory.info.type == TY_FILE_DIRECTORY) {
            for (ty_walk_history *cur = history; cur; cur = cur->prev) {
                if (ty_file_unique(&cur->info, &newhistory.info))
                    goto next;
            }
        }

        r = (*ctx->f)(filename, &newhistory, ctx->udata);
        if (r)
            goto cleanup;

next:
        free(filename);
        filename = NULL;
    }

    r = 0;
cleanup:
    free(filename);
    closedir(dp);
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

    size_t delta = 0;
    for (size_t i = 0; i < set->count; i++) {
        set->desc[i - delta] = set->desc[i];
        set->id[i - delta] = set->id[i];

        if (set->id[i] == id)
            delta++;
    }

    set->count -= delta;
}
