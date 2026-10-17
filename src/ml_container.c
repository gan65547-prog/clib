/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "ml_container.h"


int ml_set_insert(ml_container_t * set, fun_compare fun, ml_container_node_t * src)
{
    int result = 0;
    ml_rbtree_root_t * root = &set->m_root;
    ml_rbtree_node_t **new  = &(root->rb_node), *parent = NULL;

    /* 选择插入节点的位置 */
    while (*new) {
        ml_container_node_t *this = rb_entry(*new, ml_container_node_t, m_node);
        result = fun(this, src);

        parent = *new;
        if (result < 0) {
            new = &((*new)->rb_left);
        }
        else if (result > 0) {
            new = &((*new)->rb_right);
        }
        else {
            /* equal key — insert left, consistent with ml_set_lookup's
             * result <= 0 → left branch. This preserves the "find first
             * matching" semantic used by pool's free-list lookup. */
            new = &((*new)->rb_left);
        }
    }

    /* 插入新节点并平衡树结构 */
    rb_link_node(&src->m_node, parent, new);
    rb_insert_color(&src->m_node, root);
    return 0;
}


int ml_map_insert(ml_container_t * map, fun_compare fun, ml_container_node_t * src)
{
    int result = 0, flag = 0;
    ml_rbtree_root_t * root = &map->m_root;
    ml_rbtree_node_t **new = &(root->rb_node), *parent = NULL;

    /* 选择插入节点的位置 */
    while (*new) {
        ml_container_node_t *this = rb_entry(*new, ml_container_node_t, m_node);
        result = fun(this, src);

        parent = *new;
        if (result < 0) {
            new = &((*new)->rb_left);
        }
        else if (result > 0) {
            new = &((*new)->rb_right);
        }
        else {
            flag = 1;
            break;
        }
    }

    if ( !flag ) {
        /* 插入新节点并平衡树结构 */
        rb_link_node(&src->m_node, parent, new);
        rb_insert_color(&src->m_node, root);
        return 0;
    }

    return -1;
}


ml_container_node_t * ml_set_lookup(ml_container_t * set, fun_compare cmp, ml_container_node_t * src, fun_lookup lookup)
{
    ml_container_node_t *cur;
    ml_rbtree_root_t * root = &set->m_root;
    ml_rbtree_node_t *rbnode = root->rb_node;
    int flag = 0, result;

    /* 
     * ml_set_lookup: find the first node where cmp(cur, src) <= 0
     * When lookup filter is provided, the first cmp<=0 node that also
     * passes lookup() is returned. This works correctly only when the
     * data structure is ordered such that all nodes with cmp<=0 are
     * contiguous from the match point leftwards (e.g. memory size pool).
     */
    while (rbnode != NULL) {
        cur = rb_entry(rbnode, ml_container_node_t, m_node);
        result = cmp(cur, src);

        if (result <= 0) {
            if (lookup) {
                if ((lookup(cur))) {
                    flag = 1;
                    break;
                }
                /* Keep searching: try left first for smaller values,
                 * right branch is skipped since cmp <= 0 matches are
                 * only in the left subtree for this use case */
                rbnode = rbnode->rb_left;
            }
            else {
                flag = 1;
                break;
            }
        }
        else {
            rbnode = rbnode->rb_right;
        }
    }

    return (flag) ? cur : NULL;
}




ml_container_node_t * ml_map_lookup(ml_container_t * map, fun_compare cmp, ml_container_node_t * src, fun_lookup lookup)
{
    ml_container_node_t *cur;
    ml_rbtree_root_t * root = &map->m_root;
    ml_rbtree_node_t *rbnode = root->rb_node;
    long flag = 0, result;

    while (rbnode != NULL) {
        cur = rb_entry(rbnode, ml_container_node_t, m_node);
        result = cmp(cur, src);

        if (result < 0) {
            rbnode = rbnode->rb_left;
        }

        else if (result > 0) {
            rbnode = rbnode->rb_right;
        }
        else {
            flag = 1;
            break;
        }
    }

    return (flag) ? cur : NULL;
}


void ml_set_erase(ml_container_t * set, ml_container_node_t *node)
{
    ml_rbtree_root_t * root = &set->m_root;
    rb_erase(&node->m_node, root);
}



void ml_set_clean(ml_container_t * set, void * param, fun_remove remove)
{
    ml_container_node_t *cur;
    ml_rbtree_root_t * root = &set->m_root;
    ml_rbtree_node_t *rbnode = rb_first(root);

    while (rbnode != NULL) {
        cur = rb_entry(rbnode, ml_container_node_t, m_node);
        rbnode = rb_next(rbnode);

        rb_erase(&cur->m_node, root);
        if (remove)
            remove(cur, param);
    }
}



void ml_set_each_l(ml_container_t * set, void * param, fun_entry entry)
{
    ml_container_node_t *cur;
    ml_rbtree_root_t * root = &set->m_root;
    ml_rbtree_node_t *rbnode = rb_first(root);

    while (rbnode != NULL) {
        cur = rb_entry(rbnode, ml_container_node_t, m_node);
        rbnode = rb_next(rbnode);

        entry(set, cur, param);
    }
}

void ml_set_each_r(ml_container_t * set, void * param, fun_entry entry)
{
    ml_container_node_t *cur;
    ml_rbtree_root_t * root = &set->m_root;
    ml_rbtree_node_t * rbnode = rb_last(root);

    while (rbnode != NULL) {
        cur = rb_entry(rbnode, ml_container_node_t, m_node);
        rbnode = rb_prev(rbnode);

        entry(set, cur, param);
    }
}




void ml_set_each_ln(ml_container_t * set, ml_int_t start, ml_int_t count, void * param, fun_entry entry)
{
    ml_int_t i = 0, j = 0;
    ml_container_node_t *cur;
    ml_rbtree_root_t * root = &set->m_root;
    ml_rbtree_node_t *rbnode = rb_first(root);

    while (rbnode != NULL && j < count) {
        cur = rb_entry(rbnode, ml_container_node_t, m_node);
        rbnode = rb_next(rbnode);

        if (i++ < start)
            continue;

        entry(set, cur, param);
        j++;
    }
}



void ml_set_each_rn(ml_container_t * set, ml_int_t start, ml_int_t count, void * param, fun_entry entry)
{
    ml_int_t i = 0, j = 0;    
    ml_container_node_t *cur;
    ml_rbtree_root_t * root = &set->m_root;
    ml_rbtree_node_t * rbnode = rb_last(root);

    while (rbnode != NULL && j < count) {
        cur = rb_entry(rbnode, ml_container_node_t, m_node);
        rbnode = rb_prev(rbnode);

        if (i++ < start)
            continue;

        entry(set, cur, param);
        j++;
    }
}





ml_container_node_t * ml_set_lookup_l(ml_container_t * set)
{
    ml_container_node_t *cur;
    ml_rbtree_root_t *root = &set->m_root;
    ml_rbtree_node_t *rbnode = rb_first(root);

    if (rbnode) {
        cur = rb_entry(rbnode, ml_container_node_t, m_node);        
        return cur;
    }
    return NULL;
}


ml_container_node_t * ml_set_lookup_r(ml_container_t * set)
{
    ml_container_node_t *cur;
    ml_rbtree_root_t *root = &set->m_root;
    ml_rbtree_node_t *rbnode = rb_last(root);

    if (rbnode) {
        cur = rb_entry(rbnode, ml_container_node_t, m_node);        
        return cur;
    }
    return NULL;
}



ml_container_node_t *  ml_set_lookup_e(ml_container_t * set, void * param, f_lookupe lookupe)
{
    ml_container_node_t *cur;
    ml_rbtree_root_t * root = &set->m_root;
    ml_rbtree_node_t *rbnode = rb_first(root);
    int flag = 0;

    while (rbnode != NULL) {
        cur = rb_entry(rbnode, ml_container_node_t, m_node);
        rbnode = rb_next(rbnode);

        if (lookupe(param, cur)) {
            flag = 1;
            break;
        }
    }

    return (flag) ? cur : NULL;
}


