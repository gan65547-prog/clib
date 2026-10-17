/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */


#include "ml_cints.h"
#include "ml_code.h"
#include "ml_string.h"
#include "ml_mapkey.h"




static ml_int_t ml_mapvalue_compare(ml_container_node_t * data, ml_container_node_t * src)
{
    ml_mapvalue_t  * pdata, * psrc;

    pdata = ml_container_entry(data, ml_mapvalue_t, m_mapnd);
    psrc  = ml_container_entry(src, ml_mapvalue_t, m_mapnd);

    return ml_strncmp(psrc->m_value, pdata->m_value, sizeof(psrc->m_value));
}





static ml_int_t ml_mapinteger_compare(ml_container_node_t * data, ml_container_node_t * src)
{
    ml_mapvalue_t  * pdata, * psrc;

    pdata = ml_container_entry(data, ml_mapvalue_t, m_mapnd);
    psrc  = ml_container_entry(src, ml_mapvalue_t, m_mapnd);

    if (psrc->m_keyid < pdata->m_keyid) return -1;
    if (psrc->m_keyid > pdata->m_keyid) return  1;
    return 0;
}





void ml_mapvalue_setkey(ml_mapvalue_t * v, char * k)
{
    ml_memzero(v->m_value, sizeof(v->m_value));
    ml_strncpy(v->m_value, k, sizeof(v->m_value)-1);
}





char * ml_mapvalue_getkey(ml_mapvalue_t * v)
{
    return v->m_value;
}




ml_mapvalue_t * ml_mapvalue_get(ml_container_t * map, char * k)
{
    ml_mapvalue_t  lookup, *kv;
    ml_container_node_t * snd;

    ml_memzero(lookup.m_value, sizeof(lookup.m_value));
    ml_strncpy(lookup.m_value, k, sizeof(lookup.m_value)-1);


    snd = map->f_lookup(map, ml_mapvalue_compare, &lookup.m_mapnd, NULL);
    if (snd) {
        kv = ml_container_entry(snd, ml_mapvalue_t, m_mapnd);
        return kv;
    }

    return NULL;
}





ml_int_t  ml_mapvalue_set(ml_container_t * map, ml_mapvalue_t * v, void * param)
{
    ml_int_t ret;

    v->m_data = param;
    if (RB_EMPTY_NODE(&v->m_mapnd.m_node))
        ml_container_node_init(&v->m_mapnd);

    ret = map->f_insert(map, ml_mapvalue_compare, &v->m_mapnd);

    return (ret == 0) ? ML_OK : ML_ERROR;
}




void ml_mapvalue_del(ml_container_t * map, ml_mapvalue_t * v)
{
    map->f_erase(map, &v->m_mapnd);
}




ml_mapvalue_t * ml_mapinteger_get(ml_container_t * map, ml_uint_t k)
{
    ml_mapvalue_t  lookup, *kv;
    ml_container_node_t * snd;

    lookup.m_keyid = k;

    snd = map->f_lookup(map, ml_mapinteger_compare, &lookup.m_mapnd, NULL);
    if (snd) {
        kv = ml_container_entry(snd, ml_mapvalue_t, m_mapnd);
        return kv;
    }

    return NULL;
}




ml_int_t  ml_mapinteger_set(ml_container_t * map, ml_mapvalue_t * v, void * param)
{
    ml_int_t ret;

    v->m_data = param;
    if (RB_EMPTY_NODE(&v->m_mapnd.m_node))
        ml_container_node_init(&v->m_mapnd);

    ret = map->f_insert(map, ml_mapinteger_compare, &v->m_mapnd);

    return (ret == 0) ? ML_OK : ML_ERROR;
}



ml_uint_t ml_mapinteger_getkey(ml_mapvalue_t * v)
{
    return v->m_keyid;
}



void ml_mapinteger_setkey(ml_mapvalue_t * v, ml_uint_t k)
{
    v->m_keyid = k;
}



