#include "test_framework.h"
#include "../include/ml_shm_pool.h"
#include "../include/ml_code.h"

void test_shm_pool_suite(void)
{
    TEST_SUITE("Shared Memory Pool");

    RUN_TEST("create and close", {
        ml_shm_pool_t sp;
        ml_int_t rc = ml_shm_pool_open(&sp, "test_create", 4096, 1);
        ASSERT_EQ(rc, ML_OK, "open create");
        ASSERT_EQ(sp.m_header->m_magic, ML_SHM_POOL_MAGIC, "magic set");
        ASSERT(sp.m_header->m_refcount > 0, "refcount > 0");
        ml_shm_pool_close(&sp);
    });

    RUN_TEST("attach existing pool", {
        ml_shm_pool_t sp1;
        ml_shm_pool_t sp2;
        ml_int_t rc;

        rc = ml_shm_pool_open(&sp1, "test_attach", 4096, 1);
        ASSERT_EQ(rc, ML_OK, "create");
        ASSERT_EQ(sp1.m_header->m_refcount, 1, "refcount 1");

        rc = ml_shm_pool_open(&sp2, "test_attach", 4096, 0);
        ASSERT_EQ(rc, ML_OK, "attach");
        ASSERT_EQ(sp2.m_header->m_refcount, 2, "refcount 2");

        ml_shm_pool_close(&sp2);
        ml_shm_pool_close(&sp1);
    });

    RUN_TEST("alloc and used counter", {
        ml_shm_pool_t sp;
        ml_int_t rc = ml_shm_pool_open(&sp, "test_alloc", 4096, 1);
        ASSERT_EQ(rc, ML_OK, "open");

        ml_uint64_t before = ml_shm_used(&sp);
        void *p = ml_shm_alloc(&sp, 128);
        ASSERT_PTR_NOT_NULL(p, "alloc 128");
        ASSERT(ml_shm_used(&sp) > before, "used increased");

        (void)p;
        ml_shm_pool_close(&sp);
    });

    RUN_TEST("alloc too large returns NULL", {
        ml_shm_pool_t sp;
        ml_int_t rc = ml_shm_pool_open(&sp, "test_overflow", 4096, 1);
        ASSERT_EQ(rc, ML_OK, "open");

        void *p = ml_shm_alloc(&sp, 1024 * 1024);
        ASSERT_PTR_NULL(p, "too big → NULL");

        ml_shm_pool_close(&sp);
    });

    RUN_TEST("multiple allocs from same pool", {
        ml_shm_pool_t sp;
        ml_int_t rc = ml_shm_pool_open(&sp, "test_multi", 8192, 1);
        ASSERT_EQ(rc, ML_OK, "open");

        void *a = ml_shm_alloc(&sp, 256);
        void *b = ml_shm_alloc(&sp, 512);
        void *c = ml_shm_alloc(&sp, 128);
        ASSERT_PTR_NOT_NULL(a, "alloc a");
        ASSERT_PTR_NOT_NULL(b, "alloc b");
        ASSERT_PTR_NOT_NULL(c, "alloc c");
        ASSERT(a != b && b != c && a != c, "distinct pointers");

        (void)a; (void)b; (void)c;
        ml_shm_pool_close(&sp);
    });

    RUN_TEST("null parameter checks", {
        ASSERT_EQ(ml_shm_pool_open(NULL, "n", 4096, 1), ML_EPARAM, "NULL sp");
        ASSERT_EQ(ml_shm_pool_open(NULL, NULL, 4096, 1), ML_EPARAM, "NULL sp+name");
        ASSERT_EQ(ml_shm_pool_open(NULL, "", 4096, 1), ML_EPARAM, "empty name");
        ASSERT_EQ(ml_shm_pool_open(NULL, "n", 0, 1), ML_EPARAM, "size 0");

        ASSERT_PTR_NULL(ml_shm_alloc(NULL, 128), "alloc NULL");
        ASSERT_EQ(ml_shm_used(NULL), (ml_uint64_t)0, "used NULL");
        ASSERT_EQ(ml_shm_refcount(NULL), (ml_int_t)0, "refcount NULL");

        ml_shm_pool_close(NULL);
        ml_shm_free(NULL, NULL);
    });
}
