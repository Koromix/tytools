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
#include <fcntl.h>
#include <linux/hidraw.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "device_priv.h"
#include "hs/hid.h"
#include "hs/platform.h"

struct hs_handle {
    _HS_HANDLE

    int fd;

    bool numbered_reports;
    uint16_t usage_page;
    uint16_t usage;

    // Used to work around an old kernel 2.6 (pre-2.6.34) bug
    uint8_t *buf;
    size_t buf_size;
};

static bool detect_kernel26_byte_bug()
{
    static bool init, bug;

    if (!init) {
        bug = hs_linux_version() >= 20628000 && hs_linux_version() < 20634000;
        init = true;
    }

    return bug;
}

static void parse_descriptor(hs_handle *h, struct hidraw_report_descriptor *report)
{
    unsigned int collection_depth = 0;

    unsigned int size = 0;
    for (size_t i = 0; i < report->size; i += size + 1) {
        unsigned int type;
        uint32_t data;

        type = report->value[i];

        if (type == 0xFE) {
            // not interested in long items
            if (i + 1 < report->size)
                size = (unsigned int)report->value[i + 1] + 2;
            continue;
        }

        size = type & 3;
        if (size == 3)
            size = 4;
        type &= 0xFC;

        if (i + size >= report->size) {
            hs_log(HS_LOG_WARNING, "Invalid HID descriptor for device '%s'", h->dev->path);
            return;
        }

        // little endian
        switch (size) {
        case 0:
            data = 0;
            break;
        case 1:
            data = report->value[i + 1];
            break;
        case 2:
            data = (uint32_t)(report->value[i + 2] << 8) | report->value[i + 1];
            break;
        case 4:
            data = (uint32_t)((report->value[i + 4] << 24) | (report->value[i + 3] << 16)
                | (report->value[i + 2] << 8) | report->value[i + 1]);
            break;

        // silence unitialized warning
        default:
            data = 0;
            break;
        }

        switch (type) {
        // main items
        case 0xA0:
            collection_depth++;
            break;
        case 0xC0:
            collection_depth--;
            break;

        // global items
        case 0x84:
            h->numbered_reports = true;
            break;
        case 0x04:
            if (!collection_depth)
                h->usage_page = (uint16_t)data;
            break;

        // local items
        case 0x08:
            if (!collection_depth)
                h->usage = (uint16_t)data;
            break;
        }
    }
}

static int open_hidraw_device(hs_device *dev, hs_handle_mode mode, hs_handle **rh)
{
    hs_handle *h;
    struct hidraw_report_descriptor report;
    int fd_flags;
    int size, r;

    h = calloc(1, sizeof(*h));
    if (!h) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    h->dev = hs_device_ref(dev);
    h->mode = mode;

    fd_flags = O_CLOEXEC | O_NONBLOCK;
    switch (mode) {
    case HS_HANDLE_MODE_READ:
        fd_flags |= O_RDONLY;
        break;
    case HS_HANDLE_MODE_WRITE:
        fd_flags |= O_WRONLY;
        break;
    case HS_HANDLE_MODE_RW:
        fd_flags |= O_RDWR;
        break;
    }

restart:
    h->fd = open(dev->path, fd_flags);
    if (h->fd < 0) {
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

        default:
            r = hs_error(HS_ERROR_SYSTEM, "open('%s') failed: %s", dev->path, strerror(errno));
            break;
        }
        goto error;
    }

    r = ioctl(h->fd, HIDIOCGRDESCSIZE, &size);
    if (r < 0) {
        r = hs_error(HS_ERROR_SYSTEM, "ioctl('%s', HIDIOCGRDESCSIZE) failed: %s", h->dev->path,
                     strerror(errno));
        goto error;
    }
    report.size = (uint32_t)size;

    r = ioctl(h->fd, HIDIOCGRDESC, &report);
    if (r < 0) {
        r = hs_error(HS_ERROR_SYSTEM, "ioctl('%s', HIDIOCGRDESC) failed: %s", h->dev->path,
                     strerror(errno));
        goto error;
    }

    parse_descriptor(h, &report);

    *rh = h;
    return 0;

error:
    hs_handle_close(h);
    return r;
}

static void close_hidraw_device(hs_handle *h)
{
    if (h) {
        free(h->buf);

        close(h->fd);
        hs_device_unref(h->dev);
    }

    free(h);
}

static hs_descriptor get_hidraw_descriptor(const hs_handle *h)
{
    return h->fd;
}

const struct _hs_device_vtable _hs_linux_hid_vtable = {
    .open = open_hidraw_device,
    .close = close_hidraw_device,

    .get_descriptor = get_hidraw_descriptor
};

int hs_hid_parse_descriptor(hs_handle *h, hs_hid_descriptor *desc)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_HID);
    assert(desc);

    desc->usage_page = h->usage_page;
    desc->usage = h->usage;

    return 0;
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
            if (size + 1 > h->buf_size) {
                free(h->buf);
                h->buf_size = 0;

                h->buf = malloc(size + 1);
                if (!h->buf)
                    return hs_error(HS_ERROR_MEMORY, NULL);
                h->buf_size = size + 1;
            }

            r = read(h->fd, h->buf, size + 1);
            if (r > 0)
                memcpy(buf, h->buf + 1, (size_t)--r);
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
