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
#include <libudev.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "ty/device.h"
#include "device_priv.h"
#include "ty/system.h"

struct ty_device_monitor {
    ty_list_head callbacks;
    int callback_id;

    ty_list_head devices;

    struct udev_enumerate *enumerate;
    struct udev_monitor *monitor;
};

struct udev_aggregate {
    struct udev_device *dev;
    struct udev_device *usb;
    struct udev_device *iface;
};

static struct udev *udev = NULL;

static int compute_device_location(const char *key, char **rlocation)
{
    const char *end;
    uint8_t buf;
    char *location;
    int r, len;

    key = strstr(key, "/usb");
    if (!key || strlen(key) < 5)
        return 0;
    key += 5;

    r = sscanf(key, "/%hhu%n", &buf, &len);
    if (r < 1)
        return 0;
    end = key + len;

    do {
        len = 0;
        r = sscanf(end, "-%hhu%n", &buf, &len);
        end += len;
    } while (r == 1);

    if (*end != '/')
        return 0;

    key++;

    // Account for 'usb-' prefix and NUL byte
    location = malloc((size_t)(end - key) + 5);
    if (!location)
        return ty_error(TY_ERROR_MEMORY, NULL);

    strcpy(location, "usb-");
    strncat(location, key, (size_t)(end - key));

    *rlocation = location;
    return 1;
}

static int fill_device_details(ty_device *dev, struct udev_aggregate *agg)
{
    const char *buf;
    int r;

    buf = udev_device_get_subsystem(agg->dev);
    if (!buf)
        return 0;

    if (strcmp(buf, "hidraw") == 0) {
        dev->type = TY_DEVICE_HID;
    } else if (strcmp(buf, "tty") == 0) {
        dev->type = TY_DEVICE_SERIAL;
    } else {
        return 0;
    }

    buf = udev_device_get_devnode(agg->dev);
    if (!buf || access(buf, F_OK) != 0)
        return 0;
    dev->path = strdup(buf);
    if (!dev->path)
        return ty_error(TY_ERROR_MEMORY, NULL);

    dev->key = strdup(udev_device_get_devpath(agg->dev));
    if (!dev->key)
        return ty_error(TY_ERROR_MEMORY, NULL);

    r = compute_device_location(dev->key, &dev->location);
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

static int read_device_information(struct udev_device *udev_dev, ty_device **rdev)
{
    struct udev_aggregate agg;
    ty_device *dev = NULL;
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
    ty_device_unref(dev);
    return r;
}

static int list_devices(ty_device_monitor *monitor)
{
    assert(monitor);

    int r;

    r = udev_enumerate_scan_devices(monitor->enumerate);
    if (r < 0)
        return TY_ERROR_SYSTEM;

    struct udev_list_entry *cur;
    udev_list_entry_foreach(cur, udev_enumerate_get_list_entry(monitor->enumerate)) {
        struct udev_device *udev_dev;
        ty_device *dev;

        udev_dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(cur));
        if (!udev_dev) {
            switch (errno) {
            case ENOMEM:
                return ty_error(TY_ERROR_MEMORY, NULL);
            default:
                break;
            }
            continue;
        }

        r = read_device_information(udev_dev, &dev);
        udev_device_unref(udev_dev);
        if (r < 0)
            return r;
        if (!r)
            continue;

        r = _ty_device_monitor_add(monitor, dev);
        ty_device_unref(dev);

        if (r < 0)
            return r;
    }

    return 0;
}

static void free_udev(void)
{
    udev_unref(udev);
}

int ty_device_monitor_new(ty_device_monitor **rmonitor)
{
    assert(rmonitor);

    ty_device_monitor *monitor;
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

    monitor->enumerate = udev_enumerate_new(udev);
    if (!monitor->enumerate) {
        // udev prints its own error messages
        r = TY_ERROR_SYSTEM;
        goto error;
    }
    udev_enumerate_add_match_is_initialized(monitor->enumerate);

    monitor->monitor = udev_monitor_new_from_netlink(udev, "udev");
    if (!monitor->monitor) {
        r = TY_ERROR_SYSTEM;
        goto error;
    }

    r = udev_monitor_enable_receiving(monitor->monitor);
    if (r < 0) {
        r = TY_ERROR_SYSTEM;
        goto error;
    }

    r = _ty_device_monitor_init(monitor);
    if (r < 0)
        goto error;

    r = list_devices(monitor);
    if (r < 0)
        goto error;

    *rmonitor = monitor;
    return 0;

error:
    ty_device_monitor_free(monitor);
    return r;
}

void ty_device_monitor_free(ty_device_monitor *monitor)
{
    if (monitor) {
        _ty_device_monitor_release(monitor);

        udev_enumerate_unref(monitor->enumerate);
        udev_monitor_unref(monitor->monitor);
    }

    free(monitor);
}

void ty_device_monitor_get_descriptors(ty_device_monitor *monitor, ty_descriptor_set *set, int id)
{
    assert(monitor);
    assert(set);

    ty_descriptor_set_add(set, udev_monitor_get_fd(monitor->monitor), id);
}

int ty_device_monitor_refresh(ty_device_monitor *monitor)
{
    assert(monitor);

    struct udev_device *udev_dev;
    int r;

    errno = 0;
    while ((udev_dev = udev_monitor_receive_device(monitor->monitor))) {
        const char *action = udev_device_get_action(udev_dev);

        r = 0;
        if (strcmp(action, "add") == 0) {
            ty_device *dev = NULL;

            r = read_device_information(udev_dev, &dev);
            if (r > 0)
                r = _ty_device_monitor_add(monitor, dev);

            ty_device_unref(dev);
        } else if (strcmp(action, "remove") == 0) {
            _ty_device_monitor_remove(monitor, udev_device_get_devpath(udev_dev));
        }

        udev_device_unref(udev_dev);

        if (r < 0)
            return r;

        errno = 0;
    }
    if (errno) {
        switch (errno) {
        case ENOMEM:
            return ty_error(TY_ERROR_MEMORY, NULL);
        default:
            break;
        }
    }

    return 1;
}

static void parse_descriptor(ty_hid_descriptor *desc, struct hidraw_report_descriptor *report)
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

int ty_hid_parse_descriptor(ty_handle *h, ty_hid_descriptor *desc)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_HID);
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

ssize_t ty_hid_read(ty_handle *h, uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_HID);
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
        return ty_error(TY_ERROR_SYSTEM, "read('%s') failed: %s", h->dev->path, strerror(errno));
    }

    return r;
}

ssize_t ty_hid_write(ty_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_HID);
    assert(buf);

    if (size < 2)
        return 0;

    ssize_t r;

restart:
    // On linux, USB requests timeout after 5000ms
    r = write(h->fd, (const char *)buf, size);
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
        return ty_error(TY_ERROR_SYSTEM, "write('%s') failed: %s", h->dev->path, strerror(errno));
    }

    return r;
}

int ty_hid_send_feature_report(ty_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_HID);
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

    return 1;
}
