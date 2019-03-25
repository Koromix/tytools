/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "system.h"

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
