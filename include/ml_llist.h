/*
 * ml_llist — Intrusive doubly-linked list node.
 *
 * Embed llist_node_t inside a struct to link it into a list managed by
 * the ml_list anchor.  Compare: include/ml_list.h for the list head.
 */
#ifndef __ML_LLIST_H__
#define __ML_LLIST_H__

#include "ml_types.h"

typedef struct llist_node_s {
    struct llist_node_s *next;
    struct llist_node_s *prev;
} llist_node_t;

#endif
