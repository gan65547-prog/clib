/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_CONNECTION_POOL_H
#define MLFW_CONNECTION_POOL_H

#include "ml_types.h"
#include "ml_code.h"
#include "ml_pool.h"
#include "ml_thread.h"
#include "ml_event.h"
#include "ml_container.h"
#include "mlfw_list.h"

/* ── Set key-value types (formerly mlfw_set.h, now local — clib ml_container_t is the backing store) ── */
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

static inline void mlfw_set_tree_init(ml_container_t *tree)
{
    if (tree) ml_memzero(tree, sizeof(ml_container_t));
}

static inline void mlfw_set_node_init(ml_set_t *s)
{
    if (s) { s->m_into = 0; ml_container_node_init(&s->m_node); }
}

/* Forward declarations */
typedef struct mlfw_connection_s   mlfw_connection_t;
typedef struct mlfw_listening_s    mlfw_listening_t;
typedef struct mlfw_session_s      mlfw_session_t;
typedef struct mlfw_conn_pool_s    mlfw_conn_pool_t;

/* Connection base — embed in mlfw_connection_t */
typedef struct {
    ml_int_t      m_fd;
    void        (*f_rdev)(mlfw_connection_t *c);
    ml_int_t      m_proto;
} mlfw_conn_base_t;

struct mlfw_listening_s {
    ml_int_t        m_fd;
    ml_int_t        m_svrid;
    ml_set_t       *m_skv;
    void           *m_data;
    mlfw_list_t     m_queue;
    void          (*f_handler)(mlfw_connection_t *c);
};

struct mlfw_session_s {
    ml_int_t               m_index;
    ml_int_t               m_fd;
    ml_set_t              *m_skv;
    void                  *m_data;
    mlfw_listening_t      *m_listen;
    void                 (*f_close)(mlfw_session_t *s);
    void                 (*f_handler)(mlfw_session_t *s);
    void                 (*f_read)(mlfw_session_t *s);
    void                 (*f_write)(mlfw_session_t *s);
    mlfw_list_t            m_quend;
};

struct mlfw_conn_pool_s {
    ml_container_t       *m_keymap;
    ml_thread_mutex_t    *m_mutex;
    ml_thread_rwlock_t   *m_rwlock;
    mlfw_session_t       *m_session;
    mlfw_listening_t     *m_listen;
};

struct mlfw_connection_s {
    mlfw_conn_base_t   m_base;
    ml_int_t           m_fd;
    ml_int_t           m_indx;
    ml_set_t          *m_set;
    void              *m_data;
    void              *m_info;
    void             (*f_rdev)(mlfw_connection_t *c);
};

static inline mlfw_conn_base_t *mlfw_conn_to_base(mlfw_connection_t *c)
{
    return &c->m_base;
}

/* API */
ml_int_t mlfw_conn_pool_create(mlfw_conn_pool_t *co, ml_pool_t *pool,
                                ml_int_t ns, ml_int_t nl);
void mlfw_conn_pool_destroy(mlfw_conn_pool_t *co);
mlfw_session_t *mlfw_session_alloc(mlfw_conn_pool_t *co, mlfw_listening_t *ls);
void mlfw_session_free(mlfw_conn_pool_t *co, mlfw_session_t *s);
ml_int_t mlfw_session_setmap(mlfw_conn_pool_t *co, ml_set_t *kv,
                              char *k, void *param);
void mlfw_session_delmap(mlfw_conn_pool_t *co, ml_set_t *kv);

#endif /* MLFW_CONNECTION_POOL_H */
