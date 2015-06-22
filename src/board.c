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

struct tyb_monitor {
    tyd_monitor *monitor;
    ty_timer *timer;

    bool enumerated;

    ty_list_head callbacks;
    int callback_id;

    ty_mutex refresh_mutex;
    ty_cond refresh_cond;

    ty_list_head boards;
    ty_list_head missing_boards;

    ty_htable interfaces;

    void *udata;
};

struct tyb_board_model {
    TYB_BOARD_MODEL
};

struct callback {
    ty_list_head list;
    int id;

    tyb_monitor_callback_func *f;
    void *udata;
};

extern const tyb_board_family _tyb_teensy_family;

const tyb_board_family *tyb_board_families[] = {
    &_tyb_teensy_family,
    NULL
};

static const char *capability_names[] = {
    "upload",
    "reset",
    "reboot",
    "serial"
};

#define DROP_BOARD_DELAY 7000

static void drop_callback(struct callback *callback)
{
    ty_list_remove(&callback->list);
    free(callback);
}

static int trigger_callbacks(tyb_board *board, tyb_monitor_event event)
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

static int add_board(tyb_monitor *manager, tyb_board_interface *iface, tyb_board **rboard)
{
    tyb_board *board;
    int r;

    board = calloc(1, sizeof(*board));
    if (!board) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    board->refcount = 1;

    r = ty_mutex_init(&board->mutex, TY_MUTEX_RECURSIVE);
    if (r < 0)
        goto error;

    board->location = strdup(tyd_device_get_location(iface->dev));
    if (!board->location) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    ty_list_init(&board->interfaces);

    board->model = iface->model;
    board->serial = iface->serial;

    board->vid = tyd_device_get_vid(iface->dev);
    board->pid = tyd_device_get_pid(iface->dev);

    r = asprintf(&board->tag, "%s#%"PRIu64, board->location, board->serial);
    if (r < 0) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    board->manager = manager;
    ty_list_add_tail(&manager->boards, &board->list);

    *rboard = board;
    return 0;

error:
    tyb_board_unref(board);
    return r;
}

static void close_board(tyb_board *board)
{
    board->state = TYB_BOARD_STATE_MISSING;

    ty_list_foreach(cur, &board->interfaces) {
        tyb_board_interface *iface = ty_container_of(cur, tyb_board_interface, list);

        if (iface->hnode.next)
            ty_htable_remove(&iface->hnode);

        tyb_board_interface_unref(iface);
    }
    ty_list_init(&board->interfaces);

    memset(board->cap2iface, 0, sizeof(board->cap2iface));
    board->capabilities = 0;

    trigger_callbacks(board, TYB_MONITOR_EVENT_DISAPPEARED);
}

static int add_missing_board(tyb_board *board)
{
    board->missing_since = ty_millis();
    if (board->missing.prev)
        ty_list_remove(&board->missing);
    ty_list_add_tail(&board->manager->missing_boards, &board->missing);

    // There may be other boards waiting to be dropped, set timeout for the next in line
    board = ty_list_get_first(&board->manager->missing_boards, tyb_board, missing);

    return ty_timer_set(board->manager->timer, ty_adjust_timeout(DROP_BOARD_DELAY, board->missing_since), TY_TIMER_ONESHOT);
}

static void drop_board(tyb_board *board)
{
    board->state = TYB_BOARD_STATE_DROPPED;

    if (board->missing.prev)
        ty_list_remove(&board->missing);

    trigger_callbacks(board, TYB_MONITOR_EVENT_DROPPED);

    ty_list_remove(&board->list);
    board->manager = NULL;
}

static tyb_board *find_board(tyb_monitor *manager, const char *location)
{
    ty_list_foreach(cur, &manager->boards) {
        tyb_board *board = ty_container_of(cur, tyb_board, list);

        if (strcmp(board->location, location) == 0)
            return board;
    }

    return NULL;
}

static int open_interface(tyd_device *dev, tyb_board_interface **riface)
{
    tyb_board_interface *iface;
    const char *serial;
    int r;

    iface = calloc(1, sizeof(*iface));
    if (!iface) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    iface->refcount = 1;

    iface->dev = tyd_device_ref(dev);

    serial = tyd_device_get_serial_number(dev);
    if (serial)
        iface->serial = strtoull(serial, NULL, 10);

    r = 0;
    for (const tyb_board_family **cur = tyb_board_families; *cur && !r; cur++) {
        const tyb_board_family *family = *cur;

        ty_error_mask(TY_ERROR_NOT_FOUND);
        r = (*family->open_interface)(iface);
        ty_error_unmask();
        if (r < 0) {
            // FIXME: propagate the errors when the initial enumeration abortion problem is fixed
            if (r == TY_ERROR_NOT_FOUND || r == TY_ERROR_ACCESS)
                r = 0;
            goto error;
        }
    }
    if (!r)
        goto error;

    *riface = iface;
    return 1;

error:
    tyb_board_interface_unref(iface);
    return r;
}

static tyb_board_interface *find_interface(tyb_monitor *manager, tyd_device *dev)
{
    ty_htable_foreach_hash(cur, &manager->interfaces, ty_htable_hash_ptr(dev)) {
        tyb_board_interface *iface = ty_container_of(cur, tyb_board_interface, hnode);

        if (iface->dev == dev)
            return iface;
    }

    return NULL;
}

static inline bool model_is_valid(const tyb_board_model *model)
{
    return model && model->code_size;
}

static int add_interface(tyb_monitor *manager, tyd_device *dev)
{
    tyb_board_interface *iface = NULL;
    tyb_board *board = NULL;
    tyb_monitor_event event;
    int r;

    r = open_interface(dev, &iface);
    if (r <= 0)
        goto cleanup;

    board = find_board(manager, tyd_device_get_location(dev));

    /* Maybe the device notifications came in the wrong order, or somehow the device removal
       notifications were dropped somewhere and we never got it, so use heuristics to improve
       board change detection. */
    if (board) {
        tyb_board_lock(board);

        if ((model_is_valid(iface->model) && model_is_valid(board->model) && iface->model != board->model)
                || iface->serial != board->serial) {
            drop_board(board);

            tyb_board_unlock(board);
            tyb_board_unref(board);

            board = NULL;
        } else if (board->vid != tyd_device_get_vid(dev) || board->pid != tyd_device_get_pid(dev)) {
            close_board(board);

            board->vid = tyd_device_get_vid(dev);
            board->pid = tyd_device_get_pid(dev);
        }
    }

    if (board) {
        if (model_is_valid(iface->model))
            board->model = iface->model;
        if (iface->serial)
            board->serial = iface->serial;

        event = TYB_MONITOR_EVENT_CHANGED;
    } else {
        r = add_board(manager, iface, &board);
        if (r < 0)
            goto cleanup;
        tyb_board_lock(board);

        event = TYB_MONITOR_EVENT_ADDED;
    }

    iface->board = board;

    ty_list_add_tail(&board->interfaces, &iface->list);
    ty_htable_add(&manager->interfaces, ty_htable_hash_ptr(iface->dev), &iface->hnode);

    for (int i = 0; i < (int)TY_COUNTOF(board->cap2iface); i++) {
        if (iface->capabilities & (1 << i))
            board->cap2iface[i] = iface;
    }
    board->capabilities |= iface->capabilities;

    if (board->missing.prev)
        ty_list_remove(&board->missing);

    board->state = TYB_BOARD_STATE_ONLINE;
    iface = NULL;

    r = trigger_callbacks(board, event);

cleanup:
    if (board)
        tyb_board_unlock(board);
    tyb_board_interface_unref(iface);
    return r;
}

static int remove_interface(tyb_monitor *manager, tyd_device *dev)
{
    tyb_board_interface *iface;
    tyb_board *board;
    int r;

    iface = find_interface(manager, dev);
    if (!iface)
        return 0;

    board = iface->board;

    tyb_board_lock(board);

    ty_htable_remove(&iface->hnode);
    ty_list_remove(&iface->list);

    tyb_board_interface_unref(iface);

    memset(board->cap2iface, 0, sizeof(board->cap2iface));
    board->capabilities = 0;

    ty_list_foreach(cur, &board->interfaces) {
        iface = ty_container_of(cur, tyb_board_interface, list);

        for (unsigned int i = 0; i < TY_COUNTOF(board->cap2iface); i++) {
            if (iface->capabilities & (1 << i))
                board->cap2iface[i] = iface;
        }
        board->capabilities |= iface->capabilities;
    }

    if (ty_list_is_empty(&board->interfaces)) {
        close_board(board);

        r = add_missing_board(board);
        if (r < 0)
            goto cleanup;
    } else {
        r = trigger_callbacks(board, TYB_MONITOR_EVENT_CHANGED);
        if (r < 0)
            goto cleanup;
    }

    r = 0;
cleanup:
    tyb_board_unlock(board);
    return r;
}

static int device_callback(tyd_device *dev, tyd_monitor_event event, void *udata)
{
    tyb_monitor *manager = udata;

    switch (event) {
    case TYD_MONITOR_EVENT_ADDED:
        return add_interface(manager, dev);

    case TYD_MONITOR_EVENT_REMOVED:
        return remove_interface(manager, dev);
    }

    assert(false);
    __builtin_unreachable();
}

int tyb_monitor_new(tyb_monitor **rmanager)
{
    assert(rmanager);

    tyb_monitor *manager;
    int r;

    manager = calloc(1, sizeof(*manager));
    if (!manager) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    r = tyd_monitor_new(&manager->monitor);
    if (r < 0)
        goto error;

    r = tyd_monitor_register_callback(manager->monitor, device_callback, manager);
    if (r < 0)
        goto error;

    r = ty_timer_new(&manager->timer);
    if (r < 0)
        goto error;

    r = ty_mutex_init(&manager->refresh_mutex, TY_MUTEX_FAST);
    if (r < 0)
        goto error;

    r = ty_cond_init(&manager->refresh_cond);
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
    tyb_monitor_free(manager);
    return r;
}

void tyb_monitor_free(tyb_monitor *manager)
{
    if (manager) {
        ty_cond_release(&manager->refresh_cond);
        ty_mutex_release(&manager->refresh_mutex);

        tyd_monitor_free(manager->monitor);
        ty_timer_free(manager->timer);

        ty_list_foreach(cur, &manager->callbacks) {
            struct callback *callback = ty_container_of(cur, struct callback, list);
            free(callback);
        }

        ty_list_foreach(cur, &manager->boards) {
            tyb_board *board = ty_container_of(cur, tyb_board, list);

            board->manager = NULL;
            tyb_board_unref(board);
        }

        ty_htable_release(&manager->interfaces);
    }

    free(manager);
}

void tyb_monitor_set_udata(tyb_monitor *manager, void *udata)
{
    assert(manager);
    manager->udata = udata;
}

void *tyb_monitor_get_udata(const tyb_monitor *manager)
{
    assert(manager);
    return manager->udata;
}

void tyb_monitor_get_descriptors(const tyb_monitor *manager, ty_descriptor_set *set, int id)
{
    assert(manager);
    assert(set);

    tyd_monitor_get_descriptors(manager->monitor, set, id);
    ty_timer_get_descriptors(manager->timer, set, id);
}

int tyb_monitor_register_callback(tyb_monitor *manager, tyb_monitor_callback_func *f, void *udata)
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

void tyb_monitor_deregister_callback(tyb_monitor *manager, int id)
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

int tyb_monitor_refresh(tyb_monitor *manager)
{
    assert(manager);

    int r;

    if (ty_timer_rearm(manager->timer)) {
        ty_list_foreach(cur, &manager->missing_boards) {
            tyb_board *board = ty_container_of(cur, tyb_board, missing);
            int timeout;

            timeout = ty_adjust_timeout(DROP_BOARD_DELAY, board->missing_since);
            if (timeout) {
                r = ty_timer_set(manager->timer, timeout, TY_TIMER_ONESHOT);
                if (r < 0)
                    return r;
                break;
            }

            drop_board(board);

            tyb_board_unref(board);
        }
    }

    if (!manager->enumerated) {
        manager->enumerated = true;

        // FIXME: never listed devices if error on enumeration (unlink the real refresh)
        r = tyd_monitor_list(manager->monitor, device_callback, manager);
        if (r < 0)
            return r;

        return 0;
    }

    r = tyd_monitor_refresh(manager->monitor);
    if (r < 0)
        return r;

    ty_mutex_lock(&manager->refresh_mutex);
    ty_cond_broadcast(&manager->refresh_cond);
    ty_mutex_unlock(&manager->refresh_mutex);

    return 0;
}

int tyb_monitor_wait(tyb_monitor *manager, tyb_monitor_wait_func *f, void *udata, int timeout)
{
    assert(manager);

    ty_descriptor_set set = {0};
    uint64_t start;
    int r;

    tyb_monitor_get_descriptors(manager, &set, 1);

    start = ty_millis();
    do {
        r = tyb_monitor_refresh(manager);
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

int tyb_monitor_list(tyb_monitor *manager, tyb_monitor_callback_func *f, void *udata)
{
    assert(manager);
    assert(f);

    ty_list_foreach(cur, &manager->boards) {
        tyb_board *board = ty_container_of(cur, tyb_board, list);
        int r;

        if (board->state == TYB_BOARD_STATE_ONLINE) {
            r = (*f)(board, TYB_MONITOR_EVENT_ADDED, udata);
            if (r)
                return r;
        }
    }

    return 0;
}

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

const tyb_board_model *tyb_board_model_guess(const tyb_firmware *f)
{
    assert(f);

    for (const tyb_board_family **cur = tyb_board_families; *cur; cur++) {
        const tyb_board_family *family = *cur;

        const tyb_board_model *model = (*family->guess_model)(f);
        if (model)
            return model;
    }

    return NULL;
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

        ty_mutex_release(&board->mutex);

        free(board->tag);
        free(board->location);

        ty_list_foreach(cur, &board->interfaces) {
            tyb_board_interface *iface = ty_container_of(cur, tyb_board_interface, list);

            if (iface->hnode.next)
                ty_htable_remove(&iface->hnode);

            tyb_board_interface_unref(iface);
        }
    }

    free(board);
}

void tyb_board_lock(const tyb_board *board)
{
    assert(board);

    ty_mutex_lock(&((tyb_board *)board)->mutex);
}

void tyb_board_unlock(const tyb_board *board)
{
    assert(board);

    ty_mutex_unlock(&((tyb_board *)board)->mutex);
}

static int parse_tag(const char *id, char **rlocation, uint64_t *rserial)
{
    char *location = NULL;
    uint64_t serial = 0;
    char *ptr;
    int r;

    ptr = strchr(id, '#');

    if (ptr != id) {
        if (ptr) {
            location = strndup(id, (size_t)(ptr - id));
        } else {
            location = strdup(id);
        }
        if (!location) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto error;
        }
    }

    if (ptr) {
        char *end;
        serial = strtoull(++ptr, &end, 10);
        if (end == ptr || *end) {
            r = ty_error(TY_ERROR_PARAM, "#<serial> must be a number");
            goto error;
        }
    }

    *rlocation = location;
    *rserial = serial;
    return 0;

error:
    free(location);
    return r;
}

int tyb_board_matches_tag(tyb_board *board, const char *id)
{
    assert(board);

    if (!id || !*id)
        return 1;

    char *location = NULL;
    uint64_t serial = 0;
    int r;

    r = parse_tag(id, &location, &serial);
    if (r < 0)
        return r;

    r = 0;
    if (location && strcmp(location, board->location) != 0)
        goto cleanup;
    if (serial && serial != board->serial)
        goto cleanup;

    r = 1;
cleanup:
    free(location);
    return r;
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

tyb_monitor *tyb_board_get_manager(const tyb_board *board)
{
    assert(board);
    return board->manager;
}

tyb_board_state tyb_board_get_state(const tyb_board *board)
{
    assert(board);
    return board->state;
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

tyb_board_interface *tyb_board_get_interface(const tyb_board *board, tyb_board_capability cap)
{
    assert(board);
    assert((int)cap < (int)TY_COUNTOF(board->cap2iface));

    tyb_board_interface *iface;

    tyb_board_lock(board);

    iface = board->cap2iface[cap];
    if (iface)
        tyb_board_interface_ref(iface);

    tyb_board_unlock(board);

    return iface;
}

int tyb_board_get_capabilities(const tyb_board *board)
{
    assert(board);
    return board->capabilities;
}

uint64_t tyb_board_get_serial_number(const tyb_board *board)
{
    assert(board);
    return board->serial;
}

tyd_device *tyb_board_get_device(const tyb_board *board, tyb_board_capability cap)
{
    assert(board);

    tyb_board_interface *iface;
    tyd_device *dev;

    iface = tyb_board_get_interface(board, cap);
    if (!iface)
        return NULL;

    dev = iface->dev;

    tyb_board_interface_unref(iface);
    return dev;
}

tyd_handle *tyb_board_get_handle(const tyb_board *board, tyb_board_capability cap)
{
    assert(board);

    tyb_board_interface *iface;
    tyd_handle *h;

    iface = tyb_board_get_interface(board, cap);
    if (!iface)
        return NULL;

    h = iface->h;

    tyb_board_interface_unref(iface);
    return h;
}

void tyb_board_get_descriptors(const tyb_board *board, tyb_board_capability cap, struct ty_descriptor_set *set, int id)
{
    assert(board);

    tyb_board_interface *iface = tyb_board_get_interface(board, cap);
    if (!iface)
        return;

    tyd_device_get_descriptors(iface->h, set, id);
    tyb_board_interface_unref(iface);
}

int tyb_board_list_interfaces(tyb_board *board, tyb_board_list_interfaces_func *f, void *udata)
{
    assert(board);
    assert(f);

    int r;

    tyb_board_lock(board);

    ty_list_foreach(cur, &board->interfaces) {
        tyb_board_interface *iface = ty_container_of(cur, tyb_board_interface, list);

        r = (*f)(iface, udata);
        if (r)
            goto cleanup;
    }

    r = 0;
cleanup:
    tyb_board_unlock(board);
    return r;
}

struct wait_for_context {
    tyb_board *board;
    tyb_board_capability capability;
};

static int wait_for_callback(tyb_monitor *manager, void *udata)
{
    TY_UNUSED(manager);

    struct wait_for_context *ctx = udata;

    if (ctx->board->state == TYB_BOARD_STATE_DROPPED)
        return ty_error(TY_ERROR_NOT_FOUND, "Board has disappeared");

    return tyb_board_has_capability(ctx->board, ctx->capability);
}

int tyb_board_wait_for(tyb_board *board, tyb_board_capability capability, bool parallel, int timeout)
{
    assert(board);

    tyb_monitor *manager = board->manager;

    struct wait_for_context ctx;
    int r;

    if (!manager)
        return ty_error(TY_ERROR_NOT_FOUND, "Board has disappeared");

    ctx.board = board;
    ctx.capability = capability;

    if (parallel) {
        uint64_t start;

        ty_mutex_lock(&manager->refresh_mutex);

        start = ty_millis();
        while (!(r = wait_for_callback(manager, &ctx))) {
            r = ty_cond_wait(&manager->refresh_cond, &manager->refresh_mutex, ty_adjust_timeout(timeout, start));
            if (!r)
                break;
        }

        ty_mutex_unlock(&manager->refresh_mutex);

        return r;
    } else {
        return tyb_monitor_wait(manager, wait_for_callback, &ctx, timeout);
    }
}

int tyb_board_serial_set_attributes(tyb_board *board, uint32_t rate, int flags)
{
    assert(board);

    tyb_board_interface *iface;
    int r;

    iface = tyb_board_get_interface(board, TYB_BOARD_CAPABILITY_SERIAL);
    if (!iface)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    r = (*iface->vtable->serial_set_attributes)(iface, rate, flags);

    tyb_board_interface_unref(iface);
    return r;
}

ssize_t tyb_board_serial_read(tyb_board *board, char *buf, size_t size, int timeout)
{
    assert(board);
    assert(buf);
    assert(size);

    tyb_board_interface *iface;
    ssize_t r;

    iface = tyb_board_get_interface(board, TYB_BOARD_CAPABILITY_SERIAL);
    if (!iface)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    r = (*iface->vtable->serial_read)(iface, buf, size, timeout);

    tyb_board_interface_unref(iface);
    return r;
}

ssize_t tyb_board_serial_write(tyb_board *board, const char *buf, size_t size)
{
    assert(board);
    assert(buf);

    tyb_board_interface *iface;
    ssize_t r;

    iface = tyb_board_get_interface(board, TYB_BOARD_CAPABILITY_SERIAL);
    if (!iface)
        return ty_error(TY_ERROR_MODE, "Serial transfer is not available in this mode");

    if (!size)
        size = strlen(buf);

    r = (*iface->vtable->serial_write)(iface, buf, size);

    tyb_board_interface_unref(iface);
    return r;
}

int tyb_board_upload(tyb_board *board, tyb_firmware *f, int flags, tyb_board_upload_progress_func *pf, void *udata)
{
    assert(board);
    assert(f);

    tyb_board_interface *iface;
    int r;

    iface = tyb_board_get_interface(board, TYB_BOARD_CAPABILITY_UPLOAD);
    if (!iface) {
        r = ty_error(TY_ERROR_MODE, "Firmware upload is not available in this mode");
        goto cleanup;
    }

    if (!model_is_valid(board->model)) {
        r = ty_error(TY_ERROR_MODE, "Cannot upload to unknown board model");
        goto cleanup;
    }

    // FIXME: detail error message (max allowed, ratio)
    if (f->size > board->model->code_size) {
        r = ty_error(TY_ERROR_RANGE, "Firmware is too big for %s", board->model->name);
        goto cleanup;
    }

    if (!(flags & TYB_BOARD_UPLOAD_NOCHECK)) {
        const tyb_board_model *guess;

        guess = tyb_board_model_guess(f);
        if (!guess) {
            r = ty_error(TY_ERROR_FIRMWARE, "This firmware was not compiled for a known device");
            goto cleanup;
        }

        if (guess != board->model) {
            r = ty_error(TY_ERROR_FIRMWARE, "This firmware was compiled for %s", guess->name);
            goto cleanup;
        }
    }

    r = (*iface->vtable->upload)(iface, f, flags, pf, udata);

cleanup:
    tyb_board_interface_unref(iface);
    return r;
}

int tyb_board_reset(tyb_board *board)
{
    assert(board);

    tyb_board_interface *iface;
    int r;

    iface = tyb_board_get_interface(board, TYB_BOARD_CAPABILITY_RESET);
    if (!iface)
        return ty_error(TY_ERROR_MODE, "Cannot reset in this mode");

    r = (*iface->vtable->reset)(iface);

    tyb_board_interface_unref(iface);
    return r;
}

int tyb_board_reboot(tyb_board *board)
{
    assert(board);

    tyb_board_interface *iface;
    int r;

    iface = tyb_board_get_interface(board, TYB_BOARD_CAPABILITY_REBOOT);
    if (!iface)
        return ty_error(TY_ERROR_MODE, "Cannot reboot in this mode");

    r = (*iface->vtable->reboot)(iface);

    tyb_board_interface_unref(iface);
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
    }

    free(iface);
}

const char *tyb_board_interface_get_desc(const tyb_board_interface *iface)
{
    assert(iface);
    return iface->desc;
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

    tyd_device_get_descriptors(iface->h, set, id);
}
