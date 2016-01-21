/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_MONITOR_H
#define TY_MONITOR_H

#include "common.h"

TY_C_BEGIN

struct tyb_board;

typedef struct tyb_monitor tyb_monitor;

enum {
    TYB_MONITOR_PARALLEL_WAIT = 1
};

typedef enum tyb_monitor_event {
    TYB_MONITOR_EVENT_ADDED,
    TYB_MONITOR_EVENT_CHANGED,
    TYB_MONITOR_EVENT_DISAPPEARED,
    TYB_MONITOR_EVENT_DROPPED
} tyb_monitor_event;

typedef int tyb_monitor_callback_func(struct tyb_board *board, tyb_monitor_event event, void *udata);
typedef int tyb_monitor_wait_func(tyb_monitor *monitor, void *udata);

TY_PUBLIC int tyb_monitor_new(int flags, tyb_monitor **rmonitor);
TY_PUBLIC void tyb_monitor_free(tyb_monitor *monitor);

TY_PUBLIC void tyb_monitor_set_udata(tyb_monitor *monitor, void *udata);
TY_PUBLIC void *tyb_monitor_get_udata(const tyb_monitor *monitor);

TY_PUBLIC void tyb_monitor_get_descriptors(const tyb_monitor *monitor, struct ty_descriptor_set *set, int id);

TY_PUBLIC int tyb_monitor_register_callback(tyb_monitor *monitor, tyb_monitor_callback_func *f, void *udata);
TY_PUBLIC void tyb_monitor_deregister_callback(tyb_monitor *monitor, int id);

TY_PUBLIC int tyb_monitor_refresh(tyb_monitor *monitor);
TY_PUBLIC int tyb_monitor_wait(tyb_monitor *monitor, tyb_monitor_wait_func *f, void *udata, int timeout);

TY_PUBLIC int tyb_monitor_list(tyb_monitor *monitor, tyb_monitor_callback_func *f, void *udata);

TY_C_END

#endif
