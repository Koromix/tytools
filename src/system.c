/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
