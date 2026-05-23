/*
 * ml_mapkey — String and integer key-value lookup on a red-black tree.
 *
 * Thin wrapper around ml_container (rbtree) for key-value storage.
 * Supports string-keyed maps (ml_mapvalue_*) and integer-keyed maps
 * (ml_mapinteger_*).
 */
#ifndef __ML_MAPKEY_H__
#define __ML_MAPKEY_H__

#include "ml_types.h"
#include "ml_container.h"
#include "ml_config.h"

typedef struct ml_mapvalue_s {
    ml_container_node_t m_mapnd;
    char             m_value[MAX_SKEY_LENGTH];
    ml_uint_t       m_keyid;
    void            *m_data;
} ml_mapvalue_t;

/* ---- String-keyed operations ---- */
void  ml_mapvalue_setkey(ml_mapvalue_t *v, char *k);
char *ml_mapvalue_getkey(ml_mapvalue_t *v);
ml_mapvalue_t *ml_mapvalue_get(ml_container_t *map, char *k);
ml_int_t ml_mapvalue_set(ml_container_t *map, ml_mapvalue_t *v, void *param);
void  ml_mapvalue_del(ml_container_t *map, ml_mapvalue_t *v);

/* ---- Integer-keyed operations ---- */
ml_mapvalue_t *ml_mapinteger_get(ml_container_t *map, ml_uint_t k);
ml_int_t ml_mapinteger_set(ml_container_t *map, ml_mapvalue_t *v, void *param);
ml_uint_t ml_mapinteger_getkey(ml_mapvalue_t *v);
void  ml_mapinteger_setkey(ml_mapvalue_t *v, ml_uint_t k);

#endif
