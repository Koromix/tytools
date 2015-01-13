/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include "ty/board.h"
#include "board_priv.h"
#include "ty/firmware.h"
#include "list.h"
#include "ty/system.h"
#include "ty/timer.h"

struct ty_board_manager {
    ty_device_monitor *monitor;
    ty_timer *timer;

    ty_list_head callbacks;
    int callback_id;

    ty_list_head boards;
    ty_list_head missing_boards;

    void *udata;
};

struct ty_board_mode {
    struct ty_board_mode_;
};

struct ty_board_model {
    struct ty_board_model_;
};

struct callback {
    ty_list_head list;
    int id;

    ty_board_manager_callback_func *f;
    void *udata;
};

struct firmware_signature {
    const ty_board_model *model;
    uint8_t magic[8];
};

extern const ty_board_mode _ty_teensy_bootloader_mode;
extern const ty_board_mode _ty_teensy_flightsim_mode;
extern const ty_board_mode _ty_teensy_hid_mode;
extern const ty_board_mode _ty_teensy_midi_mode;
extern const ty_board_mode _ty_teensy_rawhid_mode;
extern const ty_board_mode _ty_teensy_serial_mode;
extern const ty_board_mode _ty_teensy_serial_hid_mode;

const ty_board_mode *ty_board_modes[] = {
    &_ty_teensy_bootloader_mode,
    &_ty_teensy_flightsim_mode,
    &_ty_teensy_hid_mode,
    &_ty_teensy_midi_mode,
    &_ty_teensy_rawhid_mode,
    &_ty_teensy_serial_mode,
    &_ty_teensy_serial_hid_mode,
    NULL
};

extern const ty_board_model _ty_teensy_pp10_model;
extern const ty_board_model _ty_teensy_20_model;
extern const ty_board_model _ty_teensy_pp20_model;
extern const ty_board_model _ty_teensy_30_model;
extern const ty_board_model _ty_teensy_31_model;

const ty_board_model *ty_board_models[] = {
#ifdef TY_EXPERIMENTAL
    &_ty_teensy_pp10_model,
    &_ty_teensy_20_model,
    &_ty_teensy_pp20_model,
#endif
    &_ty_teensy_30_model,
    &_ty_teensy_31_model,
    NULL
};

static const struct firmware_signature signatures[] = {
    {&_ty_teensy_pp10_model, {0x0C, 0x94, 0x00, 0x7E, 0xFF, 0xCF, 0xF8, 0x94}},
    {&_ty_teensy_20_model,   {0x0C, 0x94, 0x00, 0x3F, 0xFF, 0xCF, 0xF8, 0x94}},
    {&_ty_teensy_pp20_model, {0x0C, 0x94, 0x00, 0xFE, 0xFF, 0xCF, 0xF8, 0x94}},
    {&_ty_teensy_30_model,   {0x38, 0x80, 0x04, 0x40, 0x82, 0x3F, 0x04, 0x00}},
    {&_ty_teensy_31_model,   {0x30, 0x80, 0x04, 0x40, 0x82, 0x3F, 0x04, 0x00}},
    {0}
};

static const int drop_board_delay = 3000;

int ty_board_manager_new(ty_board_manager **rmanager)
{
    assert(rmanager);

    ty_board_manager *manager;
    int r;

    manager = calloc(1, sizeof(*manager));
    if (!manager) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    r = ty_timer_new(&manager->timer);
    if (r < 0)
        goto error;

    ty_list_init(&manager->boards);
    ty_list_init(&manager->missing_boards);

    ty_list_init(&manager->callbacks);

    *rmanager = manager;
    return 0;

error:
    ty_board_manager_free(manager);
    return r;
}

void ty_board_manager_free(ty_board_manager *manager)
{
    if (manager) {
        ty_device_monitor_free(manager->monitor);
        ty_timer_free(manager->timer);

        ty_list_foreach(cur, &manager->callbacks) {
            struct callback *callback = ty_container_of(cur, struct callback, list);
            free(callback);
        }

        ty_list_foreach(cur, &manager->boards) {
            ty_board *board = ty_container_of(cur, ty_board, list);

            board->manager = NULL;
            ty_board_unref(board);
        }
    }

    free(manager);
}

void ty_board_manager_set_udata(ty_board_manager *manager, void *udata)
{
    assert(manager);
    manager->udata = udata;
}

void *ty_board_manager_get_udata(const ty_board_manager *manager)
{
    assert(manager);
    return manager->udata;
}

void ty_board_manager_get_descriptors(const ty_board_manager *manager, ty_descriptor_set *set, int id)
{
    assert(manager);
    assert(set);

    ty_device_monitor_get_descriptors(manager->monitor, set, id);
    ty_timer_get_descriptors(manager->timer, set, id);
}

int ty_board_manager_register_callback(ty_board_manager *manager, ty_board_manager_callback_func *f, void *udata)
{
    assert(manager);
    assert(f);

    struct callback *callback = calloc(1, sizeof(*callback));
    if (!callback)
        return ty_error(TY_ERROR_MEMORY, NULL);

    callback->id = manager->callback_id++;
    callback->f = f;
    callback->udata = udata;

    ty_list_add_tail(&manager->callbacks, &callback->list);

    return callback->id;
}

static void drop_callback(struct callback *callback)
{
    ty_list_remove(&callback->list);
    free(callback);
}

void ty_board_manager_deregister_callback(ty_board_manager *manager, int id)
{
    assert(manager);
    assert(id >= 0);

    ty_list_foreach(cur, &manager->callbacks) {
        struct callback *callback = ty_container_of(cur, struct callback, list);
        if (callback->id == id) {
            drop_callback(callback);
            break;
        }
    }
}

static int trigger_callbacks(ty_board *board, ty_board_event event)
{
    ty_list_foreach(cur, &board->manager->callbacks) {
        struct callback *callback = ty_container_of(cur, struct callback, list);
        int r;

        r = (*callback->f)(board, event, callback->udata);
        if (r < 0)
            return r;
        if (r)
            drop_callback(callback);
    }

    return 0;
}

static int open_board(ty_board *board, ty_device *dev)
{
    const ty_board_mode *mode = NULL;
    uint16_t vid, pid;
    uint64_t old_serial;
    const char *s;
    int r;

    vid = ty_device_get_vid(dev);
    pid = ty_device_get_pid(dev);

    const ty_board_mode **cur;
    for (cur = ty_board_modes; *cur; cur++) {
        mode = *cur;
        if (mode->vid == vid && mode->pid == pid)
            break;
    }
    if (!*cur)
        return 0;

    if (ty_device_get_interface_number(dev) != mode->iface)
        return 0;

    ty_device_unref(board->dev);
    board->dev = ty_device_ref(dev);

    board->mode = mode;

    // Detect serial number changes: if it's different, we know it's probably a different board
    // and the things we knew about it (such as the model) are not valid anymore.
    old_serial = board->serial;

    s = ty_device_get_serial_number(dev);
    if (s) {
        board->serial = strtoull(s, NULL, 10);
    } else {
        board->serial = 0;
    }

    if (mode->vtable->open) {
        r = mode->vtable->open(board);
        if (r <= 0)
            return r;
    }

    // Yup, different board (probably)
    if (!board->serial || board->serial != old_serial)
        board->model = NULL;

    ty_device_close(board->h);
    board->h = NULL;

    ty_error_mask(TY_ERROR_NOT_FOUND);
    r = ty_device_open(board->dev, false, &board->h);
    ty_error_unmask();
    if (r < 0) {
        if (r == TY_ERROR_NOT_FOUND)
            return 0;
        return r;
    }

    if (ty_board_has_capability(board, TY_BOARD_CAPABILITY_IDENTIFY)) {
        r = board->mode->vtable->identify(board);
        if (r < 0)
            return r;
    }

    board->state = TY_BOARD_STATE_ONLINE;

    return 1;
}

static int add_board(ty_board_manager *manager, ty_device *dev)
{
    ty_board *board;
    int r;

    board = calloc(1, sizeof(*board));
    if (!board)
        return ty_error(TY_ERROR_MEMORY, NULL);
    board->refcount = 1;

    r = open_board(board, dev);
    if (r <= 0)
        goto error;

    board->manager = manager;
    ty_list_add_tail(&manager->boards, &board->list);

    return trigger_callbacks(board, TY_BOARD_EVENT_ADDED);

error:
    ty_board_unref(board);
    return r;
}

static int change_board(ty_board *board, ty_device *dev)
{
    int r;

    r = open_board(board, dev);
    if (r <= 0)
        return r;

    if (board->missing.prev) {
        ty_list_remove(&board->missing);

        if (ty_list_is_empty(&board->manager->missing_boards))
            ty_timer_set(board->manager->timer, -1, 0);
    }

    return trigger_callbacks(board, TY_BOARD_EVENT_CHANGED);
}

static void close_board(ty_board *board)
{
    board->state = TY_BOARD_STATE_CLOSED;

    ty_device_close(board->h);
    board->h = NULL;

    if (board->missing.prev)
        ty_list_remove(&board->missing);
    ty_list_add_tail(&board->manager->missing_boards, &board->missing);
    board->missing_since = ty_millis();

    board->mode = NULL;

    trigger_callbacks(board, TY_BOARD_EVENT_CLOSED);
}

static void drop_board(ty_board *board)
{
    board->state = TY_BOARD_STATE_DROPPED;

    if (board->missing.prev)
        ty_list_remove(&board->missing);

    trigger_callbacks(board, TY_BOARD_EVENT_DROPPED);

    ty_list_remove(&board->list);
    board->manager = NULL;

    ty_board_unref(board);
}

static int device_callback(ty_device *dev, ty_device_event event, void *udata)
{
    ty_board_manager *manager = udata;

    const char *location;
    int r;

    location = ty_device_get_location(dev);

    switch (event) {
    case TY_DEVICE_EVENT_ADDED:
        ty_list_foreach(cur, &manager->boards) {
            ty_board *board = ty_container_of(cur, ty_board, list);

            if (strcmp(ty_device_get_location(board->dev), location) == 0)
                return change_board(board, dev);
        }

        return add_board(manager, dev);

    case TY_DEVICE_EVENT_REMOVED:
        ty_list_foreach(cur, &manager->boards) {
            ty_board *board = ty_container_of(cur, ty_board, list);

            if (board->dev == dev) {
                close_board(board);

                // If there's only one element, it must be us because (see close_board)
                // so the timer is either not set or wrong.
                if (ty_list_is_singular(&manager->missing_boards)) {
                    r = ty_timer_set(manager->timer, drop_board_delay, TY_TIMER_ONESHOT);
                    if (r < 0)
                        return r;
                }

                break;
            }
        }

        return 0;
    }

    assert(false);
    __builtin_unreachable();
}

int ty_board_manager_refresh(ty_board_manager *manager)
{
    assert(manager);

    int r;

    if (ty_timer_rearm(manager->timer)) {
        ty_list_foreach(cur, &manager->missing_boards) {
            ty_board *board = ty_container_of(cur, ty_board, missing);
            int timeout;

            if (board->state != TY_BOARD_STATE_CLOSED)
                continue;

            timeout = ty_adjust_timeout(drop_board_delay, board->missing_since);
            if (timeout) {
                r = ty_timer_set(manager->timer, timeout, TY_TIMER_ONESHOT);
                if (r < 0)
                    return r;
                break;
            }

            drop_board(board);
        }
    }

    if (!manager->monitor) {
        r = ty_device_monitor_new(&manager->monitor);
        if (r < 0)
            return r;

        r = ty_device_monitor_register_callback(manager->monitor, device_callback, manager);
        if (r < 0)
            return r;

        r = ty_device_monitor_list(manager->monitor, device_callback, manager);
        if (r < 0)
            return r;

        return 0;
    }

    r = ty_device_monitor_refresh(manager->monitor);
    if (r < 0)
        return r;

    return 0;
}

int ty_board_manager_wait(ty_board_manager *manager, ty_board_manager_wait_func *f, void *udata, int timeout)
{
    assert(manager);

    ty_descriptor_set set = {0};
    uint64_t start;
    int r;

    ty_board_manager_get_descriptors(manager, &set, 1);

    start = ty_millis();
    do {
        r = ty_board_manager_refresh(manager);
        if (r < 0)
            return (int)r;

        if (f) {
            r = (*f)(manager, udata);
            if (r)
                return (int)r;
        }

        r = ty_poll(&set, ty_adjust_timeout(timeout, start));
    } while (r > 0);

    return r;
}

int ty_board_manager_list(ty_board_manager *manager, ty_board_manager_callback_func *f, void *udata)
{
    assert(manager);
    assert(f);

    ty_list_foreach(cur, &manager->boards) {
        ty_board *board = ty_container_of(cur, ty_board, list);
        int r;

        if (board->state == TY_BOARD_STATE_ONLINE) {
            r = (*f)(board, TY_BOARD_EVENT_ADDED, udata);
            if (r)
                return r;
        }
    }

    return 0;
}

const ty_board_mode *ty_board_find_mode(const char *name)
{
    assert(name);

    for (const ty_board_mode **cur = ty_board_modes; *cur; cur++) {
        const ty_board_mode *mode = *cur;
        if (strcasecmp(mode->name, name) == 0)
            return mode;
    }

    return NULL;
}

const ty_board_model *ty_board_find_model(const char *name)
{
    assert(name);

    for (const ty_board_model **cur = ty_board_models; *cur; cur++) {
        const ty_board_model *model = *cur;
        if (strcmp(model->name, name) == 0 || strcmp(model->mcu, name) == 0)
            return model;
    }

    return NULL;
}

const char *ty_board_mode_get_name(const ty_board_mode *mode)
{
    assert(mode);
    return mode->name;
}

const char *ty_board_mode_get_desc(const ty_board_mode *mode)
{
    assert(mode);
    return mode->desc;
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

const char *ty_board_model_get_desc(const ty_board_model *model)
{
    assert(model);
    return model->desc;
}

size_t ty_board_model_get_code_size(const ty_board_model *model)
{
    assert(model);
    return model->code_size;
}

ty_board *ty_board_ref(ty_board *board)
{
    assert(board);

    board->refcount++;
    return board;
}

void ty_board_unref(ty_board *board)
{
    if (!board)
        return;

    if (board->refcount)
        board->refcount--;

    if (!board->manager && !board->refcount) {
        ty_device_close(board->h);
        ty_device_unref(board->dev);

        free(board);
    }
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

ty_board_manager *ty_board_get_manager(const ty_board *board)
{
    assert(board);
    return board->manager;
}

ty_board_state ty_board_get_state(const ty_board *board)
{
    assert(board);
    return board->state;
}

ty_device *ty_board_get_device(const ty_board *board)
{
    assert(board);
    return board->dev;
}

ty_handle *ty_board_get_handle(const ty_board *board)
{
    assert(board);
    return board->h;
}

const ty_board_mode *ty_board_get_mode(const ty_board *board)
{
    assert(board);
    return board->mode;
}

const ty_board_model *ty_board_get_model(const ty_board *board)
{
    assert(board);
    return board->model;
}

uint32_t ty_board_get_capabilities(const ty_board *board)
{
    assert(board);

    if (!board->mode)
        return 0;

    return board->mode->capabilities;
}

uint64_t ty_board_get_serial_number(const ty_board *board)
{
    assert(board);
    return board->serial;
}

struct wait_for_context {
    ty_board *board;
    ty_board_capability capability;
};

static int wait_callback(ty_board_manager *manager, void *udata)
{
    TY_UNUSED(manager);

    struct wait_for_context *ctx = udata;

    if (ctx->board->state == TY_BOARD_STATE_DROPPED)
        return ty_error(TY_ERROR_NOT_FOUND, "Board has disappeared");

    return ty_board_has_capability(ctx->board, ctx->capability);
}

int ty_board_wait_for(ty_board *board, ty_board_capability capability, int timeout)
{
    assert(board);
    assert(board->manager);

    struct wait_for_context ctx;
    int r;

    ctx.board = ty_board_ref(board);
    ctx.capability = capability;

    r = ty_board_manager_wait(board->manager, wait_callback, &ctx, timeout);
    ty_board_unref(board);

    return r;
}

int ty_board_serial_set_attributes(ty_board *board, uint32_t rate, uint16_t flags)
{
    assert(board);

    int r;

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_SERIAL))
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    if (ty_device_get_type(board->dev) != TY_DEVICE_SERIAL)
        return 0;

    r = ty_serial_set_attributes(board->h, rate, flags);
    if (r < 0)
        return r;

    return 0;
}

ssize_t ty_board_serial_read(ty_board *board, char *buf, size_t size)
{
    assert(board);
    assert(buf);
    assert(size);

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_SERIAL))
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    return board->mode->vtable->serial_read(board, buf, size);
}

ssize_t ty_board_serial_write(ty_board *board, const char *buf, size_t size)
{
    assert(board);
    assert(buf);

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_SERIAL))
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    if (!size)
        size = strlen(buf);

    return board->mode->vtable->serial_write(board, buf, size);
}

int ty_board_upload(ty_board *board, ty_firmware *f, uint16_t flags, ty_board_upload_progress_func *pf, void *udata)
{
    assert(board);
    assert(f);

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_UPLOAD))
        return ty_error(TY_ERROR_MODE, "Firmware upload is not available in this mode");

    // FIXME: detail error message (max allowed, ratio)
    if (f->size > board->model->code_size)
        return ty_error(TY_ERROR_RANGE, "Firmware is too big for %s", board->model->desc);

    if (!(flags & TY_BOARD_UPLOAD_NOCHECK)) {
        const ty_board_model *guess;

        guess = ty_board_test_firmware(f);
        if (!guess)
            return ty_error(TY_ERROR_FIRMWARE, "This firmware was not compiled for a known device");

        // board->model may have been carried over
        if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_IDENTIFY))
            return ty_error(TY_ERROR_MODE, "Cannot detect board model");
        
        if (guess != board->model)
            return ty_error(TY_ERROR_FIRMWARE, "This firmware was compiled for %s", guess->desc);
    }

    return board->mode->vtable->upload(board, f, flags, pf, udata);
}

int ty_board_reset(ty_board *board)
{
    assert(board);

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_RESET))
        return ty_error(TY_ERROR_MODE, "Cannot reset in this mode");

    return board->mode->vtable->reset(board);
}

int ty_board_reboot(ty_board *board)
{
    assert(board);

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_REBOOT))
        return ty_error(TY_ERROR_MODE, "Cannot reboot in this mode");

    return board->mode->vtable->reboot(board);
}

const ty_board_model *ty_board_test_firmware(const ty_firmware *f)
{
    assert(f);

    // Naive search with each board's signature, not pretty but unless
    // thousands of models appear this is good enough.

    size_t magic_size = sizeof(signatures[0].magic);

    if (f->size < magic_size)
        return NULL;

    for (size_t i = 0; i < f->size - magic_size; i++) {
        for (const struct firmware_signature *sig = signatures; sig->model; sig++) {
            if (memcmp(f->image + i, sig->magic, magic_size) == 0)
                return sig->model;
        }
    }

    return NULL;
}
