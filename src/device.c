/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include <unistd.h>
#include "ty/device.h"
#include "device_priv.h"
#include "ty/system.h"

struct tyd_monitor {
    TYD_MONITOR
};

struct tyd_handle {
    TYD_HANDLE
};

struct callback {
    ty_list_head list;
    int id;

    tyd_device_callback_func *f;
    void *udata;
};

int _tyd_device_monitor_init(tyd_monitor *monitor)
{
    int r;

    ty_list_init(&monitor->callbacks);

    r = ty_htable_init(&monitor->devices, 64);
    if (r < 0)
        return r;

    return 0;
}

void _tyd_device_monitor_release(tyd_monitor *monitor)
{
    ty_list_foreach(cur, &monitor->callbacks) {
        struct callback *callback = ty_container_of(cur, struct callback, list);
        free(callback);
    }

    ty_htable_foreach(cur, &monitor->devices) {
        tyd_device *dev = ty_container_of(cur, tyd_device, hnode);

        dev->monitor = NULL;
        tyd_device_unref(dev);
    }
    ty_htable_release(&monitor->devices);
}

void tyd_monitor_set_udata(tyd_monitor *monitor, void *udata)
{
    assert(monitor);
    monitor->udata = udata;
}

void *tyd_monitor_get_udata(const tyd_monitor *monitor)
{
    assert(monitor);
    return monitor->udata;
}

int tyd_monitor_register_callback(tyd_monitor *monitor, tyd_device_callback_func *f, void *udata)
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

static void drop_callback(struct callback *callback)
{
    ty_list_remove(&callback->list);
    free(callback);
}

void tyd_monitor_deregister_callback(tyd_monitor *monitor, int id)
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

static int trigger_callbacks(tyd_device *dev, tyd_monitor_event event)
{
    ty_list_foreach(cur, &dev->monitor->callbacks) {
        struct callback *callback = ty_container_of(cur, struct callback, list);
        int r;

        r = (*callback->f)(dev, event, callback->udata);
        if (r < 0)
            return r;
        if (r) {
            ty_list_remove(&callback->list);
            free(callback);
        }
    }

    return 0;
}

int _tyd_device_monitor_add(tyd_monitor *monitor, tyd_device *dev)
{
    ty_htable_foreach_hash(cur, &monitor->devices, ty_htable_hash_str(dev->key)) {
        tyd_device *dev2 = ty_container_of(cur, tyd_device, hnode);

        if (strcmp(dev2->key, dev->key) == 0 && dev2->iface == dev->iface)
            return 0;
    }

    tyd_device_ref(dev);

    dev->monitor = monitor;
    ty_htable_add(&monitor->devices, ty_htable_hash_str(dev->key), &dev->hnode);

    return trigger_callbacks(dev, TYD_MONITOR_EVENT_ADDED);
}

void _tyd_device_monitor_remove(tyd_monitor *monitor, const char *key)
{
    ty_htable_foreach_hash(cur, &monitor->devices, ty_htable_hash_str(key)) {
        tyd_device *dev = ty_container_of(cur, tyd_device, hnode);

        if (strcmp(dev->key, key) == 0) {
            trigger_callbacks(dev, TYD_MONITOR_EVENT_REMOVED);

            ty_htable_remove(&dev->hnode);
            dev->monitor = NULL;

            tyd_device_unref(dev);
        }
    }
}

int tyd_monitor_list(tyd_monitor *monitor, tyd_device_callback_func *f, void *udata)
{
    assert(monitor);
    assert(f);

    ty_htable_foreach(cur, &monitor->devices) {
        tyd_device *dev = ty_container_of(cur, tyd_device, hnode);
        int r;

        r = (*f)(dev, TYD_MONITOR_EVENT_ADDED, udata);
        if (r)
            return r;
    }

    return 0;
}

tyd_device *tyd_device_ref(tyd_device *dev)
{
    assert(dev);

    __atomic_fetch_add(&dev->refcount, 1, __ATOMIC_RELAXED);
    return dev;
}

void tyd_device_unref(tyd_device *dev)
{
    if (dev) {
        if (__atomic_fetch_sub(&dev->refcount, 1, __ATOMIC_RELEASE) > 1)
            return;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);

        free(dev->key);
        free(dev->location);
        free(dev->path);
        free(dev->serial);
    }

    free(dev);
}

void tyd_device_set_udata(tyd_device *dev, void *udata)
{
    assert(dev);
    dev->udata = udata;
}

void *tyd_device_get_udata(const tyd_device *dev)
{
    assert(dev);
    return dev->udata;
}

int tyd_device_open(tyd_device *dev, tyd_handle **rh)
{
    assert(dev);
    assert(rh);

    return (*dev->vtable->open)(dev, rh);
}

void tyd_device_close(tyd_handle *h)
{
    if (!h)
        return;

    (*h->dev->vtable->close)(h);
}

void tyd_device_get_descriptors(const tyd_handle *h, ty_descriptor_set *set, int id)
{
    assert(h);
    assert(set);

    (*h->dev->vtable->get_descriptors)(h, set, id);
}

tyd_device_type tyd_device_get_type(const tyd_device *dev)
{
    assert(dev);
    return dev->type;
}

const char *tyd_device_get_location(const tyd_device *dev)
{
    assert(dev);
    return dev->location;
}

const char *tyd_device_get_path(const tyd_device *dev)
{
    assert(dev);
    return dev->path;
}

uint16_t tyd_device_get_vid(const tyd_device *dev)
{
    assert(dev);
    return dev->vid;
}

uint16_t tyd_device_get_pid(const tyd_device *dev)
{
    assert(dev);
    return dev->pid;
}

const char *tyd_device_get_serial_number(const tyd_device *dev)
{
    assert(dev);
    return dev->serial;
}

uint8_t tyd_device_get_interface_number(const tyd_device *dev)
{
    assert(dev);
    return dev->iface;
}
