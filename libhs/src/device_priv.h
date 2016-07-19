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

#ifndef _HS_DEVICE_PRIV_H
#define _HS_DEVICE_PRIV_H

#include "util.h"
#include "hs/device.h"
#include "htable.h"

struct hs_monitor;

struct _hs_device_vtable {
    int (*open)(hs_device *dev, hs_handle_mode mode, hs_handle **rh);
    void (*close)(hs_handle *h);

    hs_descriptor (*get_descriptor)(const hs_handle *h);
};

struct hs_device {
    _hs_htable_head hnode;

    unsigned int refcount;

    char *key;

    hs_device_type type;
    const struct _hs_device_vtable *vtable;

    hs_device_status state;

    char *location;
    char *path;

    uint16_t vid;
    uint16_t pid;

    char *manufacturer;
    char *product;
    char *serial;

    uint8_t iface;

    union {
        struct {
            uint16_t usage_page;
            uint16_t usage;
#ifdef __linux__
            // Needed to work around a bug on old Linux kernels
            bool numbered_reports;
#endif
        } hid;
    } u;

};

#define _HS_HANDLE \
    hs_device *dev; \
    hs_handle_mode mode;

void _hs_device_log(const struct hs_device *dev, const char *verb);

#endif
