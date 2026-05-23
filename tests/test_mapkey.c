/**
 * Test mapkey - string-keyed and integer-keyed map on container (rbtree).
 */
#include "test_framework.h"
#include "../include/ml_cints.h"
#include "../include/ml_mapkey.h"
#include "../src/ml_mapkey.c"

static void test_mapvalue_string_set_get(void) {
    ml_container_t map;
    ml_map_init(&map);

    ml_mapvalue_t v1, v2;
    char *data1 = (char*)"payload_a";
    char *data2 = (char*)"payload_b";

    ml_mapvalue_setkey(&v1, "key_a");
    ASSERT_EQ(ml_mapvalue_set(&map, &v1, data1), ML_OK, "insert key_a should succeed");
    ASSERT_EQ(v1.m_data, data1, "data pointer should be stored");

    ml_mapvalue_setkey(&v2, "key_b");
    ASSERT_EQ(ml_mapvalue_set(&map, &v2, data2), ML_OK, "insert key_b should succeed");

    /* Lookup */
    ml_mapvalue_t *found = ml_mapvalue_get(&map, "key_a");
    ASSERT_PTR_NOT_NULL(found, "should find key_a");
    ASSERT_EQ(found->m_data, data1, "found data should match key_a");
    ASSERT_EQ(strcmp(ml_mapvalue_getkey(found), "key_a"), 0,
              "key should be key_a");

    found = ml_mapvalue_get(&map, "key_b");
    ASSERT_PTR_NOT_NULL(found, "should find key_b");

    /* Lookup non-existent */
    found = ml_mapvalue_get(&map, "no_such_key");
    ASSERT_PTR_NULL(found, "non-existent key should return NULL");

    /* Delete and verify gone */
    ml_mapvalue_del(&map, &v1);
    found = ml_mapvalue_get(&map, "key_a");
    ASSERT_PTR_NULL(found, "deleted key should not be found");
}

static void test_mapvalue_duplicate_key(void) {
    ml_container_t map;
    ml_map_init(&map);

    ml_mapvalue_t v;
    ml_mapvalue_setkey(&v, "dup_key");
    ASSERT_EQ(ml_mapvalue_set(&map, &v, NULL), ML_OK, "first insert should succeed");

    /* Duplicate insert should fail */
    ml_mapvalue_t v2;
    ml_mapvalue_setkey(&v2, "dup_key");
    ASSERT_EQ(ml_mapvalue_set(&map, &v2, NULL), ML_ERROR,
              "duplicate key should be rejected");
}

static void test_mapvalue_delete_reinsert(void) {
    ml_container_t map;
    ml_map_init(&map);

    ml_mapvalue_t v;
    char *data = (char*)"test_data";

    ml_mapvalue_setkey(&v, "reusable");
    ml_mapvalue_set(&map, &v, data);
    ml_mapvalue_del(&map, &v);

    /* Re-insert with same key should now work */
    ASSERT_EQ(ml_mapvalue_set(&map, &v, data), ML_OK,
              "re-insert after delete should succeed");
}

static void test_mapinteger_set_get(void) {
    ml_container_t map;
    ml_map_init(&map);

    ml_mapvalue_t v[5];
    char *payloads[5] = { (char*)"p0", (char*)"p1", (char*)"p2", (char*)"p3", (char*)"p4" };

    for (int i = 0; i < 5; i++) {
        ml_mapinteger_setkey(&v[i], (ml_uint_t)(i * 10));
        ASSERT_EQ(ml_mapinteger_set(&map, &v[i], payloads[i]), ML_OK,
                  "integer insert should succeed");
    }

    /* Lookup by key */
    ml_mapvalue_t *found = ml_mapinteger_get(&map, 20);
    ASSERT_PTR_NOT_NULL(found, "should find key 20");
    ASSERT_EQ(found->m_data, payloads[2], "data should match");
    ASSERT_EQ(ml_mapinteger_getkey(found), 20u, "key should be 20");

    /* Non-existent */
    found = ml_mapinteger_get(&map, 999);
    ASSERT_PTR_NULL(found, "non-existent key should return NULL");
}

static void test_mapvalue_multiple_lookup(void) {
    ml_container_t map;
    ml_map_init(&map);

    ml_mapvalue_t v[10];
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "item_%02d", i);
        ml_mapvalue_setkey(&v[i], key);
        ml_mapvalue_set(&map, &v[i], (void*)(intptr_t)i);
    }

    /* All keys should be findable */
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "item_%02d", i);
        ml_mapvalue_t *f = ml_mapvalue_get(&map, key);
        ASSERT_PTR_NOT_NULL(f, "each key should be findable");
        ASSERT_EQ((int)(intptr_t)f->m_data, i, "data should match index");
    }
}

void test_mapkey_suite(void) {
    TEST_SUITE("Map Key-Value");
    RUN_TEST("string set/get/delete", test_mapvalue_string_set_get());
    RUN_TEST("duplicate key rejected", test_mapvalue_duplicate_key());
    RUN_TEST("delete and re-insert", test_mapvalue_delete_reinsert());
    RUN_TEST("integer set/get", test_mapinteger_set_get());
    RUN_TEST("multiple lookup (10 keys)", test_mapvalue_multiple_lookup());
}
