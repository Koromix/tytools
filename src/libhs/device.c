/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#ifdef _MSC_VER
    #include <windows.h>
#endif
#include "device_priv.h"
#include "monitor.h"
#include "platform.h"

hs_device *hs_device_ref(hs_device *dev)
{
    assert(dev);

#ifdef _MSC_VER
    InterlockedIncrement(&dev->refcount);
#else
    __atomic_fetch_add(&dev->refcount, 1, __ATOMIC_RELAXED);
#endif
    return dev;
}

void hs_device_unref(hs_device *dev)
{
    if (dev) {
#ifdef _MSC_VER
        if (InterlockedDecrement(&dev->refcount))
            return;
#else
        if (__atomic_fetch_sub(&dev->refcount, 1, __ATOMIC_RELEASE) > 1)
            return;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif

        free(dev->key);
        free(dev->location);
        free(dev->path);

        free(dev->manufacturer_string);
        free(dev->product_string);
        free(dev->serial_number_string);
    }

    free(dev);
}

void _hs_device_log(const hs_device *dev, const char *verb)
{
    switch (dev->type) {
        case HS_DEVICE_TYPE_SERIAL: {
            hs_log(HS_LOG_DEBUG, "%s serial device '%s' on iface %u\n"
                                 "  - USB VID/PID = %04x:%04x, USB location = %s\n"
                                 "  - USB manufacturer = %s, product = %s, S/N = %s",
                   verb, dev->key, dev->iface_number, dev->vid, dev->pid, dev->location,
                   dev->manufacturer_string ? dev->manufacturer_string : "(none)",
                   dev->product_string ? dev->product_string : "(none)",
                   dev->serial_number_string ? dev->serial_number_string : "(none)");
        } break;

        case HS_DEVICE_TYPE_HID: {
            hs_log(HS_LOG_DEBUG, "%s HID device '%s' on iface %u\n"
                                 "  - USB VID/PID = %04x:%04x, USB location = %s\n"
                                 "  - USB manufacturer = %s, product = %s, S/N = %s\n"
                                 "  - HID usage page = 0x%x, HID usage = 0x%x",
                   verb, dev->key, dev->iface_number, dev->vid, dev->pid, dev->location,
                   dev->manufacturer_string ? dev->manufacturer_string : "(none)",
                   dev->product_string ? dev->product_string : "(none)",
                   dev->serial_number_string ? dev->serial_number_string : "(none)",
                   dev->u.hid.usage_page, dev->u.hid.usage);
        } break;
    }
}

int hs_port_open(hs_device *dev, hs_port_mode mode, hs_port **rport)
{
    assert(dev);
    assert(rport);

    if (dev->status != HS_DEVICE_STATUS_ONLINE)
        return hs_error(HS_ERROR_NOT_FOUND, "Device '%s' is not connected", dev->path);

    switch (dev->type) {
        case HS_DEVICE_TYPE_HID: {
#ifdef __APPLE__
            return _hs_darwin_open_hid_port(dev, mode, rport);
#else
            return _hs_open_file_port(dev, mode, rport);
#endif
        } break;

        case HS_DEVICE_TYPE_SERIAL: {
            return _hs_open_file_port(dev, mode, rport);
        } break;
    }

    assert(false);
    return 0;
}

void hs_port_close(hs_port *port)
{
    if (!port)
        return;

    switch (port->type) {
        case HS_DEVICE_TYPE_HID: {
#ifdef __APPLE__
            _hs_darwin_close_hid_port(port);
#else
            _hs_close_file_port(port);
#endif
            return;
        } break;

        case HS_DEVICE_TYPE_SERIAL: {
            _hs_close_file_port(port);
            return;
        } break;
    }

    assert(false);
}

hs_device *hs_port_get_device(const hs_port *port)
{
    assert(port);
    return port->dev;
}

hs_handle hs_port_get_poll_handle(const hs_port *port)
{
    assert(port);

    switch (port->type) {
        case HS_DEVICE_TYPE_HID: {
#ifdef __APPLE__
            return _hs_darwin_get_hid_port_poll_handle(port);
#else
            return _hs_get_file_port_poll_handle(port);
#endif
        } break;

        case HS_DEVICE_TYPE_SERIAL: {
            return _hs_get_file_port_poll_handle(port);
        } break;
    }

    assert(false);
    return 0;
}
