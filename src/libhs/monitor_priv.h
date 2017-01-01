/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

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
