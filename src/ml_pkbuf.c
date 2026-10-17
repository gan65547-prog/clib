/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */


#include "ml_types.h"
#include "ml_code.h"
#include "ml_pool.h"
#include "ml_pkbuf.h"


ml_pkbuf_t * ml_pkbuf_alloc(ml_int_t size, ml_pool_t *pool)
{
    ml_pkbuf_t * pkb;

    if (size <= 0) return NULL;

    if (pool) {
        pkb = (ml_pkbuf_t *)pool->f_alloc(pool, sizeof(ml_pkbuf_t) + (size_t)size);
    } else {
        pkb = (ml_pkbuf_t *)ml_alloc(sizeof(ml_pkbuf_t) + (size_t)size);
    }

    if (pkb) {
        pkb->f_handler   = NULL;
        pkb->m_node.next = NULL;
        INIT_LIST_HEAD(&pkb->m_list);
        pkb->m_blen  = size;
        pkb->m_plen  = 0;
        pkb->m_ulen  = 0;
        pkb->m_proto = 0;
        pkb->m_from  = 0;
        pkb->m_to    = 0;
        pkb->m_event = 0;
        pkb->m_resrv = 0;
        pkb->m_ssid  = 0;
        pkb->m_dsid  = 0;
        pkb->m_buff  = (ml_uint8_t*)(pkb+1);
        pkb->m_pool  = pool;

        return pkb;
    }
    return NULL;
}

void ml_pkbuf_free(ml_pkbuf_t *pkb)
{
    if (!pkb) return;
    if (pkb->m_pool)
        pkb->m_pool->f_free(pkb->m_pool, pkb);
    else
        ml_free(pkb);
}
