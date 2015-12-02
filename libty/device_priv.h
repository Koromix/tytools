/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_DEVICE_PRIV_H
#define TY_DEVICE_PRIV_H

#include "ty/common.h"
#include "ty/device.h"
#include "htable.h"
#include "list.h"

TY_C_BEGIN

struct ty_descriptor_set;

#define TYD_MONITOR \
    ty_list_head callbacks; \
    int callback_id; \
    \
    ty_htable devices; \
    \
    void *udata;

struct _tyd_device_vtable {
    int (*open)(tyd_device *dev, tyd_handle **rh);
    void (*close)(tyd_handle *h);

    void (*get_descriptors)(const tyd_handle *h, struct ty_descriptor_set *set, int id);
};

struct tyd_device {
    tyd_monitor *monitor;
    ty_htable_head hnode;

    volatile unsigned int refcount;

    char *key;

    tyd_device_type type;
    const struct _tyd_device_vtable *vtable;

    char *location;
    char *path;

    uint16_t vid;
    uint16_t pid;
    char *serial;

    uint8_t iface;

    void *udata;
};

#define TYD_HANDLE \
    tyd_device *dev;

int _tyd_monitor_init(tyd_monitor *monitor);
void _tyd_monitor_release(tyd_monitor *monitor);

int _tyd_monitor_add(tyd_monitor *monitor, tyd_device *dev);
void _tyd_monitor_remove(tyd_monitor *monitor, const char *key);

TY_C_END

#endif
