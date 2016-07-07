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
#include "ty/monitor.h"
#include "ty/system.h"
#include "task_priv.h"
#include "ty/timer.h"

struct ty_board_model {
    TY_BOARD_MODEL
};

struct ty_task {
    TY_TASK

    ty_board *board;
    union {
        struct {
            ty_firmware **fws;
            unsigned int fws_count;
            int flags;
        } upload;
    };
};

extern const ty_board_family _ty_teensy_family;

const ty_board_family *ty_board_families[] = {
    &_ty_teensy_family,
    NULL
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
    #define FINAL_TASK_TIMEOUT 8000
#else
    #define MANUAL_REBOOT_DELAY 5000
    #define FINAL_TASK_TIMEOUT 5000
#endif

const char *ty_board_family_get_name(const ty_board_family *family)
{
    assert(family);
    return family->name;
}

int ty_board_model_list(ty_board_model_list_func *f, void *udata)
{
    assert(f);

    for (const ty_board_family **cur = ty_board_families; *cur; cur++) {
        const ty_board_family *family = *cur;

        for (const ty_board_model **cur2 = family->models; *cur2; cur2++) {
            const ty_board_model *model = *cur2;

            int r = (*f)(model, udata);
            if (r)
                return r;
        }
    }

    return 0;
}

const ty_board_model *ty_board_model_find(const char *name)
{
    assert(name);

    for (const ty_board_family **cur = ty_board_families; *cur; cur++) {
        const ty_board_family *family = *cur;

        for (const ty_board_model **cur2 = family->models; *cur2; cur2++) {
            const ty_board_model *model = *cur2;

            if (strcmp(model->name, name) == 0)
                return model;
        }
    }

    return NULL;
}

bool ty_board_model_is_real(const ty_board_model *model)
{
    return model && model->code_size;
}

bool ty_board_model_test_firmware(const ty_board_model *model, const ty_firmware *fw,
                                   const ty_board_model **rguesses, unsigned int *rcount)
{
    assert(fw);
    assert(!!rguesses == !!rcount);
    if (rguesses)
        assert(*rcount);

    bool compatible = false;
    unsigned int count = 0;

    for (const ty_board_family **cur = ty_board_families; *cur; cur++) {
        const ty_board_family *family = *cur;

        const ty_board_model *family_guesses[8];
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

const char *ty_board_model_get_name(const ty_board_model *model)
{
    assert(model);
    return model->name;
}

const char *ty_board_model_get_mcu(const ty_board_model *model)
{
    assert(model);
    return model->mcu;
}

size_t ty_board_model_get_code_size(const ty_board_model *model)
{
    assert(model);
    return model->code_size;
}

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
    if (family && strcmp(family, board->model->family->name) != 0)
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

void ty_board_set_model(ty_board *board, const ty_board_model *model)
{
    assert(board);
    board->model = model;
}

const ty_board_model *ty_board_get_model(const ty_board *board)
{
    assert(board);
    return board->model;
}

const char *ty_board_get_model_name(const ty_board *board)
{
    assert(board);

    const ty_board_model *model = board->model;
    if (!model)
        return NULL;

    return model->name;
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

// FIXME: this function probably belongs to the monitor API
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

int ty_board_serial_set_attributes(ty_board *board, uint32_t rate, int flags)
{
    assert(board);

    ty_board_interface *iface;
    int r;

    r = ty_board_open_interface(board, TY_BOARD_CAPABILITY_SERIAL, &iface);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available for '%s", board->tag);

    r = (*iface->vtable->serial_set_attributes)(iface, rate, flags);

    ty_board_interface_close(iface);
    return r;
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

    if (!size)
        size = strlen(buf);

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

    if (ty_firmware_get_size(fw) > board->model->code_size) {
        r = ty_error(TY_ERROR_RANGE, "Firmware is too big for %s", board->model->name);
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

static int new_task(ty_board *board, const struct _ty_task_vtable *vtable, ty_task **rtask)
{
    ty_task *task = NULL;
    int r;

    if (board->current_task)
        return ty_error(TY_ERROR_BUSY, "Board '%s' is busy with another task", board->tag);

    r = _ty_task_new(sizeof(*task), vtable, &task);
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

static int get_compatible_firmware(ty_board *board, ty_firmware **fws, unsigned int fws_count,
                                   ty_firmware **rfw)
{
    if (fws_count > 1) {
        for (unsigned int i = 0; i < fws_count; i++) {
            if (ty_board_model_test_firmware(board->model, fws[i], NULL, 0)) {
                *rfw = fws[i];
                return 0;
            }
        }

        return ty_error(TY_ERROR_FIRMWARE, "No firmware is compatible with '%s' (%s)",
                        board->tag, board->model->name);
    } else {
        const ty_board_model *guesses[8];
        unsigned int count;

        count = TY_COUNTOF(guesses);
        if (ty_board_model_test_firmware(board->model, fws[0], guesses, &count)) {
            *rfw = fws[0];
            return 0;
        }

        if (count) {
            char buf[256], *ptr;

            ptr = buf;
            for (unsigned int i = 0; i < count && ptr < buf + sizeof(buf); i++)
                ptr += snprintf(ptr, (size_t)(buf + sizeof(buf) - ptr), "%s%s",
                                i ? (i + 1 < count ? ", " : " and ") : "", guesses[i]->name);

            return ty_error(TY_ERROR_FIRMWARE, "Firmware '%s' is only compatible with %s",
                            ty_firmware_get_name(fws[0]), buf);
        } else {
            return ty_error(TY_ERROR_FIRMWARE, "Firmware '%s' is not compatible with '%s'",
                            ty_firmware_get_name(fws[0]), board->tag);
        }
    }
}

static int upload_progress_callback(const ty_board *board, const ty_firmware *fw,
                                    size_t uploaded, void *udata)
{
    TY_UNUSED(board);
    TY_UNUSED(udata);

    ty_progress("Uploading", (unsigned int)uploaded, (unsigned int)ty_firmware_get_size(fw));
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
    } else if (ty_board_model_is_real(board->model)) {
        r = get_compatible_firmware(board, task->upload.fws, task->upload.fws_count, &fw);
        if (r < 0)
            return r;
    } else {
        // Maybe we can identify the board and test the firmwares in bootloader mode?
        fw = NULL;
    }

    ty_log(TY_LOG_INFO, "Uploading to board '%s' (%s)", board->tag, board->model->name);

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
        r = get_compatible_firmware(board, task->upload.fws, task->upload.fws_count, &fw);
        if (r < 0)
            return r;
    }

    ty_log(TY_LOG_INFO, "Firmware: %s", ty_firmware_get_name(fw));
    fw_size = ty_firmware_get_size(fw);
    if (fw_size >= 1024) {
        ty_log(TY_LOG_INFO, "Flash usage: %zu kiB (%.1f%%)",
               (fw_size + 1023) / 1024,
               (double)fw_size / (double)ty_board_model_get_code_size(board->model) * 100.0);
    } else {
        ty_log(TY_LOG_INFO, "Flash usage: %zu bytes (%.1f%%)",
               fw_size,
               (double)fw_size / (double)ty_board_model_get_code_size(board->model) * 100.0);
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

    r = new_task(board, &upload_task_vtable, &task);
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

    ty_log(TY_LOG_INFO, "Resetting board '%s' (%s)", board->tag, board->model->name);

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

    return new_task(board, &reset_task_vtable, rtask);
}

static int run_reboot(ty_task *task)
{
    ty_board *board = task->board;
    int r;

    ty_log(TY_LOG_INFO, "Rebooting board '%s' (%s)", board->tag, board->model->name);

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

    return new_task(board, &reboot_task_vtable, rtask);
}
