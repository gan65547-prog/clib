/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_SET_H
#define MLFW_SET_H

#include "ml_types.h"
#include "ml_code.h"
#include "ml_container.h"

#define ML_SET_STRING  1
#define ML_SET_NUMBER  2

#define ML_SET_KEY_LEN  64

typedef struct ml_set_s {
    ml_container_node_t  m_node;
    ml_int_t             m_into;
    ml_uint_t            m_number;
    char                 m_string[ML_SET_KEY_LEN];
    void                *m_data;
} ml_set_t;

typedef void (*ml_set_clean_pt)(ml_set_t *set);

ml_int_t mlfw_set_insert(ml_container_t *tree, ml_int_t type, ml_set_t *s);
ml_set_t *mlfw_set_lookup(ml_container_t *tree, ml_int_t type, void *value, ml_int_t len);
void mlfw_set_remove(ml_container_t *tree, ml_set_t *s);
void mlfw_set_cleanup(ml_container_t *tree, ml_set_clean_pt cb);

/* Inline helpers called from framework source */
static inline void mlfw_set_tree_init(ml_container_t *tree)
{
    if (tree) ml_memzero(tree, sizeof(ml_container_t));
}

static inline void mlfw_set_node_init(ml_set_t *s)
{
    if (s) { s->m_into = 0; ml_container_node_init(&s->m_node); }
}

#endif /* MLFW_SET_H */
