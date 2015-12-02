/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
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

extern const struct _tyd_device_vtable _tyd_posix_device_vtable;

#endif
