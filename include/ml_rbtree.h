/*
 * ml_rbtree — Red-black tree (intrusive, generic).
 *
 * Classic Linux-kernel-style intrusive red-black tree.  Nodes are embedded
 * in the caller's struct and accessed via rb_entry / container_of.
 * Supports insert, delete, search, and ordered traversal.
 */
#ifndef	__ML_RBTREE_H__
#define	__ML_RBTREE_H__

#include "ml_types.h"

typedef struct rb_node   ml_rbtree_node_t;
typedef struct rb_root   ml_rbtree_root_t;

struct rb_node
{
	unsigned long  rb_parent_color;
	struct rb_node *rb_right;
	struct rb_node *rb_left;
};

struct rb_root
{
	struct rb_node *rb_node;
};

static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)
{
	rb->rb_parent_color = (rb->rb_parent_color & 3) | (unsigned long)p;
}
static inline void rb_set_color(struct rb_node *rb, int color)
{
	rb->rb_parent_color = (rb->rb_parent_color & ~1) | color;
}

#define RB_EMPTY_NODE(node)	(rb_parent(node) == node)
#define RB_CLEAR_NODE(node)	(rb_set_parent(node, node))

#define RB_ROOT	(struct rb_root) { NULL, }
#define RB_EMPTY_ROOT(root)	((root)->rb_node == NULL)

#define rb_parent(r)   ((struct rb_node *)((r)->rb_parent_color & ~3))
#define rb_color(r)   ((r)->rb_parent_color & 1)
#define rb_is_red(r)   (!rb_color(r))
#define rb_is_black(r) rb_color(r)
#define rb_set_red(r)  do { (r)->rb_parent_color &= ~1; } while (0)
#define rb_set_black(r)  do { (r)->rb_parent_color |= 1; } while (0)

#define rb_entry(ptr, type, member) container_of(ptr, type, member)
#define rb_each_left(root, ptr)   for ( (ptr) = rb_first(root); (ptr); (ptr) = rb_next(ptr) )
#define rb_each_right(root, ptr)  for ( (ptr) = rb_last(root); (ptr); (ptr) = rb_prev(ptr) )

static inline void rb_init_node(struct rb_node *rb)
{
	rb->rb_parent_color = 0;
	rb->rb_right = NULL;
	rb->rb_left = NULL;
	RB_CLEAR_NODE(rb);
}

void rb_insert_color(struct rb_node *, struct rb_root *);
void rb_erase(struct rb_node *, struct rb_root *);

/* Find logical next and previous nodes in a tree */
struct rb_node *rb_next(struct rb_node *);
struct rb_node *rb_prev(struct rb_node *);
struct rb_node *rb_first(struct rb_root *);
struct rb_node *rb_last(struct rb_root *);

/* Replace one node with another without rebalancing */
extern void rb_replace_node(struct rb_node *victim, struct rb_node *new_node,
			    struct rb_root *root);

void rb_link_node(struct rb_node * node, struct rb_node * parent,
		  struct rb_node ** rb_link);

#endif	/* __ML_RBTREE_H__ */
