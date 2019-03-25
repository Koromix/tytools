/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include <fcntl.h>
#include <linux/hidraw.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "device_priv.h"
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

ssize_t hs_hid_read(hs_port *port, uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    ssize_t r;

    if (timeout) {
        struct pollfd pfd;
        uint64_t start;

        pfd.events = POLLIN;
        pfd.fd = port->u.file.fd;

        start = hs_millis();
restart:
        r = poll(&pfd, 1, hs_adjust_timeout(timeout, start));
        if (r < 0) {
            if (errno == EINTR)
                goto restart;

            return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", port->path,
                            strerror(errno));
        }
        if (!r)
            return 0;
    }

    if (port->u.file.numbered_hid_reports) {
        /* Work around a hidraw bug introduced in Linux 2.6.28 and fixed in Linux 2.6.34, see
           https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=5a38f2c7c4dd53d5be097930902c108e362584a3 */
        if (detect_kernel26_byte_bug()) {
            if (size + 1 > port->u.file.read_buf_size) {
                free(port->u.file.read_buf);
                port->u.file.read_buf_size = 0;

                port->u.file.read_buf = (uint8_t *)malloc(size + 1);
                if (!port->u.file.read_buf)
                    return hs_error(HS_ERROR_MEMORY, NULL);
                port->u.file.read_buf_size = size + 1;
            }

            r = read(port->u.file.fd, port->u.file.read_buf, size + 1);
            if (r > 0)
                memcpy(buf, port->u.file.read_buf + 1, (size_t)--r);
        } else {
            r = read(port->u.file.fd, buf, size);
        }
    } else {
        r = read(port->u.file.fd, buf + 1, size - 1);
        if (r > 0) {
            buf[0] = 0;
            r++;
        }
    }
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", port->path,
                        strerror(errno));
    }

    return r;
}

ssize_t hs_hid_write(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    if (size < 2)
        return 0;

    ssize_t r;

restart:
    // On linux, USB requests timeout after 5000ms and O_NONBLOCK isn't honoured for write
    r = write(port->u.file.fd, (const char *)buf, size);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", port->path,
                        strerror(errno));
    }

    return r;
}

ssize_t hs_hid_get_feature_report(hs_port *port, uint8_t report_id, uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    ssize_t r;

    if (size >= 2)
        buf[1] = report_id;

restart:
    r = ioctl(port->u.file.fd, HIDIOCGFEATURE(size - 1), (const char *)buf + 1);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", port->path,
                        strerror(errno));
    }

    buf[0] = report_id;
    return r + 1;
}

ssize_t hs_hid_send_feature_report(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    if (size < 2)
        return 0;

    ssize_t r;

restart:
    r = ioctl(port->u.file.fd, HIDIOCSFEATURE(size), (const char *)buf);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", port->path,
                        strerror(errno));
    }

    return r;
}
