/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "ty/common.h"
#include "compat.h"
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ty/device.h"
#include "device_priv.h"
#include "device_posix_priv.h"
#include "ty/system.h"

static int open_posix_device(tyd_device *dev, tyd_handle **rh)
{
    tyd_handle *h;
#ifdef __APPLE__
    unsigned int retry = 4;
#endif
    int r;

    h = calloc(1, sizeof(*h));
    if (!h)
        return ty_error(TY_ERROR_MEMORY, NULL);
    h->dev = tyd_device_ref(dev);

restart:
    h->fd = open(dev->path, O_RDWR | O_CLOEXEC | O_NOCTTY | O_NONBLOCK);
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
            r = ty_error(TY_ERROR_IO, "I/O error while opening device '%s'", dev->path);
            break;
        case ENOENT:
        case ENOTDIR:
            r = ty_error(TY_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
            break;

#ifdef __APPLE__
        /* On El Capitan (and maybe before), the open fails for some time (around 40 - 50 ms on my
           computer) after the device notification. */
        case EBUSY:
            if (retry--) {
                ty_delay(20);
                goto restart;
            }
#endif
        default:
            r = ty_error(TY_ERROR_SYSTEM, "open('%s') failed: %s", dev->path, strerror(errno));
            break;
        }
        goto error;
    }

#ifdef __APPLE__
    if (dev->type == TYD_DEVICE_SERIAL)
        ioctl(h->fd, TIOCSDTR);
#endif

    *rh = h;
    return 0;

error:
    tyd_device_close(h);
    return r;
}

static void close_posix_device(tyd_handle *h)
{
    if (h) {
        if (h->fd >= 0)
            close(h->fd);
        tyd_device_unref(h->dev);
    }

    free(h);
}

static void get_posix_descriptors(const tyd_handle *h, ty_descriptor_set *set, int id)
{
    ty_descriptor_set_add(set, h->fd, id);
}

const struct _tyd_device_vtable _tyd_posix_device_vtable = {
    .open = open_posix_device,
    .close = close_posix_device,

    .get_descriptors = get_posix_descriptors
};

int tyd_serial_set_attributes(tyd_handle *h, uint32_t rate, int flags)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_SERIAL);

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
        break;
    }

    cfsetispeed(&tio, rate);
    cfsetospeed(&tio, rate);

    tio.c_cflag &= (unsigned int)~CSIZE;
    switch (flags & TYD_SERIAL_CSIZE_MASK) {
    case 0:
        tio.c_cflag |= CS8;
        break;
    case TYD_SERIAL_5BITS_CSIZE:
        tio.c_cflag |= CS5;
        break;
    case TYD_SERIAL_6BITS_CSIZE:
        tio.c_cflag |= CS6;
        break;
    case TYD_SERIAL_7BITS_CSIZE:
        tio.c_cflag |= CS7;
        break;
    }

    tio.c_cflag &= (unsigned int)~(PARENB | PARODD);
    switch (flags & TYD_SERIAL_PARITY_MASK) {
    case 0:
        break;
    case TYD_SERIAL_ODD_PARITY:
        tio.c_cflag |= PARENB | PARODD;
        break;
    case TYD_SERIAL_EVEN_PARITY:
        tio.c_cflag |= PARENB;
        break;

    default:
        assert(false);
        break;
    }

    tio.c_cflag &= (unsigned int)~CSTOPB;
    if (flags & TYD_SERIAL_2BITS_STOP)
        tio.c_cflag |= CSTOPB;

    tio.c_cflag &= (unsigned int)~CRTSCTS;
    tio.c_iflag &= (unsigned int)~(IXON | IXOFF);
    switch (flags & TYD_SERIAL_FLOW_MASK) {
    case 0:
        break;
    case TYD_SERIAL_XONXOFF_FLOW:
        tio.c_iflag |= IXON | IXOFF;
        break;
    case TYD_SERIAL_RTSCTS_FLOW:
        tio.c_cflag |= CRTSCTS;
        break;

    default:
        assert(false);
        break;
    }

    tio.c_cflag &= (unsigned int)~HUPCL;
    if (!(flags & TYD_SERIAL_NOHUP_CLOSE))
        tio.c_cflag |= HUPCL;

    r = tcsetattr(h->fd, TCSANOW, &tio);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "Unable to change serial port settings: %s",
                        strerror(errno));

    return 0;
}

ssize_t tyd_serial_read(struct tyd_handle *h, char *buf, size_t size, int timeout)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_SERIAL);
    assert(buf);
    assert(size);

    ssize_t r;

    if (timeout) {
        struct pollfd pfd;
        uint64_t start;

        pfd.events = POLLIN;
        pfd.fd = h->fd;

        start = ty_millis();
restart:
        r = poll(&pfd, 1, ty_adjust_timeout(timeout, start));
        if (r < 0) {
            if (errno == EINTR)
                goto restart;
            return ty_error(TY_ERROR_SYSTEM, "poll('%s') failed: %s", h->dev->path,
                            strerror(errno));
        }
        if (!r)
            return 0;
    }

    r = read(h->fd, buf, size);
    if (r < 0) {
        switch (errno) {
        case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
            return 0;
        case EIO:
        case ENXIO:
            return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->path);
        }
        return ty_error(TY_ERROR_SYSTEM, "read('%s') failed: %s", h->dev->path, strerror(errno));
    }

    return r;
}

ssize_t tyd_serial_write(struct tyd_handle *h, const char *buf, ssize_t size)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_SERIAL);
    assert(buf);

    if (size < 0)
        size = (ssize_t)strlen(buf);
    if (!size)
        return 0;

    struct pollfd pfd;
    ssize_t r;

    pfd.events = POLLOUT;
    pfd.fd = h->fd;

restart:
    r = poll(&pfd, 1, -1);
    if (r < 0) {
        switch (errno) {
        case EINTR:
            goto restart;
        case EIO:
        case ENXIO:
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
        }
        return ty_error(TY_ERROR_SYSTEM, "poll('%s') failed: %s", h->dev->path,
                        strerror(errno));
    }
    assert(r == 1);

    r = write(h->fd, buf, (size_t)size);
    if (r < 0) {
        switch (errno) {
        case EIO:
        case ENXIO:
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
        }
        return ty_error(TY_ERROR_SYSTEM, "write('%s') failed: %s", h->dev->path,
                        strerror(errno));
    }

    return r;
}
