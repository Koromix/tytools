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
#include <linux/hidraw.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "device_posix_priv.h"
#include "hid.h"
#include "platform.h"

static bool detect_kernel26_byte_bug()
{
    static bool init, bug;

    if (!init) {
        bug = hs_linux_version() >= 20628000 && hs_linux_version() < 20634000;
        init = true;
    }

    return bug;
}

ssize_t hs_hid_read(hs_handle *h, uint8_t *buf, size_t size, int timeout)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_HID);
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

    if (h->dev->u.hid.numbered_reports) {
        /* Work around a hidraw bug introduced in Linux 2.6.28 and fixed in Linux 2.6.34, see
           https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=5a38f2c7c4dd53d5be097930902c108e362584a3 */
        if (detect_kernel26_byte_bug()) {
            if (size + 1 > h->read_buf_size) {
                free(h->read_buf);
                h->read_buf_size = 0;

                h->read_buf = malloc(size + 1);
                if (!h->read_buf)
                    return hs_error(HS_ERROR_MEMORY, NULL);
                h->read_buf_size = size + 1;
            }

            r = read(h->fd, h->read_buf, size + 1);
            if (r > 0)
                memcpy(buf, h->read_buf + 1, (size_t)--r);
        } else {
            r = read(h->fd, buf, size);
        }
    } else {
        r = read(h->fd, buf + 1, size - 1);
        if (r > 0) {
            buf[0] = 0;
            r++;
        }
    }
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", h->dev->path,
                        strerror(errno));
    }

    return r;
}

ssize_t hs_hid_write(hs_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_HID);
    assert(h->mode & HS_HANDLE_MODE_WRITE);
    assert(buf);

    if (size < 2)
        return 0;

    ssize_t r;

restart:
    // On linux, USB requests timeout after 5000ms and O_NONBLOCK isn't honoured for write
    r = write(h->fd, (const char *)buf, size);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", h->dev->path,
                        strerror(errno));
    }

    return r;
}

ssize_t hs_hid_get_feature_report(hs_handle *h, uint8_t report_id, uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_HID);
    assert(h->mode & HS_HANDLE_MODE_READ);
    assert(buf);
    assert(size);

    ssize_t r;

    if (size >= 2)
        buf[1] = report_id;

restart:
    r = ioctl(h->fd, HIDIOCGFEATURE(size - 1), (const char *)buf + 1);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", h->dev->path,
                        strerror(errno));
    }

    buf[0] = report_id;
    return r + 1;
}

ssize_t hs_hid_send_feature_report(hs_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_HID);
    assert(h->mode & HS_HANDLE_MODE_WRITE);
    assert(buf);

    if (size < 2)
        return 0;

    ssize_t r;

restart:
    r = ioctl(h->fd, HIDIOCSFEATURE(size), (const char *)buf);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", h->dev->path,
                        strerror(errno));
    }

    return r;
}
