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
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "device_posix_priv.h"
#include "hs/platform.h"

static int open_posix_device(hs_device *dev, hs_handle **rh)
{
    hs_handle *h;
#ifdef __APPLE__
    unsigned int retry = 4;
#endif
    int r;

    h = calloc(1, sizeof(*h));
    if (!h) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    h->dev = hs_device_ref(dev);

restart:
    h->fd = open(dev->path, O_RDWR | O_CLOEXEC | O_NOCTTY | O_NONBLOCK);
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

#ifdef __APPLE__
    if (dev->type == HS_DEVICE_TYPE_SERIAL)
        ioctl(h->fd, TIOCSDTR);
#endif

    *rh = h;
    return 0;

error:
    hs_handle_close(h);
    return r;
}

static void close_posix_device(hs_handle *h)
{
    if (h) {
        close(h->fd);
        hs_device_unref(h->dev);
    }

    free(h);
}

static hs_descriptor get_posix_descriptor(const hs_handle *h)
{
    return h->fd;
}

const struct _hs_device_vtable _hs_posix_device_vtable = {
    .open = open_posix_device,
    .close = close_posix_device,

    .get_descriptor = get_posix_descriptor
};
