/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "array.h"

void _hs_array_release_(struct _hs_array *array)
{
    assert(array);

    free(array->values);

    array->values = NULL;
    array->allocated = 0;
    array->count = 0;
}

int _hs_array_expand_(struct _hs_array *array, size_t value_size, size_t need)
{
    assert(array);
    assert(array->count <= SIZE_MAX - need);

    if (need > array->allocated - array->count) {
        size_t new_size;
        void *new_values;

        new_size = 4;
        while (new_size < array->count)
            new_size += new_size / 2;
        while (need > new_size - array->count)
            new_size += new_size / 2;
        new_values = realloc(array->values, new_size * value_size);
        if (!new_values)
            return hs_error(HS_ERROR_MEMORY, NULL);
        memset((uint8_t *)new_values + (array->allocated * value_size), 0,
               (new_size - array->allocated) * value_size);

        array->values = new_values;
        array->allocated = new_size;
    }

    return 0;
}

void _hs_array_shrink_(struct _hs_array *array, size_t value_size)
{
    assert(array);

    if (array->count) {
        void *new_items = realloc(array->values, array->count * value_size);
        if (!new_items)
            return;

        array->values = new_items;
        array->allocated = array->count;
    } else {
        free(array->values);

        array->values = NULL;
        array->allocated = 0;
    }
}
