/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef __ML_CONTAINER_H__
#define __ML_CONTAINER_H__


#include "ml_rbtree.h"



/**************************************************************
  红黑树是一种平衡二叉查找树的变体，它的左右子树高差有可能
  大于 1，  所以红黑树不是严格意义上的平衡二叉树（AVL），但
  进行平衡的  代价较低， 其平均统计性能要强于 AVL 。
  红黑树是每个结点都带有颜色属性的二叉查找树，颜色或红色或黑色

红黑树的性质：
性质1. 结点是红色或黑色
性质2. 根结点是黑色
性质3. 所有叶子都是黑色，（叶子是NIL结点）
性质4. 每个红色结点的两个子结点都是黑色
性质5. 从任一结点到其每个叶子的所有路径都包含相同数目的黑色结点
性质6. 从每个叶子到根的所有路径上不能有两个连续的红色结点

  这些约束强制了红黑树的关键性质: 从根到叶子的最长的可能路径
  不多于最短的可能路径的两倍长。结果是这个树大致上是平衡的。
**************************************************************/



typedef struct ml_container_node_s                 ml_container_node_t;
typedef struct ml_container_s                      ml_container_t;
typedef struct ml_container_s                      container_tree_t;


/* 
函数功能：
插入回调函数：data为容器数据，src为要插入的原数据
返回等于0：   data与src相等
返回小于0：data小于src
返回大于0：data大于src
*/
typedef int (*fun_compare)(ml_container_node_t * data, ml_container_node_t * src);

/*
函数功能：
查找回调函数：data为容器数据，src为要查找的原数据
返回0，  查找到合适的数据，
返回其他未找到合适数据
*/
typedef int (*fun_lookup)(ml_container_node_t * data);

/*
函数功能：
删除回调函数：data为容器数据
*/
typedef void (*fun_remove)(ml_container_node_t * data, void * param);


/*
函数功能：
遍历回调函数：param为参数
*/
typedef void (*fun_entry)(ml_container_t * set, ml_container_node_t * node, void * param);

/*
函数功能：
遍历查找回调：param为参数，data为容器节点
*/
typedef int (*f_lookupe)(void * param, ml_container_node_t * data);



struct ml_container_node_s {
    ml_rbtree_node_t          m_node;
    void   *               m_data;
};


struct ml_container_s {
    ml_rbtree_root_t          m_root;
    /* 插入 */
    int (*f_insert)(struct ml_container_s * root, fun_compare fun, ml_container_node_t * src);
    /* 查找 */
    struct ml_container_node_s * (*f_lookup)(struct ml_container_s * root, fun_compare cmp, ml_container_node_t * src, fun_lookup lookup);
    /* 删除 */
    void (*f_erase)(struct ml_container_s * root, struct ml_container_node_s * node);
    /* 清空 */
    void (*f_clean)(struct ml_container_s * set, void * param, fun_remove fun);
    /* 遍历，以键值小到大便利 */
    void (*f_each_l)(struct ml_container_s * set, void * param, fun_entry entry);
    /* 遍历，以键值大到小便利 */
    void (*f_each_r)(struct ml_container_s * set, void * param, fun_entry entry);

    /* 翻页遍历，以键值小到大便利 */
    void (*f_each_ln)(struct ml_container_s * set, ml_int_t start, ml_int_t count, void * param, fun_entry entry);
    /* 翻页遍历，以键值大到小便利 */
    void (*f_each_rn)(struct ml_container_s * set, ml_int_t start, ml_int_t count,void * param, fun_entry entry);

    /* 返回最左侧节点 */
    struct ml_container_node_s * (*f_lookup_l)(struct ml_container_s * set);
    /* 返回最右侧节点 */
    struct ml_container_node_s * (*f_lookup_r)(struct ml_container_s * set);
    /* 遍历查找 */
    struct ml_container_node_s * (*f_lookup_e)(struct ml_container_s * set, void * param, f_lookupe lookupe);
};


extern int ml_set_insert(ml_container_t * set, fun_compare fun, ml_container_node_t * src);
extern int ml_map_insert(ml_container_t * map, fun_compare fun, ml_container_node_t * src);
extern ml_container_node_t * ml_set_lookup(ml_container_t * set, fun_compare cmp, ml_container_node_t * src, fun_lookup lookup);
extern ml_container_node_t * ml_map_lookup(ml_container_t * map, fun_compare cmp, ml_container_node_t * src, fun_lookup lookup);
extern void ml_set_erase(ml_container_t * set, ml_container_node_t *node);
extern void ml_set_clean(ml_container_t * set, void * param, fun_remove fun);
extern void ml_set_each_l(ml_container_t * set, void * param, fun_entry entry);
extern void ml_set_each_r(ml_container_t * set, void * param, fun_entry entry);
extern void ml_set_each_ln(ml_container_t * set, ml_int_t start, ml_int_t count, void * param, fun_entry entry);
extern void ml_set_each_rn(ml_container_t * set, ml_int_t start, ml_int_t count, void * param, fun_entry entry);
extern ml_container_node_t *  ml_set_lookup_l(ml_container_t * set);
extern ml_container_node_t *  ml_set_lookup_r(ml_container_t * set);
extern ml_container_node_t *  ml_set_lookup_e(ml_container_t * set, void * param, f_lookupe lookupe);





/* 对set节点初始化 */
#define ml_container_node_init(n)                                          do { rb_init_node(&((n)->m_node)); } while (0)
#define ml_container_entry(ptr, type, member)                         rb_entry(ptr, type, member)





#define ml_map_init(map)                                     \
    do {                                                            \
        (map)->m_root       = RB_ROOT;                              \
        (map)->f_insert     = ml_map_insert;                           \
        (map)->f_lookup     = ml_map_lookup;                           \
        (map)->f_erase      = ml_set_erase;                            \
        (map)->f_clean      = ml_set_clean;                            \
        (map)->f_each_l     = ml_set_each_l;                           \
        (map)->f_each_r     = ml_set_each_r;                           \
        (map)->f_each_ln    = ml_set_each_ln;                          \
        (map)->f_each_rn    = ml_set_each_rn;                          \
        (map)->f_lookup_l   = ml_set_lookup_l;                         \
        (map)->f_lookup_r   = ml_set_lookup_r;                         \
        (map)->f_lookup_e   = ml_set_lookup_e;                         \
    } while (0)



#define ml_set_init(set)                                     \
    do {                                                            \
        (set)->m_root       = RB_ROOT;                              \
        (set)->f_insert     = ml_set_insert;                           \
        (set)->f_lookup     = ml_set_lookup;                           \
        (set)->f_erase      = ml_set_erase;                            \
        (set)->f_clean      = ml_set_clean;                            \
        (set)->f_each_l     = ml_set_each_l;                           \
        (set)->f_each_r     = ml_set_each_r;                           \
        (set)->f_each_ln    = ml_set_each_ln;                          \
        (set)->f_each_rn    = ml_set_each_rn;                          \
        (set)->f_lookup_l   = ml_set_lookup_l;                         \
        (set)->f_lookup_r   = ml_set_lookup_r;                         \
        (set)->f_lookup_e   = ml_set_lookup_e;                         \
    } while (0)



#endif


