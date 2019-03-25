/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#ifndef _WIN32
    #include <sys/stat.h>
#endif
#include "../libhs/device.h"
#include "board_priv.h"
#include "class_priv.h"
#include "firmware.h"
#include "monitor.h"
#include "system.h"
#include "task.h"
#include "timer.h"

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
        free(board->serial_number);
        free(board->location);
        free(board->description);

        ty_mutex_release(&board->ifaces_lock);

        for (size_t i = 0; i < board->ifaces.count; i++) {
            ty_board_interface *iface = board->ifaces.values[i];
            ty_board_interface_unref(iface);
        }
        _hs_array_release(&board->ifaces);
    }

    free(board);
}

struct board_id_part {
    const char *ptr;
    size_t len;
};

static void parse_board_id(const char *id, const char *delimiters, struct board_id_part parts[])
{
    size_t part_offset = 0;
    size_t delim_offset = 0;
    size_t i = 0;
    do {
        const char *d = strchr(delimiters + delim_offset, id[i]);
        if (d || !id[i]) {
            if (i - part_offset) {
                parts[delim_offset].ptr = id + part_offset;
                parts[delim_offset].len = i - part_offset;
            }
            part_offset = i + 1;
            delim_offset = (size_t)(d - delimiters + 1);
        }
    } while (id[i++]);
}

static bool compare_board_id_parts(const struct board_id_part *part1,
                                   const struct board_id_part *part2)
{
    if (!part1->ptr || !part2->ptr)
        return true;
    return part1->len == part2->len && memcmp(part1->ptr, part2->ptr, part1->len) == 0;
}

static int match_board_interface(ty_board_interface *iface, void *udata)
{
    return ty_compare_paths(ty_board_interface_get_path(iface), udata);
}

bool ty_board_matches_tag(ty_board *board, const char *id)
{
    assert(board);

    if (!id)
        return true;
    if (board->tag != board->id && strcmp(id, board->tag) == 0)
        return true;

    struct board_id_part parts1[3] = {0};
    struct board_id_part parts2[2] = {0};

    parse_board_id(id, "-@", parts1);
    parse_board_id(board->id, "-", parts2);

    if (!compare_board_id_parts(&parts1[0], &parts2[0]))
        return false;
    if (!compare_board_id_parts(&parts1[1], &parts2[1]))
        return false;
    /* The last part is necessarily NUL-terminated so we can just use regular
       C string functions. */
    if (parts1[2].ptr && strcmp(parts1[2].ptr, board->location) != 0 &&
            !ty_board_list_interfaces(board, match_board_interface, (void *)parts1[2].ptr))
        return false;

    return true;
}

ty_monitor *ty_board_get_monitor(const ty_board *board)
{
    assert(board);
    return board->monitor;
}

ty_board_status ty_board_get_status(const ty_board *board)
{
    assert(board);
    return board->status;
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

const char *ty_board_get_serial_number(const ty_board *board)
{
    assert(board);
    return board->serial_number;
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

    ty_mutex_lock(&board->ifaces_lock);

    r = 0;
    for (size_t i = 0; i < board->ifaces.count; i++) {
        ty_board_interface *iface = board->ifaces.values[i];

        r = (*f)(iface, udata);
        if (r)
            break;
    }

    ty_mutex_unlock(&board->ifaces_lock);
    return r;
}

int ty_board_open_interface(ty_board *board, ty_board_capability cap, ty_board_interface **riface)
{
    assert(board);
    assert((int)cap < (int)TY_COUNTOF(board->cap2iface));
    assert(riface);

    ty_board_interface *iface;
    int r;

    ty_mutex_lock(&board->ifaces_lock);

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
    ty_mutex_unlock(&board->ifaces_lock);
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

    if (board->status == TY_BOARD_STATUS_DROPPED)
        return ty_error(TY_ERROR_NOT_FOUND, "Board '%s' has disappeared", board->tag);

    return ty_board_has_capability(board, ctx->capability);
}

int ty_board_wait_for(ty_board *board, ty_board_capability capability, int timeout)
{
    assert(board);

    ty_monitor *monitor = board->monitor;
    struct wait_for_context ctx;

    if (board->status == TY_BOARD_STATUS_DROPPED)
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
        return ty_error(TY_ERROR_MODE, "Board '%s' is not available for serial I/O", board->tag);

    r = (*iface->class_vtable->serial_read)(iface, buf, size, timeout);

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
        return ty_error(TY_ERROR_MODE, "Board '%s' is not available for serial I/O", board->tag);

    r = (*iface->class_vtable->serial_write)(iface, buf, size);

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
        r = ty_error(TY_ERROR_MODE, "Cannot upload to board '%s'", board->tag);
        goto cleanup;
    }
    assert(board->model);

    r = (*iface->class_vtable->upload)(iface, fw, pf, udata);

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
        return ty_error(TY_ERROR_MODE, "Cannot reset board '%s'", board->tag);

    r = (*iface->class_vtable->reset)(iface);

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
        return ty_error(TY_ERROR_MODE, "Cannot reboot board '%s'", board->tag);

    r = (*iface->class_vtable->reboot)(iface);

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

        hs_port_close(iface->port);
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

    if (!iface->port) {
        r = (*iface->class_vtable->open_interface)(iface);
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
        (*iface->class_vtable->close_interface)(iface);
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
    return iface->dev->path;
}

uint8_t ty_board_interface_get_interface_number(const ty_board_interface *iface)
{
    assert(iface);
    return iface->dev->iface_number;
}

hs_device *ty_board_interface_get_device(const ty_board_interface *iface)
{
    assert(iface);
    return iface->dev;
}

hs_port *ty_board_interface_get_handle(const ty_board_interface *iface)
{
    assert(iface);
    return iface->port;
}

void ty_board_interface_get_descriptors(const ty_board_interface *iface, struct ty_descriptor_set *set, int id)
{
    assert(iface);
    assert(set);

    if (iface->port)
        ty_descriptor_set_add(set, hs_port_get_poll_handle(iface->port), id);
}

static int new_board_task(ty_board *board, const char *action, int (*run)(ty_task *task),
                          ty_task **rtask)
{
    char task_name_buf[64];
    ty_task *task = NULL;
    int r;

    if (board->current_task)
        return ty_error(TY_ERROR_BUSY, "Board '%s' is busy on task '%s'", board->tag,
                        board->current_task->name);

    snprintf(task_name_buf, sizeof(task_name_buf), "%s@%s", action, board->tag);
    r = ty_task_new(task_name_buf, run, &task);
    if (r < 0)
        return r;

    board->current_task = ty_task_ref(task);

    *rtask = task;
    return 0;
}

static void cleanup_task_board(ty_board **board_ptr)
{
    ty_task_unref((*board_ptr)->current_task);
    (*board_ptr)->current_task = NULL;
    ty_board_unref(*board_ptr);
    *board_ptr = NULL;
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
        return ty_error(TY_ERROR_UNSUPPORTED, "No firmware is compatible with '%s' (%s)",
                        board->tag, ty_models[board->model].name);
    } else if (fw_models_count) {
        char buf[256], *ptr;

        ptr = buf;
        for (unsigned int i = 0; i < fw_models_count && ptr < buf + sizeof(buf); i++)
            ptr += snprintf(ptr, (size_t)(buf + sizeof(buf) - ptr), "%s%s",
                            i ? (i + 1 < fw_models_count ? ", " : " and ") : "",
                            ty_models[fw_models[i]].name);

        return ty_error(TY_ERROR_UNSUPPORTED, "Firmware '%s' is only compatible with %s",
                        fws[0]->name, buf);
    } else {
        return ty_error(TY_ERROR_UNSUPPORTED, "Firmware '%s' is not compatible with '%s'",
                        fws[0]->name, board->tag);
    }
}

static int upload_progress_callback(const ty_board *board, const ty_firmware *fw,
                                    size_t uploaded_size, size_t flash_size, void *udata)
{
    TY_UNUSED(board);
    TY_UNUSED(udata);

    if (!uploaded_size) {
        ty_log(TY_LOG_INFO, "Firmware: %s", fw->name);
        if (fw->total_size >= 1024) {
            ty_log(TY_LOG_INFO, "Flash usage: %zu kiB (%.1f%%)",
                   (fw->total_size + 1023) / 1024,
                   (double)fw->total_size / (double)flash_size * 100.0);
        } else {
            ty_log(TY_LOG_INFO, "Flash usage: %zu bytes (%.1f%%)",
                   fw->total_size,
                   (double)fw->total_size / (double)flash_size * 100.0);
        }
    }
    ty_progress("Uploading", uploaded_size, fw->total_size);

    return 0;
}

static void unref_upload_firmware(void *ptr)
{
    ty_firmware_unref(ptr);
}

static int run_upload(ty_task *task)
{
    ty_board *board = task->u.upload.board;
    ty_firmware *fw;
    int flags = task->u.upload.flags, r;

    if (flags & TY_UPLOAD_NOCHECK) {
        fw = task->u.upload.fws[0];
    } else if (ty_models[board->model].mcu) {
        r = select_compatible_firmware(board, task->u.upload.fws, task->u.upload.fws_count, &fw);
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
        r = select_compatible_firmware(board, task->u.upload.fws, task->u.upload.fws_count, &fw);
        if (r < 0)
            return r;
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

    task->result = ty_firmware_ref(fw);
    task->result_cleanup = unref_upload_firmware;
    return 0;
}

static void finalize_upload(ty_task *task)
{
    for (unsigned int i = 0; i < task->u.upload.fws_count; i++)
        ty_firmware_unref(task->u.upload.fws[i]);
    free(task->u.upload.fws);

    cleanup_task_board(&task->u.upload.board);
}

int ty_upload(ty_board *board, ty_firmware **fws, unsigned int fws_count, int flags,
               ty_task **rtask)
{
    assert(board);
    assert(fws);
    assert(fws_count);
    assert(rtask);

    ty_task *task = NULL;
    int r;

    r = new_board_task(board, "upload", run_upload, &task);
    if (r < 0)
        goto error;
    task->u.upload.board = ty_board_ref(board);
    task->task_finalize = finalize_upload;

    if (fws_count > TY_UPLOAD_MAX_FIRMWARES) {
        ty_log(TY_LOG_WARNING, "Cannot select more than %d firmwares per upload",
               TY_UPLOAD_MAX_FIRMWARES);
        fws_count = TY_UPLOAD_MAX_FIRMWARES;
    }
    if (flags & TY_UPLOAD_NOCHECK)
        fws_count = 1;

    task->u.upload.fws = malloc(fws_count * sizeof(ty_firmware *));
    if (!task->u.upload.fws) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    for (unsigned int i = 0; i < fws_count; i++)
        task->u.upload.fws[i] = ty_firmware_ref(fws[i]);
    task->u.upload.fws_count = fws_count;
    task->u.upload.flags = flags;

    *rtask = task;
    return 0;

error:
    ty_task_unref(task);
    return r;
}

static int run_reset(ty_task *task)
{
    ty_board *board = task->u.reset.board;
    int r;

    ty_log(TY_LOG_INFO, "Resetting board '%s' (%s)", board->tag, ty_models[board->model].name);

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_RESET) &&
            ty_board_has_capability(board, TY_BOARD_CAPABILITY_REBOOT)) {
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

static void finalize_reset(ty_task *task)
{
    cleanup_task_board(&task->u.reset.board);
}

int ty_reset(ty_board *board, ty_task **rtask)
{
    assert(board);
    assert(rtask);

    ty_task *task = NULL;
    int r;

    r = new_board_task(board, "reset", run_reset, &task);
    if (r < 0)
        return r;
    task->u.reset.board = ty_board_ref(board);
    task->task_finalize = finalize_reset;

    *rtask = task;
    return 0;
}

static int run_reboot(ty_task *task)
{
    ty_board *board = task->u.reboot.board;
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

static void finalize_reboot(ty_task *task)
{
    cleanup_task_board(&task->u.reboot.board);
}

int ty_reboot(ty_board *board, ty_task **rtask)
{
    assert(board);
    assert(rtask);

    ty_task *task = NULL;
    int r;

    r = new_board_task(board, "reboot", run_reboot, &task);
    if (r < 0)
        return r;
    task->u.reboot.board = ty_board_ref(board);
    task->task_finalize = finalize_reboot;

    *rtask = task;
    return 0;
}

static int run_send(ty_task *task)
{
    ty_board *board = task->u.send.board;
    const char *buf = task->u.send.buf;
    size_t size = task->u.send.size;
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

static void finalize_send(ty_task *task)
{
    free(task->u.send.buf);
    cleanup_task_board(&task->u.send.board);
}

int ty_send(ty_board *board, const char *buf, size_t size, ty_task **rtask)
{
    assert(board);
    assert(buf);
    assert(rtask);

    ty_task *task = NULL;
    int r;

    r = new_board_task(board, "send", run_send, &task);
    if (r < 0)
        goto error;
    task->u.send.board = ty_board_ref(board);
    task->task_finalize = finalize_send;

    task->u.send.buf = malloc(size);
    if (!task->u.send.buf) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    memcpy(task->u.send.buf, buf, size);
    task->u.send.size = size;

    *rtask = task;
    return 0;

error:
    ty_task_unref(task);
    return r;
}

static int run_send_file(ty_task *task)
{
    ty_board *board = task->u.send_file.board;
    FILE *fp = task->u.send_file.fp;
    size_t size = task->u.send_file.size;
    const char *filename = task->u.send_file.filename;
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

static void finalize_send_file(ty_task *task)
{
    free(task->u.send_file.filename);
    if (task->u.send_file.fp)
        fclose(task->u.send_file.fp);
    cleanup_task_board(&task->u.send_file.board);
}

int ty_send_file(ty_board *board, const char *filename, ty_task **rtask)
{
    assert(board);
    assert(filename);
    assert(rtask);

    ty_task *task = NULL;
    int r;

    r = new_board_task(board, "send", run_send_file, &task);
    if (r < 0)
        goto error;
    task->u.send_file.board = ty_board_ref(board);
    task->task_finalize = finalize_send_file;

#ifdef _WIN32
    task->u.send_file.fp = fopen(filename, "rb");
#else
    task->u.send_file.fp = fopen(filename, "rbe");
#endif
    if (!task->u.send_file.fp) {
        switch (errno) {
            case EACCES: {
                r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", filename);
            } break;
            case EIO: {
                r = ty_error(TY_ERROR_IO, "I/O error while opening '%s' for reading", filename);
            } break;
            case ENOENT:
            case ENOTDIR: {
                r = ty_error(TY_ERROR_NOT_FOUND, "File '%s' does not exist", filename);
            } break;

            default: {
                r = ty_error(TY_ERROR_SYSTEM, "fopen('%s') failed: %s", filename, strerror(errno));
            } break;
        }
        goto error;
    }

    fseek(task->u.send_file.fp, 0, SEEK_END);
#ifdef _WIN32
    task->u.send_file.size = (size_t)_ftelli64(task->u.send_file.fp);
#else
    task->u.send_file.size = (size_t)ftello(task->u.send_file.fp);
#endif
    rewind(task->u.send_file.fp);
    if (!task->u.send_file.size) {
        r = ty_error(TY_ERROR_UNSUPPORTED, "Failed to read size of '%s', is it a regular file?",
                     filename);
        goto error;
    }

    task->u.send_file.filename = strdup(filename);
    if (!task->u.send_file.filename) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    *rtask = task;
    return 0;

error:
    ty_task_unref(task);
    return r;
}
