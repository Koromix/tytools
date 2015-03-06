/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_BOARD_PRIV_H
#define TY_BOARD_PRIV_H

#include "ty/common.h"
#include <pthread.h>
#include "ty/board.h"
#include "htable.h"
#include "list.h"

TY_C_BEGIN

struct _ty_board_interface_vtable {
    int (*serial_set_attributes)(ty_board_interface *iface, uint32_t rate, uint16_t flags);
    ssize_t (*serial_read)(ty_board_interface *iface, char *buf, size_t size, int timeout);
    ssize_t (*serial_write)(ty_board_interface *iface, const char *buf, size_t size);

    int (*reset)(ty_board_interface *iface);
    int (*upload)(ty_board_interface *iface, struct ty_firmware *firmware, uint16_t flags, ty_board_upload_progress_func *pf, void *udata);

    int (*reboot)(ty_board_interface *iface);
};

struct ty_board_interface {
    ty_htable_head hnode;

    ty_board *board;
    ty_list_head list;

    volatile unsigned int refcount;

    const struct _ty_board_interface_vtable *vtable;

    const char *desc;

    const ty_board_model *model;
    uint64_t serial;

    ty_device *dev;
    ty_handle *h;

    uint16_t capabilities;
};

struct _ty_board_vendor {
    int (*open_interface)(ty_board_interface *iface);
};

struct ty_board {
    ty_board_manager *manager;
    ty_list_head list;

    volatile unsigned int refcount;
    pthread_mutex_t mutex;
    bool mutex_init;

    ty_board_state state;

    char *identity;
    char *location;

    uint16_t vid;
    uint16_t pid;
    uint64_t serial;

    ty_list_head interfaces;

    uint16_t capabilities;
    ty_board_interface *cap2iface[16];

    ty_list_head missing;
    uint64_t missing_since;

    const ty_board_model *model;

    void *udata;
};

struct _ty_board_model_vtable {
};

#define TY_BOARD_MODEL \
    const char *name; \
    const char *mcu; \
    const char *desc; \
    \
    const struct _ty_board_model_vtable *vtable; \
    \
    size_t code_size;

TY_C_END

#endif
