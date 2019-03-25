/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef _HS_DEVICE_PRIV_H
#define _HS_DEVICE_PRIV_H

#include "common_priv.h"
#include "device.h"

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
            size_t read_buf_size;
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

void _hs_device_log(const hs_device *dev, const char *verb);

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
