/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "ty/common.h"
#include "compat.h"
#ifndef _WIN32
    #include <sys/stat.h>
#endif
#include "board_priv.h"
#include "ty/firmware.h"
#include "ty/monitor.h"
#include "ty/system.h"
#include "task_priv.h"
#include "ty/timer.h"

struct tyb_board_model {
    TYB_BOARD_MODEL
};

struct ty_task {
    TY_TASK

    tyb_board *board;
    union {
        struct {
            tyb_firmware **fws;
            unsigned int fws_count;
            int flags;
        } upload;
    };
};

extern const tyb_board_family _tyb_teensy_family;

const tyb_board_family *tyb_board_families[] = {
    &_tyb_teensy_family,
    NULL
};

static const char *capability_names[] = {
    "run",
    "upload",
    "reset",
    "reboot",
    "serial"
};

#ifdef _WIN32
    #define MANUAL_REBOOT_DELAY 15000
    #define FINAL_TASK_TIMEOUT 8000
#else
    #define MANUAL_REBOOT_DELAY 5000
    #define FINAL_TASK_TIMEOUT 5000
#endif

const char *tyb_board_family_get_name(const tyb_board_family *family)
{
    assert(family);
    return family->name;
}

int tyb_board_family_list_models(const tyb_board_family *family, tyb_board_family_list_models_func *f, void *udata)
{
    assert(family);
    assert(f);

    for (const tyb_board_model **cur = family->models; *cur; cur++) {
        const tyb_board_model *model = *cur;

        int r = (*f)(model, udata);
        if (r)
            return r;
    }

    return 0;
}

bool tyb_board_model_is_real(const tyb_board_model *model)
{
    return model && model->code_size;
}

bool tyb_board_model_test_firmware(const tyb_board_model *model, const tyb_firmware *fw,
                                   const tyb_board_model **rguesses, unsigned int *rcount)
{
    assert(fw);
    assert(!!rguesses == !!rcount);
    if (rguesses)
        assert(*rcount);

    bool compatible = false;
    unsigned int count = 0;

    for (const tyb_board_family **cur = tyb_board_families; *cur; cur++) {
        const tyb_board_family *family = *cur;

        const tyb_board_model *family_guesses[8];
        unsigned int family_count;

        family_count = (*family->guess_models)(fw, family_guesses, TY_COUNTOF(family_guesses));

        for (unsigned int i = 0; i < family_count; i++) {
            if (family_guesses[i] == model)
                compatible = true;
            if (rguesses && count < *rcount)
                rguesses[count++] = family_guesses[i];
        }
    }

    if (rcount)
        *rcount = count;
    return compatible;
}

const char *tyb_board_model_get_name(const tyb_board_model *model)
{
    assert(model);
    return model->name;
}

const char *tyb_board_model_get_mcu(const tyb_board_model *model)
{
    assert(model);
    return model->mcu;
}

size_t tyb_board_model_get_code_size(const tyb_board_model *model)
{
    assert(model);
    return model->code_size;
}

const char *tyb_board_capability_get_name(tyb_board_capability cap)
{
    assert((int)cap >= 0 && (int)cap < TYB_BOARD_CAPABILITY_COUNT);
    return capability_names[cap];
}

tyb_board *tyb_board_ref(tyb_board *board)
{
    assert(board);

    __atomic_add_fetch(&board->refcount, 1, __ATOMIC_RELAXED);
    return board;
}

void tyb_board_unref(tyb_board *board)
{
    if (board) {
        if (__atomic_fetch_sub(&board->refcount, 1, __ATOMIC_RELEASE) > 1)
            return;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);

        if (board->tag != board->id)
            free(board->tag);
        free(board->id);
        free(board->location);

        ty_mutex_release(&board->interfaces_lock);

        ty_list_foreach(cur, &board->interfaces) {
            tyb_board_interface *iface = ty_container_of(cur, tyb_board_interface, list);

            if (iface->hnode.next)
                ty_htable_remove(&iface->hnode);
            tyb_board_interface_unref(iface);
        }
    }

    free(board);
}

static int match_interface(tyb_board_interface *iface, void *udata)
{
    return ty_compare_paths(tyb_board_interface_get_path(iface), udata);
}

bool tyb_board_matches_tag(tyb_board *board, const char *id)
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
    if (family && strcmp(family, board->model->family->name) != 0)
        return false;
    if (location && strcmp(location, board->location) != 0 &&
            !tyb_board_list_interfaces(board, match_interface, location))
        return false;

    return true;
}

void tyb_board_set_udata(tyb_board *board, void *udata)
{
    assert(board);
    board->udata = udata;
}

void *tyb_board_get_udata(const tyb_board *board)
{
    assert(board);
    return board->udata;
}

tyb_monitor *tyb_board_get_monitor(const tyb_board *board)
{
    assert(board);
    return board->monitor;
}

tyb_board_state tyb_board_get_state(const tyb_board *board)
{
    assert(board);
    return board->state;
}

const char *tyb_board_get_id(const tyb_board *board)
{
    assert(board);
    return board->id;
}

int tyb_board_set_tag(tyb_board *board, const char *tag)
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

const char *tyb_board_get_tag(const tyb_board *board)
{
    assert(board);
    return board->tag;
}

const char *tyb_board_get_location(const tyb_board *board)
{
    assert(board);
    return board->location;
}

uint64_t tyb_board_get_serial_number(const tyb_board *board)
{
    assert(board);
    return board->serial;
}

const tyb_board_model *tyb_board_get_model(const tyb_board *board)
{
    assert(board);
    return board->model;
}

const char *tyb_board_get_model_name(const tyb_board *board)
{
    assert(board);

    const tyb_board_model *model = board->model;
    if (!model)
        return NULL;

    return model->name;
}

int tyb_board_get_capabilities(const tyb_board *board)
{
    assert(board);
    return board->capabilities;
}

int tyb_board_list_interfaces(tyb_board *board, tyb_board_list_interfaces_func *f, void *udata)
{
    assert(board);
    assert(f);

    int r;

    ty_mutex_lock(&board->interfaces_lock);

    r = 0;
    ty_list_foreach(cur, &board->interfaces) {
        tyb_board_interface *iface = ty_container_of(cur, tyb_board_interface, list);

        r = (*f)(iface, udata);
        if (r)
            break;
    }

    ty_mutex_unlock(&board->interfaces_lock);
    return r;
}

int tyb_board_open_interface(tyb_board *board, tyb_board_capability cap, tyb_board_interface **riface)
{
    assert(board);
    assert((int)cap < (int)TY_COUNTOF(board->cap2iface));
    assert(riface);

    tyb_board_interface *iface;
    int r;

    ty_mutex_lock(&board->interfaces_lock);

    iface = board->cap2iface[cap];
    if (!iface) {
        r = 0;
        goto cleanup;
    }

    r = tyb_board_interface_open(iface);
    if (r < 0)
        goto cleanup;

    *riface = iface;
    r = 1;

cleanup:
    ty_mutex_unlock(&board->interfaces_lock);
    return r;
}

struct wait_for_context {
    tyb_board *board;
    tyb_board_capability capability;
};

static int wait_for_callback(tyb_monitor *monitor, void *udata)
{
    TY_UNUSED(monitor);

    struct wait_for_context *ctx = udata;

    if (ctx->board->state == TYB_BOARD_STATE_DROPPED)
        return ty_error(TY_ERROR_NOT_FOUND, "Board has disappeared");

    return tyb_board_has_capability(ctx->board, ctx->capability);
}

int tyb_board_wait_for(tyb_board *board, tyb_board_capability capability, int timeout)
{
    assert(board);

    tyb_monitor *monitor = board->monitor;
    struct wait_for_context ctx;

    if (board->state == TYB_BOARD_STATE_DROPPED)
        return ty_error(TY_ERROR_NOT_FOUND, "Board has disappeared");

    ctx.board = board;
    ctx.capability = capability;

    return tyb_monitor_wait(monitor, wait_for_callback, &ctx, timeout);
}

int tyb_board_serial_set_attributes(tyb_board *board, uint32_t rate, int flags)
{
    assert(board);

    tyb_board_interface *iface;
    int r;

    r = tyb_board_open_interface(board, TYB_BOARD_CAPABILITY_SERIAL, &iface);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    r = (*iface->vtable->serial_set_attributes)(iface, rate, flags);

    tyb_board_interface_close(iface);
    return r;
}

ssize_t tyb_board_serial_read(tyb_board *board, char *buf, size_t size, int timeout)
{
    assert(board);
    assert(buf);
    assert(size);

    tyb_board_interface *iface;
    ssize_t r;

    r = tyb_board_open_interface(board, TYB_BOARD_CAPABILITY_SERIAL, &iface);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    r = (*iface->vtable->serial_read)(iface, buf, size, timeout);

    tyb_board_interface_close(iface);
    return r;
}

ssize_t tyb_board_serial_write(tyb_board *board, const char *buf, size_t size)
{
    assert(board);
    assert(buf);

    tyb_board_interface *iface;
    ssize_t r;

    r = tyb_board_open_interface(board, TYB_BOARD_CAPABILITY_SERIAL, &iface);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    if (!size)
        size = strlen(buf);

    r = (*iface->vtable->serial_write)(iface, buf, size);

    tyb_board_interface_close(iface);
    return r;
}

int tyb_board_upload(tyb_board *board, tyb_firmware *fw, tyb_board_upload_progress_func *pf, void *udata)
{
    assert(board);
    assert(fw);

    tyb_board_interface *iface = NULL;
    int r;

    r = tyb_board_open_interface(board, TYB_BOARD_CAPABILITY_UPLOAD, &iface);
    if (r < 0)
        goto cleanup;
    if (!r) {
        r = ty_error(TY_ERROR_MODE, "Firmware upload is not available in this mode");
        goto cleanup;
    }
    assert(board->model);

    if (tyb_firmware_get_size(fw) > board->model->code_size) {
        r = ty_error(TY_ERROR_RANGE, "Firmware is too big for %s", board->model->name);
        goto cleanup;
    }

    r = (*iface->vtable->upload)(iface, fw, pf, udata);

cleanup:
    tyb_board_interface_close(iface);
    return r;
}

int tyb_board_reset(tyb_board *board)
{
    assert(board);

    tyb_board_interface *iface;
    int r;

    r = tyb_board_open_interface(board, TYB_BOARD_CAPABILITY_RESET, &iface);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_MODE, "Cannot reset in this mode");

    r = (*iface->vtable->reset)(iface);

    tyb_board_interface_close(iface);
    return r;
}

int tyb_board_reboot(tyb_board *board)
{
    assert(board);

    tyb_board_interface *iface;
    int r;

    r = tyb_board_open_interface(board, TYB_BOARD_CAPABILITY_REBOOT, &iface);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_MODE, "Cannot reboot in this mode");

    r = (*iface->vtable->reboot)(iface);

    tyb_board_interface_close(iface);
    return r;
}

tyb_board_interface *tyb_board_interface_ref(tyb_board_interface *iface)
{
    assert(iface);

    __atomic_add_fetch(&iface->refcount, 1, __ATOMIC_RELAXED);
    return iface;
}

void tyb_board_interface_unref(tyb_board_interface *iface)
{
    if (iface) {
        if (__atomic_fetch_sub(&iface->refcount, 1, __ATOMIC_RELEASE) > 1)
            return;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);

        tyd_device_close(iface->h);
        tyd_device_unref(iface->dev);

        ty_mutex_release(&iface->open_lock);
    }

    free(iface);
}

int tyb_board_interface_open(tyb_board_interface *iface)
{
    assert(iface);

    int r;

    ty_mutex_lock(&iface->open_lock);

    if (!iface->h) {
        r = tyd_device_open(iface->dev, &iface->h);
        if (r < 0)
            goto cleanup;
    }
    iface->open_count++;

    tyb_board_interface_ref(iface);
    r = 0;

cleanup:
    ty_mutex_unlock(&iface->open_lock);
    return r;
}

void tyb_board_interface_close(tyb_board_interface *iface)
{
    if (!iface)
        return;

    ty_mutex_lock(&iface->open_lock);
    if (!--iface->open_count) {
        tyd_device_close(iface->h);
        iface->h = NULL;
    }
    ty_mutex_unlock(&iface->open_lock);

    tyb_board_interface_unref(iface);
}

const char *tyb_board_interface_get_name(const tyb_board_interface *iface)
{
    assert(iface);
    return iface->name;
}

int tyb_board_interface_get_capabilities(const tyb_board_interface *iface)
{
    assert(iface);
    return iface->capabilities;
}

const char *tyb_board_interface_get_path(const tyb_board_interface *iface)
{
    assert(iface);
    return tyd_device_get_path(iface->dev);
}

uint8_t tyb_board_interface_get_interface_number(const tyb_board_interface *iface)
{
    assert(iface);
    return tyd_device_get_interface_number(iface->dev);
}

tyd_device *tyb_board_interface_get_device(const tyb_board_interface *iface)
{
    assert(iface);
    return iface->dev;
}

tyd_handle *tyb_board_interface_get_handle(const tyb_board_interface *iface)
{
    assert(iface);
    return iface->h;
}

void tyb_board_interface_get_descriptors(const tyb_board_interface *iface, struct ty_descriptor_set *set, int id)
{
    assert(iface);
    assert(set);

    if (iface->h)
        tyd_device_get_descriptors(iface->h, set, id);
}

static int new_task(tyb_board *board, const struct _ty_task_vtable *vtable, ty_task **rtask)
{
    ty_task *task = NULL;
    int r;

    if (board->current_task)
        return ty_error(TY_ERROR_BUSY, "A task is already running for board '%s'", board->tag);

    r = _ty_task_new(sizeof(*task), vtable, &task);
    if (r < 0)
        return r;

    board->current_task = task;
    task->board = tyb_board_ref(board);

    *rtask = task;
    return 0;
}

static void cleanup_task(ty_task *task)
{
    task->board->current_task = NULL;
    tyb_board_unref(task->board);
}

static int get_compatible_firmware(tyb_board *board, tyb_firmware **fws, unsigned int fws_count,
                                   tyb_firmware **rfw)
{
    if (fws_count > 1) {
        for (unsigned int i = 0; i < fws_count; i++) {
            if (tyb_board_model_test_firmware(board->model, fws[i], NULL, 0)) {
                *rfw = fws[i];
                return 0;
            }
        }

        return ty_error(TY_ERROR_FIRMWARE, "No firmware is compatible with '%s' (%s)",
                        board->tag, board->model->name);
    } else {
        const tyb_board_model *guesses[8];
        unsigned int count;

        count = TY_COUNTOF(guesses);
        if (tyb_board_model_test_firmware(board->model, fws[0], guesses, &count)) {
            *rfw = fws[0];
            return 0;
        }

        if (count) {
            char buf[256], *ptr;

            ptr = buf;
            for (unsigned int i = 0; i < count && ptr < buf + sizeof(buf); i++)
                ptr += snprintf(ptr, (size_t)(buf + sizeof(buf) - ptr), "%s%s",
                                i ? (i + 1 < count ? ", " : " and ") : "", guesses[i]->name);

            return ty_error(TY_ERROR_FIRMWARE, "This firmware is only compatible with %s", buf);
        } else {
            return ty_error(TY_ERROR_FIRMWARE, "This firmware is not compatible with '%s'",
                            board->tag);
        }
    }
}

static int upload_progress_callback(const tyb_board *board, const tyb_firmware *fw,
                                    size_t uploaded, void *udata)
{
    TY_UNUSED(board);
    TY_UNUSED(udata);

    ty_progress("Uploading", (unsigned int)uploaded, (unsigned int)tyb_firmware_get_size(fw));
    return 0;
}

static void unref_upload_firmware(void *ptr)
{
    tyb_firmware_unref(ptr);
}

static int run_upload(ty_task *task)
{
    tyb_board *board = task->board;
    tyb_firmware *fw;
    size_t fw_size;
    int flags = task->upload.flags, r;

    if (flags & TYB_UPLOAD_NOCHECK) {
        fw = task->upload.fws[0];
    } else if (tyb_board_model_is_real(board->model)) {
        r = get_compatible_firmware(board, task->upload.fws, task->upload.fws_count, &fw);
        if (r < 0)
            return r;
    } else {
        // Maybe we can identify the board and test the firmwares in bootloader mode?
        fw = NULL;
    }

    ty_log(TY_LOG_INFO, "Uploading to board '%s' (%s)", board->tag, board->model->name);

    // Can't upload directly, should we try to reboot or wait?
    if (!tyb_board_has_capability(board, TYB_BOARD_CAPABILITY_UPLOAD)) {
        if (flags & TYB_UPLOAD_WAIT) {
            ty_log(TY_LOG_INFO, "Waiting for device (press button to reboot)...");
        } else {
            ty_log(TY_LOG_INFO, "Triggering board reboot");
            r = tyb_board_reboot(board);
            if (r < 0)
                return r;
        }
    }

wait:
    r = tyb_board_wait_for(board, TYB_BOARD_CAPABILITY_UPLOAD,
                           flags & TYB_UPLOAD_WAIT ? -1 : MANUAL_REBOOT_DELAY);
    if (r < 0)
        return r;
    if (!r) {
        ty_log(TY_LOG_INFO, "Reboot didn't work, press button manually");
        flags |= TYB_UPLOAD_WAIT;

        goto wait;
    }

    if (!fw) {
        r = get_compatible_firmware(board, task->upload.fws, task->upload.fws_count, &fw);
        if (r < 0)
            return r;
    }

    ty_log(TY_LOG_INFO, "Firmware: %s", tyb_firmware_get_name(fw));
    fw_size = tyb_firmware_get_size(fw);
    if (fw_size >= 1024) {
        ty_log(TY_LOG_INFO, "Flash usage: %zu kiB (%.1f%%)",
               (fw_size + 1023) / 1024,
               (double)fw_size / (double)tyb_board_model_get_code_size(board->model) * 100.0);
    } else {
        ty_log(TY_LOG_INFO, "Flash usage: %zu bytes (%.1f%%)",
               fw_size,
               (double)fw_size / (double)tyb_board_model_get_code_size(board->model) * 100.0);
    }

    r = tyb_board_upload(board, fw, upload_progress_callback, NULL);
    if (r < 0)
        return r;

    if (!(flags & TYB_UPLOAD_NORESET)) {
        ty_log(TY_LOG_INFO, "Sending reset command");
        r = tyb_board_reset(board);
        if (r < 0)
            return r;

        r = tyb_board_wait_for(board, TYB_BOARD_CAPABILITY_RUN, FINAL_TASK_TIMEOUT);
        if (r < 0)
            return r;
        if (!r)
            return ty_error(TY_ERROR_TIMEOUT, "Reset does not seem to work");
    } else {
        ty_log(TY_LOG_INFO, "Firmware uploaded, reset the board to use it");
    }

    _ty_task_set_result(task, tyb_firmware_ref(fw), unref_upload_firmware);
    return 0;
}

static void cleanup_upload(ty_task *task)
{
    for (unsigned int i = 0; i < task->upload.fws_count; i++)
        tyb_firmware_unref(task->upload.fws[i]);
    free(task->upload.fws);

    cleanup_task(task);
}

static const struct _ty_task_vtable upload_task_vtable = {
    .run = run_upload,
    .cleanup = cleanup_upload
};

int tyb_upload(tyb_board *board, tyb_firmware **fws, unsigned int fws_count, int flags,
               ty_task **rtask)
{
    assert(board);
    assert(fws);
    assert(fws_count);
    assert(rtask);

    ty_task *task = NULL;
    int r;

    r = new_task(board, &upload_task_vtable, &task);
    if (r < 0)
        goto error;

    if (fws_count > TYB_UPLOAD_MAX_FIRMWARES) {
        ty_log(TY_LOG_WARNING, "Cannot select more than %d firmwares per upload",
               TYB_UPLOAD_MAX_FIRMWARES);
        fws_count = TYB_UPLOAD_MAX_FIRMWARES;
    }
    if (flags & TYB_UPLOAD_NOCHECK)
        fws_count = 1;

    task->upload.fws = malloc(fws_count * sizeof(tyb_firmware *));
    if (!task->upload.fws) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    for (unsigned int i = 0; i < fws_count; i++)
        task->upload.fws[i] = tyb_firmware_ref(fws[i]);
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
    tyb_board *board = task->board;
    int r;

    ty_log(TY_LOG_INFO, "Resetting board '%s' (%s)", board->tag, board->model->name);

    if (!tyb_board_has_capability(board, TYB_BOARD_CAPABILITY_RESET)) {
        ty_log(TY_LOG_INFO, "Triggering board reboot");
        r = tyb_board_reboot(board);
        if (r < 0)
            return r;

        r = tyb_board_wait_for(board, TYB_BOARD_CAPABILITY_RESET, MANUAL_REBOOT_DELAY);
        if (r <= 0)
            return ty_error(TY_ERROR_TIMEOUT, "Reboot does not seem to work");
    }

    ty_log(TY_LOG_INFO, "Sending reset command");
    r = tyb_board_reset(board);
    if (r < 0)
        return r;

    r = tyb_board_wait_for(board, TYB_BOARD_CAPABILITY_RUN, FINAL_TASK_TIMEOUT);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_TIMEOUT, "Reset does not seem to work");

    return 0;
}

static const struct _ty_task_vtable reset_task_vtable = {
    .run = run_reset,
    .cleanup = cleanup_task
};

int tyb_reset(tyb_board *board, ty_task **rtask)
{
    assert(board);
    assert(rtask);

    return new_task(board, &reset_task_vtable, rtask);
}

static int run_reboot(ty_task *task)
{
    tyb_board *board = task->board;
    int r;

    ty_log(TY_LOG_INFO, "Rebooting board '%s' (%s)", board->tag, board->model->name);

    ty_log(TY_LOG_INFO, "Triggering board reboot");
    r = tyb_board_reboot(board);
    if (r < 0)
        return r;

    r = tyb_board_wait_for(board, TYB_BOARD_CAPABILITY_UPLOAD, FINAL_TASK_TIMEOUT);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_TIMEOUT, "Reboot does not seem to work");

    return 0;
}

static const struct _ty_task_vtable reboot_task_vtable = {
    .run = run_reboot,
    .cleanup = cleanup_task
};

int tyb_reboot(tyb_board *board, ty_task **rtask)
{
    assert(board);
    assert(rtask);

    return new_task(board, &reboot_task_vtable, rtask);
}
