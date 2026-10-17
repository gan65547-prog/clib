/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_LIST_H
#define MLFW_LIST_H

#include "ml_types.h"

typedef struct mlfw_list_s mlfw_list_t;

struct mlfw_list_s {
    mlfw_list_t *m_prev;
    mlfw_list_t *m_next;
};

static inline void mlfw_list_init(mlfw_list_t *list)
{
    list->m_prev = list;
    list->m_next = list;
}

static inline void __mlfw_list_add(mlfw_list_t *prev, mlfw_list_t *next,
                                    mlfw_list_t *node)
{
    node->m_next = next;
    node->m_prev = prev;
    next->m_prev = node;
    prev->m_next = node;
}

static inline void mlfw_list_add_tail(mlfw_list_t *head, mlfw_list_t *node)
{
    __mlfw_list_add(head->m_prev, head, node);
}

static inline void __mlfw_list_del(mlfw_list_t *prev, mlfw_list_t *next)
{
    next->m_prev = prev;
    prev->m_next = next;
}

static inline void mlfw_list_del(mlfw_list_t *node)
{
    __mlfw_list_del(node->m_prev, node->m_next);
}

static inline void mlfw_list_del_init(mlfw_list_t *node)
{
    mlfw_list_del(node);
    mlfw_list_init(node);
}

#endif /* MLFW_LIST_H */
