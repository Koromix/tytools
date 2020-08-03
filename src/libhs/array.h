/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef HS_ARRAY_H
#define HS_ARRAY_H

#include "common.h"

_HS_BEGIN_C

struct _hs_array {
    void *values;
    size_t allocated;
    size_t count;
};

#define _HS_ARRAY(Type) \
    struct { \
        Type *values; \
        size_t allocated; \
        size_t count; \
    }

#define _hs_array_release(Array) \
    _hs_array_release_((struct _hs_array *)&(Array)->values)

#define _hs_array_grow(Array, Need) \
    _hs_array_grow_((struct _hs_array *)&(Array)->values, sizeof(*(Array)->values), (Need))
#define _hs_array_push(Array, Value) \
    (_hs_array_grow((Array), 1) < 0 \
        ? HS_ERROR_MEMORY \
        : (((Array)->values[(Array)->count++] = (Value)), 0))

#define _hs_array_shrink(Array) \
    _hs_array_shrink_((struct _hs_array *)&(Array)->values, sizeof(*(Array)->values))
#define _hs_array_pop(Array, Count) \
    do { \
        (Array)->count -= (Count); \
        if ((Array)->count <= (Array)->allocated / 2) \
            _hs_array_shrink(Array); \
    } while (0)
#define _hs_array_remove(Array, Offset, Count) \
    do { \
        size_t _HS_UNIQUE_ID(start) = (Offset); \
        size_t _HS_UNIQUE_ID(count) = (Count); \
        size_t _HS_UNIQUE_ID(end) = _HS_UNIQUE_ID(start) + _HS_UNIQUE_ID(count); \
        memmove((Array)->values + _HS_UNIQUE_ID(start), \
                (Array)->values + _HS_UNIQUE_ID(end), \
                ((Array)->count - _HS_UNIQUE_ID(end)) * sizeof(*(Array)->values)); \
        _hs_array_pop((Array), _HS_UNIQUE_ID(count)); \
    } while (0)

#define _hs_array_move(Src, Dest) \
    do { \
        (Dest)->values = (Src)->values; \
        (Dest)->count = (Src)->count; \
        (Dest)->allocated = (Src)->allocated; \
        memset((Src), 0, sizeof(*(Src))); \
    } while (0)

void _hs_array_release_(struct _hs_array *array);

int _hs_array_expand_(struct _hs_array *array, size_t value_size, size_t need);
static inline int _hs_array_grow_(struct _hs_array *array, size_t value_size, size_t need)
{
    if (need > array->allocated - array->count) {
        return _hs_array_expand_(array, value_size, need);
    } else {
        return 0;
    }
}

void _hs_array_shrink_(struct _hs_array *array, size_t value_size);

_HS_END_C

#endif
