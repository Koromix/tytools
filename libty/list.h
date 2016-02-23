/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_LIST_H
#define TY_LIST_H

#include "util.h"

TY_C_BEGIN

typedef struct ty_list_head {
    struct ty_list_head *prev;
    struct ty_list_head *next;
} ty_list_head;

#define TY_LIST_HEAD(head) \
    ty_list_head head = {&head, &head}

static inline void ty_list_init(ty_list_head *head)
{
    head->prev = head;
    head->next = head;
}

static inline void _ty_list_insert(ty_list_head *prev, ty_list_head *next, ty_list_head *head)
{
    prev->next = head;
    head->prev = prev;

    next->prev = head;
    head->next = next;
}

static inline void ty_list_add(ty_list_head *head, ty_list_head *n)
{
    _ty_list_insert(head, head->next, n);
}

static inline void ty_list_add_tail(ty_list_head *head, ty_list_head *n)
{
    _ty_list_insert(head->prev, head, n);
}

static inline void ty_list_remove(ty_list_head *head)
{
    head->prev->next = head->next;
    head->next->prev = head->prev;

    ty_list_init(head);
}

static inline void ty_list_replace(ty_list_head *head, ty_list_head *n)
{
    n->next = head->next;
    n->next->prev = n;
    n->prev = head->prev;
    n->prev->next = n;

    ty_list_init(head);
}

static inline bool ty_list_is_empty(const ty_list_head *head)
{
    return head->next == head;
}

static inline bool ty_list_is_singular(const ty_list_head *head)
{
    return head->next != head && head->next == head->prev;
}

#define ty_list_get_first(head, type, member) \
    (ty_list_is_empty(head) ? NULL : ty_container_of((head)->next, type, member))
#define ty_list_get_last(head, type, member) \
    (ty_list_is_empty(head) ? NULL : ty_container_of((head)->prev, type, member))

static inline void _ty_list_splice(ty_list_head *prev, ty_list_head *next, ty_list_head *head)
{
    if (ty_list_is_empty(head))
        return;

    head->next->prev = prev;
    prev->next = head->next;

    head->prev->next = next;
    next->prev = head->prev;

    ty_list_init(head);
}

static inline void ty_list_splice(ty_list_head *head, ty_list_head *list)
{
    _ty_list_splice(head, head->next, list);
}

static inline void ty_list_splice_tail(ty_list_head *head, ty_list_head *list)
{
    _ty_list_splice(head->prev, head, list);
}

#define ty_list_foreach(cur, head) \
    if ((head)->next) \
        for (ty_list_head *cur = (head)->next, *TY_UNIQUE_ID(next) = cur->next; cur != (head); \
             cur = TY_UNIQUE_ID(next), TY_UNIQUE_ID(next) = cur->next)

TY_C_END

#endif
