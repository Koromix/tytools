/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_BOARD_PRIV_H
#define TY_BOARD_PRIV_H

#include "ty/common.h"
#include "ty/board.h"
#include "htable.h"
#include "list.h"
#include "ty/task.h"
#include "ty/thread.h"

TY_C_BEGIN

struct _tyb_board_interface_vtable {
    int (*serial_set_attributes)(tyb_board_interface *iface, uint32_t rate, int flags);
    ssize_t (*serial_read)(tyb_board_interface *iface, char *buf, size_t size, int timeout);
    ssize_t (*serial_write)(tyb_board_interface *iface, const char *buf, size_t size);

    int (*upload)(tyb_board_interface *iface, struct tyb_firmware *fw, tyb_board_upload_progress_func *pf, void *udata);
    int (*reset)(tyb_board_interface *iface);
    int (*reboot)(tyb_board_interface *iface);
};

struct tyb_board_interface {
    ty_htable_head hnode;

    tyb_board *board;
    ty_list_head list;

    volatile unsigned int refcount;

    const struct _tyb_board_interface_vtable *vtable;

    const char *desc;

    const tyb_board_model *model;
    uint64_t serial;

    tyd_device *dev;
    tyd_handle *h;

    int capabilities;
};

struct tyb_board {
    tyb_monitor *manager;
    ty_list_head list;

    volatile unsigned int refcount;
    ty_mutex mutex;

    tyb_board_state state;

    char *tag;
    char *location;

    uint16_t vid;
    uint16_t pid;
    uint64_t serial;

    ty_list_head interfaces;

    int capabilities;
    tyb_board_interface *cap2iface[16];

    ty_list_head missing;
    uint64_t missing_since;

    const tyb_board_model *model;

    ty_task *current_task;

    void *udata;
};

struct tyb_board_family {
    const char *name;

    const tyb_board_model **models;

    int (*open_interface)(tyb_board_interface *iface);
    unsigned int (*guess_models)(const struct tyb_firmware *fw,
                                 const tyb_board_model **rmodels, unsigned int max);
};

#define TYB_BOARD_MODEL \
    const tyb_board_family *family; \
    \
    const char *name; \
    const char *mcu; \
    \
    size_t code_size;

TY_C_END

#endif
