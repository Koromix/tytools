/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_BOARD_H
#define TY_BOARD_H

#include "common.h"
#include "class.h"

TY_C_BEGIN

struct ty_descriptor_set;
struct hs_device;
struct ty_monitor;
struct ty_firmware;
struct hs_port;
struct ty_task;

typedef struct ty_board ty_board;
typedef struct ty_board_interface ty_board_interface;

// Keep in sync with capability_names in board.c
typedef enum ty_board_capability {
    TY_BOARD_CAPABILITY_UNIQUE,
    TY_BOARD_CAPABILITY_RUN,
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

enum {
    TY_UPLOAD_WAIT = 1,
    TY_UPLOAD_NORESET = 2,
    TY_UPLOAD_NOCHECK = 4
};

#define TY_UPLOAD_MAX_FIRMWARES 256

typedef int ty_board_list_interfaces_func(ty_board_interface *iface, void *udata);
typedef int ty_board_upload_progress_func(const ty_board *board, const struct ty_firmware *fw, size_t uploaded, void *udata);

TY_PUBLIC const char *ty_board_capability_get_name(ty_board_capability cap);

TY_PUBLIC ty_board *ty_board_ref(ty_board *board);
TY_PUBLIC void ty_board_unref(ty_board *board);

TY_PUBLIC bool ty_board_matches_tag(ty_board *board, const char *id);

TY_PUBLIC struct ty_monitor *ty_board_get_monitor(const ty_board *board);

TY_PUBLIC ty_board_state ty_board_get_state(const ty_board *board);

TY_PUBLIC const char *ty_board_get_id(const ty_board *board);
TY_PUBLIC int ty_board_set_tag(ty_board *board, const char *tag);
TY_PUBLIC const char *ty_board_get_tag(const ty_board *board);

TY_PUBLIC const char *ty_board_get_location(const ty_board *board);
TY_PUBLIC const char *ty_board_get_serial_number(const ty_board *board);
TY_PUBLIC const char *ty_board_get_description(const ty_board *board);

TY_PUBLIC void ty_board_set_model(ty_board *board, ty_model model);
TY_PUBLIC ty_model ty_board_get_model(const ty_board *board);

TY_PUBLIC int ty_board_list_interfaces(ty_board *board, ty_board_list_interfaces_func *f, void *udata);
TY_PUBLIC int ty_board_open_interface(ty_board *board, ty_board_capability cap, ty_board_interface **riface);

TY_PUBLIC int ty_board_get_capabilities(const ty_board *board);
static inline bool ty_board_has_capability(const ty_board *board, ty_board_capability cap)
{
    return ty_board_get_capabilities(board) & (1 << cap);
}

TY_PUBLIC int ty_board_wait_for(ty_board *board, ty_board_capability capability, int timeout);

TY_PUBLIC ssize_t ty_board_serial_read(ty_board *board, char *buf, size_t size, int timeout);
TY_PUBLIC ssize_t ty_board_serial_write(ty_board *board, const char *buf, size_t size);

TY_PUBLIC int ty_board_upload(ty_board *board, struct ty_firmware *fw, ty_board_upload_progress_func *pf, void *udata);
TY_PUBLIC int ty_board_reset(ty_board *board);
TY_PUBLIC int ty_board_reboot(ty_board *board);

TY_PUBLIC ty_board_interface *ty_board_interface_ref(ty_board_interface *iface);
TY_PUBLIC void ty_board_interface_unref(ty_board_interface *iface);
TY_PUBLIC int ty_board_interface_open(ty_board_interface *iface);
TY_PUBLIC void ty_board_interface_close(ty_board_interface *iface);

TY_PUBLIC const char *ty_board_interface_get_name(const ty_board_interface *iface);
TY_PUBLIC int ty_board_interface_get_capabilities(const ty_board_interface *iface);

TY_PUBLIC uint8_t ty_board_interface_get_interface_number(const ty_board_interface *iface);
TY_PUBLIC const char *ty_board_interface_get_path(const ty_board_interface *iface);

TY_PUBLIC struct hs_device *ty_board_interface_get_device(const ty_board_interface *iface);
TY_PUBLIC struct hs_port *ty_board_interface_get_handle(const ty_board_interface *iface);
TY_PUBLIC void ty_board_interface_get_descriptors(const ty_board_interface *iface, struct ty_descriptor_set *set, int id);

TY_PUBLIC int ty_upload(ty_board *board, struct ty_firmware **fws, unsigned int fws_count,
                         int flags, struct ty_task **rtask);
TY_PUBLIC int ty_reset(ty_board *board, struct ty_task **rtask);
TY_PUBLIC int ty_reboot(ty_board *board, struct ty_task **rtask);
TY_PUBLIC int ty_send(ty_board *board, const char *buf, size_t size, struct ty_task **rtask);
TY_PUBLIC int ty_send_file(ty_board *board, const char *filename, struct ty_task **rtask);

TY_C_END

#endif
