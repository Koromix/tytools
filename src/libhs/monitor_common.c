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

#include "common_priv.h"
#include "device_priv.h"
#include "match.h"
#include "monitor_priv.h"

static int find_callback(hs_device *dev, void *udata)
{
    hs_device **rdev = udata;

    *rdev = hs_device_ref(dev);
    return 1;
}

int hs_find(const hs_match *matches, unsigned int count, hs_device **rdev)
{
    assert(rdev);
    return hs_enumerate(matches, count, find_callback, rdev);
}

void _hs_monitor_clear_devices(_hs_htable *devices)
{
    _hs_htable_foreach(cur, devices) {
        hs_device *dev = _hs_container_of(cur, hs_device, hnode);
        hs_device_unref(dev);
    }
    _hs_htable_clear(devices);
}

bool _hs_monitor_has_device(_hs_htable *devices, const char *key, uint8_t iface)
{
    _hs_htable_foreach_hash(cur, devices, _hs_htable_hash_str(key)) {
        hs_device *dev = _hs_container_of(cur, hs_device, hnode);

        if (strcmp(dev->key, key) == 0 && dev->iface_number == iface)
            return true;
    }

    return false;
}

int _hs_monitor_add(_hs_htable *devices, hs_device *dev, hs_enumerate_func *f, void *udata)
{
    if (_hs_monitor_has_device(devices, dev->key, dev->iface_number))
        return 0;

    hs_device_ref(dev);
    _hs_htable_add(devices, _hs_htable_hash_str(dev->key), &dev->hnode);

    _hs_device_log(dev, "Add");

    if (f) {
        return (*f)(dev, udata);
    } else {
        return 0;
    }
}

void _hs_monitor_remove(_hs_htable *devices, const char *key, hs_enumerate_func *f,
                        void *udata)
{
    _hs_htable_foreach_hash(cur, devices, _hs_htable_hash_str(key)) {
        hs_device *dev = _hs_container_of(cur, hs_device, hnode);

        if (strcmp(dev->key, key) == 0) {
            dev->status = HS_DEVICE_STATUS_DISCONNECTED;

            hs_log(HS_LOG_DEBUG, "Remove device '%s'", dev->key);

            if (f)
                (*f)(dev, udata);

            _hs_htable_remove(&dev->hnode);
            hs_device_unref(dev);
        }
    }
}

int _hs_monitor_list(_hs_htable *devices, hs_enumerate_func *f, void *udata)
{
    _hs_htable_foreach(cur, devices) {
        hs_device *dev = _hs_container_of(cur, hs_device, hnode);
        int r;

        r = (*f)(dev, udata);
        if (r)
            return r;
    }

    return 0;
}
