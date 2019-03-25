/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "htable.h"

int _hs_htable_init(_hs_htable *table, unsigned int size)
{
    table->heads = (void **)malloc(size * sizeof(*table->heads));
    if (!table->heads)
        return hs_error(HS_ERROR_MEMORY, NULL);
    table->size = size;

    _hs_htable_clear(table);

    return 0;
}

void _hs_htable_release(_hs_htable *table)
{
    free(table->heads);
}

_hs_htable_head *_hs_htable_get_head(_hs_htable *table, uint32_t key)
{
    return (_hs_htable_head *)&table->heads[key % table->size];
}

void _hs_htable_add(_hs_htable *table, uint32_t key, _hs_htable_head *n)
{
    _hs_htable_head *head = _hs_htable_get_head(table, key);

    n->key = key;

    n->next = head->next;
    head->next = n;
}

void _hs_htable_insert(_hs_htable_head *prev, _hs_htable_head *n)
{
    n->key = prev->key;

    n->next = prev->next;
    prev->next = n;
}

void _hs_htable_remove(_hs_htable_head *head)
{
    for (_hs_htable_head *prev = head->next; prev != head; prev = prev->next) {
        if (prev->next == head) {
            prev->next = head->next;
            head->next = NULL;

            break;
        }
    }
}

void _hs_htable_clear(_hs_htable *table)
{
    for (unsigned int i = 0; i < table->size; i++)
        table->heads[i] = (_hs_htable_head *)&table->heads[i];
}
