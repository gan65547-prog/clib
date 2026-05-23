#include "test_framework.h"
#include "../include/ml_set_rbtree.h"

void test_set_rbtree_suite(void)
{
    TEST_SUITE("Set RBTree");

    RUN_TEST("init tree with sentinel", {
        set_rbtree_node_t sentinel;
        set_rbtree_t tree;

        set_rbtree_sentinel_init(&sentinel);
        set_rbtree_init(&tree, &sentinel, set_rbtree_insert_value);

        ASSERT_PTR_NOT_NULL(tree.root, "root set");
        ASSERT(tree.root == &sentinel, "root is sentinel");
        ASSERT(tree.sentinel == &sentinel, "sentinel set");
        ASSERT(set_rbt_is_black(tree.root), "sentinel is black");
    });

    RUN_TEST("insert single node", {
        set_rbtree_node_t sentinel;
        set_rbtree_node_t n1;
        set_rbtree_t tree;

        set_rbtree_sentinel_init(&sentinel);
        set_rbtree_init(&tree, &sentinel, set_rbtree_insert_value);

        ml_memzero(&n1, sizeof(n1));
        n1.key = 100;
        set_rbtree_insert(&tree, &n1);

        ASSERT(tree.root == &n1, "n1 is root");
        ASSERT(set_rbt_is_black(&n1), "root is black");
    });

    RUN_TEST("insert multiple and traverse", {
        set_rbtree_node_t sentinel;
        set_rbtree_t tree;
        set_rbtree_node_t nodes[5];
        set_rbtree_node_t *cur;
        int count = 0;
        set_rbtree_sentinel_init(&sentinel);
        set_rbtree_init(&tree, &sentinel, set_rbtree_insert_value);

        {
            int i;
            unsigned long ks[5];
            ks[0] = 50; ks[1] = 30; ks[2] = 70; ks[3] = 20; ks[4] = 40;
            for (i = 0; i < 5; i++) {
                ml_memzero(&nodes[i], sizeof(nodes[i]));
                nodes[i].key = ks[i];
                set_rbtree_insert(&tree, &nodes[i]);
            }
        }

        cur = set_rbtree_min(tree.root, tree.sentinel);
        ASSERT_PTR_NOT_NULL(cur, "min found");
        ASSERT_EQ((int)cur->key, 20, "min is 20");

        while (cur) {
            count++;
            cur = set_rbtree_next(&tree, cur);
        }
        ASSERT_EQ(count, 5, "5 nodes traversed");
    });

    RUN_TEST("delete node maintains tree", {
        set_rbtree_node_t sentinel;
        set_rbtree_node_t n1;
        set_rbtree_node_t n2;
        set_rbtree_node_t n3;
        set_rbtree_t tree;
        set_rbtree_node_t *cur;
        int count = 0;

        set_rbtree_sentinel_init(&sentinel);
        set_rbtree_init(&tree, &sentinel, set_rbtree_insert_value);

        n1.key = 10; set_rbtree_insert(&tree, &n1);
        n2.key = 20; set_rbtree_insert(&tree, &n2);
        n3.key = 30; set_rbtree_insert(&tree, &n3);

        set_rbtree_delete(&tree, &n2);

        cur = set_rbtree_min(tree.root, tree.sentinel);
        while (cur) { count++; cur = set_rbtree_next(&tree, cur); }
        ASSERT_EQ(count, 2, "2 nodes after delete");
    });

    RUN_TEST("delete root", {
        set_rbtree_node_t sentinel;
        set_rbtree_node_t n1;
        set_rbtree_node_t n2;
        set_rbtree_t tree;

        set_rbtree_sentinel_init(&sentinel);
        set_rbtree_init(&tree, &sentinel, set_rbtree_insert_value);

        n1.key = 1; set_rbtree_insert(&tree, &n1);
        n2.key = 2; set_rbtree_insert(&tree, &n2);

        ASSERT(tree.root == &n1, "n1 is root");
        set_rbtree_delete(&tree, &n1);
        ASSERT(tree.root == &n2, "n2 becomes root");
    });

    RUN_TEST("many nodes (50)", {
        set_rbtree_node_t sentinel;
        set_rbtree_t tree;
        set_rbtree_node_t nodes[50];
        set_rbtree_node_t *cur;
        int i;
        int count = 0;
        unsigned long prev;

        set_rbtree_sentinel_init(&sentinel);
        set_rbtree_init(&tree, &sentinel, set_rbtree_insert_value);

        for (i = 0; i < 50; i++) {
            ml_memzero(&nodes[i], sizeof(nodes[i]));
            nodes[i].key = (unsigned long)((i * 7 + 3) % 97);
            set_rbtree_insert(&tree, &nodes[i]);
        }

        cur = set_rbtree_min(tree.root, tree.sentinel);
        prev = 0;
        while (cur) {
            ASSERT(cur->key >= prev, "order maintained");
            prev = cur->key;
            count++;
            cur = set_rbtree_next(&tree, cur);
        }
        ASSERT_EQ(count, 50, "50 nodes");
    });
}
