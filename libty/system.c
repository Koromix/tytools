/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "ty/common.h"
#include "compat.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "ty/system.h"

int ty_adjust_timeout(int timeout, uint64_t start)
{
    if (timeout < 0)
        return -1;

    uint64_t now = ty_millis();

    if (now > start + (uint64_t)timeout)
        return 0;
    return (int)(start + (uint64_t)timeout - now);
}

void ty_descriptor_set_clear(ty_descriptor_set *set)
{
    assert(set);

    set->count = 0;
}

void ty_descriptor_set_add(ty_descriptor_set *set, ty_descriptor desc, int id)
{
    assert(set);
    assert(set->count < TY_COUNTOF(set->desc));
#ifdef _WIN32
    assert(desc);
#else
    assert(desc >= 0);
#endif

    set->desc[set->count] = desc;
    set->id[set->count] = id;

    set->count++;
}

void ty_descriptor_set_remove(ty_descriptor_set *set, int id)
{
    assert(set);

    unsigned int count = 0;
    for (unsigned int i = 0; i < set->count; i++) {
        if (set->id[i] != id) {
            set->desc[count] = set->desc[i];
            set->id[count] = set->id[i];

            count++;
        }
    }

    set->count = count;
}
