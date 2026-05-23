/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef __ML_SET_RBTREE_H__
#define __ML_SET_RBTREE_H__



typedef unsigned long  set_rbtree_key_t;
typedef unsigned long  set_uint64_t;
typedef long   set_rbtree_key_int_t;



#ifndef NULL
    #define NULL ((void*)0)
#endif


typedef struct set_rbtree_node_s  set_rbtree_node_t;

struct set_rbtree_node_s {
    set_uint64_t           color;
    set_uint64_t           key;
    set_rbtree_node_t     *left;
    set_rbtree_node_t     *right;
    set_rbtree_node_t     *parent;
};


typedef struct set_rbtree_s  set_rbtree_t;

typedef void (*set_rbtree_insert_pt) (set_rbtree_node_t *root,
    set_rbtree_node_t *node, set_rbtree_node_t *sentinel);

struct set_rbtree_s {
    set_rbtree_node_t     *root;
    set_rbtree_node_t     *sentinel;
    set_rbtree_insert_pt   insert;
};


#define set_rbtree_init(tree, s, i) \
    set_rbtree_sentinel_init(s); \
    (tree)->root = s;            \
    (tree)->sentinel = s;        \
    (tree)->insert = i


void set_rbtree_insert(set_rbtree_t *tree, set_rbtree_node_t *node);
void set_rbtree_delete(set_rbtree_t *tree, set_rbtree_node_t *node);
void set_rbtree_insert_value(set_rbtree_node_t *root, set_rbtree_node_t *node,
    set_rbtree_node_t *sentinel);
void set_rbtree_insert_timer_value(set_rbtree_node_t *root,
    set_rbtree_node_t *node, set_rbtree_node_t *sentinel);
set_rbtree_node_t *set_rbtree_next(set_rbtree_t *tree,
    set_rbtree_node_t *node);


#define set_rbt_red(node)               ((node)->color = 1)
#define set_rbt_black(node)             ((node)->color = 0)
#define set_rbt_is_red(node)            ((node)->color)
#define set_rbt_is_black(node)          (!set_rbt_is_red(node))
#define set_rbt_copy_color(n1, n2)      (n1->color = n2->color)

/* a sentinel must be black */

#define set_rbtree_sentinel_init(node)  set_rbt_black(node)


static inline set_rbtree_node_t * set_rbtree_min(set_rbtree_node_t *node, set_rbtree_node_t *sentinel)
{
    while (node->left != sentinel) {
        node = node->left;
    }

    return node;
}





#endif


