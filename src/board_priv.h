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

    ssize_t (*read_serial)(ty_board *board, char *buf, size_t size);
    ssize_t (*write_serial)(ty_board *board, const char *buf, size_t size);

    int (*reset)(ty_board *board);
    int (*upload)(ty_board *board, struct ty_firmware *firmware, uint16_t flags);

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
