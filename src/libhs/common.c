/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "common_priv.h"
#include <stdarg.h>

static hs_log_handler_func *log_handler = hs_log_default_handler;
static void *log_handler_udata;

static _HS_THREAD_LOCAL hs_error_code error_masks[32];
static _HS_THREAD_LOCAL unsigned int error_masks_count;

static _HS_THREAD_LOCAL char last_error_msg[512];

uint32_t hs_version(void)
{
    return HS_VERSION;
}

const char *hs_version_string(void)
{
    return HS_VERSION_STRING;
}

static const char *generic_message(int err)
{
    if (err >= 0)
        return "Success";

    switch ((hs_error_code)err) {
    case HS_ERROR_MEMORY:
        return "Memory error";
    case HS_ERROR_NOT_FOUND:
        return "Not found";
    case HS_ERROR_ACCESS:
        return "Permission error";
    case HS_ERROR_IO:
        return "I/O error";
    case HS_ERROR_SYSTEM:
        return "System error";
    }

    return "Unknown error";
}

void hs_log_set_handler(hs_log_handler_func *f, void *udata)
{
    assert(f);
    assert(f != hs_log_default_handler || !udata);

    log_handler = f;
    log_handler_udata = udata;
}

void hs_log_default_handler(hs_log_level level, int err, const char *msg, void *udata)
{
    _HS_UNUSED(err);
    _HS_UNUSED(udata);

    if (level == HS_LOG_DEBUG && !getenv("LIBHS_DEBUG"))
        return;

    fputs(msg, stderr);
    fputc('\n', stderr);
}

void hs_error_mask(hs_error_code err)
{
    assert(error_masks_count < _HS_COUNTOF(error_masks));

    error_masks[error_masks_count++] = err;
}

void hs_error_unmask(void)
{
    assert(error_masks_count);

    error_masks_count--;
}

int hs_error_is_masked(int err)
{
    if (err >= 0)
        return 0;

    for (unsigned int i = 0; i < error_masks_count; i++) {
        if (error_masks[i] == err)
            return 1;
    }

    return 0;
}

const char *hs_error_last_message()
{
    return last_error_msg;
}

void hs_log(hs_log_level level, const char *fmt, ...)
{
    assert(fmt);

    va_list ap;
    char buf[sizeof(last_error_msg)];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    (*log_handler)(level, 0, buf, log_handler_udata);
}

int hs_error(hs_error_code err, const char *fmt, ...)
{
    va_list ap;
    char buf[sizeof(last_error_msg)];

    /* Don't copy directly to last_error_message because we need to support
       ty_error(err, "%s", ty_error_last_message()). */
    if (fmt) {
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
    } else {
        strncpy(buf, generic_message(err), sizeof(buf));
        buf[sizeof(buf) - 1] = 0;
    }

    strcpy(last_error_msg, buf);
    if (!hs_error_is_masked(err))
        (*log_handler)(HS_LOG_ERROR, err, buf, log_handler_udata);

    return err;
}
