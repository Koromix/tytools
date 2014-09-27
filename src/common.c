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
#include <stdarg.h>

static void default_handler(ty_err err, const char *msg, void *udata);

static ty_error_func *handler = default_handler;
static void *handler_udata = NULL;

static ty_err mask[32];
static size_t mask_count = 0;

static const char *generic_message(int err)
{
    if (err >= 0)
        return "Success";

    switch ((ty_err)err) {
    case TY_ERROR_MEMORY:
        return "Memory error";
    case TY_ERROR_PARAM:
        return "Incorrect parameter";
    case TY_ERROR_UNSUPPORTED:
        return "Option not supported";
    case TY_ERROR_NOT_FOUND:
        return "Not found";
    case TY_ERROR_EXISTS:
        return "Already exists";
    case TY_ERROR_ACCESS:
        return "Permission error";
    case TY_ERROR_IO:
        return "I/O error";
    case TY_ERROR_MODE:
        return "Wrong mode";
    case TY_ERROR_RANGE:
        return "Out of range error";
    case TY_ERROR_SYSTEM:
        return "System error";
    case TY_ERROR_PARSE:
        return "Parse error";
    case TY_ERROR_FIRMWARE:
        return "Firmware error";

    case TY_ERROR_OTHER:
        break;
    }

    return "Unknown error";
}

static void default_handler(ty_err err, const char *msg, void *udata)
{
    TY_UNUSED(err);
    TY_UNUSED(udata);

    fputs(msg, stderr);
    fputc('\n', stderr);
}

void ty_error_redirect(ty_error_func *f, void *udata)
{
    handler = f;
    handler_udata = udata;
}

void ty_error_mask(ty_err err)
{
    assert(mask_count < TY_COUNTOF(mask));

    mask[mask_count++] = err;
}

void ty_error_unmask(void)
{
    assert(mask_count);

    mask_count--;
}

int ty_error(ty_err err, const char *fmt, ...)
{
    va_list ap;
    char buf[512];

    va_start(ap, fmt);

    for (size_t i = 0; i < mask_count; i++) {
        if (mask[i] == err)
            goto cleanup;
    }

    if (fmt) {
        vsnprintf(buf, sizeof(buf), fmt, ap);
    } else {
        strncpy(buf, generic_message(err), sizeof(buf));
        buf[sizeof(buf) - 1] = 0;
    }

    (*handler)(err, buf, handler_udata);

cleanup:
    va_end(ap);
    return err;
}
