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

#ifndef _HS_LIST_H
#define _HS_LIST_H

#include "util.h"

typedef struct _hs_list_head {
    struct _hs_list_head *prev;
    struct _hs_list_head *next;
} _hs_list_head;

#define _HS_LIST(list) \
    _hs_list_head list = {&list, &list}

static inline void _hs_list_init(_hs_list_head *list)
{
    list->prev = list;
    list->next = list;
}

static inline void _hs_list_insert_internal(_hs_list_head *prev, _hs_list_head *next, _hs_list_head *head)
{
    prev->next = head;
    head->prev = prev;

    next->prev = head;
    head->next = next;
}

static inline void _hs_list_add(_hs_list_head *list, _hs_list_head *head)
{
    _hs_list_insert_internal(list, list->next, head);
}

static inline void _hs_list_add_tail(_hs_list_head *list, _hs_list_head *head)
{
    _hs_list_insert_internal(list->prev, list, head);
}

static inline void _hs_list_remove(_hs_list_head *head)
{
    head->prev->next = head->next;
    head->next->prev = head->prev;

    head->prev = NULL;
    head->next = NULL;
}

static inline bool _hs_list_is_empty(const _hs_list_head *head)
{
    return head->next == head;
}

static inline bool _hs_list_is_singular(const _hs_list_head *head)
{
    return head->next != head && head->next == head->prev;
}

#define _hs_list_get_first(head, type, member) \
    (_hs_list_is_empty(head) ? NULL : _hs_container_of((head)->next, type, member))
#define _hs_list_get_last(head, type, member) \
    (_hs_list_is_empty(head) ? NULL : _hs_container_of((head)->prev, type, member))

static inline void _hs_list_splice_internal(_hs_list_head *prev, _hs_list_head *next, _hs_list_head *head)
{
    if (_hs_list_is_empty(head))
        return;

    head->next->prev = prev;
    prev->next = head->next;

    head->prev->next = next;
    next->prev = head->prev;

    _hs_list_init(head);
}

static inline void _hs_list_splice(_hs_list_head *list, _hs_list_head *from)
{
    _hs_list_splice_internal(list, list->next, from);
}

static inline void _hs_list_splice_tail(_hs_list_head *list, _hs_list_head *from)
{
    _hs_list_splice_internal(list->prev, list, from);
}

#define _hs_list_foreach(cur, head) \
    if ((head)->next) \
        for (_hs_list_head *cur = (head)->next, *_HS_UNIQUE_ID(next) = cur->next; cur != (head); \
             cur = _HS_UNIQUE_ID(next), _HS_UNIQUE_ID(next) = cur->next)

#endif
