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
#include "device_priv.h"
#include "monitor_priv.h"

struct hs_monitor {
    _HS_MONITOR
};

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

int _hs_monitor_init(hs_monitor *monitor, const hs_match *matches, unsigned int count)
{
    int r;

    r = _hs_filter_init(&monitor->filter, matches, count);
    if (r < 0)
        return r;

    r = _hs_htable_init(&monitor->devices, 64);
    if (r < 0)
        return r;

    return 0;
}

void _hs_monitor_release(hs_monitor *monitor)
{
    _hs_monitor_clear(monitor);
    _hs_htable_release(&monitor->devices);

    _hs_filter_release(&monitor->filter);
}

void _hs_monitor_clear(hs_monitor *monitor)
{
    _hs_htable_foreach(cur, &monitor->devices) {
        hs_device *dev = _hs_container_of(cur, hs_device, hnode);
        hs_device_unref(dev);
    }
    _hs_htable_clear(&monitor->devices);
}

int _hs_monitor_add(hs_monitor *monitor, hs_device *dev, hs_enumerate_func *f, void *udata)
{
    if (!_hs_filter_match_device(&monitor->filter, dev))
        return 0;

    _hs_htable_foreach_hash(cur, &monitor->devices, _hs_htable_hash_str(dev->key)) {
        hs_device *dev2 = _hs_container_of(cur, hs_device, hnode);

        if (strcmp(dev2->key, dev->key) == 0 && dev2->iface == dev->iface)
            return 0;
    }


    hs_device_ref(dev);
    _hs_htable_add(&monitor->devices, _hs_htable_hash_str(dev->key), &dev->hnode);

    return f ? (*f)(dev, udata) : 0;
}

void _hs_monitor_remove(hs_monitor *monitor, const char *key, hs_enumerate_func *f,
                        void *udata)
{
    _hs_htable_foreach_hash(cur, &monitor->devices, _hs_htable_hash_str(key)) {
        hs_device *dev = _hs_container_of(cur, hs_device, hnode);

        if (strcmp(dev->key, key) == 0) {
            dev->state = HS_DEVICE_STATUS_DISCONNECTED;

            if (f)
                (*f)(dev, udata);

            _hs_htable_remove(&dev->hnode);
            hs_device_unref(dev);
        }
    }
}

int hs_monitor_list(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    assert(monitor);
    assert(f);

    _hs_htable_foreach(cur, &monitor->devices) {
        hs_device *dev = _hs_container_of(cur, hs_device, hnode);
        int r;

        r = (*f)(dev, udata);
        if (r)
            return r;
    }

    return 0;
}
