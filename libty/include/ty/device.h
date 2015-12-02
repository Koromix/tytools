/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_DEVICE_H
#define TY_DEVICE_H

#include "common.h"

TY_C_BEGIN

struct ty_descriptor_set;

typedef struct tyd_monitor tyd_monitor;
typedef struct tyd_device tyd_device;
typedef struct tyd_handle tyd_handle;

typedef enum tyd_device_type {
    TYD_DEVICE_HID,
    TYD_DEVICE_SERIAL
} tyd_device_type;

typedef enum tyd_monitor_event {
    TYD_MONITOR_EVENT_ADDED,
    TYD_MONITOR_EVENT_REMOVED
} tyd_monitor_event;

typedef struct tyd_hid_descriptor {
    uint16_t usage;
    uint16_t usage_page;
} tyd_hid_descriptor;

typedef int tyd_device_callback_func(tyd_device *dev, tyd_monitor_event event, void *udata);

enum {
    TYD_SERIAL_CSIZE_MASK   = 0x3,
    TYD_SERIAL_7BITS_CSIZE  = 0x1,
    TYD_SERIAL_6BITS_CSIZE  = 0x2,
    TYD_SERIAL_5BITS_CSIZE  = 0x3,

    TYD_SERIAL_PARITY_MASK  = 0xC,
    TYD_SERIAL_ODD_PARITY   = 0x4,
    TYD_SERIAL_EVEN_PARITY  = 0x8,

    TYD_SERIAL_STOP_MASK    = 0x10,
    TYD_SERIAL_2BITS_STOP   = 0x10,

    TYD_SERIAL_FLOW_MASK    = 0x60,
    TYD_SERIAL_XONXOFF_FLOW = 0x20,
    TYD_SERIAL_RTSCTS_FLOW  = 0x40,

    TYD_SERIAL_CLOSE_MASK   = 0x80,
    TYD_SERIAL_NOHUP_CLOSE  = 0x80,
};

TY_PUBLIC int tyd_monitor_new(tyd_monitor **rmonitor);
TY_PUBLIC void tyd_monitor_free(tyd_monitor *monitor);

TY_PUBLIC void tyd_monitor_set_udata(tyd_monitor *monitor, void *udata);
TY_PUBLIC void *tyd_monitor_get_udata(const tyd_monitor *monitor);

TY_PUBLIC void tyd_monitor_get_descriptors(const tyd_monitor *monitor, struct ty_descriptor_set *set, int id);

TY_PUBLIC int tyd_monitor_register_callback(tyd_monitor *monitor, tyd_device_callback_func *f, void *udata);
TY_PUBLIC void tyd_monitor_deregister_callback(tyd_monitor *monitor, int id);

TY_PUBLIC int tyd_monitor_refresh(tyd_monitor *monitor);

TY_PUBLIC int tyd_monitor_list(tyd_monitor *monitor, tyd_device_callback_func *f, void *udata);

TY_PUBLIC tyd_device *tyd_device_ref(tyd_device *dev);
TY_PUBLIC void tyd_device_unref(tyd_device *dev);

TY_PUBLIC void tyd_device_set_udata(tyd_device *dev, void *udata);
TY_PUBLIC void *tyd_device_get_udata(const tyd_device *dev);

TY_PUBLIC int tyd_device_open(tyd_device *dev, tyd_handle **rh);
TY_PUBLIC void tyd_device_close(tyd_handle *h);

TY_PUBLIC void tyd_device_get_descriptors(const tyd_handle *h, struct ty_descriptor_set *set, int id);

TY_PUBLIC tyd_device_type tyd_device_get_type(const tyd_device *dev);

TY_PUBLIC const char *tyd_device_get_location(const tyd_device *dev);
TY_PUBLIC const char *tyd_device_get_path(const tyd_device *dev);

TY_PUBLIC uint16_t tyd_device_get_vid(const tyd_device *dev);
TY_PUBLIC uint16_t tyd_device_get_pid(const tyd_device *dev);
TY_PUBLIC const char *tyd_device_get_serial_number(const tyd_device *dev);

TY_PUBLIC uint8_t tyd_device_get_interface_number(const tyd_device *dev);

TY_PUBLIC int tyd_serial_set_attributes(tyd_handle *h, uint32_t rate, int flags);

TY_PUBLIC ssize_t tyd_serial_read(tyd_handle *h, char *buf, size_t size, int timeout);
TY_PUBLIC ssize_t tyd_serial_write(tyd_handle *h, const char *buf, ssize_t size);

TY_PUBLIC int tyd_hid_parse_descriptor(tyd_handle *h, tyd_hid_descriptor *desc);

TY_PUBLIC ssize_t tyd_hid_read(tyd_handle *h, uint8_t *buf, size_t size, int timeout);
TY_PUBLIC ssize_t tyd_hid_write(tyd_handle *h, const uint8_t *buf, size_t size);

TY_PUBLIC ssize_t tyd_hid_send_feature_report(tyd_handle *h, const uint8_t *buf, size_t size);

TY_C_END

#endif
