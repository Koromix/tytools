/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include <libudev.h>
#include <linux/hidraw.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "ty/device.h"
#include "device_priv.h"
#include "device_posix_priv.h"
#include "ty/system.h"

struct tyd_monitor {
    TYD_MONITOR

    struct udev_monitor *monitor;
};

struct udev_aggregate {
    struct udev_device *dev;
    struct udev_device *usb;
    struct udev_device *iface;
};

static const char *device_subsystems[] = {
    "input",
    "hidraw",
    "tty",
    NULL
};

static struct udev *udev;

static int compute_device_location(struct udev_device *dev, char **rlocation)
{
    const char *busnum, *devpath;
    char *location;
    int r;

    busnum = udev_device_get_sysattr_value(dev, "busnum");
    devpath = udev_device_get_sysattr_value(dev, "devpath");

    if (!busnum || !devpath)
        return 0;

    r = asprintf(&location, "usb-%s-%s", busnum, devpath);
    if (r < 0)
        return ty_error(TY_ERROR_MEMORY, NULL);

    for (char *ptr = location; *ptr; ptr++) {
        if (*ptr == '.')
            *ptr = '-';
    }

    *rlocation = location;
    return 1;
}

static int fill_device_details(tyd_device *dev, struct udev_aggregate *agg)
{
    const char *buf;
    int r;

    buf = udev_device_get_subsystem(agg->dev);
    if (!buf)
        return 0;

    if (strcmp(buf, "hidraw") == 0) {
        dev->type = TYD_DEVICE_HID;
    } else if (strcmp(buf, "tty") == 0) {
        dev->type = TYD_DEVICE_SERIAL;
    } else {
        return 0;
    }
    dev->vtable = &_tyd_posix_device_vtable;

    buf = udev_device_get_devnode(agg->dev);
    if (!buf || access(buf, F_OK) != 0)
        return 0;
    dev->path = strdup(buf);
    if (!dev->path)
        return ty_error(TY_ERROR_MEMORY, NULL);

    dev->key = strdup(udev_device_get_devpath(agg->dev));
    if (!dev->key)
        return ty_error(TY_ERROR_MEMORY, NULL);

    r = compute_device_location(agg->usb, &dev->location);
    if (r <= 0)
        return r;

    errno = 0;
    buf = udev_device_get_property_value(agg->usb, "ID_VENDOR_ID");
    if (!buf)
        return 0;
    dev->vid = (uint16_t)strtoul(buf, NULL, 16);
    if (errno)
        return 0;

    errno = 0;
    buf = udev_device_get_property_value(agg->usb, "ID_MODEL_ID");
    if (!buf)
        return 0;
    dev->pid = (uint16_t)strtoul(buf, NULL, 16);
    if (errno)
        return 0;

    buf = udev_device_get_property_value(agg->usb, "ID_SERIAL_SHORT");
    if (buf) {
        dev->serial = strdup(buf);
        if (!dev->serial)
            return ty_error(TY_ERROR_MEMORY, NULL);
    }

    errno = 0;
    buf = udev_device_get_devpath(agg->iface);
    buf += strlen(buf) - 1;
    dev->iface = (uint8_t)strtoul(buf, NULL, 10);
    if (errno)
        return 0;

    return 1;
}

static int read_device_information(struct udev_device *udev_dev, tyd_device **rdev)
{
    struct udev_aggregate agg;
    tyd_device *dev = NULL;
    int r;

    agg.dev = udev_dev;
    agg.usb = udev_device_get_parent_with_subsystem_devtype(agg.dev, "usb", "usb_device");
    agg.iface = udev_device_get_parent_with_subsystem_devtype(agg.dev, "usb", "usb_interface");
    if (!agg.usb || !agg.iface) {
        r = 0;
        goto cleanup;
    }

    dev = calloc(1, sizeof(*dev));
    if (!dev) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }
    dev->refcount = 1;

    r = fill_device_details(dev, &agg);
    if (r <= 0)
        goto cleanup;

    *rdev = dev;
    dev = NULL;

    r = 1;
cleanup:
    tyd_device_unref(dev);
    return r;
}

static int list_devices(tyd_monitor *monitor)
{
    assert(monitor);

    struct udev_enumerate *enumerate;
    int r;

    enumerate = udev_enumerate_new(udev);
    if (!enumerate)
        return ty_error(TY_ERROR_MEMORY, NULL);

    udev_enumerate_add_match_is_initialized(enumerate);
    for (const char **cur = device_subsystems; *cur; cur++) {
        r = udev_enumerate_add_match_subsystem(enumerate, *cur);
        if (r < 0) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto cleanup;
        }
    }

    // Current implementation of udev_enumerate_scan_devices() does not fail
    r = udev_enumerate_scan_devices(enumerate);
    if (r < 0) {
        r = ty_error(TY_ERROR_SYSTEM, "udev_enumerate_scan_devices() failed");
        goto cleanup;
    }

    struct udev_list_entry *cur;
    udev_list_entry_foreach(cur, udev_enumerate_get_list_entry(enumerate)) {
        struct udev_device *udev_dev;
        tyd_device *dev;

        udev_dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(cur));
        if (!udev_dev) {
            if (errno == ENOMEM) {
                r = ty_error(TY_ERROR_MEMORY, NULL);
                goto cleanup;
            }
            continue;
        }

        r = read_device_information(udev_dev, &dev);
        udev_device_unref(udev_dev);
        if (r < 0)
            goto cleanup;
        if (!r)
            continue;

        r = _tyd_monitor_add(monitor, dev);
        tyd_device_unref(dev);

        if (r < 0)
            goto cleanup;
    }

    r = 0;
cleanup:
    udev_enumerate_unref(enumerate);
    return r;
}

static void free_udev(void)
{
    udev_unref(udev);
}

int tyd_monitor_new(tyd_monitor **rmonitor)
{
    assert(rmonitor);

    tyd_monitor *monitor;
    int r;

    if (!udev) {
        // Quick inspection of libudev reveals it fails with malloc only
        udev = udev_new();
        if (!udev)
            return ty_error(TY_ERROR_MEMORY, NULL);

        // valgrind compliance ;)
        atexit(free_udev);
    }

    monitor = calloc(1, sizeof(*monitor));
    if (!monitor)
        return ty_error(TY_ERROR_MEMORY, NULL);

    monitor->monitor = udev_monitor_new_from_netlink(udev, "udev");
    if (!monitor->monitor) {
        if (errno == ENOMEM) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
        } else {
            r = ty_error(TY_ERROR_SYSTEM, "udev_monitor_new_from_netlink() failed");
        }
        goto error;
    }

    for (const char **cur = device_subsystems; *cur; cur++) {
        r = udev_monitor_filter_add_match_subsystem_devtype(monitor->monitor, *cur, NULL);
        if (r < 0) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto error;
        }
    }

    r = udev_monitor_enable_receiving(monitor->monitor);
    if (r < 0) {
        if (r == -ENOMEM) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
        } else {
            r = ty_error(TY_ERROR_SYSTEM, "udev_monitor_enable_receiving() failed");
        }
        goto error;
    }

    r = _tyd_monitor_init(monitor);
    if (r < 0)
        goto error;

    r = list_devices(monitor);
    if (r < 0)
        goto error;

    *rmonitor = monitor;
    return 0;

error:
    tyd_monitor_free(monitor);
    return r;
}

void tyd_monitor_free(tyd_monitor *monitor)
{
    if (monitor) {
        _tyd_monitor_release(monitor);
        udev_monitor_unref(monitor->monitor);
    }

    free(monitor);
}

void tyd_monitor_get_descriptors(const tyd_monitor *monitor, ty_descriptor_set *set, int id)
{
    assert(monitor);
    assert(set);

    ty_descriptor_set_add(set, udev_monitor_get_fd(monitor->monitor), id);
}

int tyd_monitor_refresh(tyd_monitor *monitor)
{
    assert(monitor);

    struct udev_device *udev_dev;
    int r;

    errno = 0;
    while ((udev_dev = udev_monitor_receive_device(monitor->monitor))) {
        const char *action = udev_device_get_action(udev_dev);

        r = 0;
        if (strcmp(action, "add") == 0) {
            tyd_device *dev = NULL;

            r = read_device_information(udev_dev, &dev);
            if (r > 0)
                r = _tyd_monitor_add(monitor, dev);

            tyd_device_unref(dev);
        } else if (strcmp(action, "remove") == 0) {
            _tyd_monitor_remove(monitor, udev_device_get_devpath(udev_dev));
        }

        udev_device_unref(udev_dev);

        if (r < 0)
            return r;

        errno = 0;
    }
    if (errno == ENOMEM)
        return ty_error(TY_ERROR_MEMORY, NULL);

    return 1;
}

static void parse_descriptor(tyd_hid_descriptor *desc, struct hidraw_report_descriptor *report)
{
    size_t size;
    for (size_t i = 0; i < report->size; i += size + 1) {
        unsigned int type;
        uint32_t data;

        type = report->value[i] & 0xFC;
        size = report->value[i] & 3;
        if (size == 3)
            size = 4;

        if (i + size >= report->size)
            break;

        // Little Endian
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
            data = (uint32_t)(report->value[i + 4] << 24) | (uint32_t)(report->value[i + 3] << 16)
                | (uint32_t)(report->value[i + 2] << 8) | report->value[i + 1];
            break;

        // WTF?
        default:
            assert(false);
        }

        switch (type) {
        case 0x04:
            desc->usage_page = (uint16_t)data;
            break;
        case 0x08:
            desc->usage = (uint16_t)data;
            break;

        // Collection
        case 0xA0:
            return;
        }
    }
}

int tyd_hid_parse_descriptor(tyd_handle *h, tyd_hid_descriptor *desc)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_HID);
    assert(desc);

    struct hidraw_report_descriptor report;
    int size, r;

    r = ioctl(h->fd, HIDIOCGRDESCSIZE, &size);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "ioctl('%s', HIDIOCGRDESCSIZE) failed: %s",
                        h->dev->path, strerror(errno));

    report.size = (uint32_t)size;
    r = ioctl(h->fd, HIDIOCGRDESC, &report);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "ioctl('%s', HIDIOCGRDESC) failed: %s", h->dev->path,
                        strerror(errno));

    memset(desc, 0, sizeof(*desc));
    parse_descriptor(desc, &report);

    return 0;
}

ssize_t tyd_hid_read(tyd_handle *h, uint8_t *buf, size_t size, int timeout)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_HID);
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

ssize_t tyd_hid_write(tyd_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_HID);
    assert(buf);

    if (size < 2)
        return 0;

    ssize_t r;

restart:
    // On linux, USB requests timeout after 5000ms and O_NONBLOCK isn't honoured for write
    r = write(h->fd, (const char *)buf, size);
    if (r < 0) {
        switch (errno) {
        case EINTR:
            goto restart;
        case EIO:
        case ENXIO:
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
        }
        return ty_error(TY_ERROR_SYSTEM, "write('%s') failed: %s", h->dev->path, strerror(errno));
    }

    return r;
}

ssize_t tyd_hid_send_feature_report(tyd_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TYD_DEVICE_HID);
    assert(buf);

    if (size < 2)
        return 0;

    int r;

restart:
    r = ioctl(h->fd, HIDIOCSFEATURE(size), (const char *)buf);
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
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);
        }
        return ty_error(TY_ERROR_SYSTEM, "ioctl('%s', HIDIOCSFEATURE) failed: %s", h->dev->path,
                        strerror(errno));
    }

    return (ssize_t)size;
}
