/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef _HS_MONITOR_PRIV_H
#define _HS_MONITOR_PRIV_H

#include "common_priv.h"
#include "htable.h"
#include "monitor.h"

struct hs_device;

void _hs_monitor_clear_devices(_hs_htable *devices);

bool _hs_monitor_has_device(_hs_htable *devices, const char *key, uint8_t iface);

int _hs_monitor_add(_hs_htable *devices, struct hs_device *dev, hs_enumerate_func *f, void *udata);
void _hs_monitor_remove(_hs_htable *devices, const char *key, hs_enumerate_func *f, void *udata);

int _hs_monitor_list(_hs_htable *devices, hs_enumerate_func *f, void *udata);

#endif
