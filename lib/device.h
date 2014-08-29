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

#ifndef TY_USB_H
#define TY_USB_H

#include "common.h"

TY_C_BEGIN

enum {
    TY_SERIAL_CSIZE_MASK   = 0x3,
    TY_SERIAL_7BITS_CSIZE  = 0x1,
    TY_SERIAL_6BITS_CSIZE  = 0x2,
    TY_SERIAL_5BITS_CSIZE  = 0x3,

    TY_SERIAL_PARITY_MASK  = 0xC,
    TY_SERIAL_ODD_PARITY   = 0x4,
    TY_SERIAL_EVEN_PARITY  = 0x8,

    TY_SERIAL_STOP_MASK    = 0x10,
    TY_SERIAL_2BITS_STOP   = 0x10,

    TY_SERIAL_FLOW_MASK    = 0x60,
    TY_SERIAL_XONXOFF_FLOW = 0x20,
    TY_SERIAL_RTSCTS_FLOW  = 0x40,

    TY_SERIAL_CLOSE_MASK   = 0x80,
    TY_SERIAL_NOHUP_CLOSE  = 0x80,
};

typedef enum ty_device_type {
    TY_DEVICE_HID,
    TY_DEVICE_SERIAL
} ty_device_type;

typedef struct ty_device {
    unsigned int refcount;

    char *node;

    ty_device_type type;

    char *path;

    uint16_t vid;
    uint16_t pid;
    char *serial;

    uint8_t iface;

#ifdef _WIN32
    char *key;
    char *id;
#endif
} ty_device;

typedef struct ty_handle {
    ty_device *dev;

#ifdef _WIN32
    bool block;
    void *handle; // HANDLE
    struct _OVERLAPPED *ov;
    uint8_t *buf;
    uint8_t *ptr;
    size_t len;
#else
    int fd;
#endif
} ty_handle;

typedef int ty_device_walker(ty_device *dev, void *udata);

typedef struct ty_hid_descriptor {
    uint16_t usage;
    uint16_t usage_page;
} ty_hid_descriptor;

int ty_usb_list_devices(ty_device_type type, ty_device_walker *f, void *udata);

ty_device *ty_device_ref(ty_device *dev);
void ty_device_unref(ty_device *dev);
int ty_device_dup(ty_device *dev, ty_device **rdev);

int ty_device_open(ty_handle **rh, ty_device *dev, bool block);
void ty_device_close(ty_handle *h);

int ty_serial_set_control(ty_handle *h, uint32_t rate, uint16_t flags);

ssize_t ty_serial_read(ty_handle *h, char *buf, size_t size);
ssize_t ty_serial_write(ty_handle *h, const char *buf, ssize_t size);

int ty_hid_parse_descriptor(ty_handle *h, ty_hid_descriptor *desc);

ssize_t ty_hid_read(ty_handle *h, uint8_t *buf, size_t size);
ssize_t ty_hid_write(ty_handle *h, const uint8_t *buf, size_t size);

int ty_hid_send_feature_report(ty_handle *h, const uint8_t *buf, size_t size);

TY_C_END

#endif
