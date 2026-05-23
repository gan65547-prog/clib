#include "test_framework.h"
#include <unistd.h>
#include "../include/ml_types.h"
#include "../include/ml_code.h"
#include "../include/ml_pool.h"
#include "../include/ml_thread.h"
#include "../include/ml_thread_pool.h"
#include "../src/ml_thread_pool.c"

static ml_int_t g_counter;
static ml_thread_mutex_t g_ctr_mtx;

static void task_increment(void *arg)
{
    (void)arg;
    ml_thread_mutex_lock(&g_ctr_mtx);
    g_counter++;
    ml_thread_mutex_unlock(&g_ctr_mtx);
}

static void task_sleep_inc(void *arg)
{
    (void)arg;
    ml_pthread_sleep(10000);
    ml_thread_mutex_lock(&g_ctr_mtx);
    g_counter++;
    ml_thread_mutex_unlock(&g_ctr_mtx);
}

static void test_create_destroy(void)
{
    ml_pool_t pool;
    ml_pool_create(&pool, 4096);

    ml_thread_pool_t *tp = ml_thread_pool_create(&pool, 2, 0);
    ASSERT_PTR_NOT_NULL(tp, "thread pool create should succeed");
    ml_thread_pool_destroy(tp);
    ml_pool_destroy(&pool);
}

static void test_single_task(void)
{
    ml_pool_t pool;
    ml_pool_create(&pool, 4096);
    ml_thread_mutex_create(&g_ctr_mtx);

    g_counter = 0;
    ml_thread_pool_t *tp = ml_thread_pool_create(&pool, 2, 0);
    ASSERT_PTR_NOT_NULL(tp, "create");

    ASSERT_EQ(ml_thread_pool_submit(tp, task_increment, NULL), ML_OK, "submit");
    ml_thread_pool_wait(tp);

    ASSERT_EQ(g_counter, 1, "counter should be 1");
    ml_thread_pool_destroy(tp);
    ml_thread_mutex_destroy(&g_ctr_mtx);
    ml_pool_destroy(&pool);
}

static void test_many_tasks(void)
{
    ml_pool_t pool;
    ml_pool_create(&pool, 8192);
    ml_thread_mutex_create(&g_ctr_mtx);

    g_counter = 0;
    ml_thread_pool_t *tp = ml_thread_pool_create(&pool, 4, 0);
    ASSERT_PTR_NOT_NULL(tp, "create");

    ml_int_t i;
    for (i = 0; i < 100; i++) {
        ASSERT_EQ(ml_thread_pool_submit(tp, task_increment, NULL), ML_OK, "submit many");
    }
    ml_thread_pool_wait(tp);

    ASSERT_EQ(g_counter, 100, "counter should be 100");
    ml_thread_pool_destroy(tp);
    ml_thread_mutex_destroy(&g_ctr_mtx);
    ml_pool_destroy(&pool);
}

static void test_concurrent_execution(void)
{
    ml_pool_t pool;
    ml_pool_create(&pool, 8192);
    ml_thread_mutex_create(&g_ctr_mtx);

    g_counter = 0;
    ml_thread_pool_t *tp = ml_thread_pool_create(&pool, 8, 0);
    ASSERT_PTR_NOT_NULL(tp, "create");

    ml_int_t i;
    for (i = 0; i < 50; i++) {
        ASSERT_EQ(ml_thread_pool_submit(tp, task_sleep_inc, NULL), ML_OK, "submit");
    }
    ml_thread_pool_wait(tp);

    ASSERT_EQ(g_counter, 50, "counter should be 50");
    ml_thread_pool_destroy(tp);
    ml_thread_mutex_destroy(&g_ctr_mtx);
    ml_pool_destroy(&pool);
}

static ml_int_t g_block;
static ml_thread_mutex_t g_block_mtx;
static ml_thread_cond_t g_block_cond;

static void task_unblock(void *arg)
{
    (void)arg;
    ml_thread_mutex_lock(&g_block_mtx);
    g_block = 1;
    ml_thread_cond_signal(&g_block_cond);
    /* Wait for main thread to fill the queue */
    while (g_block == 1) {
        ml_thread_cond_wait(&g_block_cond, &g_block_mtx);
    }
    ml_thread_mutex_unlock(&g_block_mtx);
}

static void test_queue_limit(void)
{
    ml_pool_t pool;
    ml_pool_create(&pool, 4096);
    ml_thread_mutex_create(&g_block_mtx);
    ml_thread_cond_create(&g_block_cond);

    g_block = 0;
    ml_thread_pool_t *tp = ml_thread_pool_create(&pool, 1, 2);
    ASSERT_PTR_NOT_NULL(tp, "create with max_queue=2");

    /* Submit a task that blocks until we say go */
    ASSERT_EQ(ml_thread_pool_submit(tp, task_unblock, NULL), ML_OK, "submit blocking task");

    /* Wait for worker to pick up and enter the blocking task */
    ml_thread_mutex_lock(&g_block_mtx);
    while (!g_block) {
        ml_thread_cond_wait(&g_block_cond, &g_block_mtx);
    }

    /* Worker is now blocked. Fill the queue (max 2). */
    ASSERT_EQ(ml_thread_pool_submit(tp, task_increment, NULL), ML_OK, "queue slot 1");
    ASSERT_EQ(ml_thread_pool_submit(tp, task_increment, NULL), ML_OK, "queue slot 2");
    ASSERT_EQ(ml_thread_pool_submit(tp, task_increment, NULL), ML_ERROR, "queue full");

    /* Release worker */
    g_block = 0;
    ml_thread_cond_signal(&g_block_cond);
    ml_thread_mutex_unlock(&g_block_mtx);

    ml_thread_pool_wait(tp);
    ml_thread_pool_destroy(tp);

    ml_thread_cond_destroy(&g_block_cond);
    ml_thread_mutex_destroy(&g_block_mtx);
    ml_pool_destroy(&pool);
}

static void test_null_checks(void)
{
    ml_pool_t pool;
    ml_pool_create(&pool, 4096);

    ASSERT_PTR_NULL(ml_thread_pool_create(NULL, 2, 0), "NULL pool returns NULL");
    ASSERT_PTR_NULL(ml_thread_pool_create(&pool, 0, 0), "0 threads returns NULL");
    ASSERT_PTR_NULL(ml_thread_pool_create(&pool, -1, 0), "negative threads returns NULL");

    ml_thread_pool_t *tp = ml_thread_pool_create(&pool, 1, 0);
    ASSERT_EQ(ml_thread_pool_submit(tp, NULL, NULL), ML_ERROR, "NULL func returns error");
    ml_thread_pool_destroy(tp);

    ml_thread_pool_destroy(NULL);
    ml_thread_pool_wait(NULL);

    ml_pool_destroy(&pool);
}

void test_thread_pool_suite(void)
{
    TEST_SUITE("Thread Pool");
    RUN_TEST("create / destroy", test_create_destroy());
    RUN_TEST("single task", test_single_task());
    RUN_TEST("100 tasks", test_many_tasks());
    RUN_TEST("concurrent execution", test_concurrent_execution());
    RUN_TEST("queue limit", test_queue_limit());
    RUN_TEST("null checks", test_null_checks());
}
