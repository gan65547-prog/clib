/*
 * ml_list — Lightweight linked-list anchor.
 *
 * Stores first/last void pointers and a count.  Used as the list head for
 * intrusive linked-lists (ml_llist) and connection tracking.
 */
#ifndef __ML_LIST_H__
#define __ML_LIST_H__

#include "ml_types.h"

typedef struct ml_list_s {
    void *m_first;
    void *m_last;
    ml_uint_t m_count;
} ml_list_t;

/* Initialise a list to empty */
#define INIT_LIST_HEAD(list)  do { (list)->m_first = NULL; (list)->m_last = NULL; (list)->m_count = 0; } while(0)

#endif
