#include "test_framework.h"
#include "../include/ml_dictionary.h"
#include "../include/ml_pool.h"

static ml_pool_t g_pool;

void test_dictionary_suite(void)
{
    ml_pool_create(&g_pool, 0);

    TEST_SUITE("Dictionary (Hash Table)");

    RUN_TEST("pool NULL guard", {
        ASSERT_PTR_NULL(ml_dictionary_new(0, NULL), "NULL pool returns NULL");
    });

    RUN_TEST("create and delete", {
        ml_dictionary_t *d = ml_dictionary_new(0, &g_pool);
        ASSERT_PTR_NOT_NULL(d, "create");
        ml_dictionary_delete(d);
    });

    RUN_TEST("set and get", {
        ml_dictionary_t *d = ml_dictionary_new(0, &g_pool);
        ASSERT_EQ(ml_dictionary_set(d, "key1", "val1"), 0, "set ok");
        ASSERT_STR_EQ(ml_dictionary_get(d, "key1", NULL), "val1", "get val1");
        ml_dictionary_delete(d);
    });

    RUN_TEST("overwrite existing key", {
        ml_dictionary_t *d = ml_dictionary_new(0, &g_pool);
        ml_dictionary_set(d, "k", "v1");
        ml_dictionary_set(d, "k", "v2");
        ASSERT_STR_EQ(ml_dictionary_get(d, "k", NULL), "v2", "overwritten");
        ml_dictionary_delete(d);
    });

    RUN_TEST("default value for missing key", {
        ml_dictionary_t *d = ml_dictionary_new(0, &g_pool);
        ASSERT_STR_EQ(ml_dictionary_get(d, "nope", "fallback"), "fallback", "default");
        ml_dictionary_delete(d);
    });

    RUN_TEST("unset key", {
        ml_dictionary_t *d = ml_dictionary_new(0, &g_pool);
        ml_dictionary_set(d, "k", "v");
        ml_dictionary_unset(d, "k");
        ASSERT_STR_EQ(ml_dictionary_get(d, "k", "DEFAULT"), "DEFAULT", "gone");
        ml_dictionary_delete(d);
    });

    RUN_TEST("multiple keys (10)", {
        ml_dictionary_t *d = ml_dictionary_new(0, &g_pool);
        char key[16];
        char val[16];
        int i;
        for (i = 0; i < 10; i++) {
            snprintf(key, sizeof(key), "k%d", i);
            snprintf(val, sizeof(val), "v%d", i);
            ASSERT_EQ(ml_dictionary_set(d, key, val), 0, key);
        }
        for (i = 0; i < 10; i++) {
            snprintf(key, sizeof(key), "k%d", i);
            snprintf(val, sizeof(val), "v%d", i);
            ASSERT_STR_EQ(ml_dictionary_get(d, key, NULL), val, key);
        }
        ml_dictionary_delete(d);
    });

    RUN_TEST("grow beyond initial size", {
        ml_dictionary_t *d = ml_dictionary_new(4, &g_pool);
        char key[16];
        char val[16];
        int i;
        for (i = 0; i < 200; i++) {
            snprintf(key, sizeof(key), "key_%d", i);
            snprintf(val, sizeof(val), "val_%d", i);
            ASSERT_EQ(ml_dictionary_set(d, key, val), 0, key);
        }
        ASSERT_STR_EQ(ml_dictionary_get(d, "key_0", NULL), "val_0", "first");
        ASSERT_STR_EQ(ml_dictionary_get(d, "key_199", NULL), "val_199", "last");
        ml_dictionary_delete(d);
    });

    RUN_TEST("hash function", {
        unsigned h1 = ml_dictionary_hash("abc");
        unsigned h2 = ml_dictionary_hash("abc");
        ASSERT_EQ(h1, h2, "hash deterministic");
        ASSERT_EQ(ml_dictionary_hash(NULL), (unsigned)0, "hash NULL -> 0");
    });

    RUN_TEST("dump output", {
        ml_dictionary_t *d = ml_dictionary_new(0, &g_pool);
        ml_dictionary_set(d, "a", "1");
        FILE *f = tmpfile();
        ASSERT_PTR_NOT_NULL(f, "tmpfile");
        ml_dictionary_dump(d, f);
        fclose(f);
        ml_dictionary_delete(d);
    });

    RUN_TEST("null parameter checks", {
        ASSERT_PTR_NULL(ml_dictionary_get(NULL, "k", NULL), "get NULL dict");
        ASSERT_PTR_NULL(ml_dictionary_get(NULL, NULL, NULL), "get all NULL");
        ASSERT_EQ(ml_dictionary_set(NULL, "k", "v"), -1, "set NULL dict");
        ml_dictionary_unset(NULL, "k");
        ml_dictionary_delete(NULL);
        ml_dictionary_dump(NULL, NULL);
    });

    ml_pool_destroy(&g_pool);
}
