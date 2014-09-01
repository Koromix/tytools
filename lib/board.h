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

#ifndef TY_BOARD_H
#define TY_BOARD_H

#include "common.h"
#include "device.h"

TY_C_BEGIN

struct ty_firmware;

typedef enum ty_toolchain {
    TY_TOOLCHAIN_AVR,
    TY_TOOLCHAIN_ARM
} ty_toolchain;

typedef enum ty_board_capability {
    TY_BOARD_CAPABILITY_IDENTIFY = 1,
    TY_BOARD_CAPABILITY_UPLOAD   = 2,
    TY_BOARD_CAPABILITY_RESET    = 4,
    TY_BOARD_CAPABILITY_SERIAL   = 8,
    TY_BOARD_CAPABILITY_REBOOT   = 16
} ty_board_capability;

enum {
    TY_BOARD_UPLOAD_NOCHECK = 1
};

typedef struct ty_board_model {
    const char *name;
    const char *mcu;
    const char *desc;

    // Bootloader settings
    uint8_t usage;
    uint8_t halfkay_version;
    size_t code_size;
    size_t block_size;

    // Build settings
    ty_toolchain toolchain;
    const char *core;
    uint32_t frequencies[8];
    const char *cflags;
    const char *cxxflags;
    const char *ldflags;
} ty_board_model;

typedef struct ty_board_mode {
    const char *name;
    const char *desc;

    // Identification
    uint16_t pid;
    ty_device_type type;
    uint8_t iface;

    uint16_t capabilities;

    // Build settings
    const char *flags;
} ty_board_mode;

typedef struct ty_board {
    unsigned int refcount;

    ty_device *dev;
    uint64_t serial;

    ty_handle *h;
    uint64_t fail_at;

    const ty_board_mode *mode;
    const ty_board_model *model;
} ty_board;

typedef int ty_board_walker(ty_board *board, void *udata);

extern const ty_board_model *ty_board_models[];
extern const ty_board_mode *ty_board_modes[];

const ty_board_model *ty_board_find_model(const char *name);
const ty_board_mode *ty_board_find_mode(const char *name);

int ty_board_list(ty_board_walker *f, void *udata);
int ty_board_find(const char *path, uint64_t serial, ty_board **rboard);

ty_board *ty_board_ref(ty_board *teensy);
void ty_board_unref(ty_board *teensy);

uint32_t ty_board_get_capabilities(ty_board *board);
static inline bool ty_board_has_capability(ty_board *board, ty_board_capability cap)
    { return ty_board_get_capabilities(board) & cap; }

int ty_board_probe(ty_board *board, int timeout);
void ty_board_close(ty_board *board);

ssize_t ty_board_read_serial(ty_board *board, char *buf, size_t size);
ssize_t ty_board_write_serial(ty_board *board, const char *buf, size_t size);

int ty_board_upload(ty_board *board, struct ty_firmware *firmware, uint16_t flags);
int ty_board_reset(ty_board *board);

int ty_board_reboot(ty_board *board);

const ty_board_model *ty_board_test_firmware(struct ty_firmware *f);

TY_C_END

#endif
