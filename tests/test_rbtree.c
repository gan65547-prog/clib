#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_rbtree.h"

/* Simple wrapper for testing */
typedef struct {
    ml_rbtree_node_t node;
    int key;
    char value[32];
} test_entry_t;

static test_entry_t* entry_of(ml_rbtree_node_t *n) {
    return rb_entry(n, test_entry_t, node);
}

static void test_insert_and_lookup(void) {
    ml_rbtree_root_t root = RB_ROOT;
    test_entry_t entries[10];

    /* Insert entries */
    for (int i = 0; i < 10; i++) {
        entries[i].key = i * 10;
        snprintf(entries[i].value, sizeof(entries[i].value), "val_%d", i);
        rb_init_node(&entries[i].node);

        ml_rbtree_node_t **new = &(root.rb_node), *parent = NULL;
        while (*new) {
            test_entry_t *cur = entry_of(*new);
            parent = *new;
            if (entries[i].key < cur->key)
                new = &((*new)->rb_left);
            else
                new = &((*new)->rb_right);
        }
        rb_link_node(&entries[i].node, parent, new);
        rb_insert_color(&entries[i].node, &root);
    }

    /* Verify first and last */
    ml_rbtree_node_t *first = rb_first(&root);
    ASSERT(first != NULL, "first should not be NULL");
    ASSERT_EQ(entry_of(first)->key, 0, "first key should be 0");

    ml_rbtree_node_t *last = rb_last(&root);
    ASSERT(last != NULL, "last should not be NULL");
    ASSERT_EQ(entry_of(last)->key, 90, "last key should be 90");

    /* Verify sorted order */
    int prev = -1;
    ml_rbtree_node_t *n;
    for (n = rb_first(&root); n; n = rb_next(n)) {
        test_entry_t *e = entry_of(n);
        ASSERT(e->key > prev, "keys must be in ascending order");
        prev = e->key;
    }

    /* Verify reverse order */
    prev = 999;
    for (n = rb_last(&root); n; n = rb_prev(n)) {
        test_entry_t *e = entry_of(n);
        ASSERT(e->key < prev, "keys must be in descending order");
        prev = e->key;
    }
}

static void test_delete(void) {
    ml_rbtree_root_t root = RB_ROOT;
    test_entry_t entries[5];
    int keys[] = {50, 30, 70, 20, 40};

    for (int i = 0; i < 5; i++) {
        entries[i].key = keys[i];
        rb_init_node(&entries[i].node);
        ml_rbtree_node_t **new = &(root.rb_node), *parent = NULL;
        while (*new) {
            test_entry_t *cur = entry_of(*new);
            parent = *new;
            if (entries[i].key < cur->key)
                new = &((*new)->rb_left);
            else
                new = &((*new)->rb_right);
        }
        rb_link_node(&entries[i].node, parent, new);
        rb_insert_color(&entries[i].node, &root);
    }

    /* Delete middle node (30) */
    rb_erase(&entries[1].node, &root);

    /* Verify it's gone */
    int count = 0;
    ml_rbtree_node_t *n;
    for (n = rb_first(&root); n; n = rb_next(n)) {
        test_entry_t *e = entry_of(n);
        ASSERT(e->key != 30, "deleted key 30 should not be found");
        count++;
    }
    ASSERT_EQ(count, 4, "should have 4 nodes after delete");

    /* Delete root (50) */
    rb_erase(&entries[0].node, &root);
    count = 0;
    for (n = rb_first(&root); n; n = rb_next(n)) count++;
    ASSERT_EQ(count, 3, "should have 3 nodes after deleting root");

    /* Delete remaining */
    rb_erase(&entries[2].node, &root); /* 70 */
    rb_erase(&entries[3].node, &root); /* 20 */
    rb_erase(&entries[4].node, &root); /* 40 */
    ASSERT_PTR_NULL(root.rb_node, "tree should be empty");
}

static void test_replace_node(void) {
    ml_rbtree_root_t root = RB_ROOT;
    test_entry_t e1, e2;

    e1.key = 100;
    e2.key = 200;

    rb_init_node(&e1.node);
    rb_init_node(&e2.node);

    rb_link_node(&e1.node, NULL, &root.rb_node);
    rb_insert_color(&e1.node, &root);

    rb_replace_node(&e1.node, &e2.node, &root);

    ASSERT(root.rb_node == &e2.node, "root should be e2 after replace");
    ASSERT(root.rb_node->rb_left == NULL, "replaced node should have no left child");
}

void test_rbtree_suite(void) {
    TEST_SUITE("Red-Black Tree");
    RUN_TEST("insert and ordered traversal", test_insert_and_lookup());
    RUN_TEST("delete and rebalance", test_delete());
    RUN_TEST("replace node", test_replace_node());
}
