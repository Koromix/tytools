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

#include "util.h"
#include "filter.h"
#include "htable.h"
#include "list.h"
#include "hs/monitor.h"

struct hs_device;

#define _HS_MONITOR \
    _hs_filter filter; \
    \
    _hs_htable devices; \

int _hs_monitor_init(hs_monitor *monitor, const hs_match *matches, unsigned int count);
void _hs_monitor_release(hs_monitor *monitor);

void _hs_monitor_clear(hs_monitor *monitor);

int _hs_monitor_add(hs_monitor *monitor, struct hs_device *dev, hs_enumerate_func *f,
                    void *udata);
void _hs_monitor_remove(hs_monitor *monitor, const char *key, hs_enumerate_func *f,
                        void *udata);

#endif
