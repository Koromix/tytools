/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "device_priv.h"
#include "match.h"
#include "monitor_priv.h"

static int find_callback(hs_device *dev, void *udata)
{
    hs_device **rdev = (hs_device **)udata;

    *rdev = hs_device_ref(dev);
    return 1;
}

int hs_find(const hs_match_spec *matches, unsigned int count, hs_device **rdev)
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
