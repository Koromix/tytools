/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_BOARD_PRIV_H
#define TY_BOARD_PRIV_H

#include "common_priv.h"
#include "board.h"
#include "class_priv.h"
#include "../libhs/device.h"
#include "htable.h"
#include "list.h"
#include "task.h"
#include "thread.h"

TY_C_BEGIN

struct ty_board_interface {
    const struct _ty_class_vtable *class_vtable;
    unsigned int refcount;

    ty_htable_head monitor_hnode;
    ty_board *board;
    ty_list_head board_node;

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
    ty_list_head monitor_node;

    ty_board_state state;
    ty_list_head missing_node;
    uint64_t missing_since;

    ty_model model;
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

TY_C_END

#endif
