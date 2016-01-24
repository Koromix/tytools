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
#include "htable.h"

int _hs_htable_init(_hs_htable *table, unsigned int size)
{
    table->heads = malloc(size * sizeof(*table->heads));
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
