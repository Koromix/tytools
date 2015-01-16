/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_BOARD_PRIV_H
#define TY_BOARD_PRIV_H

#include "ty/common.h"
#include "ty/board.h"
#include "list.h"

TY_C_BEGIN

struct ty_board {
    ty_board_manager *manager;
    ty_list_head list;

    unsigned int refcount;

    ty_board_state state;

    ty_device *dev;
    ty_handle *h;

    ty_list_head missing;
    uint64_t missing_since;

    const ty_board_mode *mode;
    const ty_board_model *model;
    uint64_t serial;

    void *udata;
};

struct _ty_board_mode_vtable {
    int (*open)(ty_board *board);

    int (*identify)(ty_board *board);

    int (*serial_set_attributes)(ty_board *board, uint32_t rate, uint16_t flags);
    ssize_t (*serial_read)(ty_board *board, char *buf, size_t size);
    ssize_t (*serial_write)(ty_board *board, const char *buf, size_t size);

    int (*reset)(ty_board *board);
    int (*upload)(ty_board *board, struct ty_firmware *firmware, uint16_t flags, ty_board_upload_progress_func *pf, void *udata);

    int (*reboot)(ty_board *board);
};

struct ty_board_mode_ {
    const char *name;
    const char *desc;

    const struct _ty_board_mode_vtable *vtable;

    uint16_t pid;
    uint16_t vid;
    ty_device_type type;
    uint8_t iface;

    uint16_t capabilities;
};

struct _ty_board_model_vtable {
};

struct ty_board_model_ {
    const char *name;
    const char *mcu;
    const char *desc;

    const struct _ty_board_model_vtable *vtable;

    size_t code_size;
};

TY_C_END

#endif
