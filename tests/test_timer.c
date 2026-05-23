/**
 * Test timer module (red-black tree based timer wheel).
 */
#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_code.h"
#include "../include/ml_set_rbtree.h"
#include "../include/ml_timer.h"
#include "../src/ml_set_rbtree.c"
#include "../src/ml_timer.c"

static void test_timer_tree_init(void) {
    ml_timer_tree_t tree;
    ml_timer_tree_init(&tree);
    /* After init, root should point to sentinel */
    ASSERT(tree.m_tree.root == &tree.m_sentinel, "root = sentinel after init");
}

static void test_timer_node_init(void) {
    ml_timer_node_t node;
    /* Fill with garbage, then init */
    ml_memset(&node, 0xFF, sizeof(node));
    ml_timer_node_init(&node);
    ASSERT_EQ(node.m_rep, 0u, "m_rep = 0 after init");
    ASSERT_EQ(node.m_in, 0u, "m_in = 0 after init");
    ASSERT_PTR_NULL(node.m_param, "m_param = NULL");
    ASSERT_PTR_NULL(node.f_handler, "f_handler = NULL");
}

static void test_timer_add_find_del(void) {
    ml_timer_tree_t tree;
    ml_timer_node_t node;
    ml_msec_t msec = 1000; /* current time */

    ml_timer_tree_init(&tree);
    ml_timer_node_init(&node);
    node.m_out = 500;  /* offset */
    node.f_handler = NULL;

    /* Add timer (expires at msec + m_out = 1500) */
    ml_timer_tree_add(&tree, &node, msec);
    ASSERT_EQ(ml_timer_node_int(&node), 1u, "node should be in tree");

    /* Find nearest timer - should be ~500ms from now */
    ml_msec_t delta = ml_tree_find_timer(&tree, msec);
    ASSERT(delta > 0, "should find timer with positive delta");

    /* Delete timer */
    ml_timer_tree_del(&tree, &node);
    ASSERT_EQ(ml_timer_node_int(&node), 0u, "node should not be in tree");

    /* After delete, no timers remain */
    delta = ml_tree_find_timer(&tree, msec);
    ASSERT_EQ(delta, (ml_msec_t)ML_ETINFN, "no timer should return ETINFN");
}

static void test_timer_multiple_nodes(void) {
    ml_timer_tree_t tree;
    ml_timer_node_t nodes[3];
    ml_msec_t msec = 1000;

    ml_timer_tree_init(&tree);

    /* Add 3 timers with different offsets */
    for (int i = 0; i < 3; i++) {
        ml_timer_node_init(&nodes[i]);
        nodes[i].m_out = (ml_uint_t)((i + 1) * 100);
        ml_timer_tree_add(&tree, &nodes[i], msec);
    }

    /* Nearest timer should be ~100ms */
    ml_msec_t delta = ml_tree_find_timer(&tree, msec);
    ASSERT(delta > 0, "should find nearest timer");

    /* Clean up */
    for (int i = 0; i < 3; i++) {
        ml_timer_tree_del(&tree, &nodes[i]);
    }
}

static int expire_count = 0;
static void timer_expire_handler(ml_timer_node_t *ev) {
    expire_count++;
    (void)ev;
}

static void test_timer_expire(void) {
    ml_timer_tree_t tree;
    ml_timer_node_t node;
    ml_msec_t msec = 1000;

    ml_timer_tree_init(&tree);
    ml_timer_node_init(&node);
    node.m_out = 100;
    node.m_rep = 0;  /* one-shot */
    node.f_handler = timer_expire_handler;
    expire_count = 0;

    ml_timer_tree_add(&tree, &node, msec);
    ASSERT_EQ(ml_timer_node_int(&node), 1u, "node in tree before expire");

    /* Expire with time past the timer's expiry */
    ml_tree_expire_timers(&tree, msec + 200);

    /* Node should have been removed (one-shot) */
    ASSERT_EQ(ml_timer_node_int(&node), 0u, "one-shot node removed after expire");
    ASSERT_EQ(expire_count, 1, "handler called exactly once");
}

void test_timer_suite(void) {
    TEST_SUITE("Timer (RB-Tree Wheel)");
    RUN_TEST("tree init", test_timer_tree_init());
    RUN_TEST("node init", test_timer_node_init());
    RUN_TEST("add / find / del", test_timer_add_find_del());
    RUN_TEST("multiple nodes", test_timer_multiple_nodes());
    RUN_TEST("timer expire (one-shot)", test_timer_expire());
}
