/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "../libhs/device.h"
#include "../libhs/array.h"
#include "../libhs/monitor.h"
#include "board_priv.h"
#include "class_priv.h"
#include "monitor.h"
#include "system.h"
#include "timer.h"

struct callback {
    int id;
    ty_monitor_callback_func *f;
    void *udata;
};

struct ty_monitor {
    int drop_delay;

    bool started;
    hs_monitor *device_monitor;
    ty_timer *timer;
    bool timer_running;

    _HS_ARRAY(struct callback) callbacks;
    int current_callback_id;

    ty_mutex refresh_mutex;
    ty_cond refresh_cond;
    int refresh_callback_ret;

    _HS_ARRAY(ty_board *) boards;
    _hs_htable ifaces;

    ty_thread_id main_thread_id;
};

#define DROP_BOARD_DELAY 15000

static int change_board_status(ty_board *board, ty_board_status status, ty_monitor_event event)
{
    ty_monitor *monitor = board->monitor;
    int r = 0;

    // Set new board status, engage drop timer if needed
    if (status == TY_BOARD_STATUS_MISSING && status != board->status) {
        board->status = TY_BOARD_STATUS_MISSING;
        board->missing_since = ty_millis();

        if (!monitor->timer_running) {
            int timer_delay = ty_adjust_timeout(monitor->drop_delay, board->missing_since);
            r = ty_timer_set(monitor->timer, timer_delay, TY_TIMER_ONESHOT);
            if (r < 0)
                return r;
            monitor->timer_running = true;
        }
    } else {
        board->status = status;
    }

    /* Notify callbacks and do some additional stuff as we go:
       - Drop callback that return r > 0
       - Stop calling them is one returns r < 0 */
    size_t remove_count = 0;
    for (size_t i = 0; i < monitor->callbacks.count; i++) {
        struct callback *callback_it = &monitor->callbacks.values[i - remove_count];
        if (remove_count)
            *callback_it = monitor->callbacks.values[i];

        if (!r) {
            r = (*callback_it->f)(board, event, callback_it->udata);
            if (r > 0) {
                remove_count++;
                r = 0;
            }
        }
    }
    monitor->callbacks.count -= remove_count;

    return r;
}

static int create_board(ty_monitor *monitor, ty_board_interface *iface, ty_board **rboard)
{
    ty_board *board;
    int r;

    board = calloc(1, sizeof(*board));
    if (!board) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    board->refcount = 1;

    board->location = strdup(iface->dev->location);
    if (!board->location) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    r = ty_mutex_init(&board->ifaces_lock);
    if (r < 0)
        goto error;

    board->vid = iface->dev->vid;
    board->pid = iface->dev->pid;

    r = (*iface->class_vtable->update_board)(iface, board, true);
    if (r <= 0)
        goto error;
    board->tag = board->id;

    board->monitor = monitor;
    r = _hs_array_push(&monitor->boards, board);
    if (r < 0) {
        r = ty_libhs_translate_error(r);
        goto error;
    }

    *rboard = board;
    return 1;

error:
    ty_board_unref(board);
    return r;
}

static int close_board(ty_board *board)
{
    _HS_ARRAY(ty_board_interface *) ifaces;
    int r;

    // Reset capabilities to 0, except TY_BOARD_CAPABILITY_UNIQUE
    ty_mutex_lock(&board->ifaces_lock);
    _hs_array_move(&board->ifaces, &ifaces);
    memset(board->cap2iface, 0, sizeof(board->cap2iface));
    board->capabilities &= 1 << TY_BOARD_CAPABILITY_UNIQUE;
    ty_mutex_unlock(&board->ifaces_lock);

    // Set missing board status
    r = change_board_status(board, TY_BOARD_STATUS_MISSING, TY_MONITOR_EVENT_DISAPPEARED);

    // Drop all remaining interfaces (sometimes we close even though some interfaces remain)
    for (size_t i = 0; i < ifaces.count; i++) {
        ty_board_interface *iface_it = ifaces.values[i];

        if (iface_it->monitor_hnode.next)
            _hs_htable_remove(&iface_it->monitor_hnode);
        ty_board_interface_unref(iface_it);
    }
    _hs_array_release(&ifaces);

    return r;
}

static void drop_board(ty_board *board)
{
    ty_monitor *monitor = board->monitor;

    // Change board status
    change_board_status(board, TY_BOARD_STATUS_DROPPED, TY_MONITOR_EVENT_DROPPED);

    // Remove this board from the monitor list
    board->monitor = NULL;
    for (size_t i = 0; i < monitor->boards.count; i++) {
        if (monitor->boards.values[i] == board)
            _hs_array_remove(&monitor->boards, i, 1);
    }
}

static ty_board *find_monitor_board(ty_monitor *monitor, const char *location)
{
    for (size_t i = 0; i < monitor->boards.count; i++) {
        ty_board *board_it = monitor->boards.values[i];

        if (strcmp(board_it->location, location) == 0)
            return board_it;
    }

    return NULL;
}

static ty_board_interface *find_monitor_interface(ty_monitor *monitor, hs_device *dev)
{
   _hs_htable_foreach_hash(cur, &monitor->ifaces, _hs_htable_hash_ptr(dev)) {
        ty_board_interface *iface = ty_container_of(cur, ty_board_interface, monitor_hnode);

        if (iface->dev == dev)
            return iface;
    }

    return NULL;
}

static int open_new_interface(hs_device *dev, ty_board_interface **riface)
{
    const struct _ty_class_vtable *class_vtable;
    ty_board_interface *iface;
    int r;

    class_vtable = dev->match_udata;
    // This particular device match was disabled by the user
    if (!class_vtable)
        return 0;

    iface = calloc(1, sizeof(*iface));
    if (!iface) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    iface->refcount = 1;

    r = ty_mutex_init(&iface->open_lock);
    if (r < 0)
        goto error;
    iface->dev = hs_device_ref(dev);

    ty_error_mask(TY_ERROR_NOT_FOUND);
    r = (*class_vtable->load_interface)(iface);
    ty_error_unmask();
    if (r <= 0) {
        if (r == TY_ERROR_NOT_FOUND || r == TY_ERROR_ACCESS)
            r = 0;
        goto error;
    }

    *riface = iface;
    return 1;

error:
    ty_board_interface_unref(iface);
    return r;
}

static int update_or_create_board(ty_monitor *monitor, ty_board_interface *iface,
                                  ty_board **rboard, ty_monitor_event *revent)
{
    ty_board *board;
    ty_monitor_event event;
    int r;

    board = find_monitor_board(monitor, iface->dev->location);

    if (board) {
        // Update board information
        bool update_tag_pointer = false;
        if (board->tag == board->id)
            update_tag_pointer = true;
        r = (*iface->class_vtable->update_board)(iface, board, false);
        if (r < 0)
            return r;
        if (update_tag_pointer)
            board->tag = board->id;

        /* The class function update_board() returns 1 if the interface is compatible with
           this board, or 0 if not. In the latter case, the old board is dropped and a new
           one is used. */
        if (r) {
            if (board->vid != iface->dev->vid || board->pid != iface->dev->pid) {
                /* Theoretically, this should not happen unless device removal notifications
                   where dropped somewhere. */
                if (board->status == TY_BOARD_STATUS_ONLINE)
                    close_board(board);

                board->vid = iface->dev->vid;
                board->pid = iface->dev->pid;
            }

            event = TY_MONITOR_EVENT_CHANGED;
        } else {
            if (board->status == TY_BOARD_STATUS_ONLINE)
                close_board(board);
            drop_board(board);
            ty_board_unref(board);

            r = create_board(monitor, iface, &board);
            if (r <= 0)
                return r;

            event = TY_MONITOR_EVENT_ADDED;
        }
    } else {
        r = create_board(monitor, iface, &board);
        if (r <= 0)
            return r;

        event = TY_MONITOR_EVENT_ADDED;
    }
    iface->board = board;

    *rboard = board;
    if (revent)
        *revent = event;

    return 1;
}

static int register_interface(ty_board *board, ty_board_interface *iface)
{
    int r;

    ty_mutex_lock(&board->ifaces_lock);

    // Add interface to monitor and board
    ty_board_interface_ref(iface);
    r = _hs_array_push(&board->ifaces, iface);
    if (r < 0) {
        r = ty_libhs_translate_error(r);
        goto cleanup;
    }
    _hs_htable_add(&board->monitor->ifaces, _hs_htable_hash_ptr(iface->dev),
                   &iface->monitor_hnode);

    // Update board capabilities
    for (int i = 0; i < (int)TY_COUNTOF(board->cap2iface); i++) {
        if (iface->capabilities & (1 << i))
            board->cap2iface[i] = iface;
    }
    board->capabilities |= iface->capabilities;

    r = 0;
cleanup:
    ty_mutex_unlock(&board->ifaces_lock);
    return r;
}

static int add_interface_for_device(ty_monitor *monitor, hs_device *dev)
{
    ty_board_interface *iface = NULL;
    ty_board *board = NULL;
    ty_monitor_event event = TY_MONITOR_EVENT_ADDED;
    int r;

    r = open_new_interface(dev, &iface);
    if (r <= 0)
        goto error;
    r = update_or_create_board(monitor, iface, &board, &event);
    if (r <= 0)
        goto error;
    r = register_interface(board, iface);
    if (r < 0)
        goto error;

    return change_board_status(board, TY_BOARD_STATUS_ONLINE, event);

error:
    if (event == TY_MONITOR_EVENT_ADDED)
        ty_board_unref(board);
    ty_board_interface_unref(iface);
    return r;
}

static int remove_interface_with_device(ty_monitor *monitor, hs_device *dev)
{
    ty_board_interface *iface;
    ty_board *board;
    int r;

    // Find interface associated with this device
    iface = find_monitor_interface(monitor, dev);
    if (!iface)
        return 0;
    board = iface->board;

    // Unregister from monitor
    _hs_htable_remove(&iface->monitor_hnode);
    ty_board_interface_unref(iface);

    ty_mutex_lock(&board->ifaces_lock);

    // Unregister from board and update capabilities
    for (size_t i = 0; i < board->ifaces.count; i++) {
        if (board->ifaces.values[i] == iface) {
            _hs_array_remove(&board->ifaces, i, 1);
            break;
        }
    }
    ty_board_interface_unref(iface);
    memset(board->cap2iface, 0, sizeof(board->cap2iface));
    board->capabilities &= 1 << TY_BOARD_CAPABILITY_UNIQUE;
    for (size_t i = 0; i < board->ifaces.count; i++) {
        ty_board_interface *iface_it = board->ifaces.values[i];

        for (unsigned int j = 0; j < TY_COUNTOF(board->cap2iface); j++) {
            if (iface_it->capabilities & (1 << j))
                board->cap2iface[j] = iface_it;
        }
        board->capabilities |= iface_it->capabilities;
    }

    ty_mutex_unlock(&board->ifaces_lock);

    // Change status and trigger callbacks
    if (!board->ifaces.count) {
        r = close_board(board);
    } else {
        r = change_board_status(board, TY_BOARD_STATUS_ONLINE, TY_MONITOR_EVENT_CHANGED);
    }

    return r;
}

static int device_callback(hs_device *dev, void *udata)
{
    ty_monitor *monitor = udata;

    switch (dev->status) {
        case HS_DEVICE_STATUS_ONLINE: {
            monitor->refresh_callback_ret = add_interface_for_device(monitor, dev);
            return !!monitor->refresh_callback_ret;
        } break;

        case HS_DEVICE_STATUS_DISCONNECTED: {
            monitor->refresh_callback_ret = remove_interface_with_device(monitor, dev);
            return !!monitor->refresh_callback_ret;
        } break;
    }

    assert(false);
    return 0;
}

int ty_monitor_new(ty_monitor **rmonitor)
{
    assert(rmonitor);

    ty_monitor *monitor;
    int r;

    monitor = calloc(1, sizeof(*monitor));
    if (!monitor) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    if (getenv("TYTOOLS_DROP_BOARD_DELAY")) {
        monitor->drop_delay = (int)strtol(getenv("TYTOOLS_DROP_BOARD_DELAY"), NULL, 10);
    } else {
        monitor->drop_delay = DROP_BOARD_DELAY;
    }

    r = hs_monitor_new(_ty_class_match_specs, _ty_class_match_specs_count, &monitor->device_monitor);
    if (r < 0) {
        r = ty_libhs_translate_error(r);
        goto error;
    }

    r = ty_timer_new(&monitor->timer);
    if (r < 0)
        goto error;

    r = ty_mutex_init(&monitor->refresh_mutex);
    if (r < 0)
        goto error;

    r = ty_cond_init(&monitor->refresh_cond);
    if (r < 0)
        goto error;

    r = _hs_htable_init(&monitor->ifaces, 64);
    if (r < 0)
        goto error;

    monitor->main_thread_id = ty_thread_get_self_id();

    *rmonitor = monitor;
    return 0;

error:
    ty_monitor_free(monitor);
    return r;
}

void ty_monitor_free(ty_monitor *monitor)
{
    if (monitor) {
        ty_monitor_stop(monitor);

        _hs_array_release(&monitor->callbacks);
        _hs_htable_release(&monitor->ifaces);

        ty_cond_release(&monitor->refresh_cond);
        ty_mutex_release(&monitor->refresh_mutex);
        hs_monitor_free(monitor->device_monitor);
        ty_timer_free(monitor->timer);
    }

    free(monitor);
}

int ty_monitor_start(ty_monitor *monitor)
{
    assert(monitor);

    int r;

    if (monitor->started)
        return 0;

    r = hs_monitor_start(monitor->device_monitor);
    if (r < 0) {
        r = ty_libhs_translate_error(r);
        goto error;
    }
    monitor->started = true;

    r = hs_monitor_list(monitor->device_monitor, device_callback, monitor);
    if (r < 0)
        goto error;

    return 0;

error:
    ty_monitor_stop(monitor);
    return r;
}

void ty_monitor_stop(ty_monitor *monitor)
{
    assert(monitor);

    if (!monitor->started)
        return;

    // Stop device monitor and timer
    hs_monitor_stop(monitor->device_monitor);
    ty_timer_set(monitor->timer, -1, 0);
    monitor->timer_running = false;

    // Clear registered boards
    for (size_t i = 0; i < monitor->boards.count; i++) {
        ty_board *board_it = monitor->boards.values[i];

        board_it->monitor = NULL;
        ty_board_unref(board_it);
    }
    _hs_array_release(&monitor->boards);

    // Clear registered interfaces
    _hs_htable_foreach(cur, &monitor->ifaces) {
        ty_board_interface *iface_it = ty_container_of(cur, ty_board_interface, monitor_hnode);

        if (iface_it->monitor_hnode.next)
            _hs_htable_remove(&iface_it->monitor_hnode);
        ty_board_interface_unref(iface_it);
    }
    _hs_htable_clear(&monitor->ifaces);

    monitor->started = false;
}

void ty_monitor_get_descriptors(const ty_monitor *monitor, ty_descriptor_set *set, int id)
{
    assert(monitor);
    assert(set);

    ty_descriptor_set_add(set, hs_monitor_get_poll_handle(monitor->device_monitor), id);
    ty_timer_get_descriptors(monitor->timer, set, id);
}

int ty_monitor_register_callback(ty_monitor *monitor, ty_monitor_callback_func *f, void *udata)
{
    assert(monitor);
    assert(f);

    struct callback callback = {
        .id = monitor->current_callback_id++,
        .f = f,
        .udata = udata
    };
    return ty_libhs_translate_error(_hs_array_push(&monitor->callbacks, callback));
}

void ty_monitor_deregister_callback(ty_monitor *monitor, int id)
{
    assert(monitor);
    assert(id >= 0);

    for (size_t i = 0; i < monitor->callbacks.count; i++) {
        if (monitor->callbacks.values[i].id == id) {
            _hs_array_remove(&monitor->callbacks, i, 1);
            break;
        }
    }
}

int ty_monitor_refresh(ty_monitor *monitor)
{
    assert(monitor);

    int r;

    if (ty_timer_rearm(monitor->timer)) {
        int timer_delay = -1;

        for (size_t i = 0; i < monitor->boards.count; i++) {
            ty_board *board_it = monitor->boards.values[i];

            if (board_it->status == TY_BOARD_STATUS_MISSING) {
                int board_timeout = ty_adjust_timeout(monitor->drop_delay, board_it->missing_since);
                /* Drop boards that are about to expire (< 20 ms) to deal with limited timer
                   resolution (e.g. TickCount64() on Windows). */
                if (board_timeout < 20) {
                    drop_board(board_it);
                    ty_board_unref(board_it);
                } else if (board_timeout < timer_delay || timer_delay == -1) {
                    timer_delay = board_timeout;
                }
            }
        }

        r = ty_timer_set(monitor->timer, timer_delay, TY_TIMER_ONESHOT);
        if (r < 0)
            return r;
        monitor->timer_running = (timer_delay >= 0);
    }

    r = hs_monitor_refresh(monitor->device_monitor, device_callback, monitor);
    if (r < 0) {
        /* The callback is in libty, and we need a way to get the error code without it
           being converted from a libhs error code. */
        if (monitor->refresh_callback_ret) {
            r = monitor->refresh_callback_ret;
            monitor->refresh_callback_ret = 0;
            return r;
        }

        return ty_libhs_translate_error(r);
    }

    ty_mutex_lock(&monitor->refresh_mutex);
    ty_cond_broadcast(&monitor->refresh_cond);
    ty_mutex_unlock(&monitor->refresh_mutex);

    return 0;
}

int ty_monitor_wait(ty_monitor *monitor, ty_monitor_wait_func *f, void *udata, int timeout)
{
    assert(monitor);
    assert(f || (monitor->main_thread_id == ty_thread_get_self_id()));

    ty_descriptor_set set = {0};
    uint64_t start;
    int r;

    start = ty_millis();
    if (monitor->main_thread_id != ty_thread_get_self_id()) {
        ty_mutex_lock(&monitor->refresh_mutex);
        while (!(r = (*f)(monitor, udata))) {
            r = ty_cond_wait(&monitor->refresh_cond, &monitor->refresh_mutex,
                             ty_adjust_timeout(timeout, start));
            if (!r)
                break;
        }
        ty_mutex_unlock(&monitor->refresh_mutex);

        return r;
    } else {
        ty_monitor_get_descriptors(monitor, &set, 1);

        do {
            r = ty_monitor_refresh(monitor);
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

int ty_monitor_list(ty_monitor *monitor, ty_monitor_callback_func *f, void *udata)
{
    assert(monitor);
    assert(f);

    for (size_t i = 0; i < monitor->boards.count; i++) {
        ty_board *board_it = monitor->boards.values[i];

        if (board_it->status == TY_BOARD_STATUS_ONLINE) {
            int r = (*f)(board_it, TY_MONITOR_EVENT_ADDED, udata);
            if (r)
                return r;
        }
    }

    return 0;
}
