/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef HS_HTABLE_H
#define HS_HTABLE_H

#include "common.h"

HS_BEGIN_C

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

HS_END_C

#endif
