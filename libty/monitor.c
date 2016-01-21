/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "ty/common.h"
#include "compat.h"
#include "board_priv.h"
#include "ty/monitor.h"
#include "ty/system.h"
#include "ty/timer.h"

struct tyb_monitor {
    int flags;

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

#define DROP_BOARD_DELAY 15000

static void drop_callback(struct callback *callback)
{
    ty_list_remove(&callback->list);
    free(callback);
}

static int trigger_callbacks(tyb_board *board, tyb_monitor_event event)
{
    ty_list_foreach(cur, &board->monitor->callbacks) {
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

static int add_board(tyb_monitor *monitor, tyb_board_interface *iface, tyb_board **rboard)
{
    tyb_board *board;
    int r;

    board = calloc(1, sizeof(*board));
    if (!board) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    board->refcount = 1;

    board->location = strdup(tyd_device_get_location(iface->dev));
    if (!board->location) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    r = ty_mutex_init(&board->interfaces_lock, TY_MUTEX_FAST);
    if (r < 0)
        goto error;

    ty_list_init(&board->interfaces);

    assert(iface->model);
    board->model = iface->model;
    board->serial = iface->serial;

    board->vid = tyd_device_get_vid(iface->dev);
    board->pid = tyd_device_get_pid(iface->dev);

    r = asprintf(&board->id, "%"PRIu64"-%s", board->serial, board->model->family->name);
    if (r < 0) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    board->tag = board->id;

    board->monitor = monitor;
    ty_list_add_tail(&monitor->boards, &board->list);

    *rboard = board;
    return 0;

error:
    tyb_board_unref(board);
    return r;
}

static void close_board(tyb_board *board)
{
    ty_list_head ifaces;

    ty_mutex_lock(&board->interfaces_lock);

    ty_list_replace(&board->interfaces, &ifaces);
    memset(board->cap2iface, 0, sizeof(board->cap2iface));
    board->capabilities = 0;

    ty_mutex_unlock(&board->interfaces_lock);

    board->state = TYB_BOARD_STATE_MISSING;
    trigger_callbacks(board, TYB_MONITOR_EVENT_DISAPPEARED);

    ty_list_foreach(cur, &ifaces) {
        tyb_board_interface *iface = ty_container_of(cur, tyb_board_interface, list);

        if (iface->hnode.next)
            ty_htable_remove(&iface->hnode);

        tyb_board_interface_unref(iface);
    }
}

static int add_missing_board(tyb_board *board)
{
    tyb_monitor *monitor = board->monitor;

    board->missing_since = ty_millis();
    if (board->missing.prev)
        ty_list_remove(&board->missing);
    ty_list_add_tail(&monitor->missing_boards, &board->missing);

    // There may be other boards waiting to be dropped, set timeout for the next in line
    board = ty_list_get_first(&monitor->missing_boards, tyb_board, missing);

    return ty_timer_set(monitor->timer, ty_adjust_timeout(DROP_BOARD_DELAY, board->missing_since),
                        TY_TIMER_ONESHOT);
}

static void drop_board(tyb_board *board)
{
    if (board->missing.prev)
        ty_list_remove(&board->missing);

    board->state = TYB_BOARD_STATE_DROPPED;
    trigger_callbacks(board, TYB_MONITOR_EVENT_DROPPED);

    ty_list_remove(&board->list);
}

static tyb_board *find_board(tyb_monitor *monitor, const char *location)
{
    ty_list_foreach(cur, &monitor->boards) {
        tyb_board *board = ty_container_of(cur, tyb_board, list);

        if (strcmp(board->location, location) == 0)
            return board;
    }

    return NULL;
}

static int open_new_interface(tyd_device *dev, tyb_board_interface **riface)
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

    r = ty_mutex_init(&iface->open_lock, TY_MUTEX_FAST);
    if (r < 0)
        goto error;

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

static tyb_board_interface *find_interface(tyb_monitor *monitor, tyd_device *dev)
{
    ty_htable_foreach_hash(cur, &monitor->interfaces, ty_htable_hash_ptr(dev)) {
        tyb_board_interface *iface = ty_container_of(cur, tyb_board_interface, hnode);

        if (iface->dev == dev)
            return iface;
    }

    return NULL;
}

static bool iface_is_compatible(tyb_board_interface *iface, tyb_board *board)
{
    if (tyb_board_model_is_real(iface->model) && tyb_board_model_is_real(board->model)
            && iface->model != board->model)
        return false;
    if (iface->serial && board->serial && iface->serial != board->serial)
        return false;

    return true;
}

static int add_interface(tyb_monitor *monitor, tyd_device *dev)
{
    tyb_board_interface *iface = NULL;
    tyb_board *board = NULL;
    tyb_monitor_event event;
    int r;

    r = open_new_interface(dev, &iface);
    if (r <= 0)
        goto error;

    board = find_board(monitor, tyd_device_get_location(dev));

    /* Maybe the device notifications came in the wrong order, or somehow the device
       removal notifications were dropped somewhere and we never got them, so use
       heuristics to improve board change detection. */
    if (board && !iface_is_compatible(iface, board)) {
        if (board->state == TYB_BOARD_STATE_ONLINE)
            close_board(board);
        drop_board(board);

        tyb_board_unref(board);
        board = NULL;
    }

    if (board) {
        if (board->vid != tyd_device_get_vid(dev) || board->pid != tyd_device_get_pid(dev)) {
            if (board->state == TYB_BOARD_STATE_ONLINE)
                close_board(board);

            board->vid = tyd_device_get_vid(dev);
            board->pid = tyd_device_get_pid(dev);
        }

        if (tyb_board_model_is_real(iface->model))
            board->model = iface->model;
        if (iface->serial)
            board->serial = iface->serial;

        event = TYB_MONITOR_EVENT_CHANGED;
    } else {
        r = add_board(monitor, iface, &board);
        if (r < 0)
            goto error;

        event = TYB_MONITOR_EVENT_ADDED;
    }

    iface->board = board;

    ty_mutex_lock(&board->interfaces_lock);

    ty_list_add_tail(&board->interfaces, &iface->list);
    ty_htable_add(&monitor->interfaces, ty_htable_hash_ptr(iface->dev), &iface->hnode);

    for (int i = 0; i < (int)TY_COUNTOF(board->cap2iface); i++) {
        if (iface->capabilities & (1 << i))
            board->cap2iface[i] = iface;
    }
    board->capabilities |= iface->capabilities;

    ty_mutex_unlock(&board->interfaces_lock);

    if (board->missing.prev)
        ty_list_remove(&board->missing);

    board->state = TYB_BOARD_STATE_ONLINE;
    return trigger_callbacks(board, event);

error:
    tyb_board_interface_unref(iface);
    return r;
}

static int remove_interface(tyb_monitor *monitor, tyd_device *dev)
{
    tyb_board_interface *iface;
    tyb_board *board;
    int r;

    iface = find_interface(monitor, dev);
    if (!iface)
        return 0;

    board = iface->board;

    ty_mutex_lock(&board->interfaces_lock);

    ty_htable_remove(&iface->hnode);
    ty_list_remove(&iface->list);

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

    ty_mutex_unlock(&board->interfaces_lock);

    if (ty_list_is_empty(&board->interfaces)) {
        close_board(board);
        r = add_missing_board(board);
    } else {
        r = trigger_callbacks(board, TYB_MONITOR_EVENT_CHANGED);
    }

    tyb_board_interface_unref(iface);

    return r;
}

static int device_callback(tyd_device *dev, tyd_monitor_event event, void *udata)
{
    tyb_monitor *monitor = udata;

    switch (event) {
    case TYD_MONITOR_EVENT_ADDED:
        return add_interface(monitor, dev);

    case TYD_MONITOR_EVENT_REMOVED:
        return remove_interface(monitor, dev);
    }

    assert(false);
    return 0;
}

// FIXME: improve the sequential/parallel API
int tyb_monitor_new(int flags, tyb_monitor **rmonitor)
{
    assert(rmonitor);

    tyb_monitor *monitor;
    int r;

    monitor = calloc(1, sizeof(*monitor));
    if (!monitor) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    monitor->flags = flags;

    r = tyd_monitor_new(&monitor->monitor);
    if (r < 0)
        goto error;

    r = tyd_monitor_register_callback(monitor->monitor, device_callback, monitor);
    if (r < 0)
        goto error;

    r = ty_timer_new(&monitor->timer);
    if (r < 0)
        goto error;

    r = ty_mutex_init(&monitor->refresh_mutex, TY_MUTEX_FAST);
    if (r < 0)
        goto error;

    r = ty_cond_init(&monitor->refresh_cond);
    if (r < 0)
        goto error;

    ty_list_init(&monitor->boards);
    ty_list_init(&monitor->missing_boards);

    r = ty_htable_init(&monitor->interfaces, 64);
    if (r < 0)
        goto error;

    ty_list_init(&monitor->callbacks);

    *rmonitor = monitor;
    return 0;

error:
    tyb_monitor_free(monitor);
    return r;
}

void tyb_monitor_free(tyb_monitor *monitor)
{
    if (monitor) {
        ty_cond_release(&monitor->refresh_cond);
        ty_mutex_release(&monitor->refresh_mutex);

        tyd_monitor_free(monitor->monitor);
        ty_timer_free(monitor->timer);

        ty_list_foreach(cur, &monitor->callbacks) {
            struct callback *callback = ty_container_of(cur, struct callback, list);
            free(callback);
        }

        ty_list_foreach(cur, &monitor->boards) {
            tyb_board *board = ty_container_of(cur, tyb_board, list);
            tyb_board_unref(board);
        }

        ty_htable_release(&monitor->interfaces);
    }

    free(monitor);
}

void tyb_monitor_set_udata(tyb_monitor *monitor, void *udata)
{
    assert(monitor);
    monitor->udata = udata;
}

void *tyb_monitor_get_udata(const tyb_monitor *monitor)
{
    assert(monitor);
    return monitor->udata;
}

void tyb_monitor_get_descriptors(const tyb_monitor *monitor, ty_descriptor_set *set, int id)
{
    assert(monitor);
    assert(set);

    tyd_monitor_get_descriptors(monitor->monitor, set, id);
    ty_timer_get_descriptors(monitor->timer, set, id);
}

int tyb_monitor_register_callback(tyb_monitor *monitor, tyb_monitor_callback_func *f, void *udata)
{
    assert(monitor);
    assert(f);

    struct callback *callback = calloc(1, sizeof(*callback));
    if (!callback)
        return ty_error(TY_ERROR_MEMORY, NULL);

    callback->id = monitor->callback_id++;
    callback->f = f;
    callback->udata = udata;

    ty_list_add_tail(&monitor->callbacks, &callback->list);

    return callback->id;
}

void tyb_monitor_deregister_callback(tyb_monitor *monitor, int id)
{
    assert(monitor);
    assert(id >= 0);

    ty_list_foreach(cur, &monitor->callbacks) {
        struct callback *callback = ty_container_of(cur, struct callback, list);
        if (callback->id == id) {
            drop_callback(callback);
            break;
        }
    }
}

int tyb_monitor_refresh(tyb_monitor *monitor)
{
    assert(monitor);

    int r;

    if (ty_timer_rearm(monitor->timer)) {
        ty_list_foreach(cur, &monitor->missing_boards) {
            tyb_board *board = ty_container_of(cur, tyb_board, missing);
            int timeout;

            timeout = ty_adjust_timeout(DROP_BOARD_DELAY, board->missing_since);
            if (timeout) {
                r = ty_timer_set(monitor->timer, timeout, TY_TIMER_ONESHOT);
                if (r < 0)
                    return r;
                break;
            }

            drop_board(board);
            tyb_board_unref(board);
        }
    }

    if (!monitor->enumerated) {
        monitor->enumerated = true;

        // FIXME: never listed devices if error on enumeration (unlink the real refresh)
        r = tyd_monitor_list(monitor->monitor, device_callback, monitor);
        if (r < 0)
            return r;

        return 0;
    }

    r = tyd_monitor_refresh(monitor->monitor);
    if (r < 0)
        return r;

    ty_mutex_lock(&monitor->refresh_mutex);
    ty_cond_broadcast(&monitor->refresh_cond);
    ty_mutex_unlock(&monitor->refresh_mutex);

    return 0;
}

int tyb_monitor_wait(tyb_monitor *monitor, tyb_monitor_wait_func *f, void *udata, int timeout)
{
    assert(monitor);
    assert(f || !(monitor->flags & TYB_MONITOR_PARALLEL_WAIT));

    ty_descriptor_set set = {0};
    uint64_t start;
    int r;

    start = ty_millis();
    if (monitor->flags & TYB_MONITOR_PARALLEL_WAIT) {
        ty_mutex_lock(&monitor->refresh_mutex);
        while (!(r = (*f)(monitor, udata))) {
            r = ty_cond_wait(&monitor->refresh_cond, &monitor->refresh_mutex, ty_adjust_timeout(timeout, start));
            if (!r)
                break;
        }
        ty_mutex_unlock(&monitor->refresh_mutex);

        return r;
    } else {
        tyb_monitor_get_descriptors(monitor, &set, 1);

        do {
            r = tyb_monitor_refresh(monitor);
            if (r < 0)
                return (int)r;

            if (f) {
                r = (*f)(monitor, udata);
                if (r)
                    return r;
            }

            r = ty_poll(&set, ty_adjust_timeout(timeout, start));
        } while (r > 0);
        return r;
    }
}

int tyb_monitor_list(tyb_monitor *monitor, tyb_monitor_callback_func *f, void *udata)
{
    assert(monitor);
    assert(f);

    ty_list_foreach(cur, &monitor->boards) {
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
