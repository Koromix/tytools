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

#include "util.h"
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include "device_posix_priv.h"
#include "hs/platform.h"
#include "hs/serial.h"

int hs_serial_set_attributes(hs_handle *h, uint32_t rate, int flags)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_SERIAL);

    struct termios tio;
    int r;

    r = tcgetattr(h->fd, &tio);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to read serial port settings: %s",
                        strerror(errno));

    cfmakeraw(&tio);
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    tio.c_cflag |= CLOCAL;

    switch (rate) {
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
    switch (flags & HS_SERIAL_MASK_CSIZE) {
    case HS_SERIAL_CSIZE_5BITS:
        tio.c_cflag |= CS5;
        break;
    case HS_SERIAL_CSIZE_6BITS:
        tio.c_cflag |= CS6;
        break;
    case HS_SERIAL_CSIZE_7BITS:
        tio.c_cflag |= CS7;
        break;

    default:
        tio.c_cflag |= CS8;
        break;
    }

    tio.c_cflag &= (unsigned int)~(PARENB | PARODD);
    switch (flags & HS_SERIAL_MASK_PARITY) {
    case 0:
        break;
    case HS_SERIAL_PARITY_ODD:
        tio.c_cflag |= PARENB | PARODD;
        break;
    case HS_SERIAL_PARITY_EVEN:
        tio.c_cflag |= PARENB;
        break;

    default:
        assert(false);
        break;
    }

    tio.c_cflag &= (unsigned int)~CSTOPB;
    if (flags & HS_SERIAL_STOP_2BITS)
        tio.c_cflag |= CSTOPB;

    tio.c_cflag &= (unsigned int)~CRTSCTS;
    tio.c_iflag &= (unsigned int)~(IXON | IXOFF);
    switch (flags & HS_SERIAL_MASK_FLOW) {
    case 0:
        break;
    case HS_SERIAL_FLOW_XONXOFF:
        tio.c_iflag |= IXON | IXOFF;
        break;
    case HS_SERIAL_FLOW_RTSCTS:
        tio.c_cflag |= CRTSCTS;
        break;

    default:
        assert(false);
        break;
    }

    tio.c_cflag &= (unsigned int)~HUPCL;
    if (!(flags & HS_SERIAL_CLOSE_NOHUP))
        tio.c_cflag |= HUPCL;

    r = tcsetattr(h->fd, TCSANOW, &tio);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to change serial port settings: %s",
                        strerror(errno));

    return 0;
}

ssize_t hs_serial_read(hs_handle *h, uint8_t *buf, size_t size, int timeout)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_SERIAL);
    assert(h->mode & HS_HANDLE_MODE_READ);
    assert(buf);
    assert(size);

    ssize_t r;

    if (timeout) {
        struct pollfd pfd;
        uint64_t start;

        pfd.events = POLLIN;
        pfd.fd = h->fd;

        start = hs_millis();
restart:
        r = poll(&pfd, 1, hs_adjust_timeout(timeout, start));
        if (r < 0) {
            if (errno == EINTR)
                goto restart;

            return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", h->dev->path,
                            strerror(errno));
        }
        if (!r)
            return 0;
    }

    r = read(h->fd, buf, size);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", h->dev->path,
                        strerror(errno));
    }

    return r;
}

ssize_t hs_serial_write(hs_handle *h, const uint8_t *buf, ssize_t size)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_SERIAL);
    assert(h->mode & HS_HANDLE_MODE_WRITE);
    assert(buf);

    if (!size)
        return 0;

    struct pollfd pfd;
    ssize_t r;

    pfd.events = POLLOUT;
    pfd.fd = h->fd;

restart:
    r = poll(&pfd, 1, -1);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", h->dev->path,
                        strerror(errno));
    }
    assert(r == 1);

    r = write(h->fd, buf, (size_t)size);
    if (r < 0)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", h->dev->path,
                        strerror(errno));

    return r;
}
