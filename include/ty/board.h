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

typedef struct ty_board_manager ty_board_manager;
typedef struct ty_board ty_board;

typedef struct ty_board_model ty_board_model;
typedef struct ty_board_mode ty_board_mode;

typedef enum ty_board_capability {
    TY_BOARD_CAPABILITY_IDENTIFY = 1,
    TY_BOARD_CAPABILITY_UPLOAD   = 2,
    TY_BOARD_CAPABILITY_RESET    = 4,
    TY_BOARD_CAPABILITY_SERIAL   = 8,
    TY_BOARD_CAPABILITY_REBOOT   = 16
} ty_board_capability;

typedef enum ty_board_state {
    TY_BOARD_STATE_DROPPED,
    TY_BOARD_STATE_CLOSED,
    TY_BOARD_STATE_ONLINE
} ty_board_state;

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

extern const ty_board_mode *ty_board_modes[];
extern const ty_board_model *ty_board_models[];

int ty_board_manager_new(ty_board_manager **rmanager);
void ty_board_manager_free(ty_board_manager *manager);

void ty_board_manager_set_udata(ty_board_manager *manager, void *udata);
void *ty_board_manager_get_udata(ty_board_manager *manager);

void ty_board_manager_get_descriptors(ty_board_manager *manager, struct ty_descriptor_set *set, int id);

int ty_board_manager_register_callback(ty_board_manager *manager, ty_board_manager_callback_func *f, void *udata);
void ty_board_manager_deregister_callback(ty_board_manager *manager, int id);

int ty_board_manager_refresh(ty_board_manager *manager);
int ty_board_manager_wait(ty_board_manager *manager, ty_board_manager_wait_func *f, void *udata, int timeout);

int ty_board_manager_list(ty_board_manager *manager, ty_board_manager_callback_func *f, void *udata);

const ty_board_mode *ty_board_find_mode(const char *name);
const ty_board_model *ty_board_find_model(const char *name);

const char *ty_board_mode_get_name(const ty_board_mode *mode);
const char *ty_board_mode_get_desc(const ty_board_mode *mode);

const char *ty_board_model_get_name(const ty_board_model *model);
const char *ty_board_model_get_mcu(const ty_board_model *model);
const char *ty_board_model_get_desc(const ty_board_model *model);
size_t ty_board_model_get_code_size(const ty_board_model *model);

ty_board *ty_board_ref(ty_board *teensy);
void ty_board_unref(ty_board *teensy);

void ty_board_set_udata(ty_board *board, void *udata);
void *ty_board_get_udata(ty_board *board);

ty_board_manager *ty_board_get_manager(ty_board *board);

ty_board_state ty_board_get_state(ty_board *board);

ty_device *ty_board_get_device(ty_board *board);
static inline const char *ty_board_get_location(ty_board *board)
{
    return ty_device_get_location(ty_board_get_device(board));
}
static inline const char *ty_board_get_path(ty_board *board)
{
    return ty_device_get_path(ty_board_get_device(board));
}
ty_handle *ty_board_get_handle(ty_board *board);

const ty_board_mode *ty_board_get_mode(ty_board *board);
const ty_board_model *ty_board_get_model(ty_board *board);

uint64_t ty_board_get_serial_number(ty_board *board);

uint32_t ty_board_get_capabilities(ty_board *board);
static inline bool ty_board_has_capability(ty_board *board, ty_board_capability cap)
{
    return ty_board_get_capabilities(board) & cap;
}

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
