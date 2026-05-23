/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * Intrusive doubly-linked list.
 */

#ifndef MLFW_LIST_H
#define MLFW_LIST_H

typedef struct mlfw_list_s {
    struct mlfw_list_s *next, *prev;
} mlfw_list_t;

#define MLFW_LIST_HEAD_INIT(name) { &(name), &(name) }
#define MLFW_LIST_HEAD(name)      mlfw_list_t name = MLFW_LIST_HEAD_INIT(name)

#define mlfw_list_init(ptr) do { \
    (ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

static __inline__ void __mlfw_list_add(mlfw_list_t *nw, mlfw_list_t *prev, mlfw_list_t *next)
{
    next->prev = nw;
    nw->next = next;
    nw->prev = prev;
    prev->next = nw;
}

static __inline__ void mlfw_list_add(mlfw_list_t *nw, mlfw_list_t *head)
    { __mlfw_list_add(nw, head, head->next); }

static __inline__ void mlfw_list_add_tail(mlfw_list_t *nw, mlfw_list_t *head)
    { __mlfw_list_add(nw, head->prev, head); }

static __inline__ void __mlfw_list_del(mlfw_list_t *prev, mlfw_list_t *next)
    { next->prev = prev; prev->next = next; }

static __inline__ void mlfw_list_del(mlfw_list_t *entry)
    { __mlfw_list_del(entry->prev, entry->next); }

static __inline__ void mlfw_list_del_init(mlfw_list_t *entry)
    { __mlfw_list_del(entry->prev, entry->next); mlfw_list_init(entry); }

static __inline__ int mlfw_list_empty(mlfw_list_t *head)
    { return head->next == head; }

static __inline__ void mlfw_list_splice(mlfw_list_t *list, mlfw_list_t *head)
{
    mlfw_list_t *first = list->next;
    if (first != list) {
        mlfw_list_t *last = list->prev;
        mlfw_list_t *at = head->next;
        first->prev = head;
        head->next = first;
        last->next = at;
        at->prev = last;
    }
}

static __inline__ void mlfw_list_splice_tail(mlfw_list_t *list, mlfw_list_t *head)
{
    mlfw_list_t *at = head->prev;
    mlfw_list_t *first = list->next;
    mlfw_list_t *last = list->prev;
    if (first != list) {
        first->prev = at;
        at->next = first;
        last->next = head;
        head->prev = last;
    }
}

static __inline__ void mlfw_list_move(mlfw_list_t *list, mlfw_list_t *node)
    { mlfw_list_del_init(node); mlfw_list_add_tail(node, list); }

#define mlfw_list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

#define mlfw_list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define mlfw_list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#endif
