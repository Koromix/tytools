/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "device_priv.h"
#include "platform.h"

int _hs_open_file_port(hs_device *dev, hs_port_mode mode, hs_port **rport)
{
    hs_port *port;
#ifdef __APPLE__
    unsigned int retry = 4;
#endif
    int fd_flags;
    int r;

    port = (hs_port *)calloc(1, sizeof(*port));
    if (!port) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    port->type = dev->type;
    port->u.file.fd = -1;

    port->mode = mode;
    port->path = dev->path;
    port->dev = hs_device_ref(dev);

    fd_flags = O_CLOEXEC | O_NOCTTY | O_NONBLOCK;
    switch (mode) {
        case HS_PORT_MODE_READ: { fd_flags |= O_RDONLY; } break;
        case HS_PORT_MODE_WRITE: { fd_flags |= O_WRONLY; } break;
        case HS_PORT_MODE_RW: { fd_flags |= O_RDWR; } break;
    }

restart:
    port->u.file.fd = open(dev->path, fd_flags);
    if (port->u.file.fd < 0) {
        switch (errno) {
            case EINTR: {
                goto restart;
            } break;

            case EACCES: {
                r = hs_error(HS_ERROR_ACCESS, "Permission denied for device '%s'", dev->path);
            } break;
            case EIO:
            case ENXIO:
            case ENODEV: {
                r = hs_error(HS_ERROR_IO, "I/O error while opening device '%s'", dev->path);
            } break;
            case ENOENT:
            case ENOTDIR: {
                r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
            } break;

#ifdef __APPLE__
            /* On El Capitan (and maybe before), the open fails for some time (around 40 - 50 ms
               on my computer) after the device notification. */
            case EBUSY: {
                if (retry--) {
                    usleep(20000);
                    goto restart;
                }
            } // fallthrough
#endif

            default: {
                r = hs_error(HS_ERROR_SYSTEM, "open('%s') failed: %s", dev->path, strerror(errno));
            } break;
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
#ifdef __linux__
    else if (dev->type == HS_DEVICE_TYPE_HID) {
        port->u.file.numbered_hid_reports = dev->u.hid.numbered_reports;
    }
#endif

    *rport = port;
    return 0;

error:
    hs_port_close(port);
    return r;
}

void _hs_close_file_port(hs_port *port)
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

hs_handle _hs_get_file_port_poll_handle(const hs_port *port)
{
    return port->u.file.fd;
}
