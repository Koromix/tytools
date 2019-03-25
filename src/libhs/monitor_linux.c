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
#include <libudev.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "device_priv.h"
#include "match_priv.h"
#include "monitor_priv.h"
#include "platform.h"

struct hs_monitor {
    _hs_match_helper match_helper;
    _hs_htable devices;

    struct udev_monitor *udev_mon;
    int wait_fd;
};

struct device_subsystem {
    const char *subsystem;
    hs_device_type type;
};

struct udev_aggregate {
    struct udev_device *dev;
    struct udev_device *usb;
    struct udev_device *iface;
};

static struct device_subsystem device_subsystems[] = {
    {"hidraw", HS_DEVICE_TYPE_HID},
    {"tty",    HS_DEVICE_TYPE_SERIAL},
    {NULL}
};

static pthread_mutex_t udev_init_lock = PTHREAD_MUTEX_INITIALIZER;
static struct udev *udev;
static int common_eventfd = -1;

#ifndef _GNU_SOURCE
int dup3(int oldfd, int newfd, int flags);
#endif

static int compute_device_location(struct udev_device *dev, char **rlocation)
{
    const char *busnum, *devpath;
    char *location;
    int r;

    busnum = udev_device_get_sysattr_value(dev, "busnum");
    devpath = udev_device_get_sysattr_value(dev, "devpath");

    if (!busnum || !devpath)
        return 0;

    r = _hs_asprintf(&location, "usb-%s-%s", busnum, devpath);
    if (r < 0)
        return hs_error(HS_ERROR_MEMORY, NULL);

    for (char *ptr = location; *ptr; ptr++) {
        if (*ptr == '.')
            *ptr = '-';
    }

    *rlocation = location;
    return 1;
}

static int fill_device_details(struct udev_aggregate *agg, hs_device *dev)
{
    const char *buf;
    int r;

    buf = udev_device_get_subsystem(agg->dev);
    if (!buf)
        return 0;

    if (strcmp(buf, "hidraw") == 0) {
        dev->type = HS_DEVICE_TYPE_HID;
    } else if (strcmp(buf, "tty") == 0) {
        dev->type = HS_DEVICE_TYPE_SERIAL;
    } else {
        return 0;
    }

    buf = udev_device_get_devnode(agg->dev);
    if (!buf || access(buf, F_OK) != 0)
        return 0;
    dev->path = strdup(buf);
    if (!dev->path)
        return hs_error(HS_ERROR_MEMORY, NULL);

    dev->key = strdup(udev_device_get_devpath(agg->dev));
    if (!dev->key)
        return hs_error(HS_ERROR_MEMORY, NULL);

    r = compute_device_location(agg->usb, &dev->location);
    if (r <= 0)
        return r;

    errno = 0;
    buf = udev_device_get_sysattr_value(agg->usb, "idVendor");
    if (!buf)
        return 0;
    dev->vid = (uint16_t)strtoul(buf, NULL, 16);
    if (errno)
        return 0;

    errno = 0;
    buf = udev_device_get_sysattr_value(agg->usb, "idProduct");
    if (!buf)
        return 0;
    dev->pid = (uint16_t)strtoul(buf, NULL, 16);
    if (errno)
        return 0;

    errno = 0;
    buf = udev_device_get_sysattr_value(agg->usb, "bcdDevice");
    if (!buf)
        return 0;
    dev->bcd_device = (uint16_t)strtoul(buf, NULL, 16);
    if (errno)
        return 0;

    buf = udev_device_get_sysattr_value(agg->usb, "manufacturer");
    if (buf) {
        dev->manufacturer_string = strdup(buf);
        if (!dev->manufacturer_string)
            return hs_error(HS_ERROR_MEMORY, NULL);
    }

    buf = udev_device_get_sysattr_value(agg->usb, "product");
    if (buf) {
        dev->product_string = strdup(buf);
        if (!dev->product_string)
            return hs_error(HS_ERROR_MEMORY, NULL);
    }

    buf = udev_device_get_sysattr_value(agg->usb, "serial");
    if (buf) {
        dev->serial_number_string = strdup(buf);
        if (!dev->serial_number_string)
            return hs_error(HS_ERROR_MEMORY, NULL);
    }

    errno = 0;
    buf = udev_device_get_devpath(agg->iface);
    buf += strlen(buf) - 1;
    dev->iface_number = (uint8_t)strtoul(buf, NULL, 10);
    if (errno)
        return 0;

    return 1;
}

static size_t read_hid_descriptor_sysfs(struct udev_aggregate *agg, uint8_t *desc_buf,
                                        size_t desc_buf_size)
{
    struct udev_device *hid_dev;
    char report_path[4096];
    int fd;
    ssize_t r;

    hid_dev = udev_device_get_parent_with_subsystem_devtype(agg->dev, "hid", NULL);
    if (!hid_dev)
        return 0;
    snprintf(report_path, sizeof(report_path), "%s/report_descriptor",
             udev_device_get_syspath(hid_dev));

    fd = open(report_path, O_RDONLY);
    if (fd < 0)
        return 0;
    r = read(fd, desc_buf, desc_buf_size);
    close(fd);
    if (r < 0)
        return 0;

    return (size_t)r;
}

static size_t read_hid_descriptor_hidraw(struct udev_aggregate *agg, uint8_t *desc_buf,
                                         size_t desc_buf_size)
{
    const char *node_path;
    int fd = -1;
    int hidraw_desc_size = 0;
    struct hidraw_report_descriptor hidraw_desc;
    int r;

    node_path = udev_device_get_devnode(agg->dev);
    if (!node_path)
        goto cleanup;
    fd = open(node_path, O_RDONLY);
    if (fd < 0)
        goto cleanup;

    r = ioctl(fd, HIDIOCGRDESCSIZE, &hidraw_desc_size);
    if (r < 0)
        goto cleanup;
    hidraw_desc.size = (uint32_t)hidraw_desc_size;
    r = ioctl(fd, HIDIOCGRDESC, &hidraw_desc);
    if (r < 0) {
        hidraw_desc_size = 0;
        goto cleanup;
    }

    if (desc_buf_size > hidraw_desc.size)
        desc_buf_size = hidraw_desc.size;
    memcpy(desc_buf, hidraw_desc.value, desc_buf_size);

cleanup:
    close(fd);
    return (size_t)hidraw_desc_size;
}

static void parse_hid_descriptor(hs_device *dev, uint8_t *desc, size_t desc_size)
{
    unsigned int collection_depth = 0;

    unsigned int item_size = 0;
    for (size_t i = 0; i < desc_size; i += item_size + 1) {
        unsigned int item_type;
        uint32_t item_data;

        item_type = desc[i];

        if (item_type == 0xFE) {
            // not interested in long items
            if (i + 1 < desc_size)
                item_size = (unsigned int)desc[i + 1] + 2;
            continue;
        }

        item_size = item_type & 3;
        if (item_size == 3)
            item_size = 4;
        item_type &= 0xFC;

        if (i + item_size >= desc_size) {
            hs_log(HS_LOG_WARNING, "Invalid HID descriptor for device '%s'", dev->path);
            return;
        }

        // little endian
        switch (item_size) {
            case 0: {
                item_data = 0;
            } break;
            case 1: {
                item_data = desc[i + 1];
            } break;
            case 2: {
                item_data = (uint32_t)(desc[i + 2] << 8) | desc[i + 1];
            } break;
            case 4: {
                item_data = (uint32_t)((desc[i + 4] << 24) | (desc[i + 3] << 16) |
                                       (desc[i + 2] << 8) | desc[i + 1]);
            } break;

            // silence unitialized warning
            default: {
                item_data = 0;
            } break;
        }

        switch (item_type) {
            // main items
            case 0xA0: {
                collection_depth++;
            } break;
            case 0xC0: {
                collection_depth--;
            } break;

            // global items
            case 0x84: {
                dev->u.hid.numbered_reports = true;
            } break;
            case 0x04: {
                if (!collection_depth)
                    dev->u.hid.usage_page = (uint16_t)item_data;
            } break;

            // local items
            case 0x08: {
                if (!collection_depth)
                    dev->u.hid.usage = (uint16_t)item_data;
            } break;
        }
    }
}

static void fill_hid_properties(struct udev_aggregate *agg, hs_device *dev)
{
    uint8_t desc[HID_MAX_DESCRIPTOR_SIZE];
    size_t desc_size;

    // The sysfs report_descriptor file appeared in 2011, somewhere around Linux 2.6.38
    desc_size = read_hid_descriptor_sysfs(agg, desc, sizeof(desc));
    if (!desc_size) {
        desc_size = read_hid_descriptor_hidraw(agg, desc, sizeof(desc));
        if (!desc_size) {
            // This will happen pretty often on old kernels, most HID nodes are root-only
            hs_log(HS_LOG_DEBUG, "Cannot get HID report descriptor from '%s'", dev->path);
            return;
        }
    }

    parse_hid_descriptor(dev, desc, desc_size);
}

static int read_device_information(struct udev_device *udev_dev, hs_device **rdev)
{
    struct udev_aggregate agg;
    hs_device *dev = NULL;
    int r;

    agg.dev = udev_dev;
    agg.usb = udev_device_get_parent_with_subsystem_devtype(agg.dev, "usb", "usb_device");
    agg.iface = udev_device_get_parent_with_subsystem_devtype(agg.dev, "usb", "usb_interface");
    if (!agg.usb || !agg.iface) {
        r = 0;
        goto cleanup;
    }

    dev = (hs_device *)calloc(1, sizeof(*dev));
    if (!dev) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }
    dev->refcount = 1;
    dev->status = HS_DEVICE_STATUS_ONLINE;

    r = fill_device_details(&agg, dev);
    if (r <= 0)
        goto cleanup;

    if (dev->type == HS_DEVICE_TYPE_HID)
        fill_hid_properties(&agg, dev);

    *rdev = dev;
    dev = NULL;

    r = 1;
cleanup:
    hs_device_unref(dev);
    return r;
}

static void release_udev(void)
{
    close(common_eventfd);
    udev_unref(udev);
    pthread_mutex_destroy(&udev_init_lock);
}

static int init_udev(void)
{
    static bool atexit_called;
    int r;

    // fast path
    if (udev && common_eventfd >= 0)
        return 0;

    pthread_mutex_lock(&udev_init_lock);

    if (!atexit_called) {
        atexit(release_udev);
        atexit_called = true;
    }

    if (!udev) {
        udev = udev_new();
        if (!udev) {
            r = hs_error(HS_ERROR_SYSTEM, "udev_new() failed");
            goto cleanup;
        }
    }

    if (common_eventfd < 0) {
        /* We use this as a never-ready placeholder descriptor for all newly created monitors,
           until hs_monitor_start() creates the udev monitor and its socket. */
        common_eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (common_eventfd < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "eventfd() failed: %s", strerror(errno));
            goto cleanup;
        }
    }

    r = 0;
cleanup:
    pthread_mutex_unlock(&udev_init_lock);
    return r;
}

static int enumerate(_hs_match_helper *match_helper, hs_enumerate_func *f, void *udata)
{
    struct udev_enumerate *enumerate;
    int r;

    enumerate = udev_enumerate_new(udev);
    if (!enumerate) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    udev_enumerate_add_match_is_initialized(enumerate);
    for (unsigned int i = 0; device_subsystems[i].subsystem; i++) {
        if (_hs_match_helper_has_type(match_helper, device_subsystems[i].type)) {
            r = udev_enumerate_add_match_subsystem(enumerate, device_subsystems[i].subsystem);
            if (r < 0) {
                r = hs_error(HS_ERROR_MEMORY, NULL);
                goto cleanup;
            }
        }
    }

    // Current implementation of udev_enumerate_scan_devices() does not fail
    r = udev_enumerate_scan_devices(enumerate);
    if (r < 0) {
        r = hs_error(HS_ERROR_SYSTEM, "udev_enumerate_scan_devices() failed");
        goto cleanup;
    }

    struct udev_list_entry *cur;
    udev_list_entry_foreach(cur, udev_enumerate_get_list_entry(enumerate)) {
        struct udev_device *udev_dev;
        hs_device *dev;

        udev_dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(cur));
        if (!udev_dev) {
            if (errno == ENOMEM) {
                r = hs_error(HS_ERROR_MEMORY, NULL);
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

        if (_hs_match_helper_match(match_helper, dev, &dev->match_udata)) {
            r = (*f)(dev, udata);
            hs_device_unref(dev);
            if (r)
                goto cleanup;
        } else {
            hs_device_unref(dev);
        }
    }

    r = 0;
cleanup:
    udev_enumerate_unref(enumerate);
    return r;
}

struct enumerate_enumerate_context {
    hs_enumerate_func *f;
    void *udata;
};

static int enumerate_enumerate_callback(hs_device *dev, void *udata)
{
    struct enumerate_enumerate_context *ctx = (struct enumerate_enumerate_context *)udata;

    _hs_device_log(dev, "Enumerate");
    return (*ctx->f)(dev, ctx->udata);
}

int hs_enumerate(const hs_match_spec *matches, unsigned int count, hs_enumerate_func *f,
                 void *udata)
{
    assert(f);

    _hs_match_helper match_helper = {0};
    struct enumerate_enumerate_context ctx;
    int r;

    r = init_udev();
    if (r < 0)
        return r;

    r = _hs_match_helper_init(&match_helper, matches, count);
    if (r < 0)
        return r;

    ctx.f = f;
    ctx.udata = udata;

    r = enumerate(&match_helper, enumerate_enumerate_callback, &ctx);

    _hs_match_helper_release(&match_helper);
    return r;
}

int hs_monitor_new(const hs_match_spec *matches, unsigned int count, hs_monitor **rmonitor)
{
    assert(rmonitor);

    hs_monitor *monitor;
    int r;

    monitor = (hs_monitor *)calloc(1, sizeof(*monitor));
    if (!monitor) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    monitor->wait_fd = -1;

    r = _hs_match_helper_init(&monitor->match_helper, matches, count);
    if (r < 0)
        goto error;

    r = _hs_htable_init(&monitor->devices, 64);
    if (r < 0)
        goto error;

    r = init_udev();
    if (r < 0)
        goto error;

    monitor->wait_fd = fcntl(common_eventfd, F_DUPFD_CLOEXEC, 0);
    if (monitor->wait_fd < 0) {
        r = hs_error(HS_ERROR_SYSTEM, "fcntl(F_DUPFD_CLOEXEC) failed: %s", strerror(errno));
        goto error;
    }

    *rmonitor = monitor;
    return 0;

error:
    hs_monitor_free(monitor);
    return r;
}

void hs_monitor_free(hs_monitor *monitor)
{
    if (monitor) {
        close(monitor->wait_fd);
        udev_monitor_unref(monitor->udev_mon);

        _hs_monitor_clear_devices(&monitor->devices);
        _hs_htable_release(&monitor->devices);
        _hs_match_helper_release(&monitor->match_helper);
    }

    free(monitor);
}

static int monitor_enumerate_callback(hs_device *dev, void *udata)
{
    hs_monitor *monitor = (hs_monitor *)udata;
    return _hs_monitor_add(&monitor->devices, dev, NULL, NULL);
}

int hs_monitor_start(hs_monitor *monitor)
{
    assert(monitor);

    int r;

    if (monitor->udev_mon)
        return 0;

    monitor->udev_mon = udev_monitor_new_from_netlink(udev, "udev");
    if (!monitor->udev_mon) {
        r = hs_error(HS_ERROR_SYSTEM, "udev_monitor_new_from_netlink() failed");
        goto error;
    }

    for (unsigned int i = 0; device_subsystems[i].subsystem; i++) {
        if (_hs_match_helper_has_type(&monitor->match_helper, device_subsystems[i].type)) {
            r = udev_monitor_filter_add_match_subsystem_devtype(monitor->udev_mon, device_subsystems[i].subsystem, NULL);
            if (r < 0) {
                r = hs_error(HS_ERROR_SYSTEM, "udev_monitor_filter_add_match_subsystem_devtype() failed");
                goto error;
            }
        }
    }

    r = udev_monitor_enable_receiving(monitor->udev_mon);
    if (r < 0) {
        r = hs_error(HS_ERROR_SYSTEM, "udev_monitor_enable_receiving() failed");
        goto error;
    }

    r = enumerate(&monitor->match_helper, monitor_enumerate_callback, monitor);
    if (r < 0)
        goto error;

    /* Given the documentation of dup3() and the kernel code handling it, I'm reasonably sure
       nothing can make this call fail. */
    dup3(udev_monitor_get_fd(monitor->udev_mon), monitor->wait_fd, O_CLOEXEC);

    return 0;

error:
    hs_monitor_stop(monitor);
    return r;
}

void hs_monitor_stop(hs_monitor *monitor)
{
    assert(monitor);

    if (!monitor->udev_mon)
        return;

    _hs_monitor_clear_devices(&monitor->devices);

    dup3(common_eventfd, monitor->wait_fd, O_CLOEXEC);
    udev_monitor_unref(monitor->udev_mon);
    monitor->udev_mon = NULL;
}

hs_handle hs_monitor_get_poll_handle(const hs_monitor *monitor)
{
    assert(monitor);
    return monitor->wait_fd;
}

int hs_monitor_refresh(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    assert(monitor);

    struct udev_device *udev_dev;
    int r;

    if (!monitor->udev_mon)
        return 0;

    errno = 0;
    while ((udev_dev = udev_monitor_receive_device(monitor->udev_mon))) {
        const char *action = udev_device_get_action(udev_dev);

        r = 0;
        if (strcmp(action, "add") == 0) {
            hs_device *dev = NULL;

            r = read_device_information(udev_dev, &dev);
            if (r > 0) {
                r = _hs_match_helper_match(&monitor->match_helper, dev, &dev->match_udata);
                if (r)
                    r = _hs_monitor_add(&monitor->devices, dev, f, udata);
            }

            hs_device_unref(dev);
        } else if (strcmp(action, "remove") == 0) {
            _hs_monitor_remove(&monitor->devices, udev_device_get_devpath(udev_dev), f, udata);
        }
        udev_device_unref(udev_dev);
        if (r)
            return r;

        errno = 0;
    }
    if (errno == ENOMEM)
        return hs_error(HS_ERROR_MEMORY, NULL);

    return 0;
}

int hs_monitor_list(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    return _hs_monitor_list(&monitor->devices, f, udata);
}
