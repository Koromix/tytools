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

#include "common_priv.h"
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "device_priv.h"
#include "platform.h"

static int open_posix_device(hs_device *dev, hs_port_mode mode, hs_port **rport)
{
    hs_port *port;
#ifdef __APPLE__
    unsigned int retry = 4;
#endif
    int fd_flags;
    int r;

    port = calloc(1, sizeof(*port));
    if (!port) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    port->dev = hs_device_ref(dev);
    port->mode = mode;

    fd_flags = O_CLOEXEC | O_NOCTTY | O_NONBLOCK;
    switch (mode) {
    case HS_PORT_MODE_READ:
        fd_flags |= O_RDONLY;
        break;
    case HS_PORT_MODE_WRITE:
        fd_flags |= O_WRONLY;
        break;
    case HS_PORT_MODE_RW:
        fd_flags |= O_RDWR;
        break;
    }

restart:
    port->u.file.fd = open(dev->path, fd_flags);
    if (port->u.file.fd < 0) {
        switch (errno) {
        case EINTR:
            goto restart;
        case EACCES:
            r = hs_error(HS_ERROR_ACCESS, "Permission denied for device '%s'", dev->path);
            break;
        case EIO:
        case ENXIO:
        case ENODEV:
            r = hs_error(HS_ERROR_IO, "I/O error while opening device '%s'", dev->path);
            break;
        case ENOENT:
        case ENOTDIR:
            r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
            break;

#ifdef __APPLE__
        /* On El Capitan (and maybe before), the open fails for some time (around 40 - 50 ms on my
           computer) after the device notification. */
        case EBUSY:
            if (retry--) {
                usleep(20000);
                goto restart;
            }
#endif
        default:
            r = hs_error(HS_ERROR_SYSTEM, "open('%s') failed: %s", dev->path, strerror(errno));
            break;
        }
        goto error;
    }

    if (dev->type == HS_DEVICE_TYPE_SERIAL) {
        struct termios tio;
        int modem_bits;

        r = tcgetattr(port->u.file.fd, &tio);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "tcgetattr() failed on '%s': %s", dev->path,
                         strerror(errno));
            goto error;
        }

        /* Use raw I/O and sane settings, set DTR by default even on platforms that don't
           enforce that. */
        cfmakeraw(&tio);
        tio.c_cc[VMIN] = 0;
        tio.c_cc[VTIME] = 0;
        tio.c_cflag |= CLOCAL | CREAD | HUPCL;
        modem_bits = TIOCM_DTR;

        r = tcsetattr(port->u.file.fd, TCSANOW, &tio);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "tcsetattr() failed on '%s': %s", dev->path,
                         strerror(errno));
            goto error;
        }
        r = ioctl(port->u.file.fd, TIOCMBIS, &modem_bits);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "ioctl(TIOCMBIS, TIOCM_DTR) failed on '%s': %s",
                         dev->path, strerror(errno));
            goto error;
        }
        r = tcflush(port->u.file.fd, TCIFLUSH);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "tcflush(TCIFLUSH) failed on '%s': %s",
                         dev->path, strerror(errno));
            goto error;
        }
    }

    *rport = port;
    return 0;

error:
    hs_port_close(port);
    return r;
}

static void close_posix_device(hs_port *port)
{
    if (port) {
#ifdef __linux__
        // Only used for hidraw to work around a bug on old kernels
        free(port->u.file.read_buf);
#endif

        close(port->u.file.fd);
        hs_device_unref(port->dev);
    }

    free(port);
}

static hs_descriptor get_posix_descriptor(const hs_port *port)
{
    return port->u.file.fd;
}

const struct _hs_device_vtable _hs_posix_device_vtable = {
    .open = open_posix_device,
    .close = close_posix_device,

    .get_descriptor = get_posix_descriptor
};
