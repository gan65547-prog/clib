/*
 * ml_timer — Timer wheel backed by a red-black tree.
 *
 * Provides millisecond-precision timers with one-shot and repeating modes.
 * Uses set_rbtree for O(log n) insert/delete and O(1) expire-next lookup.
 */
#ifndef __ML_TIMER_H__
#define __ML_TIMER_H__

#include "ml_types.h"
#include "ml_set_rbtree.h"
#include "ml_pool.h"

typedef ml_int64_t                         ml_msec_t;
typedef struct ml_timer_node_s             ml_timer_node_t;
typedef struct ml_timer_tree_s             ml_timer_tree_t;
typedef void (*ml_timer_handler_pt)(ml_timer_node_t *);

/* Timer node (millisecond precision) */
struct ml_timer_node_s {
    /* repeat timer if set */
    ml_uint_t              m_rep:1;
    ml_uint_t              m_in:1;

    ml_uint_t              m_out;
    ml_uint_t              m_data;
    ml_uint_t              m_info;
    ml_uint_t              m_indx;
    ml_uint_t              m_temp;

    void  *                 m_param;
    void  *                 m_poit;

    set_rbtree_node_t       m_timer;
    ml_timer_handler_pt    f_handler;
};

struct ml_timer_tree_s {
    set_rbtree_t            m_tree;
    set_rbtree_node_t       m_sentinel;
};

/* Zero-initialise a timer node */
#define ml_timer_node_init(node)                                                   \
        ml_memzero(node, sizeof(ml_timer_node_t))

/* Initialise the timer tree */
#define ml_timer_tree_init(tree)                                                   \
        set_rbtree_init(&((tree)->m_tree), &((tree)->m_sentinel),                   \
        set_rbtree_insert_timer_value)

/* Returns non-zero if the node is currently in the timer tree */
#define ml_timer_node_int(node)                                                    ((node)->m_in)

/* Insert a timer node, set to expire at (now + msec). Re-inserts if already present. */
#define ml_timer_tree_add(tree, node, msec)                                        \
    do {                                                                            \
        if ((node)->m_in) {                                                         \
            set_rbtree_delete(&((tree)->m_tree), &((node)->m_timer));               \
        }                                                                           \
        ((node)->m_timer.key)  = (((node)->m_out) + (msec));                        \
        set_rbtree_insert(&((tree)->m_tree), &((node)->m_timer));                   \
        (node)->m_in = 1;                                                           \
    } while(0)

/* Remove a timer node from the tree */
#define ml_timer_tree_del(tree, node)                                              \
    do {                                                                            \
        if ((node)->m_in) {                                                         \
            set_rbtree_delete(&((tree)->m_tree), &((node)->m_timer));               \
            (node)->m_in = 0;                                                       \
            (node)->m_timer.left   = NULL;                                          \
            (node)->m_timer.right  = NULL;                                          \
            (node)->m_timer.parent = NULL;                                          \
        }                                                                           \
    } while(0)

/* Expire and fire all timers whose deadline has passed (requires current time). */
void ml_tree_expire_timers(ml_timer_tree_t * tree, ml_msec_t msec);

/* Get the time until the next timer expires (ms). Returns 0 or -1 if none. */
ml_msec_t ml_tree_find_timer(ml_timer_tree_t * tree, ml_msec_t msec);

#endif
