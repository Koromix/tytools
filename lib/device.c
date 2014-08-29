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
#include <unistd.h>
#include "device.h"

ty_device *ty_device_ref(ty_device *dev)
{
    assert(dev);

    dev->refcount++;
    return dev;
}

void ty_device_unref(ty_device *dev)
{
    if (dev && !--dev->refcount) {
        free(dev->node);
        free(dev->path);
        free(dev->serial);

#ifdef _WIN32
        free(dev->key);
        free(dev->id);
#endif

        free(dev);
    }
}

int ty_device_dup(ty_device *dev, ty_device **rdev)
{
    ty_device *copy;
    int r;

#define STRDUP(s,dest) \
        do { \
            if (s) { \
                dest = strdup(s); \
                if (!dest) { \
                    r = ty_error(TY_ERROR_MEMORY, NULL); \
                    goto error; \
                } \
            } \
        } while (false)

    copy = calloc(1, sizeof(*copy));
    if (!copy)
        return ty_error(TY_ERROR_MEMORY, NULL);
    copy->refcount = 1;

    copy->type = dev->type;
    STRDUP(dev->node, copy->node);
    STRDUP(dev->path, copy->path);
    copy->iface = dev->iface;
    copy->vid = dev->vid;
    copy->pid = dev->pid;
    STRDUP(dev->serial, copy->serial);

#ifdef _WIN32
    STRDUP(dev->key, copy->key);
    STRDUP(dev->id, copy->id);
#endif

#undef STRDUP

    *rdev = copy;
    return 0;

error:
    ty_device_unref(copy);
    return r;
}
