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
#ifdef _MSC_VER
    #include <windows.h>
#endif
#include "device_priv.h"
#include "hs/monitor.h"
#include "hs/platform.h"

struct hs_handle {
    _HS_HANDLE
};

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

        free(dev->manufacturer);
        free(dev->product);
        free(dev->serial);
    }

    free(dev);
}

hs_device_status hs_device_get_status(const hs_device *dev)
{
    assert(dev);
    return dev->state;
}

hs_device_type hs_device_get_type(const hs_device *dev)
{
    assert(dev);
    return dev->type;
}

const char *hs_device_get_location(const hs_device *dev)
{
    assert(dev);
    return dev->location;
}

uint8_t hs_device_get_interface_number(const hs_device *dev)
{
    assert(dev);
    return dev->iface;
}

const char *hs_device_get_path(const hs_device *dev)
{
    assert(dev);
    return dev->path;
}

uint16_t hs_device_get_vid(const hs_device *dev)
{
    assert(dev);
    return dev->vid;
}

uint16_t hs_device_get_pid(const hs_device *dev)
{
    assert(dev);
    return dev->pid;
}

const char *hs_device_get_manufacturer_string(const hs_device *dev)
{
    assert(dev);
    return dev->manufacturer;
}

const char *hs_device_get_product_string(const hs_device *dev)
{
    assert(dev);
    return dev->product;
}

const char *hs_device_get_serial_number_string(const hs_device *dev)
{
    assert(dev);
    return dev->serial;
}

hs_monitor *hs_device_get_monitor(const hs_device *dev)
{
    assert(dev);
    return dev->monitor;
}

void hs_device_set_userdata(hs_device *dev, void *udata)
{
    assert(dev);
    dev->udata = udata;
}

void *hs_device_get_userdata(const hs_device *dev)
{
    assert(dev);
    return dev->udata;
}

int hs_device_open(hs_device *dev, hs_handle **rh)
{
    assert(dev);
    assert(rh);

    if (dev->state != HS_DEVICE_STATUS_ONLINE)
        return hs_error(HS_ERROR_NOT_FOUND, "Device '%s' is not connected", dev->path);

    return (*dev->vtable->open)(dev, rh);
}

void hs_handle_close(hs_handle *h)
{
    if (!h)
        return;

    (*h->dev->vtable->close)(h);
}

hs_device *hs_handle_get_device(const hs_handle *h)
{
    assert(h);
    return h->dev;
}

hs_descriptor hs_handle_get_descriptor(const hs_handle *h)
{
    assert(h);
    return (*h->dev->vtable->get_descriptor)(h);
}

void hs_handle_set_userdata(hs_handle *h, void *udata)
{
    assert(h);
    h->udata = udata;
}

void *hs_handle_get_userdata(const hs_handle *h)
{
    assert(h);
    return h->udata;
}
