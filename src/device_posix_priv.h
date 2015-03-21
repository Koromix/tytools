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

struct tyd_handle {
    TYD_HANDLE

    int fd;
};

extern const struct _tyd_device_vtable _tyb_posix_device_vtable;

#endif
