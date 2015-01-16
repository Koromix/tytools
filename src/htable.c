/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

    n->key = key;

    n->next = head;
    head->next = n;
}

void ty_htable_insert(ty_htable_head *prev, ty_htable_head *n)
{
    assert(prev);
    assert(n);

    n->key = prev->key;

    n->next = prev->next;
    prev->next = n;
}

void ty_htable_remove(ty_htable_head *head)
{
    assert(head);

    for (ty_htable_head *prev = head->next; prev != head; prev = prev->next) {
        if (prev->next == head) {
            prev->next = head->next;
            head->next = NULL;

            break;
        }
    }
}
