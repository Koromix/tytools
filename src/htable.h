/**
 * ty, command-line program to manage Teensy devices
 * http://github.com/Koromix/ty
 * Copyright (C) 2014 Niels Martignène
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TY_HTABLE_H
#define TY_HTABLE_H

#include "ty/common.h"

TY_C_BEGIN

typedef struct ty_htable_head {
    // Keep first!
    struct ty_htable_head *next;
    uint32_t key;
} ty_htable_head;

typedef struct ty_htable {
    size_t size;
    ty_htable_head **heads;
} ty_htable;

int ty_htable_init(ty_htable *table, size_t size);
void ty_htable_release(ty_htable *table);

ty_htable_head *ty_htable_get_head(ty_htable *table, uint32_t key);

void ty_htable_add(ty_htable *table, uint32_t key, ty_htable_head *head);
void ty_htable_insert(ty_htable_head *prev, ty_htable_head *head);

void ty_htable_remove(ty_htable_head *head);

static inline uint32_t ty_htable_hash_str(const char *s)
{
    assert(s);

    uint32_t hash = 0;
    while (*s)
        hash = hash * 101 + (unsigned char)*s++;

    return hash;
}

#define ty_htable_entry(head, type, member) \
    ((type *)((char *)(head) - (size_t)(&((type *)0)->member)))

#define ty_htable_foreach(cur, table) \
    for (ty_htable_head **TY_UNIQUE_ID(head) = (table)->heads; TY_UNIQUE_ID(head) < (table)->heads + (table)->size; TY_UNIQUE_ID(head)++) \
        for (ty_htable_head *cur = *TY_UNIQUE_ID(head), *TY_UNIQUE_ID(next) = cur->next; cur != (ty_htable_head *)TY_UNIQUE_ID(head); \
             cur = TY_UNIQUE_ID(next), TY_UNIQUE_ID(next) = cur->next)

#define ty_htable_foreach_hash(cur, table, k) \
    if ((table)->size) \
        for (ty_htable_head *TY_UNIQUE_ID(head) = ty_htable_get_head((table), (k)), *cur = TY_UNIQUE_ID(head)->next, *TY_UNIQUE_ID(next) = cur->next; \
             cur != TY_UNIQUE_ID(head); cur = TY_UNIQUE_ID(next), TY_UNIQUE_ID(next) = cur->next) \
            if (cur->key == (k))

TY_C_END

#endif
