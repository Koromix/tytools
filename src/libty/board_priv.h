/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_BOARD_PRIV_H
#define TY_BOARD_PRIV_H

#include "util.h"
#include "ty/board.h"
#include "hs/device.h"
#include "htable.h"
#include "list.h"
#include "ty/task.h"
#include "ty/thread.h"

TY_C_BEGIN

struct _ty_board_interface_vtable {
    int (*open_interface)(ty_board_interface *iface);
    void (*close_interface)(ty_board_interface *iface);

    int (*serial_set_attributes)(ty_board_interface *iface, uint32_t rate, int flags);
    ssize_t (*serial_read)(ty_board_interface *iface, char *buf, size_t size, int timeout);
    ssize_t (*serial_write)(ty_board_interface *iface, const char *buf, size_t size);

    int (*upload)(ty_board_interface *iface, struct ty_firmware *fw, ty_board_upload_progress_func *pf, void *udata);
    int (*reset)(ty_board_interface *iface);
    int (*reboot)(ty_board_interface *iface);
};

struct ty_board_interface {
    const struct _ty_board_interface_vtable *vtable;
    unsigned int refcount;

    ty_htable_head monitor_hnode;
    ty_board *board;
    ty_list_head board_node;

    const char *name;
    int capabilities;
    const ty_board_model *model;

    hs_device *dev;
    ty_mutex open_lock;
    unsigned int open_count;
    hs_handle *h;
};

struct ty_board {
    unsigned int refcount;

    struct ty_monitor *monitor;
    ty_list_head monitor_node;

    ty_board_state state;
    ty_list_head missing_node;
    uint64_t missing_since;

    const ty_board_model *model;
    char *id;
    char *tag;
    uint16_t vid;
    uint16_t pid;
    uint64_t serial;
    char *description;
    char *location;

    ty_mutex interfaces_lock;
    ty_list_head interfaces;
    int capabilities;
    ty_board_interface *cap2iface[16];

    ty_task *current_task;

    void *udata;
};

struct ty_board_family {
    const char *name;

    const ty_board_model **models;

    int (*load_interface)(ty_board_interface *iface);
    int (*update_board)(ty_board_interface *iface, ty_board *board);

    unsigned int (*guess_models)(const struct ty_firmware *fw,
                                 const ty_board_model **rmodels, unsigned int max);
};

#define TY_BOARD_MODEL \
    const ty_board_family *family; \
    \
    const char *name; \
    const char *mcu; \
    \
    size_t code_size;

TY_C_END

#endif
