/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_DEVICE_PRIV_H
#define TY_DEVICE_PRIV_H

#include "ty/common.h"
#include "ty/device.h"
#include "htable.h"
#include "list.h"

TY_C_BEGIN

struct ty_descriptor_set;

#define TY_DEVICE_MONITOR \
    ty_list_head callbacks; \
    int callback_id; \
    \
    ty_htable devices; \
    \
    void *udata;

struct _ty_device_vtable {
    int (*open)(ty_device *dev, bool block, ty_handle **rh);
    void (*close)(ty_handle *h);

    void (*get_descriptors)(const ty_handle *h, struct ty_descriptor_set *set, int id);
};

struct ty_device {
    ty_device_monitor *monitor;
    ty_htable_head hnode;

    unsigned int refcount;

    char *key;

    ty_device_type type;
    const struct _ty_device_vtable *vtable;

    char *location;
    char *path;

    uint16_t vid;
    uint16_t pid;
    char *serial;

    uint8_t iface;

    void *udata;
};

#define TY_HANDLE \
    ty_device *dev;

int _ty_device_monitor_init(ty_device_monitor *monitor);
void _ty_device_monitor_release(ty_device_monitor *monitor);

int _ty_device_monitor_add(ty_device_monitor *monitor, ty_device *dev);
void _ty_device_monitor_remove(ty_device_monitor *monitor, const char *key);

TY_C_END

#endif
