/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include "ml_set_rbtree.h"


/*
 * The red-black tree code is based on the algorithm described in
 * the "Introduction to Algorithms" by Cormen, Leiserson and Rivest.
 */

static inline void set_rbtree_left_rotate(set_rbtree_node_t **root,
    set_rbtree_node_t *sentinel, set_rbtree_node_t *node);
static inline void set_rbtree_right_rotate(set_rbtree_node_t **root,
    set_rbtree_node_t *sentinel, set_rbtree_node_t *node);


void
set_rbtree_insert(set_rbtree_t *tree, set_rbtree_node_t *node)
{
    set_rbtree_node_t  **root, *temp, *sentinel;

    /* a binary tree insert */

    root = &tree->root;
    sentinel = tree->sentinel;

    if (*root == sentinel) {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        set_rbt_black(node);
        *root = node;

        return;
    }

    tree->insert(*root, node, sentinel);

    /* re-balance tree */

    while (node != *root && set_rbt_is_red(node->parent)) {

        if (node->parent == node->parent->parent->left) {
            temp = node->parent->parent->right;

            if (set_rbt_is_red(temp)) {
                set_rbt_black(node->parent);
                set_rbt_black(temp);
                set_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    set_rbtree_left_rotate(root, sentinel, node);
                }

                set_rbt_black(node->parent);
                set_rbt_red(node->parent->parent);
                set_rbtree_right_rotate(root, sentinel, node->parent->parent);
            }

        } else {
            temp = node->parent->parent->left;

            if (set_rbt_is_red(temp)) {
                set_rbt_black(node->parent);
                set_rbt_black(temp);
                set_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    set_rbtree_right_rotate(root, sentinel, node);
                }

                set_rbt_black(node->parent);
                set_rbt_red(node->parent->parent);
                set_rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }

    set_rbt_black(*root);
}


void
set_rbtree_insert_value(set_rbtree_node_t *temp, set_rbtree_node_t *node,
    set_rbtree_node_t *sentinel)
{
    set_rbtree_node_t  **p;

    for ( ;; ) {

        p = (node->key < temp->key) ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    set_rbt_red(node);
}


void
set_rbtree_insert_timer_value(set_rbtree_node_t *temp, set_rbtree_node_t *node,
    set_rbtree_node_t *sentinel)
{
    set_rbtree_node_t  **p;

    for ( ;; ) {

        /*
         * Timer values
         * 1) are spread in small range, usually several minutes,
         * 2) and overflow each 49 days, if milliseconds are stored in 32 bits.
         * The comparison takes into account that overflow.
         */

        /*  node->key < temp->key */
        p = ((set_rbtree_key_int_t) (node->key - temp->key) < 0)
            ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    set_rbt_red(node);
}


void
set_rbtree_delete(set_rbtree_t *tree, set_rbtree_node_t *node)
{
    set_uint64_t           red;
    set_rbtree_node_t  **root, *sentinel, *subst, *temp, *w;

    /* a binary tree delete */

    root = &tree->root;
    sentinel = tree->sentinel;

    if (node->left == sentinel) {
        temp = node->right;
        subst = node;

    } else if (node->right == sentinel) {
        temp = node->left;
        subst = node;

    } else {
        subst = set_rbtree_min(node->right, sentinel);

        if (subst->left != sentinel) {
            temp = subst->left;
        } else {
            temp = subst->right;
        }
    }

    if (subst == *root) {
        *root = temp;
        set_rbt_black(temp);

        /* clear deleted node */
        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;
        node->key = 0;

        return;
    }

    red = set_rbt_is_red(subst);

    if (subst == subst->parent->left) {
        subst->parent->left = temp;

    } else {
        subst->parent->right = temp;
    }

    if (subst == node) {

        temp->parent = subst->parent;

    } else {

        if (subst->parent == node) {
            temp->parent = subst;

        } else {
            temp->parent = subst->parent;
        }

        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        set_rbt_copy_color(subst, node);

        if (node == *root) {
            *root = subst;

        } else {
            if (node == node->parent->left) {
                node->parent->left = subst;
            } else {
                node->parent->right = subst;
            }
        }

        if (subst->left != sentinel) {
            subst->left->parent = subst;
        }

        if (subst->right != sentinel) {
            subst->right->parent = subst;
        }
    }

    /* DEBUG stuff */
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0;

    if (red) {
        return;
    }

    /* a delete fixup */

    while (temp != *root && set_rbt_is_black(temp)) {

        if (temp == temp->parent->left) {
            w = temp->parent->right;

            if (set_rbt_is_red(w)) {
                set_rbt_black(w);
                set_rbt_red(temp->parent);
                set_rbtree_left_rotate(root, sentinel, temp->parent);
                w = temp->parent->right;
            }

            if (set_rbt_is_black(w->left) && set_rbt_is_black(w->right)) {
                set_rbt_red(w);
                temp = temp->parent;

            } else {
                if (set_rbt_is_black(w->right)) {
                    set_rbt_black(w->left);
                    set_rbt_red(w);
                    set_rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }

                set_rbt_copy_color(w, temp->parent);
                set_rbt_black(temp->parent);
                set_rbt_black(w->right);
                set_rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;
            }

        } else {
            w = temp->parent->left;

            if (set_rbt_is_red(w)) {
                set_rbt_black(w);
                set_rbt_red(temp->parent);
                set_rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }

            if (set_rbt_is_black(w->left) && set_rbt_is_black(w->right)) {
                set_rbt_red(w);
                temp = temp->parent;

            } else {
                if (set_rbt_is_black(w->left)) {
                    set_rbt_black(w->right);
                    set_rbt_red(w);
                    set_rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }

                set_rbt_copy_color(w, temp->parent);
                set_rbt_black(temp->parent);
                set_rbt_black(w->left);
                set_rbtree_right_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
    }

    set_rbt_black(temp);
}


static inline void
set_rbtree_left_rotate(set_rbtree_node_t **root, set_rbtree_node_t *sentinel,
    set_rbtree_node_t *node)
{
    set_rbtree_node_t  *temp;

    temp = node->right;
    node->right = temp->left;

    if (temp->left != sentinel) {
        temp->left->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->left) {
        node->parent->left = temp;

    } else {
        node->parent->right = temp;
    }

    temp->left = node;
    node->parent = temp;
}


static inline void
set_rbtree_right_rotate(set_rbtree_node_t **root, set_rbtree_node_t *sentinel,
    set_rbtree_node_t *node)
{
    set_rbtree_node_t  *temp;

    temp = node->left;
    node->left = temp->right;

    if (temp->right != sentinel) {
        temp->right->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->right) {
        node->parent->right = temp;

    } else {
        node->parent->left = temp;
    }

    temp->right = node;
    node->parent = temp;
}


set_rbtree_node_t *
set_rbtree_next(set_rbtree_t *tree, set_rbtree_node_t *node)
{
    set_rbtree_node_t  *root, *sentinel, *parent;

    sentinel = tree->sentinel;

    if (node->right != sentinel) {
        return set_rbtree_min(node->right, sentinel);
    }

    root = tree->root;

    for ( ;; ) {
        parent = node->parent;

        if (node == root) {
            return NULL;
        }

        if (node == parent->left) {
            return parent;
        }

        node = parent;
    }
}





