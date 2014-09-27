/**
 * ty, command-line program to manage Teensy devices
 * http://github.com/Koromix/ty
 * Copyright (C) 2014 Niels Martignène
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ty/common.h"
#include "compat.h"
#include <unistd.h>
#include "ty/device.h"
#include "device_priv.h"

struct ty_device_monitor {
    struct ty_device_monitor_;
};

struct callback {
    ty_list_head list;
    int id;

    ty_device_callback_func *f;
    void *udata;
};

int _ty_device_monitor_init(ty_device_monitor *monitor)
{
    int r;

    ty_list_init(&monitor->callbacks);

    r = ty_htable_init(&monitor->devices, 64);
    if (r < 0)
        return r;

    return 0;
}

void _ty_device_monitor_release(ty_device_monitor *monitor)
{
    ty_list_foreach(cur, &monitor->callbacks) {
        struct callback *callback = ty_list_entry(cur, struct callback, list);
        free(callback);
    }

    ty_htable_foreach(cur, &monitor->devices) {
        ty_device *dev = ty_htable_entry(cur, ty_device, table);

        dev->monitor = NULL;
        ty_device_unref(dev);
    }
    ty_htable_release(&monitor->devices);
}

void ty_device_monitor_set_udata(ty_device_monitor *monitor, void *udata)
{
    assert(monitor);
    monitor->udata = udata;
}

void *ty_device_monitor_get_udata(ty_device_monitor *monitor)
{
    assert(monitor);
    return monitor->udata;
}

int ty_device_monitor_register_callback(ty_device_monitor *monitor, ty_device_callback_func *f, void *udata)
{
    assert(monitor);
    assert(f);

    struct callback *callback = calloc(1, sizeof(*callback));
    if (!callback)
        return ty_error(TY_ERROR_MEMORY, NULL);

    callback->id = monitor->callback_id++;
    callback->f = f;
    callback->udata = udata;

    ty_list_append(&monitor->callbacks, &callback->list);

    return callback->id;
}

static void drop_callback(struct callback *callback)
{
    ty_list_remove(&callback->list);
    free(callback);
}

void ty_device_monitor_deregister_callback(ty_device_monitor *monitor, int id)
{
    assert(monitor);
    assert(id >= 0);

    ty_list_foreach(cur, &monitor->callbacks) {
        struct callback *callback = ty_list_entry(cur, struct callback, list);
        if (callback->id == id) {
            drop_callback(callback);
            break;
        }
    }
}

static ty_device *find_device(ty_device_monitor *monitor, const char *key)
{
    ty_htable_foreach_hash(cur, &monitor->devices, ty_htable_hash_str(key)) {
        ty_device *dev = ty_htable_entry(cur, ty_device, table);

        if (strcmp(dev->key, key) == 0)
            return dev;
    }

    return NULL;
}

static int trigger_callbacks(ty_device *dev, ty_device_event event)
{
    ty_list_foreach(cur, &dev->monitor->callbacks) {
        struct callback *callback = ty_list_entry(cur, struct callback, list);
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

int _ty_device_monitor_add(ty_device_monitor *monitor, ty_device *dev)
{
    if (find_device(monitor, dev->key))
        return 0;

    ty_device_ref(dev);

    dev->monitor = monitor;
    ty_htable_add(&monitor->devices, ty_htable_hash_str(dev->key), &dev->table);

    return trigger_callbacks(dev, TY_DEVICE_EVENT_ADDED);
}

void _ty_device_monitor_remove(ty_device_monitor *monitor, const char *key)
{
    ty_device *dev;

    dev = find_device(monitor, key);
    if (!dev)
        return;

    trigger_callbacks(dev, TY_DEVICE_EVENT_REMOVED);

    ty_htable_remove(&dev->table);
    dev->monitor = NULL;

    ty_device_unref(dev);
}

int ty_device_monitor_list(ty_device_monitor *monitor, ty_device_callback_func *f, void *udata)
{
    assert(monitor);
    assert(f);

    ty_htable_foreach(cur, &monitor->devices) {
        ty_device *dev = ty_htable_entry(cur, ty_device, table);
        int r;

        r = (*f)(dev, TY_DEVICE_EVENT_ADDED, udata);
        if (r)
            return r;
    }

    return 0;
}

ty_device *ty_device_ref(ty_device *dev)
{
    assert(dev);

    dev->refcount++;
    return dev;
}

void ty_device_unref(ty_device *dev)
{
    if (!dev)
        return;

    if (dev->refcount)
        dev->refcount--;

    if (!dev->monitor && !dev->refcount) {
        free(dev->key);
        free(dev->location);
        free(dev->path);
        free(dev->serial);

        free(dev);
    }
}

void ty_device_set_udata(ty_device *dev, void *udata)
{
    assert(dev);
    dev->udata = udata;
}

void *ty_device_get_udata(ty_device *dev)
{
    assert(dev);
    return dev->udata;
}

ty_device_type ty_device_get_type(ty_device *dev)
{
    assert(dev);
    return dev->type;
}

const char *ty_device_get_location(ty_device *dev)
{
    assert(dev);
    return dev->location;
}

const char *ty_device_get_path(ty_device *dev)
{
    assert(dev);
    return dev->path;
}

uint16_t ty_device_get_vid(ty_device *dev)
{
    assert(dev);
    return dev->vid;
}

uint16_t ty_device_get_pid(ty_device *dev)
{
    assert(dev);
    return dev->pid;
}

const char *ty_device_get_serial_number(ty_device *dev)
{
    assert(dev);
    return dev->serial;
}

uint8_t ty_device_get_interface_number(ty_device *dev)
{
    assert(dev);
    return dev->iface;
}
