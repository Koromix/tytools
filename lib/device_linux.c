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

#include "common.h"
#include <libudev.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "device.h"

struct udev_aggregate {
    struct udev_device *dev;
    struct udev_device *usb;
    struct udev_device *iface;
};

static struct udev *udev = NULL;

static void free_udev(void)
{
    udev_unref(udev);
}

static int init_udev(void)
{
    if (udev)
        return 0;

    // Quick inspection of libudev reveals it fails with malloc only
    udev = udev_new();
    if (!udev)
        return ty_error(TY_ERROR_MEMORY, NULL);

    // Avoid noise in valgrind (suppression file may be better?)
    atexit(free_udev);

    return 0;
}

static int get_device_path(char **rpath, struct udev_aggregate *agg)
{
    const char *path, *end;
    uint8_t buf;
    int r, len;

    path = udev_device_get_devpath(agg->dev);
    if (!path)
        return 0;

    path = strstr(path, "/usb");
    if (!path || strlen(path) < 5)
        return 0;
    path += 5;

    r = sscanf(path, "/%hhu%n", &buf, &len);
    if (r < 1)
        return 0;
    end = path + len;

    do {
        len = 0;
        r = sscanf(end, "-%hhu%n", &buf, &len);
        end += len;
    } while (r == 1);

    if (*end != '/')
        return 0;

    path++;

    // Account for 'usb-' prefix and NUL byte
    *rpath = malloc((size_t)(end - path) + 5);
    if (!*rpath)
        return ty_error(TY_ERROR_MEMORY, NULL);

    strcpy(*rpath, "usb-");
    strncat(*rpath, path, (size_t)(end - path));

    return 1;
}

static int fill_device_details(ty_device *dev, struct udev_aggregate *agg)
{
    const char *buf;
    int r;

    buf = udev_device_get_devnode(agg->dev);
    if (!buf || access(buf, F_OK) != 0)
        return 0;
    dev->node = strdup(buf);
    if (!buf)
        return ty_error(TY_ERROR_MEMORY, NULL);

    r = get_device_path(&dev->path, agg);
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
    if (!buf)
        return 0;
    dev->serial = strdup(buf);
    if (!buf)
        return ty_error(TY_ERROR_MEMORY, NULL);

    errno = 0;
    buf = udev_device_get_devpath(agg->iface);
    if (!buf)
        return 0;
    buf += strlen(buf) - 1;
    dev->iface = (uint8_t)strtoul(buf, NULL, 10);
    if (errno)
        return 0;

    return 1;
}

static int read_device_information(ty_device **rdev, struct udev_list_entry *entry,
                                   ty_device_type type)
{
    const char *name;
    struct udev_aggregate agg;
    ty_device *dev = NULL;
    int r;

    name = udev_list_entry_get_name(entry);
    if (!name)
        return 0;

    agg.dev = udev_device_new_from_syspath(udev, name);
    if (!agg.dev)
        return 0;

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

    dev->type = type;

    r = fill_device_details(dev, &agg);
    if (r <= 0)
        goto cleanup;

    *rdev = dev;
    dev = NULL;

    r = 1;
cleanup:
    ty_device_unref(dev);
    if (agg.dev)
        udev_device_unref(agg.dev);
    return r;
}

int ty_device_list(ty_device_type type, ty_device_walker *f, void *udata)
{
    const char *subsystem = NULL;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *entries;
    int r;

    switch (type) {
    case TY_DEVICE_HID:
        subsystem = "hidraw";
        break;
    case TY_DEVICE_SERIAL:
        subsystem = "tty";
        break;
    };

    if (!udev) {
        r = init_udev();
        if (r < 0)
            return r;
    }

    // Fails only for memory reasons
    enumerate = udev_enumerate_new(udev);
    if (!enumerate)
        return ty_error(TY_ERROR_MEMORY, NULL);

    udev_enumerate_add_match_subsystem(enumerate, subsystem);

    r = udev_enumerate_scan_devices(enumerate);
    if (r < 0) {
        r = ty_error(TY_ERROR_SYSTEM, "Unable to enumerate devices: %s", strerror(errno));
        goto cleanup;
    }
    entries = udev_enumerate_get_list_entry(enumerate);

    struct udev_list_entry *cur;
    udev_list_entry_foreach(cur, entries) {
        ty_device *dev = NULL;

        r = read_device_information(&dev, cur, type);
        if (r < 0)
            goto cleanup;
        if (!r)
            continue;

        r = (*f)(dev, udata);
        ty_device_unref(dev);
        if (r <= 0)
            goto cleanup;
    }

    r = 1;
cleanup:
    udev_enumerate_unref(enumerate);
    return r;
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
                        h->dev->node, strerror(errno));

    report.size = (uint32_t)size;
    r = ioctl(h->fd, HIDIOCGRDESC, &report);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "ioctl('%s', HIDIOCGRDESC) failed: %s", h->dev->node,
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
            return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", h->dev->node);
        }
        return ty_error(TY_ERROR_SYSTEM, "read('%s') failed: %s", h->dev->node, strerror(errno));
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
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->node);
        }
        return ty_error(TY_ERROR_SYSTEM, "write('%s') failed: %s", h->dev->node, strerror(errno));
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
            return ty_error(TY_ERROR_IO, "I/O error while writing to '%s'", h->dev->node);
        }
        return ty_error(TY_ERROR_SYSTEM, "ioctl('%s', HIDIOCSFEATURE) failed: %s", h->dev->node,
                        strerror(errno));
    }

    return 1;
}
