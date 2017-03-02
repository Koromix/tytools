/* libhs - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/libraries

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef HS_LIST_H
#define HS_LIST_H

#include "common.h"

HS_BEGIN_C

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

HS_END_C

#endif
