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
#include "ty/ini.h"

struct ty_ini {
    FILE *fp;
    char *path;

    char *buf;
    size_t size;
    size_t line;

    char *ptr;

    char *section;
};

static inline bool test_eol(int c)
{
    return !c || c == '\r' || c == '\n';
}

static inline bool test_blank(int c)
{
    return c == ' ' || c == '\t';
}

static int parse_error(ty_ini *ini, const char *expected)
{
    return ty_error(TY_ERROR_PARSE, "Parse error (INI) on line %zu in '%s', expected %s",
                    ini->line, ini->path, expected);
}

static int fill_buffer(ty_ini *ini)
{
    ssize_t r;

    if (feof(ini->fp))
        return 0;

    r = getline(&ini->buf, &ini->size, ini->fp);
    if (r < 0) {
        if (errno == ENOMEM)
            return ty_error(TY_ERROR_MEMORY, NULL);

        if (ferror(ini->fp)) {
            if (errno == EIO)
                return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", ini->path);
            return ty_error(TY_ERROR_SYSTEM, "getline('%s') failed: %s", ini->path,
                            strerror(errno));
        } else {
            return 0;
        }
    }

    ini->ptr = ini->buf;
    ini->line++;

    return 1;
}

static int read_section(ty_ini *ini)
{
    char *section, *end;

    // We need to skip the '['
    section = ini->ptr + 1;
    while (*section && test_blank(*section))
        section++;

    end = strchr(section, ']');
    if (!end)
        return parse_error(ini, "']'");
    if (!test_eol(end[1]))
        return parse_error(ini, "end of line");
    while (end > section && test_blank(end[-1]))
        end--;
    if (end == section)
        return parse_error(ini, "[section]");
    *end = 0;

    free(ini->section);

    ini->section = strdup(section);
    if (!ini->section)
        return ty_error(TY_ERROR_MEMORY, NULL);

    return 0;
}

static int read_value(ty_ini *ini, const char **rsection, char **rkey, char **rvalue)
{
    char *key, *value, *end;

    // We've already skipped spaces
    key = ini->ptr;

    end = strchr(ini->ptr, '=');
    if (!end)
        return parse_error(ini, "key = value");
    ini->ptr = end + 1;

    while (end > key && test_blank(end[-1]))
        end--;
    if (end == key)
        return parse_error(ini, "key = value");
    *end = 0;

    value = ini->ptr;
    while (*value && test_blank(*value))
        value++;

    end = value + strlen(value);
    while (end > value && (test_eol(end[-1]) || test_blank(end[-1])))
        end--;
    *end = 0;

    *rsection = ini->section;
    *rkey = key;
    *rvalue = value;
    return 1;
}

int ty_ini_open(const char* path, ty_ini **rini)
{
    assert(path);

    ty_ini *ini;
    int r;

    ini = calloc(1, sizeof(*ini));
    if (!ini)
        return ty_error(TY_ERROR_MEMORY, NULL);

    ini->path = strdup(path);
    if (!ini->path) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

#ifdef _WIN32
    ini->fp = fopen(path, "rb");
#else
    ini->fp = fopen(path, "rbe");
#endif
    if (!ini->fp) {
        switch (errno) {
        case EACCES:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", path);
            break;
        case EIO:
            r = ty_error(TY_ERROR_IO, "I/O error while opening '%s' for reading", path);
            break;
        case ENOENT:
        case ENOTDIR:
            r = ty_error(TY_ERROR_NOT_FOUND, "File '%s' does not exist", path);
            break;

        default:
            r = ty_error(TY_ERROR_SYSTEM, "fopen('%s') failed: %s", path, strerror(errno));
            break;
        }
        goto error;
    }

    ini->line = 0;

    *rini = ini;
    return 0;

error:
    ty_ini_free(ini);
    return r;
}

void ty_ini_free(ty_ini *ini)
{
    if (ini) {
        if (ini->fp)
            fclose(ini->fp);

        free(ini->buf);
        free(ini->path);

        free(ini->section);
    }

    free(ini);
}

int ty_ini_next(ty_ini *ini, const char **rsection, char **rkey, char **rvalue)
{
    assert(ini);
    assert(rsection);
    assert(rkey);
    assert(rvalue);

    int r;

    do {
        r = fill_buffer(ini);
        if (r <= 0)
            return r;

        while (*ini->ptr && test_blank(*ini->ptr))
            ini->ptr++;

        if (test_eol(*ini->ptr) || *ini->ptr == '#' || *ini->ptr == ';') {
            // Just skip this line
            r = 0;
        } else if (*ini->ptr == '[') {
            r = read_section(ini);
        } else {
            r = read_value(ini, rsection, rkey, rvalue);
        }
    } while (!r);

    return r;
}

int ty_ini_walk(const char *path, ty_ini_callback_func *f, void *udata)
{
    assert(path);
    assert(f);

    ty_ini *ini;
    const char *section;
    char *key, *value;
    int r;

    r = ty_ini_open(path, &ini);
    if (r < 0)
        return r;

    do {
        r = ty_ini_next(ini, &section, &key, &value);
        if (r <= 0)
            break;

        r = (*f)(ini, section, key, value, udata);
    } while (!r);

    ty_ini_free(ini);

    return r;
}
