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

#ifndef _HS_HTABLE_H
#define _HS_HTABLE_H

#include "util.h"

typedef struct _hs_htable_head {
    // Keep first!
    struct _hs_htable_head *next;
    uint32_t key;
} _hs_htable_head;

typedef struct _hs_htable {
    unsigned int size;
    void **heads;
} _hs_htable;

int _hs_htable_init(_hs_htable *table, unsigned int size);
void _hs_htable_release(_hs_htable *table);

_hs_htable_head *_hs_htable_get_head(_hs_htable *table, uint32_t key);

void _hs_htable_add(_hs_htable *table, uint32_t key, _hs_htable_head *head);
void _hs_htable_insert(_hs_htable_head *prev, _hs_htable_head *head);
void _hs_htable_remove(_hs_htable_head *head);

void _hs_htable_clear(_hs_htable *table);

static inline uint32_t _hs_htable_hash_str(const char *s)
{
    assert(s);

    uint32_t hash = 0;
    while (*s)
        hash = hash * 101 + (unsigned char)*s++;

    return hash;
}

static inline uint32_t _hs_htable_hash_ptr(const void *p)
{
    return (uint32_t)((uintptr_t)p >> 3);
}

/* While a break will only end the inner loop, the outer loop will subsequently fail
   the cur == HS_UNIQUE_ID(head) test and thus break out of the outer loop too. */
#define _hs_htable_foreach(cur, table) \
    for (_hs_htable_head *_HS_UNIQUE_ID(head) = (_hs_htable_head *)(table)->heads, *cur = _HS_UNIQUE_ID(head), *_HS_UNIQUE_ID(next); \
            cur == _HS_UNIQUE_ID(head) && _HS_UNIQUE_ID(head) < (_hs_htable_head *)((table)->heads + (table)->size); \
            _HS_UNIQUE_ID(head) = (_hs_htable_head *)((_hs_htable_head **)_HS_UNIQUE_ID(head) + 1), cur = (_hs_htable_head *)((_hs_htable_head **)cur + 1)) \
        for (cur = cur->next, _HS_UNIQUE_ID(next) = cur->next; cur != _HS_UNIQUE_ID(head); cur = _HS_UNIQUE_ID(next), _HS_UNIQUE_ID(next) = cur->next) \

#define _hs_htable_foreach_hash(cur, table, k) \
    if ((table)->size) \
        for (_hs_htable_head *_HS_UNIQUE_ID(head) = _hs_htable_get_head((table), (k)), *cur = _HS_UNIQUE_ID(head)->next, *_HS_UNIQUE_ID(next) = cur->next; \
             cur != _HS_UNIQUE_ID(head); cur = _HS_UNIQUE_ID(next), _HS_UNIQUE_ID(next) = cur->next) \
            if (cur->key == (k))

#endif
