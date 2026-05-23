/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include "ml_code.h"
#include "ml_timer.h"




void ml_tree_expire_timers(ml_timer_tree_t * tree, ml_msec_t msec)
{
    ml_timer_node_t   *ev;
    set_rbtree_node_t  *node, *root, *sentinel;
    ml_timer_handler_pt handler;

    sentinel = tree->m_tree.sentinel;

    for ( ;; ) {
        root = tree->m_tree.root;

        if (root == sentinel) {
            return;
        }

        node = set_rbtree_min(root, sentinel);

        if ((ml_msec_t) (node->key - msec) > 0) {
            return;
        }

        ev = (ml_timer_node_t *) ((char *) node - offsetof(ml_timer_node_t, m_timer));

        handler = ev->f_handler;
        ml_timer_tree_del(tree, ev);

        if (handler)
            handler(ev);

        if (ev->m_rep)
            ml_timer_tree_add(tree, ev, msec);

    }

}




ml_msec_t ml_tree_find_timer(ml_timer_tree_t * tree, ml_msec_t msec)
{
    ml_msec_t      timer;
    set_rbtree_node_t  *node, *root, *sentinel;

    if (tree->m_tree.root == &tree->m_sentinel) {
        return ML_ETINFN;
    }

    root = tree->m_tree.root;
    sentinel = tree->m_tree.sentinel;

    node = set_rbtree_min(root, sentinel);

    timer = (ml_msec_t) (node->key - msec);

    return (ml_msec_t) (timer > 0 ? timer : 0);
}



