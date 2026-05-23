/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_SET_H
#define MLFW_SET_H

#include "ml_types.h"
#include "ml_config.h"
#include "ml_container.h"

#define ML_SET_NUMBER  1
#define ML_SET_STRING  2

typedef struct ml_set_s {
    ml_uint_t           m_into;
    ml_uint_t           m_number;
    void               *m_data;
    char                m_string[ML_MAX_SKEY_LENGTH];
    ml_container_node_t m_node;
} ml_set_t;

typedef void (*ml_set_clean_pt)(ml_set_t *);

#define mlfw_set_node_init(s) do { \
    (s)->m_into = 0;             \
    (s)->m_number = 0;           \
    (s)->m_string[0] = 0;        \
    ml_container_node_init(&((s)->m_node)); \
} while(0)

#define mlfw_set_tree_init(t)  ml_map_init(t)

ml_int_t  mlfw_set_insert(ml_container_t *tree, ml_int_t type, ml_set_t *s);
ml_set_t *mlfw_set_lookup(ml_container_t *tree, ml_int_t type, void *value, ml_int_t len);
void      mlfw_set_remove(ml_container_t *tree, ml_set_t *s);
void      mlfw_set_cleanup(ml_container_t *tree, ml_set_clean_pt cb);

#endif
