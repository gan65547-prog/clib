/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include "ml_os.h"
#include "ml_pool.h"
#include "ml_code.h"





static ml_page_t *  ml_pool_create_page(ml_page_head_t * pool, ml_uint64_t size)
{
    void * p;
    ml_uint8_t * cur;
    ml_page_t * page;
    ml_uint64_t length;

    length = ( (size > (sizeof(ml_page_t))) && (size < pool->m_page_size) ) ? size : pool->m_page_size;

    p = ml_alloc(length);
    if (p) {
        page = (ml_page_t *)p;
        cur  = (ml_uint8_t *)p;

        page->m_first = cur + sizeof(ml_page_t);
        page->m_data  = page->m_first;
        page->m_last  = cur + length;

        page->m_next = pool->m_page_first;
        pool->m_page_first = page;

        return page;
    }

    return NULL;
}


static ml_memory_t * ml_alloc_from_page(ml_page_head_t * pool, ml_uint64_t size)
{
    ml_uint64_t length;
    ml_page_t * cur;
    ml_memory_t * m;

    length = size + sizeof(ml_memory_t);

    /* 从页内存中查找空间 */
    cur = pool->m_page_first;
    while (cur) {

        if ((cur->m_last - cur->m_data) >= length ) {
            m = (ml_memory_t *)cur->m_data;
            ml_container_node_init(&m->m_conn);

            /* 更新页内存中的data指针 */
            cur->m_data += length;

            m->m_size = length;
            return m;
        }
        cur = cur->m_next;
    }
    

    /* 创建新页内存 */
    cur  = ml_pool_create_page(pool, pool->m_page_size);
    if (cur) {
        if ((cur->m_last - cur->m_data) >= length ) {
            m = (ml_memory_t *)cur->m_data;
            ml_container_node_init(&m->m_conn);

            /* 更新页内存中的data指针 */
            cur->m_data += length;

            m->m_size = length;
            return m;
        }
    }

    return NULL;
}



static ml_memory_t * ml_alloc_from_large(ml_large_head_t * pool, ml_uint64_t size)
{
    ml_memory_t * m;
    ml_uint64_t length, key;
    ml_uint8_t * cur;
    ml_large_t * large;
    void * p;

    key    = size + sizeof(ml_memory_t);
    length = key + sizeof(ml_large_t);

    p = ml_alloc(length);
    if (p) {
        cur = (ml_uint8_t *)p;
        large = (ml_large_t*)p;
        large->m_next = pool->m_first;
        pool->m_first = large;

        m = (ml_memory_t *)(cur + sizeof(ml_large_t));

        m->m_size = key;
        return m;
    }

    return NULL;
}



static int ml_pool_compare_callback(ml_container_node_t * data, ml_container_node_t * src)
{
    ml_memory_t * pd, * ps;

    pd = ml_container_entry(data, ml_memory_t, m_conn);
    ps = ml_container_entry(src, ml_memory_t, m_conn);

    return (ps->m_size - pd->m_size);
}


static int ml_pool_lookup_callback(ml_container_node_t * data)
{
    ml_memory_t * pd;

    pd = ml_container_entry(data, ml_memory_t, m_conn);

    return (MEMORY_STATUS_UNUSED == GET_MEMORY_USED(pd));
}



static ml_memory_t * ml_alloc_from_container(ml_pool_t * pool, ml_uint64_t size)
{
    ml_container_node_t * cn;
    ml_memory_t  src, *m;

    /* 实际申请的内存大小 */
    src.m_size = size + sizeof(ml_memory_t);

    cn = pool->m_pool_set.f_lookup(&pool->m_pool_set, ml_pool_compare_callback, &src.m_conn, ml_pool_lookup_callback);
    if (cn) {
        m = ml_container_entry(cn, ml_memory_t, m_conn);

        return m;
    }

    return NULL;
}




static void * ml_pool_alloc(ml_pool_t * pool, ml_uint64_t size)
{
    ml_memory_t * m;
    ml_uint64_t length;
    void * ptr = NULL;


    /* 申请的内存对齐 */
    size   = ML_POOL_ALIGNMENT(size, ML_POOL_ALIGNLENGTH);
    /* 实际申请的内存大小 */
    length = size + sizeof(ml_memory_t);

    m = ml_alloc_from_container(pool, size);
    if (m) {
        /* 设置set容器中的内存状态为已使用 */
        SET_MEMORY_USED(m, MEMORY_STATUS_USED);
        ptr = (ml_uint8_t*)m + sizeof(ml_memory_t);
    }
    else {
        /* 从页内存申请 */
        if (length < pool->m_page.m_page_left) {
            m = ml_alloc_from_page(&pool->m_page, size);
        }
        /* 从大内存申请 */
        else {
            m = ml_alloc_from_large(&pool->m_large, size);
        }

        /* 设置内存状态为,不在容器中 */
        if (m) {
            SET_MEMORY_SETD(m, MEMORY_STATUS_SETOUT);
            SET_MEMORY_USED(m, MEMORY_STATUS_UNUSED);
            ptr = (ml_uint8_t*)m + sizeof(ml_memory_t);
        }
    }

    return ptr;
}


static void ml_pool_free(ml_pool_t * pool, void * p)
{
    ml_memory_t * m;

    if (!p) return;

    m = (ml_memory_t *)((ml_uint8_t*)p - sizeof(ml_memory_t));

    /* 设置容器中的内存状态为未使用 */
    SET_MEMORY_USED(m, MEMORY_STATUS_UNUSED);

    /* 放入容器 */
    if (MEMORY_STATUS_SETOUT == GET_MEMORY_SETD(m)) {
        SET_MEMORY_SETD(m, MEMORY_STATUS_SETIN);
        pool->m_pool_set.f_insert(&pool->m_pool_set, ml_pool_compare_callback, &m->m_conn);
    }
}


static void ml_pool_page_destroy(ml_pool_t * pool)
{
    ml_page_t * p, * n, * h;

    h = pool->m_page.m_page_first;
    
    for (p = h; p; p = n) {
        n = p->m_next;
        ml_free(p);
    }

    pool->m_page.m_page_first = NULL;
}


static void ml_pool_large_destroy(ml_pool_t * pool)
{
    ml_large_t * p, * n, * h;

    h = pool->m_large.m_first;
    for (p = h; p; p = n) {
        n = p->m_next;
        ml_free(p);
    }
}




int  ml_pool_create(ml_pool_t * pool, ml_uint64_t size)
{
    ml_set_init(&pool->m_pool_set);

    pool->m_page.m_page_size     = (size > sizeof(ml_page_t)) ? size : ml_pagesize();
    pool->m_page.m_page_left     = pool->m_page.m_page_size - sizeof(ml_page_t);
    pool->m_page.m_page_first    = NULL;

    pool->m_large.m_first        = NULL;

    pool->f_alloc = ml_pool_alloc;
    pool->f_free  = ml_pool_free;

    /* 创建一个新页 */
    if (ml_pool_create_page(&pool->m_page, size)) {
        return ML_OK;
    }

    return ML_ENOMEM;
}


void ml_pool_destroy(ml_pool_t * pool)
{
    ml_pool_page_destroy(pool);
    ml_pool_large_destroy(pool);

    ml_set_init(&pool->m_pool_set);

    pool->m_page.m_page_size     = ml_pagesize();
    pool->m_page.m_page_left     = pool->m_page.m_page_size - sizeof(ml_page_t);
    pool->m_page.m_page_first    = NULL;

    pool->m_large.m_first        = NULL;

    pool->f_alloc = ml_pool_alloc;
    pool->f_free  = ml_pool_free;
}


