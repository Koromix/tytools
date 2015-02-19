/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_BOARD_H
#define TY_BOARD_H

#include "common.h"
#include "device.h"

TY_C_BEGIN

struct ty_firmware;

typedef struct ty_board_manager ty_board_manager;

typedef struct ty_board ty_board;
typedef struct ty_board_interface ty_board_interface;

typedef struct ty_board_model ty_board_model;

typedef enum ty_board_capability {
    TY_BOARD_CAPABILITY_UPLOAD,
    TY_BOARD_CAPABILITY_RESET,
    TY_BOARD_CAPABILITY_REBOOT,
    TY_BOARD_CAPABILITY_SERIAL,

    TY_BOARD_CAPABILITY_COUNT
} ty_board_capability;

typedef enum ty_board_state {
    TY_BOARD_STATE_DROPPED,
    TY_BOARD_STATE_MISSING,
    TY_BOARD_STATE_ONLINE
} ty_board_state;

typedef enum ty_board_event {
    TY_BOARD_EVENT_ADDED,
    TY_BOARD_EVENT_CHANGED,
    TY_BOARD_EVENT_DISAPPEARED,
    TY_BOARD_EVENT_DROPPED
} ty_board_event;

enum {
    TY_BOARD_UPLOAD_NOCHECK = 1
};

typedef int ty_board_manager_callback_func(ty_board *board, ty_board_event event, void *udata);
typedef int ty_board_manager_wait_func(ty_board_manager *manager, void *udata);

typedef int ty_board_list_interfaces_func(ty_board *board, ty_board_interface *iface, void *udata);

typedef int ty_board_upload_progress_func(const ty_board *board, const struct ty_firmware *f, size_t uploaded, void *udata);

TY_PUBLIC extern const ty_board_model *ty_board_models[];

TY_PUBLIC int ty_board_manager_new(ty_board_manager **rmanager);
TY_PUBLIC void ty_board_manager_free(ty_board_manager *manager);

TY_PUBLIC void ty_board_manager_set_udata(ty_board_manager *manager, void *udata);
TY_PUBLIC void *ty_board_manager_get_udata(const ty_board_manager *manager);

TY_PUBLIC void ty_board_manager_get_descriptors(const ty_board_manager *manager, struct ty_descriptor_set *set, int id);

TY_PUBLIC int ty_board_manager_register_callback(ty_board_manager *manager, ty_board_manager_callback_func *f, void *udata);
TY_PUBLIC void ty_board_manager_deregister_callback(ty_board_manager *manager, int id);

TY_PUBLIC int ty_board_manager_refresh(ty_board_manager *manager);
TY_PUBLIC int ty_board_manager_wait(ty_board_manager *manager, ty_board_manager_wait_func *f, void *udata, int timeout);

TY_PUBLIC int ty_board_manager_list(ty_board_manager *manager, ty_board_manager_callback_func *f, void *udata);

TY_PUBLIC const ty_board_model *ty_board_find_model(const char *name);

TY_PUBLIC const char *ty_board_model_get_name(const ty_board_model *model);
TY_PUBLIC const char *ty_board_model_get_mcu(const ty_board_model *model);
TY_PUBLIC const char *ty_board_model_get_desc(const ty_board_model *model);
TY_PUBLIC size_t ty_board_model_get_code_size(const ty_board_model *model);

TY_PUBLIC const char *ty_board_get_capability_name(ty_board_capability cap);

TY_PUBLIC ty_board *ty_board_ref(ty_board *teensy);
TY_PUBLIC void ty_board_unref(ty_board *teensy);

TY_PUBLIC void ty_board_set_udata(ty_board *board, void *udata);
TY_PUBLIC void *ty_board_get_udata(const ty_board *board);

TY_PUBLIC ty_board_manager *ty_board_get_manager(const ty_board *board);

TY_PUBLIC ty_board_state ty_board_get_state(const ty_board *board);

TY_PUBLIC const char *ty_board_get_location(const ty_board *board);

TY_PUBLIC const ty_board_model *ty_board_get_model(const ty_board *board);

TY_PUBLIC uint64_t ty_board_get_serial_number(const ty_board *board);

TY_PUBLIC int ty_board_list_interfaces(ty_board *board, ty_board_list_interfaces_func *f, void *udata);

TY_PUBLIC ty_board_interface *ty_board_get_interface(const ty_board *board, ty_board_capability cap);
static inline bool ty_board_has_capability(const ty_board *board, ty_board_capability cap)
{
    return ty_board_get_interface(board, cap);
}

TY_PUBLIC uint16_t ty_board_get_capabilities(const ty_board *board);

TY_PUBLIC ty_device *ty_board_get_device(const ty_board *board, ty_board_capability cap);
TY_PUBLIC ty_handle *ty_board_get_handle(const ty_board *board, ty_board_capability cap);
TY_PUBLIC void ty_board_get_descriptors(const ty_board *board, ty_board_capability cap, struct ty_descriptor_set *set, int id);

TY_PUBLIC int ty_board_wait_for(ty_board *board, ty_board_capability capability, bool parallel, int timeout);

TY_PUBLIC int ty_board_serial_set_attributes(ty_board *board, uint32_t rate, uint16_t flags);

TY_PUBLIC ssize_t ty_board_serial_read(ty_board *board, char *buf, size_t size);
TY_PUBLIC ssize_t ty_board_serial_write(ty_board *board, const char *buf, size_t size);

TY_PUBLIC int ty_board_upload(ty_board *board, struct ty_firmware *f, uint16_t flags, ty_board_upload_progress_func *pf, void *udata);
TY_PUBLIC int ty_board_reset(ty_board *board);

TY_PUBLIC int ty_board_reboot(ty_board *board);

TY_PUBLIC const ty_board_model *ty_board_test_firmware(const struct ty_firmware *f);

TY_PUBLIC const char *ty_board_interface_get_desc(const ty_board_interface *iface);
TY_PUBLIC uint16_t ty_board_interface_get_capabilities(const ty_board_interface *iface);

TY_PUBLIC uint8_t ty_board_interface_get_interface_number(const ty_board_interface *iface);
TY_PUBLIC const char *ty_board_interface_get_path(const ty_board_interface *iface);

TY_PUBLIC ty_device *ty_board_interface_get_device(const ty_board_interface *iface);
TY_PUBLIC ty_handle *ty_board_interface_get_handle(const ty_board_interface *iface);
TY_PUBLIC void ty_board_interface_get_descriptors(const ty_board_interface *iface, struct ty_descriptor_set *set, int id);

TY_C_END

#endif
