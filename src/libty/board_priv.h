/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_BOARD_PRIV_H
#define TY_BOARD_PRIV_H

#include "common_priv.h"
#include "board.h"
#include "class_priv.h"
#include "../libhs/array.h"
#include "../libhs/device.h"
#include "../libhs/htable.h"
#include "task.h"
#include "thread.h"

TY_C_BEGIN

struct ty_board_interface {
    const struct _ty_class_vtable *class_vtable;
    unsigned int refcount;

    _hs_htable_head monitor_hnode;
    ty_board *board;

    const char *name;
    int capabilities;
    ty_model model;

    hs_device *dev;
    ty_mutex open_lock;
    unsigned int open_count;
    hs_port *port;
};

struct ty_board {
    unsigned int refcount;

    struct ty_monitor *monitor;

    ty_board_status status;
    uint64_t missing_since;

    ty_model model;
    char *id;
    char *tag;
    uint16_t vid;
    uint16_t pid;
    char *serial_number;
    char *description;
    char *location;

    ty_mutex ifaces_lock;
    _HS_ARRAY(ty_board_interface *) ifaces;
    int capabilities;
    ty_board_interface *cap2iface[16];

    ty_task *current_task;
};

TY_C_END

#endif
