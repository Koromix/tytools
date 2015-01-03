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

#include "ty/common.h"
#include "compat.h"
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ty/device.h"
#include "device_priv.h"
#include "device_posix_priv.h"
#include "ty/system.h"

void ty_device_get_descriptors(const ty_handle *h, ty_descriptor_set *set, int id)
{
    assert(h);
    assert(set);

    ty_descriptor_set_add(set, h->fd, id);
}

static int open_posix_device(ty_device *dev, bool block, ty_handle **rh)
{
    ty_handle *h;
    int flags, r;

    h = calloc(1, sizeof(*h));
    if (!h)
        return ty_error(TY_ERROR_MEMORY, NULL);
    h->dev = ty_device_ref(dev);

    flags = O_RDWR | O_CLOEXEC | O_NOCTTY;
    if (!block)
        flags |= O_NONBLOCK;

restart:
    h->fd = open(dev->path, flags);
    if (h->fd < 0) {
        switch (errno) {
        case EINTR:
            goto restart;
        case EACCES:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied for device '%s'", dev->path);
            break;
        case EIO:
        case ENXIO:
        case ENODEV:
            r = ty_error(TY_ERROR_IO, "I/O while opening device '%s'", dev->path);
            break;
        case ENOENT:
        case ENOTDIR:
            r = ty_error(TY_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
            break;

        default:
            r = ty_error(TY_ERROR_SYSTEM, "open('%s') failed: %s", dev->path, strerror(errno));
            break;
        }
        goto error;
    }

    *rh = h;
    return 0;

error:
    ty_device_close(h);
    return r;
}

static void close_posix_device(ty_handle *h)
{
    if (h) {
        if (h->fd >= 0)
            close(h->fd);
        ty_device_unref(h->dev);
    }

    free(h);
}

const struct _ty_device_vtable _ty_posix_device_vtable = {
    .open = open_posix_device,
    .close = close_posix_device,
};

int ty_serial_set_control(ty_handle *h, uint32_t rate, uint16_t flags)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_SERIAL);

    struct termios tio;
    int r;

    r = tcgetattr(h->fd, &tio);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "Unable to read serial port settings: %s",
                        strerror(errno));

    cfmakeraw(&tio);
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    tio.c_cflag |= CLOCAL;

    switch (rate) {
    case 0:
        rate = B0;
        break;
    case 50:
        rate = B50;
        break;
    case 75:
        rate = B75;
        break;
    case 110:
        rate = B110;
        break;
    case 134:
        rate = B134;
        break;
    case 150:
        rate = B150;
        break;
    case 200:
        rate = B200;
        break;
    case 300:
        rate = B300;
        break;
    case 600:
        rate = B600;
        break;
    case 1200:
        rate = B1200;
        break;
    case 1800:
        rate = B1800;
        break;
    case 2400:
        rate = B2400;
        break;
    case 4800:
        rate = B4800;
        break;
    case 9600:
        rate = B9600;
        break;
    case 19200:
        rate = B19200;
        break;
    case 38400:
        rate = B38400;
        break;
    case 57600:
        rate = B57600;
        break;
    case 115200:
        rate = B115200;
        break;

    default:
        assert(false);
    }

    cfsetispeed(&tio, rate);
    cfsetospeed(&tio, rate);

    tio.c_cflag &= (unsigned int)~CSIZE;
    switch (flags & TY_SERIAL_CSIZE_MASK) {
    case TY_SERIAL_5BITS_CSIZE:
        tio.c_cflag |= CS5;
        break;
    case TY_SERIAL_6BITS_CSIZE:
        tio.c_cflag |= CS6;
        break;
    case TY_SERIAL_7BITS_CSIZE:
        tio.c_cflag |= CS7;
        break;

    default:
        tio.c_cflag |= CS8;
        break;
    }

    tio.c_cflag &= (unsigned int)~(PARENB | PARODD);
    switch (flags & TY_SERIAL_PARITY_MASK) {
    case 0:
        break;
    case TY_SERIAL_ODD_PARITY:
        tio.c_cflag |= PARENB | PARODD;
        break;
    case TY_SERIAL_EVEN_PARITY:
        tio.c_cflag |= PARENB;
        break;

    default:
        assert(false);
    }

    tio.c_cflag &= (unsigned int)~CSTOPB;
    if (flags & TY_SERIAL_2BITS_STOP)
        tio.c_cflag |= CSTOPB;

    tio.c_cflag &= (unsigned int)~CRTSCTS;
    tio.c_iflag &= (unsigned int)~(IXON | IXOFF);
    switch (flags & TY_SERIAL_FLOW_MASK) {
    case 0:
        break;
    case TY_SERIAL_XONXOFF_FLOW:
        tio.c_iflag |= IXON | IXOFF;
        break;
    case TY_SERIAL_RTSCTS_FLOW:
        tio.c_cflag |= CRTSCTS;
        break;

    default:
        assert(false);
    }

    tio.c_cflag &= (unsigned int)~HUPCL;
    if (!(flags & TY_SERIAL_NOHUP_CLOSE))
        tio.c_cflag |= HUPCL;

    r = tcsetattr(h->fd, TCSANOW, &tio);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "Unable to change serial port settings: %s",
                        strerror(errno));

    return 0;
}

ssize_t ty_serial_read(struct ty_handle *h, char *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_SERIAL);
    assert(buf);
    assert(size);

    ssize_t r;

restart:
    r = read(h->fd, buf, size);
    if (r < 0) {
        switch (errno) {
        case EINTR:
            goto restart;
        case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
            return 0;
        case EIO:
        case ENXIO:
            return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->path);
        }
        return ty_error(TY_ERROR_SYSTEM, "read('%s') failed: %s", h->dev->path,
                        strerror(errno));
    }

    return r;
}

ssize_t ty_serial_write(struct ty_handle *h, const char *buf, ssize_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_SERIAL);
    assert(buf);

    if (size < 0)
        size = (ssize_t)strlen(buf);
    if (!size)
        return 0;

    ssize_t r;

restart:
    r = write(h->fd, buf, (size_t)size);
    if (r < 0) {
        switch (errno) {
        case EINTR:
            goto restart;
        // hidraw honours O_NONBLOCK only on read operations.
        case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
            return 0;
        case EIO:
        case ENXIO:
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
        }
        return ty_error(TY_ERROR_SYSTEM, "write('%s') failed: %s", h->dev->path,
                        strerror(errno));
    }

    return r;
}
