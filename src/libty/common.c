/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "common_priv.h"
#include <stdarg.h>
#include "../libhs/common.h"
#include "system.h"
#include "version.h"
#include "task.h"

int ty_config_verbosity = TY_LOG_INFO;

static ty_message_func *handler = ty_message_default_handler;
static void *handler_udata = NULL;

static TY_THREAD_LOCAL ty_err mask[16];
static TY_THREAD_LOCAL unsigned int mask_count;

static TY_THREAD_LOCAL char last_error_msg[512];

const char *ty_version_string(void)
{
    return TY_VERSION;
}

static bool log_level_is_enabled(ty_log_level level)
{
    static bool init, debug;

    if (!init) {
        debug = getenv("TY_DEBUG");
        init = true;
    }

    return ty_config_verbosity >= (int)level || debug;
}

static void print_log(const ty_message_data *msg)
{
    if (!log_level_is_enabled(msg->u.log.level))
        return;

    if (msg->u.log.level == TY_LOG_INFO) {
        if (msg->ctx)
            printf("%28s  ", msg->ctx);
        printf("%s\n", msg->u.log.msg);
        fflush(stdout);
    } else {
        if (msg->ctx)
            fprintf(stderr, "%28s  ", msg->ctx);
        fprintf(stderr, "%s\n", msg->u.log.msg);
    }
}

static void print_progress(const ty_message_data *msg)
{
    static bool init = false, show_progress;

    if (!log_level_is_enabled(TY_LOG_INFO))
        return;

    if (!init) {
        show_progress = ty_standard_get_modes(TY_STANDARD_OUTPUT) & TY_DESCRIPTOR_MODE_TERMINAL;
        init = true;
    }

    if (show_progress) {
        if (msg->ctx)
            printf("%28s  ", msg->ctx);
        printf("%s... %"PRIu64"%%%c", msg->u.progress.action,
               100 * msg->u.progress.value / msg->u.progress.max,
               msg->u.progress.value < msg->u.progress.max ? '\r' : '\n');
        fflush(stdout);
    } else if (!msg->u.progress.value) {
        if (msg->ctx)
            printf("%28s  ", msg->ctx);
        printf("%s...\n", msg->u.progress.action);
    }
    fflush(stdout);
}

void ty_message_default_handler(const ty_message_data *msg, void *udata)
{
    TY_UNUSED(udata);

    switch (msg->type) {
    case TY_MESSAGE_LOG:
        print_log(msg);
        break;
    case TY_MESSAGE_PROGRESS:
        print_progress(msg);
        break;
    case TY_MESSAGE_STATUS:
        break;
    }
}

void ty_message_redirect(ty_message_func *f, void *udata)
{
    assert(f);
    assert(f != ty_message_default_handler || !udata);

    handler = f;
    handler_udata = udata;
}

void ty_log(ty_log_level level, const char *fmt, ...)
{
    assert(fmt);

    va_list ap;
    char buf[sizeof(last_error_msg)];
    ty_message_data msg = {0};

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    msg.type = TY_MESSAGE_LOG;
    msg.u.log.level = level;
    msg.u.log.msg = buf;

    ty_message(&msg);
}

static const char *generic_error(int err)
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
    case TY_ERROR_BUSY:
        return "Busy error";
    case TY_ERROR_IO:
        return "I/O error";
    case TY_ERROR_TIMEOUT:
        return "Timeout error";
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

bool ty_error_is_masked(int err)
{
    if (err >= 0)
        return false;

    for (unsigned int i = 0; i < mask_count; i++) {
        if (mask[i] == err)
            return true;
    }

    return false;
}

const char *ty_error_last_message(void)
{
    return last_error_msg;
}

int ty_error(ty_err err, const char *fmt, ...)
{
    va_list ap;
    char buf[sizeof(last_error_msg)];
    ty_message_data msg = {0};

    /* Don't copy directly to last_error_message because we need to support
       ty_error(err, "%s", ty_error_last_message()). */
    if (fmt) {
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
    } else {
        strncpy(buf, generic_error(err), sizeof(buf));
        buf[sizeof(buf) - 1] = 0;
    }
    strcpy(last_error_msg, buf);

    if (!ty_error_is_masked(err)) {
        msg.type = TY_MESSAGE_LOG;
        msg.u.log.level = TY_LOG_ERROR;
        msg.u.log.err = err;
        msg.u.log.msg = buf;

        ty_message(&msg);
    }

    return err;
}

void ty_progress(const char *action, uint64_t value, uint64_t max)
{
    assert(value <= max);
    assert(max);

    ty_message_data msg = {0};

    msg.type = TY_MESSAGE_PROGRESS;
    msg.u.progress.action = action ? action : "Processing";
    msg.u.progress.value = value;
    msg.u.progress.max = max;

    ty_message(&msg);
}

void ty_message(ty_message_data *msg)
{
    ty_task *task = msg->task;
    if (!task) {
        task = ty_task_get_current();
        msg->task = task;
    }
    if (!msg->ctx && task)
        msg->ctx = task->name;

    (*handler)(msg, handler_udata);
    if (task && task->user_callback)
        (*task->user_callback)(msg, task->user_callback_udata);
}

int ty_libhs_translate_error(int err)
{
    if (err >= 0)
        return err;

    switch ((hs_error_code)err) {
    case HS_ERROR_MEMORY:
        return TY_ERROR_MEMORY;
    case HS_ERROR_NOT_FOUND:
        return TY_ERROR_NOT_FOUND;
    case HS_ERROR_ACCESS:
        return TY_ERROR_ACCESS;
    case HS_ERROR_IO:
        return TY_ERROR_IO;
    case HS_ERROR_SYSTEM:
        return TY_ERROR_SYSTEM;
    }

    assert(false);
    return TY_ERROR_OTHER;
}

void ty_libhs_log_handler(hs_log_level level, int err, const char *log, void *udata)
{
    TY_UNUSED(udata);

    ty_message_data msg = {0};

    msg.type = TY_MESSAGE_LOG;
    switch (level) {
    case HS_LOG_DEBUG:
        msg.u.log.level = TY_LOG_DEBUG;
        break;
    case HS_LOG_WARNING:
        msg.u.log.level = TY_LOG_WARNING;
        break;
    case HS_LOG_ERROR:
        msg.u.log.level = TY_LOG_ERROR;
        msg.u.log.err = ty_libhs_translate_error(err);
        strncpy(last_error_msg, log, sizeof(last_error_msg));
        last_error_msg[sizeof(last_error_msg) - 1] = 0;
        if (ty_error_is_masked(msg.u.log.err))
            return;
        break;
    }
    msg.u.log.msg = log;

    ty_message(&msg);
}

void _ty_refcount_increase(unsigned int *rrefcount)
{
#ifdef _MSC_VER
    InterlockedIncrement(rrefcount);
#else
    __atomic_add_fetch(rrefcount, 1, __ATOMIC_RELAXED);
#endif
}

unsigned int _ty_refcount_decrease(unsigned int *rrefcount)
{
#ifdef _MSC_VER
    return InterlockedDecrement(rrefcount);
#else
    unsigned int refcount = __atomic_sub_fetch(rrefcount, 1, __ATOMIC_RELEASE);
    if (refcount)
        return refcount;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    return 0;
#endif
}
