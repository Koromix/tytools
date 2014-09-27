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

#include "ty/common.h"
#include "compat.h"
#include "htable.h"

int ty_htable_init(ty_htable *table, size_t size)
{
    assert(table);
    assert(size);

    table->heads = malloc(size * sizeof(*table->heads));
    if (!table->heads)
        return ty_error(TY_ERROR_MEMORY, NULL);
    table->size = size;

    for (size_t i = 0; i < table->size; i++)
        table->heads[i] = (ty_htable_head *)&table->heads[i];

    return 0;
}

void ty_htable_release(ty_htable *table)
{
    assert(table);

    free(table->heads);
}

ty_htable_head *ty_htable_get_head(ty_htable *table, uint32_t key)
{
    assert(table);

    return (ty_htable_head *)&table->heads[key % table->size];
}

void ty_htable_add(ty_htable *table, uint32_t key, ty_htable_head *n)
{
    assert(table);
    assert(n);

    ty_htable_head *head = ty_htable_get_head(table, key);

    n->next = head->next;
    n->key = key;

    head->next = n;
}

void ty_htable_remove(ty_htable_head *head)
{
    assert(head);

    if (head == head->next || !head->next)
        return;

    ty_htable_head *prev = head->next;
    while (prev->next != head)
        prev = prev->next;

    prev->next = head->next;

    head->next = NULL;
}

uint32_t ty_htable_hash_str(const char *s)
{
    assert(s);

    uint32_t hash = 0;
    while (*s)
        hash = hash * 101 + (unsigned char)*s++;

    return hash;
}
