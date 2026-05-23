#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_rbtree.h"
#include "../include/ml_container.h"

typedef struct {
    ml_container_node_t node;
    int id;
    char name[32];
} test_item_t;

static int item_compare(ml_container_node_t *a, ml_container_node_t *b) {
    test_item_t *ia = ml_container_entry(a, test_item_t, node);
    test_item_t *ib = ml_container_entry(b, test_item_t, node);
    return ib->id - ia->id;
}

static int item_lookup_filter(ml_container_node_t *data) {
    test_item_t *item = ml_container_entry(data, test_item_t, node);
    return (item->id >= 50);
}

static void test_set_insert_lookup(void) {
    ml_container_t set;
    ml_set_init(&set);

    test_item_t items[5];
    for (int i = 0; i < 5; i++) {
        items[i].id = (i + 1) * 10;
        snprintf(items[i].name, sizeof(items[i].name), "item_%d", items[i].id);
        ml_container_node_init(&items[i].node);
        int ret = set.f_insert(&set, item_compare, &items[i].node);
        ASSERT_EQ(ret, 0, "set insert should return 0");
    }

    /* Lookup with filter: find first with id >= 50 */
    test_item_t search;
    search.id = 50;
    ml_container_node_init(&search.node);
    ml_container_node_t *found = set.f_lookup(&set, item_compare, &search.node, item_lookup_filter);
    ASSERT_PTR_NOT_NULL(found, "should find item with id >= 50");
    test_item_t *fi = ml_container_entry(found, test_item_t, node);
    ASSERT(fi->id >= 50, "found item id should be >= 50");

    /* Lookup smallest and largest */
    ml_container_node_t *first = set.f_lookup_l(&set);
    ASSERT_PTR_NOT_NULL(first, "should find first element");
    ASSERT_EQ(ml_container_entry(first, test_item_t, node)->id, 10, "first should be 10");

    ml_container_node_t *last = set.f_lookup_r(&set);
    ASSERT_PTR_NOT_NULL(last, "should find last element");
    ASSERT_EQ(ml_container_entry(last, test_item_t, node)->id, 50, "last should be 50");
}

static void test_map_insert_duplicate(void) {
    ml_container_t map;
    ml_map_init(&map);

    test_item_t items[3];
    for (int i = 0; i < 3; i++) {
        items[i].id = 100;
        ml_container_node_init(&items[i].node);
    }

    /* First insert should succeed */
    int ret = map.f_insert(&map, item_compare, &items[0].node);
    ASSERT_EQ(ret, 0, "first map insert should succeed");

    /* Duplicate insert should fail (map semantics) */
    ret = map.f_insert(&map, item_compare, &items[1].node);
    ASSERT_EQ(ret, -1, "duplicate map insert should return -1");

    /* Different key should succeed */
    items[2].id = 200;
    ret = map.f_insert(&map, item_compare, &items[2].node);
    ASSERT_EQ(ret, 0, "different key map insert should succeed");
}

static void count_callback(ml_container_t *set, ml_container_node_t *node, void *param) {
    (void)set; (void)node;
    (*(int*)param)++;
}

static void test_set_traversal(void) {
    ml_container_t set;
    ml_set_init(&set);

    test_item_t items[8];
    for (int i = 0; i < 8; i++) {
        items[i].id = ((i * 17) % 100);  /* pseudo-random keys */
        ml_container_node_init(&items[i].node);
        set.f_insert(&set, item_compare, &items[i].node);
    }

    /* Forward traversal */
    int count = 0;
    set.f_each_l(&set, &count, count_callback);
    ASSERT_EQ(count, 8, "should traverse all 8 items forward");

    /* Reverse traversal */
    count = 0;
    set.f_each_r(&set, &count, count_callback);
    ASSERT_EQ(count, 8, "should traverse all 8 items reverse");
}

static void test_set_erase_clean(void) {
    ml_container_t set;
    ml_set_init(&set);

    test_item_t items[3];
    for (int i = 0; i < 3; i++) {
        items[i].id = i * 10;
        ml_container_node_init(&items[i].node);
        set.f_insert(&set, item_compare, &items[i].node);
    }

    /* Erase middle */
    set.f_erase(&set, &items[1].node);
    ASSERT(rb_first(&set.m_root) != NULL, "tree should not be empty after erase one");

    /* Clean all */
    set.f_clean(&set, NULL, NULL);
    ASSERT_PTR_NULL(set.m_root.rb_node, "tree should be empty after clean");
}

void test_container_suite(void) {
    TEST_SUITE("Container (Set/Map)");
    RUN_TEST("set insert and lookup", test_set_insert_lookup());
    RUN_TEST("map duplicate rejection", test_map_insert_duplicate());
    RUN_TEST("set traversal forward/reverse", test_set_traversal());
    RUN_TEST("set erase and clean", test_set_erase_clean());
}
