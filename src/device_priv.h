/**
 * ty, command-line program to manage Teensy devices
 * http://github.com/Koromix/ty
 * Copyright (C) 2014 Niels Martignène
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TY_DEVICE_PRIV_H
#define TY_DEVICE_PRIV_H

#include "common.h"
#include "device.h"
#include "list.h"

TY_C_BEGIN

struct ty_device_monitor_ {
    ty_list_head callbacks;
    int callback_id;

    ty_list_head devices;
};

struct ty_device {
    ty_device_monitor *monitor;
    ty_list_head list;

    unsigned int refcount;

    char *key;

    ty_device_type type;

    char *location;
    char *path;

    uint16_t vid;
    uint16_t pid;
    char *serial;

    uint8_t iface;
};

TY_C_END

#endif
