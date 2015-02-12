/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_DEVICE_POSIX_PRIV_H
#define TY_DEVICE_POSIX_PRIV_H

#include "ty/common.h"
#include "ty/device.h"
#include "device_priv.h"

struct ty_handle {
    TY_HANDLE

    int fd;
    bool block;
};

extern const struct _ty_device_vtable _ty_posix_device_vtable;

#endif
