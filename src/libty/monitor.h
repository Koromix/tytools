/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_MONITOR_H
#define TY_MONITOR_H

#include "common.h"

TY_C_BEGIN

struct ty_board;

typedef struct ty_monitor ty_monitor;

typedef enum ty_monitor_event {
    TY_MONITOR_EVENT_ADDED,
    TY_MONITOR_EVENT_CHANGED,
    TY_MONITOR_EVENT_DISAPPEARED,
    TY_MONITOR_EVENT_DROPPED
} ty_monitor_event;

typedef int ty_monitor_callback_func(struct ty_board *board, ty_monitor_event event, void *udata);
typedef int ty_monitor_wait_func(ty_monitor *monitor, void *udata);

int ty_monitor_new(ty_monitor **rmonitor);
void ty_monitor_free(ty_monitor *monitor);

int ty_monitor_start(ty_monitor *monitor);
void ty_monitor_stop(ty_monitor *monitor);

void ty_monitor_get_descriptors(const ty_monitor *monitor, struct ty_descriptor_set *set, int id);

int ty_monitor_register_callback(ty_monitor *monitor, ty_monitor_callback_func *f, void *udata);
void ty_monitor_deregister_callback(ty_monitor *monitor, int id);

int ty_monitor_refresh(ty_monitor *monitor);
int ty_monitor_wait(ty_monitor *monitor, ty_monitor_wait_func *f, void *udata, int timeout);

int ty_monitor_list(ty_monitor *monitor, ty_monitor_callback_func *f, void *udata);

TY_C_END

#endif
