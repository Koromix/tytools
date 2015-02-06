/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include "ty/board.h"
#include "board_priv.h"
#include "ty/firmware.h"
#include "htable.h"
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

    ty_htable interfaces;

    void *udata;
};

struct ty_board_model {
    TY_BOARD_MODEL
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

extern const struct _ty_board_vendor _ty_teensy_vendor;

static const struct _ty_board_vendor *vendors[] = {
    &_ty_teensy_vendor,
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

    r = ty_htable_init(&manager->interfaces, 64);
    if (r < 0)
        goto error;

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

        ty_htable_release(&manager->interfaces);
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

static int add_board(ty_board_manager *manager, ty_board_interface *iface, ty_board **rboard)
{
    ty_board *board;
    int r;

    board = calloc(1, sizeof(*board));
    if (!board) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    board->refcount = 1;

    board->location = strdup(ty_device_get_location(iface->dev));
    if (!board->location) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    ty_list_init(&board->interfaces);

    board->model = iface->model;
    board->serial = iface->serial;

    board->vid = ty_device_get_vid(iface->dev);
    board->pid = ty_device_get_pid(iface->dev);

    board->manager = manager;
    ty_list_add_tail(&manager->boards, &board->list);

    *rboard = board;
    return 0;

error:
    ty_board_unref(board);
    return r;
}

static void free_interface(ty_board_interface *iface);

static void close_board(ty_board *board)
{
    board->state = TY_BOARD_STATE_MISSING;

    ty_list_foreach(cur, &board->interfaces) {
        ty_board_interface *iface = ty_container_of(cur, ty_board_interface, list);

        if (iface->hnode.next)
            ty_htable_remove(&iface->hnode);

        free_interface(iface);
    }
    ty_list_init(&board->interfaces);

    memset(board->cap2iface, 0, sizeof(board->cap2iface));
    board->capabilities = 0;

    trigger_callbacks(board, TY_BOARD_EVENT_DISAPPEARED);
}

static int add_missing_board(ty_board *board)
{
    board->missing_since = ty_millis();
    if (board->missing.prev)
        ty_list_remove(&board->missing);
    ty_list_add_tail(&board->manager->missing_boards, &board->missing);

    // There may be other boards waiting to be dropped, set timeout for the next in line
    board = ty_list_get_first(&board->manager->missing_boards, ty_board, missing);

    return ty_timer_set(board->manager->timer, ty_adjust_timeout(drop_board_delay, board->missing_since), TY_TIMER_ONESHOT);
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

static ty_board *find_board(ty_board_manager *manager, const char *location)
{
    ty_list_foreach(cur, &manager->boards) {
        ty_board *board = ty_container_of(cur, ty_board, list);

        if (strcmp(board->location, location) == 0)
            return board;
    }

    return NULL;
}

static int open_interface(ty_device *dev, ty_board_interface **riface)
{
    ty_board_interface *iface;
    const char *serial;
    int r;

    iface = calloc(1, sizeof(*iface));
    if (!iface) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    iface->dev = ty_device_ref(dev);

    serial = ty_device_get_serial_number(dev);
    if (serial)
        iface->serial = strtoull(serial, NULL, 10);

    r = 0;
    for (const struct _ty_board_vendor **cur = vendors; *cur; cur++) {
        const struct _ty_board_vendor *vendor = *cur;

        r = (*vendor->open_interface)(iface);
        if (r < 0)
            goto error;
        if (r)
            break;
    }
    if (!r)
        goto error;

    *riface = iface;
    return 1;

error:
    free_interface(iface);
    return r;
}

static void free_interface(ty_board_interface *iface)
{
    if (iface) {
        ty_device_close(iface->h);
        ty_device_unref(iface->dev);
    }

    free(iface);
}

static ty_board_interface *find_interface(ty_board_manager *manager, ty_device *dev)
{
    ty_htable_foreach_hash(cur, &manager->interfaces, ty_htable_hash_ptr(dev)) {
        ty_board_interface *iface = ty_container_of(cur, ty_board_interface, hnode);

        if (iface->dev == dev)
            return iface;
    }

    return NULL;
}

static inline bool model_is_valid(const ty_board_model *model)
{
    return model && model->code_size;
}

static int add_interface(ty_board_manager *manager, ty_device *dev)
{
    ty_board_interface *iface = NULL;
    ty_board *board;
    ty_board_event event;
    int r;

    r = open_interface(dev, &iface);
    if (r <= 0)
        goto error;

    board = find_board(manager, ty_device_get_location(dev));

    /* Maybe the device notifications came in the wrong order, or somehow the device removal
       notifications were dropped somewhere and we never got it, so use heuristics to improve
       board change detection. */
    if (board) {
        if ((model_is_valid(iface->model) && model_is_valid(board->model) && iface->model != board->model)
                || iface->serial != board->serial) {
            drop_board(board);
            board = NULL;
        } else if (board->vid != ty_device_get_vid(dev) || board->pid != ty_device_get_pid(dev)) {
            close_board(board);

            board->vid = ty_device_get_vid(dev);
            board->pid = ty_device_get_pid(dev);
        }
    }

    if (board) {
        if (model_is_valid(iface->model))
            board->model = iface->model;
        if (iface->serial)
            board->serial = iface->serial;

        event = TY_BOARD_EVENT_CHANGED;
    } else {
        r = add_board(manager, iface, &board);
        if (r < 0)
            goto error;

        event = TY_BOARD_EVENT_ADDED;
    }

    iface->board = board;

    ty_list_add_tail(&board->interfaces, &iface->list);
    ty_htable_add(&manager->interfaces, ty_htable_hash_ptr(iface->dev), &iface->hnode);

    for (size_t i = 0; i < TY_COUNTOF(board->cap2iface); i++) {
        if (iface->capabilities & (1 << i))
            board->cap2iface[i] = iface;
    }
    board->capabilities |= iface->capabilities;

    if (board->missing.prev)
        ty_list_remove(&board->missing);

    board->state = TY_BOARD_STATE_ONLINE;

    return trigger_callbacks(board, event);

error:
    free_interface(iface);
    return r;
}

static int remove_interface(ty_board_manager *manager, ty_device *dev)
{
    ty_board_interface *iface;
    ty_board *board;
    int r;

    iface = find_interface(manager, dev);
    if (!iface)
        return 0;

    board = iface->board;

    ty_htable_remove(&iface->hnode);
    ty_list_remove(&iface->list);

    free_interface(iface);

    memset(board->cap2iface, 0, sizeof(board->cap2iface));
    board->capabilities = 0;

    ty_list_foreach(cur, &board->interfaces) {
        iface = ty_container_of(cur, ty_board_interface, list);

        for (size_t i = 0; i < TY_COUNTOF(board->cap2iface); i++) {
            if (iface->capabilities & (1 << i))
                board->cap2iface[i] = iface;
        }
        board->capabilities |= iface->capabilities;
    }

    if (ty_list_is_empty(&board->interfaces)) {
        close_board(board);

        r = add_missing_board(board);
        if (r < 0)
            return r;
    } else {
        r = trigger_callbacks(board, TY_BOARD_EVENT_CHANGED);
        if (r < 0)
            return r;
    }

    return 0;
}

static int device_callback(ty_device *dev, ty_device_event event, void *udata)
{
    ty_board_manager *manager = udata;

    switch (event) {
    case TY_DEVICE_EVENT_ADDED:
        return add_interface(manager, dev);

    case TY_DEVICE_EVENT_REMOVED:
        return remove_interface(manager, dev);
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
        free(board->location);

        ty_list_foreach(cur, &board->interfaces) {
            ty_board_interface *iface = ty_container_of(cur, ty_board_interface, list);

            if (iface->hnode.next)
                ty_htable_remove(&iface->hnode);

            free_interface(iface);
        }

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

const char *ty_board_get_location(const ty_board *board)
{
    assert(board);
    return board->location;
}

const ty_board_model *ty_board_get_model(const ty_board *board)
{
    assert(board);
    return board->model;
}

ty_board_interface *ty_board_get_interface(const ty_board *board, ty_board_capability cap)
{
    assert(board);
    assert(cap < TY_COUNTOF(board->cap2iface));

    return board->cap2iface[cap];
}

uint16_t ty_board_get_capabilities(const ty_board *board)
{
    assert(board);
    return board->capabilities;
}

uint64_t ty_board_get_serial_number(const ty_board *board)
{
    assert(board);
    return board->serial;
}

ty_device *ty_board_get_device(const ty_board *board, ty_board_capability cap)
{
    assert(board);

    ty_board_interface *iface = board->cap2iface[cap];
    if (!iface)
        return NULL;

    return iface->dev;
}

ty_handle *ty_board_get_handle(const ty_board *board, ty_board_capability cap)
{
    assert(board);

    ty_board_interface *iface = board->cap2iface[cap];
    if (!iface)
        return NULL;

    return iface->h;
}

void ty_board_get_descriptors(const ty_board *board, ty_board_capability cap, struct ty_descriptor_set *set, int id)
{
    assert(board);

    ty_board_interface *iface = board->cap2iface[cap];
    if (!iface)
        return;

    ty_device_get_descriptors(iface->h, set, id);
}

int ty_board_list_interfaces(ty_board *board, ty_board_list_interfaces_func *f, void *udata)
{
    assert(board);
    assert(f);

    int r;

    ty_list_foreach(cur, &board->interfaces) {
        ty_board_interface *iface = ty_container_of(cur, ty_board_interface, list);

        r = (*f)(board, iface, udata);
        if (r)
            return r;
    }

    return 0;
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

    ty_board_interface *iface = board->cap2iface[TY_BOARD_CAPABILITY_SERIAL];
    if (!iface)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    return (*iface->vtable->serial_set_attributes)(iface, rate, flags);
}

ssize_t ty_board_serial_read(ty_board *board, char *buf, size_t size)
{
    assert(board);
    assert(buf);
    assert(size);

    ty_board_interface *iface = board->cap2iface[TY_BOARD_CAPABILITY_SERIAL];
    if (!iface)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    return (*iface->vtable->serial_read)(iface, buf, size);
}

ssize_t ty_board_serial_write(ty_board *board, const char *buf, size_t size)
{
    assert(board);
    assert(buf);

    ty_board_interface *iface = board->cap2iface[TY_BOARD_CAPABILITY_SERIAL];
    if (!iface)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    if (!size)
        size = strlen(buf);

    return (*iface->vtable->serial_write)(iface, buf, size);
}

int ty_board_upload(ty_board *board, ty_firmware *f, uint16_t flags, ty_board_upload_progress_func *pf, void *udata)
{
    assert(board);
    assert(f);

    ty_board_interface *iface = board->cap2iface[TY_BOARD_CAPABILITY_UPLOAD];
    if (!iface)
        return ty_error(TY_ERROR_MODE, "Firmware upload is not available in this mode");

    if (!model_is_valid(board->model))
        return ty_error(TY_ERROR_MODE, "Cannot upload to unknown board model");

    // FIXME: detail error message (max allowed, ratio)
    if (f->size > board->model->code_size)
        return ty_error(TY_ERROR_RANGE, "Firmware is too big for %s", board->model->desc);

    if (!(flags & TY_BOARD_UPLOAD_NOCHECK)) {
        const ty_board_model *guess;

        guess = ty_board_test_firmware(f);
        if (!guess)
            return ty_error(TY_ERROR_FIRMWARE, "This firmware was not compiled for a known device");

        if (guess != board->model)
            return ty_error(TY_ERROR_FIRMWARE, "This firmware was compiled for %s", guess->desc);
    }

    return (*iface->vtable->upload)(iface, f, flags, pf, udata);
}

int ty_board_reset(ty_board *board)
{
    assert(board);

    ty_board_interface *iface = board->cap2iface[TY_BOARD_CAPABILITY_RESET];
    if (!iface)
        return ty_error(TY_ERROR_MODE, "Cannot reset in this mode");

    return (*iface->vtable->reset)(iface);
}

int ty_board_reboot(ty_board *board)
{
    assert(board);

    ty_board_interface *iface = board->cap2iface[TY_BOARD_CAPABILITY_REBOOT];
    if (!iface)
        return ty_error(TY_ERROR_MODE, "Cannot reboot in this mode");

    return (*iface->vtable->reboot)(iface);
}

const ty_board_model *ty_board_test_firmware(const ty_firmware *f)
{
    assert(f);

    size_t magic_size = sizeof(signatures[0].magic);

    if (f->size < magic_size)
        return NULL;

    /* Naive search with each board's signature, not pretty but unless
       thousands of models appear this is good enough. */
    for (size_t i = 0; i < f->size - magic_size; i++) {
        for (const struct firmware_signature *sig = signatures; sig->model; sig++) {
            if (memcmp(f->image + i, sig->magic, magic_size) == 0)
                return sig->model;
        }
    }

    return NULL;
}

const char *ty_board_interface_get_desc(const ty_board_interface *iface)
{
    assert(iface);
    return iface->desc;
}

uint16_t ty_board_interface_get_capabilities(const ty_board_interface *iface)
{
    assert(iface);
    return iface->capabilities;
}

ty_device *ty_board_interface_get_device(const ty_board_interface *iface)
{
    assert(iface);
    return iface->dev;
}

ty_handle *ty_board_interface_get_handle(const ty_board_interface *iface)
{
    assert(iface);
    return iface->h;
}

void ty_board_interface_get_descriptors(const ty_board_interface *iface, struct ty_descriptor_set *set, int id)
{
    assert(iface);
    assert(set);

    ty_device_get_descriptors(iface->h, set, id);
}
