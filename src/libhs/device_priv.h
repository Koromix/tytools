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

#include "common_priv.h"
#include "device.h"
#include "htable.h"

struct hs_monitor;

struct hs_device {
    unsigned int refcount;
    _hs_htable_head hnode;
    char *key;

    hs_device_type type;
    hs_device_status status;

    char *location;
    char *path;
    uint16_t vid;
    uint16_t pid;
    char *manufacturer_string;
    char *product_string;
    char *serial_number_string;
    uint8_t iface_number;

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

struct hs_port {
    hs_device_type type;
    const char *path;
    hs_port_mode mode;
    hs_device *dev;

    union {
#if defined(_WIN32)
        struct {
            void *h; // HANDLE

            struct _OVERLAPPED *read_ov;
            uint8_t *read_buf;
            uint8_t *read_ptr;
            size_t read_len;
            int read_status;
            unsigned long read_pending_thread; // DWORD

            void *write_handle; // HANDLE
            void *write_event; // HANDLE
        } handle;
#else
        struct {
            int fd;

    #ifdef __linux__
            // Used to work around an old kernel 2.6 (pre-2.6.34) hidraw bug
            uint8_t *read_buf;
            size_t read_buf_size;
            bool numbered_hid_reports;
    #endif
        } file;

    #ifdef __APPLE__
        struct _hs_hid_darwin *hid;
    #endif
#endif
    } u;
};

void _hs_device_log(const struct hs_device *dev, const char *verb);

int _hs_open_file_port(hs_device *dev, hs_port_mode mode, hs_port **rport);
void _hs_close_file_port(hs_port *port);
hs_handle _hs_get_file_port_poll_handle(const hs_port *port);

#if defined(_WIN32)
void _hs_win32_start_async_read(hs_port *port);
void _hs_win32_finalize_async_read(hs_port *port, int timeout);
ssize_t _hs_win32_write_sync(hs_port *port, const uint8_t *buf, size_t size, int timeout);
#elif defined(__APPLE__)
int _hs_darwin_open_hid_port(hs_device *dev, hs_port_mode mode, hs_port **rport);
void _hs_darwin_close_hid_port(hs_port *port);
hs_handle _hs_darwin_get_hid_port_poll_handle(const hs_port *port);
#endif

#endif
