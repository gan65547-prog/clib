/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_CONNECTION_POOL_H
#define MLFW_CONNECTION_POOL_H

#include "ml_types.h"
#include "ml_thread.h"
#include "mlfw_list.h"
#include "mlfw_set.h"

/* Forward declarations */
typedef struct mlfw_connection_s  mlfw_connection_t;
typedef struct mlfw_session_s     mlfw_session_t;
typedef struct mlfw_listening_s   mlfw_listening_t;

/*
 * Connection — used by epoll event loop.
 * The first fields (m_fd, m_alive, m_error, f_rdev, f_wrev) are laid out
 * identically to ml_connection_t so casts work with the event system.
 */
struct mlfw_connection_s {
    ml_fd_t     m_fd;
    ml_int_t    m_alive;
    ml_int_t    m_error;
    void (*f_rdev)(mlfw_connection_t *);
    void (*f_wrev)(mlfw_connection_t *);

    /* Framework extensions */
    mlfw_list_t  m_node;
    ml_int_t     m_indx;
    ml_uint64_t  m_expd;
    ml_uint_t    m_close:1;
    ml_uint_t    m_first:1;
    void        *m_data;
    void        *m_parm;
    void        *m_info;
    ml_set_t    *m_set;
    void        *m_timer;
};

/* Session — one per active connection */
struct mlfw_session_s {
    mlfw_list_t       m_quend;
    ml_int_t          m_index;
    void             *m_data;
    ml_set_t         *m_skv;
    mlfw_listening_t *m_listen;
    void (*f_close)(mlfw_session_t *);
    void (*f_read)(mlfw_session_t *);
    void (*f_write)(mlfw_session_t *);
    void (*f_handler)(mlfw_session_t *);
};

/* Listening endpoint */
struct mlfw_listening_s {
    void        *m_data;
    ml_int_t     m_svrid;
    mlfw_list_t  m_queue;
    ml_set_t    *m_skv;
};

/* Connection pool (thread-safe) */
typedef struct mlfw_conn_pool_s {
    ml_container_t     *m_keymap;
    ml_thread_mutex_t  *m_mutex;
    ml_thread_rwlock_t *m_rwlock;
    mlfw_session_t     *m_session;
    mlfw_listening_t   *m_listen;
} mlfw_conn_pool_t;

/* ---- Lifecycle ---- */
ml_int_t mlfw_conn_pool_create(mlfw_conn_pool_t *co, ml_pool_t *pool, ml_int_t ns, ml_int_t nl);
void     mlfw_conn_pool_destroy(mlfw_conn_pool_t *co);

/* ---- Session operations ---- */
mlfw_session_t *mlfw_session_alloc(mlfw_conn_pool_t *co, mlfw_listening_t *ls);
void            mlfw_session_free(mlfw_conn_pool_t *co, mlfw_session_t *s);

/* ---- Map operations (thread-safe keyed lookup) ---- */
ml_int_t mlfw_session_setmap(mlfw_conn_pool_t *co, ml_set_t *kv, char *k, void *param);
void     mlfw_session_delmap(mlfw_conn_pool_t *co, ml_set_t *kv);

/* Cast helper: mlfw_connection_t* → ml_connection_t* (epoll-compatible header) */
#define mlfw_conn_to_base(c)  ((ml_connection_t *)(void *)(c))

#endif
