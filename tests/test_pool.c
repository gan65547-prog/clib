/**
 * Test memory pool - alloc/free/reuse lifecycle
 * Uses stub for ml_pagesize() since this is a platform test.
 */
#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_rbtree.h"
#include "../include/ml_container.h"
#include "../include/ml_pool.h"

#include "../include/ml_code.h"
#include "../src/ml_pool.c"

static void test_pool_create(void) {
    ml_pool_t pool;
    int ret = ml_pool_create(&pool, 0);
    ASSERT_EQ(ret, 0, "pool create should return ML_OK");
    ASSERT_PTR_NOT_NULL(pool.m_page.m_page_first, "pool should have initial page");
    ml_pool_destroy(&pool);
}

static void test_pool_alloc_small(void) {
    ml_pool_t pool;
    ml_pool_create(&pool, 0);

    void *p1 = pool.f_alloc(&pool, 64);
    ASSERT_PTR_NOT_NULL(p1, "alloc 64 bytes should succeed");

    void *p2 = pool.f_alloc(&pool, 128);
    ASSERT_PTR_NOT_NULL(p2, "alloc 128 bytes should succeed");

    void *p3 = pool.f_alloc(&pool, 256);
    ASSERT_PTR_NOT_NULL(p3, "alloc 256 bytes should succeed");

    /* Verify allocations are within pool page */
    unsigned char *page_start = pool.m_page.m_page_first->m_first;
    unsigned char *page_end   = pool.m_page.m_page_first->m_last;
    ASSERT((unsigned char*)p1 >= page_start && (unsigned char*)p1 < page_end,
           "p1 should be within pool page");
    ASSERT((unsigned char*)p2 >= page_start && (unsigned char*)p2 < page_end,
           "p2 should be within pool page");

    ml_pool_destroy(&pool);
}

static void test_pool_alloc_free_reuse(void) {
    ml_pool_t pool;
    ml_pool_create(&pool, 0);

    void *p1 = pool.f_alloc(&pool, 100);
    ASSERT_PTR_NOT_NULL(p1, "alloc should succeed");
    memset(p1, 0xAB, 100);

    /* Free back to container */
    pool.f_free(&pool, p1);

    /* Re-alloc same size - should return same block */
    void *p2 = pool.f_alloc(&pool, 100);
    ASSERT_PTR_NOT_NULL(p2, "re-alloc should succeed");
    ASSERT_EQ(p1, p2, "should reuse freed block of same size");

    unsigned char *cp = (unsigned char*)p2;
    ASSERT_EQ(cp[0], 0xAB, "old data pattern should remain (pool doesn't clear)");

    ml_pool_destroy(&pool);
}

static void test_pool_large_alloc(void) {
    ml_pool_t pool;
    ml_pool_create(&pool, 0);

    long page_size = ml_pagesize();
    void *p1 = pool.f_alloc(&pool, page_size * 2);
    ASSERT_PTR_NOT_NULL(p1, "large alloc should succeed");
    memset(p1, 0xCD, page_size * 2);

    void *p2 = pool.f_alloc(&pool, page_size * 3);
    ASSERT_PTR_NOT_NULL(p2, "very large alloc should succeed");

    pool.f_free(&pool, p1);
    pool.f_free(&pool, p2);
    ml_pool_destroy(&pool);
}

static void test_pool_stress(void) {
    ml_pool_t pool;
    ml_pool_create(&pool, 0);

#define N_ALLOCS 200
    void *ptrs[N_ALLOCS];
    for (int i = 0; i < N_ALLOCS; i++) {
        ptrs[i] = pool.f_alloc(&pool, (size_t)((i % 10) + 1) * 32);
        ASSERT_PTR_NOT_NULL(ptrs[i], "batch alloc should succeed");
        if (ptrs[i]) memset(ptrs[i], (unsigned char)i, ((i % 10) + 1) * 32);
    }

    /* Free every other */
    for (int i = 0; i < N_ALLOCS; i += 2) {
        pool.f_free(&pool, ptrs[i]);
    }

    /* Allocate half again - should reuse freed blocks */
    for (int i = 0; i < N_ALLOCS / 2; i++) {
        void *p = pool.f_alloc(&pool, (size_t)(((i * 2) % 10) + 1) * 32);
        ASSERT_PTR_NOT_NULL(p, "re-alloc after free should succeed");
    }

    ml_pool_destroy(&pool);
#undef N_ALLOCS
}

void test_pool_suite(void) {
    TEST_SUITE("Memory Pool");
    RUN_TEST("create and destroy", test_pool_create());
    RUN_TEST("small allocations", test_pool_alloc_small());
    RUN_TEST("alloc-free-reuse cycle", test_pool_alloc_free_reuse());
    RUN_TEST("large (heap) allocations", test_pool_large_alloc());
    RUN_TEST("stress: 200 allocs, mixed free", test_pool_stress());
}
