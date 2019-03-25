/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "device_priv.h"
#include "platform.h"
#include "serial.h"

int hs_serial_set_config(hs_port *port, const hs_serial_config *config)
{
    assert(port);
    assert(config);

    struct termios tio;
    int modem_bits;
    int r;

    r = tcgetattr(port->u.file.fd, &tio);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to get serial port settings from '%s': %s",
                        port->path, strerror(errno));
    r = ioctl(port->u.file.fd, TIOCMGET, &modem_bits);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to get modem bits from '%s': %s",
                        port->path, strerror(errno));

    if (config->baudrate) {
        speed_t std_baudrate;

        switch (config->baudrate) {
            case 110: { std_baudrate = B110; } break;
            case 134: { std_baudrate = B134; } break;
            case 150: { std_baudrate = B150; } break;
            case 200: { std_baudrate = B200; } break;
            case 300: { std_baudrate = B300; } break;
            case 600: { std_baudrate = B600; } break;
            case 1200: { std_baudrate = B1200; } break;
            case 1800: { std_baudrate = B1800; } break;
            case 2400: { std_baudrate = B2400; } break;
            case 4800: { std_baudrate = B4800; } break;
            case 9600: { std_baudrate = B9600; } break;
            case 19200: { std_baudrate = B19200; } break;
            case 38400: { std_baudrate = B38400; } break;
            case 57600: { std_baudrate = B57600; } break;
            case 115200: { std_baudrate = B115200; } break;
            case 230400: { std_baudrate = B230400; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Unsupported baud rate value: %u",
                                config->baudrate);
            } break;
        }

        cfsetispeed(&tio, std_baudrate);
        cfsetospeed(&tio, std_baudrate);
    }

    if (config->databits) {
        tio.c_cflag &= (unsigned int)~CSIZE;

        switch (config->databits) {
            case 5: { tio.c_cflag |= CS5; } break;
            case 6: { tio.c_cflag |= CS6; } break;
            case 7: { tio.c_cflag |= CS7; } break;
            case 8: { tio.c_cflag |= CS8; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid data bits setting: %u",
                                config->databits);
            } break;
        }
    }

    if (config->stopbits) {
        tio.c_cflag &= (unsigned int)~CSTOPB;

        switch (config->stopbits) {
            case 1: {} break;
            case 2: { tio.c_cflag |= CSTOPB; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid stop bits setting: %u",
                                config->stopbits);
            } break;
        }
    }

    if (config->parity) {
        tio.c_cflag &= (unsigned int)~(PARENB | PARODD);
#ifdef CMSPAR
        tio.c_cflag &= (unsigned int)~CMSPAR;
#endif

        switch (config->parity) {
            case HS_SERIAL_CONFIG_PARITY_OFF: {} break;
            case HS_SERIAL_CONFIG_PARITY_EVEN: { tio.c_cflag |= PARENB; } break;
            case HS_SERIAL_CONFIG_PARITY_ODD: { tio.c_cflag |= PARENB | PARODD; } break;
#ifdef CMSPAR
            case HS_SERIAL_CONFIG_PARITY_SPACE: { tio.c_cflag |= PARENB | CMSPAR; } break;
            case HS_SERIAL_CONFIG_PARITY_MARK: { tio.c_cflag |= PARENB | PARODD | CMSPAR; } break;
#else

            case HS_SERIAL_CONFIG_PARITY_MARK:
            case HS_SERIAL_CONFIG_PARITY_SPACE: {
                return hs_error(HS_ERROR_SYSTEM, "Mark/space parity is not supported");
            } break;
#endif
            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid parity setting: %d", config->parity);
            } break;
        }
    }

    if (config->rts) {
        tio.c_cflag &= (unsigned int)~CRTSCTS;
        modem_bits &= ~TIOCM_RTS;

        switch (config->rts) {
            case HS_SERIAL_CONFIG_RTS_OFF: {} break;
            case HS_SERIAL_CONFIG_RTS_ON: { modem_bits |= TIOCM_RTS; } break;
            case HS_SERIAL_CONFIG_RTS_FLOW: { tio.c_cflag |= CRTSCTS; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid RTS setting: %d", config->rts);
            } break;
        }
    }

    switch (config->dtr) {
        case 0: {} break;
        case HS_SERIAL_CONFIG_DTR_OFF: { modem_bits &= ~TIOCM_DTR; } break;
        case HS_SERIAL_CONFIG_DTR_ON: { modem_bits |= TIOCM_DTR; } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid DTR setting: %d", config->dtr);
        } break;
    }

    if (config->xonxoff) {
        tio.c_iflag &= (unsigned int)~(IXON | IXOFF | IXANY);

        switch (config->xonxoff) {
            case HS_SERIAL_CONFIG_XONXOFF_OFF: {} break;
            case HS_SERIAL_CONFIG_XONXOFF_IN: { tio.c_iflag |= IXOFF; } break;
            case HS_SERIAL_CONFIG_XONXOFF_OUT: { tio.c_iflag |= IXON | IXANY; } break;
            case HS_SERIAL_CONFIG_XONXOFF_INOUT: { tio.c_iflag |= IXOFF | IXON | IXANY; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid XON/XOFF setting: %d", config->xonxoff);
            } break;
        }
    }

    r = ioctl(port->u.file.fd, TIOCMSET, &modem_bits);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to set modem bits of '%s': %s",
                        port->path, strerror(errno));
    r = tcsetattr(port->u.file.fd, TCSANOW, &tio);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to change serial port settings of '%s': %s",
                        port->path, strerror(errno));

    return 0;
}

int hs_serial_get_config(hs_port *port, hs_serial_config *config)
{
    assert(port);

    struct termios tio;
    int modem_bits;
    int r;

    r = tcgetattr(port->u.file.fd, &tio);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to read port settings from '%s': %s",
                        port->path, strerror(errno));
    r = ioctl(port->u.file.fd, TIOCMGET, &modem_bits);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to get modem bits from '%s': %s",
                        port->path, strerror(errno));

    /* 0 is the INVALID value for all parameters, we keep that value if we can't interpret
       a termios value (only a cross-platform subset of it is exposed in hs_serial_config). */
    memset(config, 0, sizeof(*config));

    switch (cfgetispeed(&tio)) {
        case B110: { config->baudrate = 110; } break;
        case B134: { config->baudrate = 134; } break;
        case B150: { config->baudrate = 150; } break;
        case B200: { config->baudrate = 200; } break;
        case B300: { config->baudrate = 300; } break;
        case B600: { config->baudrate = 600; } break;
        case B1200: { config->baudrate = 1200; } break;
        case B1800: { config->baudrate = 1800; } break;
        case B2400: { config->baudrate = 2400; } break;
        case B4800: { config->baudrate = 4800; } break;
        case B9600: { config->baudrate = 9600; } break;
        case B19200: { config->baudrate = 19200; } break;
        case B38400: { config->baudrate = 38400; } break;
        case B57600: { config->baudrate = 57600; } break;
        case B115200: { config->baudrate = 115200; } break;
        case B230400: { config->baudrate = 230400; } break;
    }

    switch (tio.c_cflag & CSIZE) {
        case CS5: { config->databits = 5; } break;
        case CS6: { config->databits = 6; } break;
        case CS7: { config->databits = 7; } break;
        case CS8: { config->databits = 8; } break;
    }

    if (tio.c_cflag & CSTOPB) {
        config->stopbits = 2;
    } else {
        config->stopbits = 1;
    }

    // FIXME: should we detect IGNPAR here?
    if (tio.c_cflag & PARENB) {
#ifdef CMSPAR
        switch (tio.c_cflag & (PARODD | CMSPAR)) {
#else
        switch (tio.c_cflag & PARODD) {
#endif
            case 0: { config->parity = HS_SERIAL_CONFIG_PARITY_EVEN; } break;
            case PARODD: { config->parity = HS_SERIAL_CONFIG_PARITY_ODD; } break;
#ifdef CMSPAR
            case CMSPAR: { config->parity = HS_SERIAL_CONFIG_PARITY_SPACE; } break;
            case CMSPAR | PARODD: { config->parity = HS_SERIAL_CONFIG_PARITY_MARK; } break;
#endif
        }
    } else {
        config->parity = HS_SERIAL_CONFIG_PARITY_OFF;
    }

    if (tio.c_cflag & CRTSCTS) {
        config->rts = HS_SERIAL_CONFIG_RTS_FLOW;
    } else if (modem_bits & TIOCM_RTS) {
        config->rts = HS_SERIAL_CONFIG_RTS_ON;
    } else {
        config->rts = HS_SERIAL_CONFIG_RTS_OFF;
    }

    if (modem_bits & TIOCM_DTR) {
        config->dtr = HS_SERIAL_CONFIG_DTR_ON;
    } else {
        config->dtr = HS_SERIAL_CONFIG_DTR_OFF;
    }

    switch (tio.c_iflag & (IXON | IXOFF)) {
        case 0: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_OFF; } break;
        case IXOFF: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_IN; } break;
        case IXON: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_OUT; } break;
        case IXOFF | IXON: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_INOUT; } break;
    }

    return 0;
}

ssize_t hs_serial_read(hs_port *port, uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_SERIAL);
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

    r = read(port->u.file.fd, buf, size);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", port->path,
                        strerror(errno));
    }

    return r;
}

ssize_t hs_serial_write(hs_port *port, const uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_SERIAL);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    struct pollfd pfd;
    uint64_t start;
    int adjusted_timeout;
    size_t written;

    pfd.events = POLLOUT;
    pfd.fd = port->u.file.fd;

    start = hs_millis();
    adjusted_timeout = timeout;

    written = 0;
    do {
        ssize_t r;

        r = poll(&pfd, 1, adjusted_timeout);
        if (r < 0) {
            if (errno == EINTR)
                continue;

            return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", port->path,
                            strerror(errno));
        }
        if (!r)
            break;

        r = write(port->u.file.fd, buf + written, size - written);
        if (r < 0) {
            if (errno == EINTR)
                continue;

            return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", port->path,
                            strerror(errno));
        }
        written += (size_t)r;

        adjusted_timeout = hs_adjust_timeout(timeout, start);
    } while (written < size && adjusted_timeout);

    return (ssize_t)written;
}
