/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_HTABLE_H
#define TY_HTABLE_H

#include "common.h"

TY_C_BEGIN

typedef struct ty_htable_head {
    // Keep first!
    struct ty_htable_head *next;
    uint32_t key;
} ty_htable_head;

typedef struct ty_htable {
    unsigned int size;
    ty_htable_head **heads;
} ty_htable;

int ty_htable_init(ty_htable *table, unsigned int size);
void ty_htable_release(ty_htable *table);

ty_htable_head *ty_htable_get_head(ty_htable *table, uint32_t key);

void ty_htable_add(ty_htable *table, uint32_t key, ty_htable_head *head);
void ty_htable_insert(ty_htable_head *prev, ty_htable_head *head);
void ty_htable_remove(ty_htable_head *head);

void ty_htable_clear(ty_htable *table);

static inline uint32_t ty_htable_hash_str(const char *s)
{
    assert(s);

    uint32_t hash = 0;
    while (*s)
        hash = hash * 101 + (unsigned char)*s++;

    return hash;
}

static inline uint32_t ty_htable_hash_ptr(const void *p)
{
    return (uint32_t)((uintptr_t)p >> 3);
}

/* While a break will only end the inner loop, the outer loop will subsequently fail
   the cur == TY_UNIQUE_ID(head) test and thus break out of the outer loop too. */
#define ty_htable_foreach(cur, table) \
    for (ty_htable_head *TY_UNIQUE_ID(head) = (ty_htable_head *)(table)->heads, *cur = TY_UNIQUE_ID(head), *TY_UNIQUE_ID(next); \
            cur == TY_UNIQUE_ID(head) && TY_UNIQUE_ID(head) < (ty_htable_head *)((table)->heads + (table)->size); \
            TY_UNIQUE_ID(head) = (ty_htable_head *)((ty_htable_head **)TY_UNIQUE_ID(head) + 1), cur = (ty_htable_head *)((ty_htable_head **)cur + 1)) \
        for (cur = cur->next, TY_UNIQUE_ID(next) = cur->next; cur != TY_UNIQUE_ID(head); cur = TY_UNIQUE_ID(next), TY_UNIQUE_ID(next) = cur->next) \

#define ty_htable_foreach_hash(cur, table, k) \
    if ((table)->size) \
        for (ty_htable_head *TY_UNIQUE_ID(head) = ty_htable_get_head((table), (k)), *cur = TY_UNIQUE_ID(head)->next, *TY_UNIQUE_ID(next) = cur->next; \
             cur != TY_UNIQUE_ID(head); cur = TY_UNIQUE_ID(next), TY_UNIQUE_ID(next) = cur->next) \
            if (cur->key == (k))

TY_C_END

#endif
