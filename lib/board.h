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

typedef struct ty_board_manager {
    ty_device_monitor *monitor;
    ty_timer *timer;

    ty_list_head callbacks;
    int callback_id;

    ty_list_head boards;
    ty_list_head missing_boards;
} ty_board_manager;

typedef enum ty_board_capability {
    TY_BOARD_CAPABILITY_IDENTIFY = 1,
    TY_BOARD_CAPABILITY_UPLOAD   = 2,
    TY_BOARD_CAPABILITY_RESET    = 4,
    TY_BOARD_CAPABILITY_SERIAL   = 8,
    TY_BOARD_CAPABILITY_REBOOT   = 16
} ty_board_capability;

typedef struct ty_board_model {
    const char *name;
    const char *mcu;
    const char *desc;

    uint8_t usage;
    uint8_t halfkay_version;
    size_t code_size;
    size_t block_size;
} ty_board_model;

typedef struct ty_board_mode {
    const char *name;
    const char *desc;

    // Identification
    uint16_t pid;
    ty_device_type type;
    uint8_t iface;

    uint16_t capabilities;
} ty_board_mode;

typedef enum ty_board_state {
    TY_BOARD_STATE_DROPPED,
    TY_BOARD_STATE_CLOSED,
    TY_BOARD_STATE_ONLINE
} ty_board_state;

typedef struct ty_board {
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
} ty_board;

typedef enum ty_board_event {
    TY_BOARD_EVENT_ADDED,
    TY_BOARD_EVENT_CHANGED,
    TY_BOARD_EVENT_CLOSED,
    TY_BOARD_EVENT_DROPPED
} ty_board_event;

enum {
    TY_BOARD_UPLOAD_NOCHECK = 1
};

typedef int ty_board_manager_callback_func(ty_board *board, ty_board_event event, void *udata);
typedef int ty_board_manager_wait_func(ty_board_manager *manager, void *udata);

extern const ty_board_model *ty_board_models[];
extern const ty_board_mode *ty_board_modes[];

int ty_board_manager_new(ty_board_manager **rmanager);
void ty_board_manager_free(ty_board_manager *manager);

void ty_board_manager_get_descriptors(ty_board_manager *manager, ty_descriptor_set *set, int id);

int ty_board_manager_register_callback(ty_board_manager *manager, ty_board_manager_callback_func *f, void *udata);
void ty_board_manager_deregister_callback(ty_board_manager *manager, int id);

int ty_board_manager_refresh(ty_board_manager *manager);
int ty_board_manager_wait(ty_board_manager *manager, ty_board_manager_wait_func *f, void *udata, int timeout);

int ty_board_manager_list(ty_board_manager *manager, ty_board_manager_callback_func *f, void *udata);

const ty_board_model *ty_board_find_model(const char *name);
const ty_board_mode *ty_board_find_mode(const char *name);

ty_board *ty_board_ref(ty_board *teensy);
void ty_board_unref(ty_board *teensy);

uint32_t ty_board_get_capabilities(ty_board *board);
static inline bool ty_board_has_capability(ty_board *board, ty_board_capability cap)
    { return ty_board_get_capabilities(board) & cap; }

int ty_board_wait_for(ty_board *board, ty_board_capability capability, int timeout);

int ty_board_control_serial(ty_board *board, uint32_t rate, uint16_t flags);

ssize_t ty_board_read_serial(ty_board *board, char *buf, size_t size);
ssize_t ty_board_write_serial(ty_board *board, const char *buf, size_t size);

int ty_board_upload(ty_board *board, struct ty_firmware *firmware, uint16_t flags);
int ty_board_reset(ty_board *board);

int ty_board_reboot(ty_board *board);

const ty_board_model *ty_board_test_firmware(struct ty_firmware *f);

TY_C_END

#endif
