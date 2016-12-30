/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "util.h"
#ifndef _WIN32
    #include <sys/stat.h>
#endif
#include "hs/device.h"
#include "board_priv.h"
#include "ty/firmware.h"
#include "model_priv.h"
#include "ty/monitor.h"
#include "ty/system.h"
#include "task_priv.h"
#include "ty/timer.h"

struct ty_task {
    TY_TASK

    ty_board *board;
    union {
        struct {
            ty_firmware **fws;
            unsigned int fws_count;
            int flags;
        } upload;

        struct {
            char *buf;
            size_t size;
        } send;

        struct {
            FILE *fp;
            size_t size;
            char *filename;
        } send_file;
    };
};

static const char *capability_names[] = {
    "unique",
    "run",
    "upload",
    "reset",
    "reboot",
    "serial"
};

#ifdef _WIN32
    #define MANUAL_REBOOT_DELAY 15000
#else
    #define MANUAL_REBOOT_DELAY 8000
#endif
#define FINAL_TASK_TIMEOUT 8000

const char *ty_board_capability_get_name(ty_board_capability cap)
{
    assert((int)cap >= 0 && (int)cap < TY_BOARD_CAPABILITY_COUNT);
    return capability_names[cap];
}

ty_board *ty_board_ref(ty_board *board)
{
    assert(board);

    _ty_refcount_increase(&board->refcount);
    return board;
}

void ty_board_unref(ty_board *board)
{
    if (board) {
        if (_ty_refcount_decrease(&board->refcount))
            return;

        if (board->tag != board->id)
            free(board->tag);
        free(board->id);
        free(board->location);
        free(board->description);

        ty_mutex_release(&board->interfaces_lock);

        ty_list_foreach(cur, &board->interfaces) {
            ty_board_interface *iface = ty_container_of(cur, ty_board_interface, board_node);

            ty_list_remove(&iface->board_node);
            ty_board_interface_unref(iface);
        }
    }

    free(board);
}

static int match_interface(ty_board_interface *iface, void *udata)
{
    return ty_compare_paths(ty_board_interface_get_path(iface), udata);
}

bool ty_board_matches_tag(ty_board *board, const char *id)
{
    assert(board);

    uint64_t serial;
    char *ptr, *family = NULL, *location = NULL;

    if (!id)
        return true;
    if (board->tag != board->id && strcmp(id, board->tag) == 0)
        return true;

    serial = strtoull(id, &ptr, 10);
    if (*ptr == '-') {
        location = strchr(++ptr, '@');
        if (location > ptr) {
            size_t len = (size_t)(location - ptr);
            if (len > 32)
                len = 32;
            family = alloca(len + 1);
            memcpy(family, ptr, len);
            family[len] = 0;
        } else if (!location && ptr[1]) {
            family = ptr;
        }
        if (location && !*++location)
            location = NULL;
    } else if (*ptr == '@') {
        if (ptr[1])
            location = ptr + 1;
    } else if (*ptr) {
        return false;
    }

    if (serial && serial != board->serial)
        return false;
    if (family && strcmp(family, strchr(board->id, '-') + 1) != 0)
        return false;
    if (location && strcmp(location, board->location) != 0 &&
            !ty_board_list_interfaces(board, match_interface, location))
        return false;

    return true;
}

void ty_board_set_udata(ty_board *board, void *udata)
{
    assert(board);
    board->udata = udata;
}

void *ty_board_get_udata(const ty_board *board)
{
    assert(board);
    return board->udata;
}

ty_monitor *ty_board_get_monitor(const ty_board *board)
{
    assert(board);
    return board->monitor;
}

ty_board_state ty_board_get_state(const ty_board *board)
{
    assert(board);
    return board->state;
}

const char *ty_board_get_id(const ty_board *board)
{
    assert(board);
    return board->id;
}

int ty_board_set_tag(ty_board *board, const char *tag)
{
    assert(board);

    char *new_tag;

    if (tag) {
        new_tag = strdup(tag);
        if (!new_tag)
            return ty_error(TY_ERROR_MEMORY, NULL);
    } else {
        new_tag = board->id;
    }

    if (board->tag != board->id)
        free(board->tag);
    board->tag = new_tag;

    return 0;
}

const char *ty_board_get_tag(const ty_board *board)
{
    assert(board);
    return board->tag;
}

const char *ty_board_get_location(const ty_board *board)
{
    assert(board);
    return board->location;
}

uint64_t ty_board_get_serial_number(const ty_board *board)
{
    assert(board);
    return board->serial;
}

const char *ty_board_get_description(const ty_board *board)
{
    assert(board);
    return board->description;
}

void ty_board_set_model(ty_board *board, ty_model model)
{
    assert(board);
    assert(board->model);

    if (board->model && ty_models[board->model].code_size && board->model != model) {
        ty_log(TY_LOG_WARNING, "Cannot set model '%s' for incompatible board '%s'",
               ty_models[model].name, board->tag);
        return;
    }

    board->model = model;
}

ty_model ty_board_get_model(const ty_board *board)
{
    assert(board);
    return board->model;
}

int ty_board_get_capabilities(const ty_board *board)
{
    assert(board);
    return board->capabilities;
}

int ty_board_list_interfaces(ty_board *board, ty_board_list_interfaces_func *f, void *udata)
{
    assert(board);
    assert(f);

    int r;

    ty_mutex_lock(&board->interfaces_lock);

    r = 0;
    ty_list_foreach(cur, &board->interfaces) {
        ty_board_interface *iface = ty_container_of(cur, ty_board_interface, board_node);

        r = (*f)(iface, udata);
        if (r)
            break;
    }

    ty_mutex_unlock(&board->interfaces_lock);
    return r;
}

int ty_board_open_interface(ty_board *board, ty_board_capability cap, ty_board_interface **riface)
{
    assert(board);
    assert((int)cap < (int)TY_COUNTOF(board->cap2iface));
    assert(riface);

    ty_board_interface *iface;
    int r;

    ty_mutex_lock(&board->interfaces_lock);

    iface = board->cap2iface[cap];
    if (!iface) {
        r = 0;
        goto cleanup;
    }

    r = ty_board_interface_open(iface);
    if (r < 0)
        goto cleanup;

    *riface = iface;
    r = 1;

cleanup:
    ty_mutex_unlock(&board->interfaces_lock);
    return r;
}

struct wait_for_context {
    ty_board *board;
    ty_board_capability capability;
};

static int wait_for_callback(ty_monitor *monitor, void *udata)
{
    TY_UNUSED(monitor);

    struct wait_for_context *ctx = udata;
    ty_board *board = ctx->board;

    if (board->state == TY_BOARD_STATE_DROPPED)
        return ty_error(TY_ERROR_NOT_FOUND, "Board '%s' has disappeared", board->tag);

    return ty_board_has_capability(board, ctx->capability);
}

// TODO: this function probably belongs to the monitor API
int ty_board_wait_for(ty_board *board, ty_board_capability capability, int timeout)
{
    assert(board);

    ty_monitor *monitor = board->monitor;
    struct wait_for_context ctx;

    if (board->state == TY_BOARD_STATE_DROPPED)
        return ty_error(TY_ERROR_NOT_FOUND, "Board '%s' has disappeared", board->tag);
    if (!monitor)
        return ty_error(TY_ERROR_NOT_FOUND, "Cannot wait on unmonitored board '%s'", board->tag);

    ctx.board = board;
    ctx.capability = capability;

    return ty_monitor_wait(monitor, wait_for_callback, &ctx, timeout);
}

ssize_t ty_board_serial_read(ty_board *board, char *buf, size_t size, int timeout)
{
    assert(board);
    assert(buf);
    assert(size);

    ty_board_interface *iface;
    ssize_t r;

    r = ty_board_open_interface(board, TY_BOARD_CAPABILITY_SERIAL, &iface);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available for '%s", board->tag);

    r = (*iface->vtable->serial_read)(iface, buf, size, timeout);

    ty_board_interface_close(iface);
    return r;
}

ssize_t ty_board_serial_write(ty_board *board, const char *buf, size_t size)
{
    assert(board);
    assert(buf);

    ty_board_interface *iface;
    ssize_t r;

    r = ty_board_open_interface(board, TY_BOARD_CAPABILITY_SERIAL, &iface);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available for '%s", board->tag);

    r = (*iface->vtable->serial_write)(iface, buf, size);

    ty_board_interface_close(iface);
    return r;
}

int ty_board_upload(ty_board *board, ty_firmware *fw, ty_board_upload_progress_func *pf, void *udata)
{
    assert(board);
    assert(fw);

    ty_board_interface *iface = NULL;
    int r;

    r = ty_board_open_interface(board, TY_BOARD_CAPABILITY_UPLOAD, &iface);
    if (r < 0)
        goto cleanup;
    if (!r) {
        r = ty_error(TY_ERROR_MODE, "Firmware upload is not available for '%s", board->tag);
        goto cleanup;
    }
    assert(board->model);

    if (ty_firmware_get_size(fw) > ty_models[board->model].code_size) {
        r = ty_error(TY_ERROR_RANGE, "Firmware is too big for %s", ty_models[board->model].name);
        goto cleanup;
    }

    r = (*iface->vtable->upload)(iface, fw, pf, udata);

cleanup:
    ty_board_interface_close(iface);
    return r;
}

int ty_board_reset(ty_board *board)
{
    assert(board);

    ty_board_interface *iface;
    int r;

    r = ty_board_open_interface(board, TY_BOARD_CAPABILITY_RESET, &iface);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_MODE, "Cannot reset '%s' in this mode", board->tag);

    r = (*iface->vtable->reset)(iface);

    ty_board_interface_close(iface);
    return r;
}

int ty_board_reboot(ty_board *board)
{
    assert(board);

    ty_board_interface *iface;
    int r;

    r = ty_board_open_interface(board, TY_BOARD_CAPABILITY_REBOOT, &iface);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_MODE, "Cannot reboot '%s' in this mode", board->tag);

    r = (*iface->vtable->reboot)(iface);

    ty_board_interface_close(iface);
    return r;
}

ty_board_interface *ty_board_interface_ref(ty_board_interface *iface)
{
    assert(iface);

    _ty_refcount_increase(&iface->refcount);
    return iface;
}

void ty_board_interface_unref(ty_board_interface *iface)
{
    if (iface) {
        if (_ty_refcount_decrease(&iface->refcount))
            return;

        hs_handle_close(iface->h);
        hs_device_unref(iface->dev);

        ty_mutex_release(&iface->open_lock);
    }

    free(iface);
}

int ty_board_interface_open(ty_board_interface *iface)
{
    assert(iface);

    int r;

    ty_mutex_lock(&iface->open_lock);

    if (!iface->h) {
        r = (*iface->vtable->open_interface)(iface);
        if (r < 0)
            goto cleanup;
    }
    iface->open_count++;

    ty_board_interface_ref(iface);
    r = 0;

cleanup:
    ty_mutex_unlock(&iface->open_lock);
    return r;
}

void ty_board_interface_close(ty_board_interface *iface)
{
    if (!iface)
        return;

    ty_mutex_lock(&iface->open_lock);
    if (!--iface->open_count)
        (*iface->vtable->close_interface)(iface);
    ty_mutex_unlock(&iface->open_lock);

    ty_board_interface_unref(iface);
}

const char *ty_board_interface_get_name(const ty_board_interface *iface)
{
    assert(iface);
    return iface->name;
}

int ty_board_interface_get_capabilities(const ty_board_interface *iface)
{
    assert(iface);
    return iface->capabilities;
}

const char *ty_board_interface_get_path(const ty_board_interface *iface)
{
    assert(iface);
    return hs_device_get_path(iface->dev);
}

uint8_t ty_board_interface_get_interface_number(const ty_board_interface *iface)
{
    assert(iface);
    return hs_device_get_interface_number(iface->dev);
}

hs_device *ty_board_interface_get_device(const ty_board_interface *iface)
{
    assert(iface);
    return iface->dev;
}

hs_handle *ty_board_interface_get_handle(const ty_board_interface *iface)
{
    assert(iface);
    return iface->h;
}

void ty_board_interface_get_descriptors(const ty_board_interface *iface, struct ty_descriptor_set *set, int id)
{
    assert(iface);
    assert(set);

    if (iface->h)
        ty_descriptor_set_add(set, hs_handle_get_descriptor(iface->h), id);
}

static int new_task(ty_board *board, const char *action, const struct _ty_task_vtable *vtable,
                    ty_task **rtask)
{
    char task_name_buf[64];
    ty_task *task = NULL;
    int r;

    if (board->current_task)
        return ty_error(TY_ERROR_BUSY, "Board '%s' is busy on task '%s'", board->tag,
                        ty_task_get_name(board->current_task));

    snprintf(task_name_buf, sizeof(task_name_buf), "%s@%s", action, board->tag);
    r = _ty_task_new(task_name_buf, sizeof(*task), vtable, &task);
    if (r < 0)
        return r;

    board->current_task = task;
    task->board = ty_board_ref(board);

    *rtask = task;
    return 0;
}

static void cleanup_task(ty_task *task)
{
    task->board->current_task = NULL;
    ty_board_unref(task->board);
}

static int select_compatible_firmware(ty_board *board, ty_firmware **fws, unsigned int fws_count,
                                      ty_firmware **rfw)
{
    ty_model fw_models[64];
    unsigned int fw_models_count = 0;

    for (unsigned int i = 0; i < fws_count; i++) {
        fw_models_count = ty_firmware_identify(fws[i], fw_models, TY_COUNTOF(fw_models));

        for (unsigned int j = 0; j < fw_models_count; j++) {
            if (fw_models[j] == board->model) {
                *rfw = fws[i];
                return 0;
            }
        }
    }

    if (fws_count > 1) {
        return ty_error(TY_ERROR_FIRMWARE, "No firmware is compatible with '%s' (%s)",
                        board->tag, ty_models[board->model].name);
    } else if (fw_models_count) {
        char buf[256], *ptr;

        ptr = buf;
        for (unsigned int i = 0; i < fw_models_count && ptr < buf + sizeof(buf); i++)
            ptr += snprintf(ptr, (size_t)(buf + sizeof(buf) - ptr), "%s%s",
                            i ? (i + 1 < fw_models_count ? ", " : " and ") : "",
                            ty_models[fw_models[i]].name);

        return ty_error(TY_ERROR_FIRMWARE, "Firmware '%s' is only compatible with %s",
                        ty_firmware_get_name(fws[0]), buf);
    } else {
        return ty_error(TY_ERROR_FIRMWARE, "Firmware '%s' is not compatible with '%s'",
                        ty_firmware_get_name(fws[0]), board->tag);
    }
}

static int upload_progress_callback(const ty_board *board, const ty_firmware *fw,
                                    size_t uploaded, void *udata)
{
    TY_UNUSED(board);
    TY_UNUSED(udata);

    ty_progress("Uploading", uploaded, ty_firmware_get_size(fw));
    return 0;
}

static void unref_upload_firmware(void *ptr)
{
    ty_firmware_unref(ptr);
}

static int run_upload(ty_task *task)
{
    ty_board *board = task->board;
    ty_firmware *fw;
    size_t fw_size;
    int flags = task->upload.flags, r;

    if (flags & TY_UPLOAD_NOCHECK) {
        fw = task->upload.fws[0];
    } else if (ty_model_is_real(board->model)) {
        r = select_compatible_firmware(board, task->upload.fws, task->upload.fws_count, &fw);
        if (r < 0)
            return r;
    } else {
        // Maybe we can identify the board and test the firmwares in bootloader mode?
        fw = NULL;
    }

    ty_log(TY_LOG_INFO, "Uploading to board '%s' (%s)", board->tag, ty_models[board->model].name);

    // Can't upload directly, should we try to reboot or wait?
    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_UPLOAD)) {
        if (flags & TY_UPLOAD_WAIT) {
            ty_log(TY_LOG_INFO, "Waiting for device (press button to reboot)...");
        } else {
            ty_log(TY_LOG_INFO, "Triggering board reboot");
            r = ty_board_reboot(board);
            if (r < 0)
                return r;
        }
    }

wait:
    r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_UPLOAD,
                           flags & TY_UPLOAD_WAIT ? -1 : MANUAL_REBOOT_DELAY);
    if (r < 0)
        return r;
    if (!r) {
        ty_log(TY_LOG_INFO, "Reboot didn't work, press button manually");
        flags |= TY_UPLOAD_WAIT;

        goto wait;
    }

    if (!fw) {
        r = select_compatible_firmware(board, task->upload.fws, task->upload.fws_count, &fw);
        if (r < 0)
            return r;
    }

    ty_log(TY_LOG_INFO, "Firmware: %s", ty_firmware_get_name(fw));
    fw_size = ty_firmware_get_size(fw);
    if (fw_size >= 1024) {
        ty_log(TY_LOG_INFO, "Flash usage: %zu kiB (%.1f%%)",
               (fw_size + 1023) / 1024,
               (double)fw_size / (double)ty_models[board->model].code_size * 100.0);
    } else {
        ty_log(TY_LOG_INFO, "Flash usage: %zu bytes (%.1f%%)",
               fw_size,
               (double)fw_size / (double)ty_models[board->model].code_size * 100.0);
    }

    r = ty_board_upload(board, fw, upload_progress_callback, NULL);
    if (r < 0)
        return r;

    if (!(flags & TY_UPLOAD_NORESET)) {
        ty_log(TY_LOG_INFO, "Sending reset command");
        r = ty_board_reset(board);
        if (r < 0)
            return r;

        r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_RUN, FINAL_TASK_TIMEOUT);
        if (r < 0)
            return r;
        if (!r)
            return ty_error(TY_ERROR_TIMEOUT, "Failed to reset board '%s'", board->tag);
    } else {
        ty_log(TY_LOG_INFO, "Firmware uploaded, reset the board to use it");
    }

    _ty_task_set_result(task, ty_firmware_ref(fw), unref_upload_firmware);
    return 0;
}

static void cleanup_upload(ty_task *task)
{
    for (unsigned int i = 0; i < task->upload.fws_count; i++)
        ty_firmware_unref(task->upload.fws[i]);
    free(task->upload.fws);

    cleanup_task(task);
}

static const struct _ty_task_vtable upload_task_vtable = {
    .run = run_upload,
    .cleanup = cleanup_upload
};

int ty_upload(ty_board *board, ty_firmware **fws, unsigned int fws_count, int flags,
               ty_task **rtask)
{
    assert(board);
    assert(fws);
    assert(fws_count);
    assert(rtask);

    ty_task *task = NULL;
    int r;

    r = new_task(board, "upload", &upload_task_vtable, &task);
    if (r < 0)
        goto error;

    if (fws_count > TY_UPLOAD_MAX_FIRMWARES) {
        ty_log(TY_LOG_WARNING, "Cannot select more than %d firmwares per upload",
               TY_UPLOAD_MAX_FIRMWARES);
        fws_count = TY_UPLOAD_MAX_FIRMWARES;
    }
    if (flags & TY_UPLOAD_NOCHECK)
        fws_count = 1;

    task->upload.fws = malloc(fws_count * sizeof(ty_firmware *));
    if (!task->upload.fws) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    for (unsigned int i = 0; i < fws_count; i++)
        task->upload.fws[i] = ty_firmware_ref(fws[i]);
    task->upload.fws_count = fws_count;
    task->upload.flags = flags;

    *rtask = task;
    return 0;

error:
    ty_task_unref(task);
    return r;
}

static int run_reset(ty_task *task)
{
    ty_board *board = task->board;
    int r;

    ty_log(TY_LOG_INFO, "Resetting board '%s' (%s)", board->tag, ty_models[board->model].name);

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_RESET)) {
        ty_log(TY_LOG_INFO, "Triggering board reboot");
        r = ty_board_reboot(board);
        if (r < 0)
            return r;

        r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_RESET, MANUAL_REBOOT_DELAY);
        if (r <= 0)
            return ty_error(TY_ERROR_TIMEOUT, "Failed to reboot board '%s'", board->tag);
    }

    ty_log(TY_LOG_INFO, "Sending reset command");
    r = ty_board_reset(board);
    if (r < 0)
        return r;

    r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_RUN, FINAL_TASK_TIMEOUT);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_TIMEOUT, "Failed to reset board '%s'", board->tag);

    return 0;
}

static const struct _ty_task_vtable reset_task_vtable = {
    .run = run_reset,
    .cleanup = cleanup_task
};

int ty_reset(ty_board *board, ty_task **rtask)
{
    assert(board);
    assert(rtask);

    return new_task(board, "reset", &reset_task_vtable, rtask);
}

static int run_reboot(ty_task *task)
{
    ty_board *board = task->board;
    int r;

    ty_log(TY_LOG_INFO, "Rebooting board '%s' (%s)", board->tag, ty_models[board->model].name);

    if (ty_board_has_capability(board, TY_BOARD_CAPABILITY_UPLOAD)) {
        ty_log(TY_LOG_INFO, "Board is already in bootloader mode");
        return 0;
    }

    ty_log(TY_LOG_INFO, "Triggering board reboot");
    r = ty_board_reboot(board);
    if (r < 0)
        return r;

    r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_UPLOAD, FINAL_TASK_TIMEOUT);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_TIMEOUT, "Failed to reboot board '%s", board->tag);

    return 0;
}

static const struct _ty_task_vtable reboot_task_vtable = {
    .run = run_reboot,
    .cleanup = cleanup_task
};

int ty_reboot(ty_board *board, ty_task **rtask)
{
    assert(board);
    assert(rtask);

    return new_task(board, "reboot", &reboot_task_vtable, rtask);
}

static int run_send(ty_task *task)
{
    ty_board *board = task->board;
    const char *buf = task->send.buf;
    size_t size = task->send.size;
    size_t written;

    written = 0;
    while (written < size) {
        size_t block_size;
        ssize_t r;

        ty_progress("Sending", written, size);

        block_size = TY_MIN(1024, size - written);
        r = ty_board_serial_write(board, buf + written, block_size);
        if (r < 0)
            return (int)r;
        written += (size_t)r;
    }

    return 0;
}

static void cleanup_send(ty_task *task)
{
    free(task->send.buf);
    cleanup_task(task);
}

static const struct _ty_task_vtable send_task_vtable = {
    .run = run_send,
    .cleanup = cleanup_send
};

int ty_send(ty_board *board, const char *buf, size_t size, ty_task **rtask)
{
    assert(board);
    assert(buf);
    assert(rtask);

    ty_task *task = NULL;
    int r;

    r = new_task(board, "send", &send_task_vtable, &task);
    if (r < 0)
        goto error;

    task->send.buf = malloc(size);
    if (!task->send.buf) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    memcpy(task->send.buf, buf, size);
    task->send.size = size;

    *rtask = task;
    return 0;

error:
    ty_task_unref(task);
    return r;
}

static int run_send_file(ty_task *task)
{
    ty_board *board = task->board;
    FILE *fp = task->send_file.fp;
    size_t size = task->send_file.size;
    const char *filename = task->send_file.filename;
    size_t written;

    written = 0;
    while (written < size) {
        char buf[1024];
        size_t block_size;
        size_t block_written;

        ty_progress("Sending", written, size);

        block_size = fread(buf, 1, sizeof(buf), fp);
        if (!block_size) {
            if (feof(fp)) {
                break;
            } else {
                return ty_error(TY_ERROR_IO, "I/O error while reading '%s'", filename);
            }
        }

        block_written = 0;
        while (block_written < block_size) {
            ssize_t r = ty_board_serial_write(board, buf + block_written,
                                              block_size - block_written);
            if (r < 0)
                return (int)r;
            block_written += (size_t)r;
        }

        written += block_size;
    }
    ty_progress("Sending", size, size);

    return 0;
}

static void cleanup_send_file(ty_task *task)
{
    free(task->send_file.filename);
    if (task->send_file.fp)
        fclose(task->send_file.fp);
    cleanup_task(task);
}

static const struct _ty_task_vtable send_file_task_vtable = {
    .run = run_send_file,
    .cleanup = cleanup_send_file
};

int ty_send_file(ty_board *board, const char *filename, ty_task **rtask)
{
    assert(board);
    assert(filename);
    assert(rtask);

    ty_task *task = NULL;
    int r;

    r = new_task(board, "send", &send_file_task_vtable, &task);
    if (r < 0)
        goto error;

#ifdef _WIN32
    task->send_file.fp = fopen(filename, "rb");
#else
    task->send_file.fp = fopen(filename, "rbe");
#endif
    if (!task->send_file.fp) {
        switch (errno) {
        case EACCES:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", filename);
            break;
        case EIO:
            r = ty_error(TY_ERROR_IO, "I/O error while opening '%s' for reading", filename);
            break;
        case ENOENT:
        case ENOTDIR:
            r = ty_error(TY_ERROR_NOT_FOUND, "File '%s' does not exist", filename);
            break;

        default:
            r = ty_error(TY_ERROR_SYSTEM, "fopen('%s') failed: %s", filename, strerror(errno));
            break;
        }
        goto error;
    }

    fseek(task->send_file.fp, 0, SEEK_END);
#ifdef _WIN32
    task->send_file.size = (size_t)_ftelli64(task->send_file.fp);
#else
    task->send_file.size = (size_t)ftello(task->send_file.fp);
#endif
    rewind(task->send_file.fp);
    if (!task->send_file.size) {
        r = ty_error(TY_ERROR_UNSUPPORTED, "Failed to read size of '%s', is it a regular file?",
                     filename);
        goto error;
    }

    task->send_file.filename = strdup(filename);
    if (!task->send_file.filename) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    *rtask = task;
    return 0;

error:
    ty_task_unref(task);
    return r;
}
