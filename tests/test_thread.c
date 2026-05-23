/**
 * Test thread synchronization primitives (pthread wrappers).
 */
#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_code.h"
#include "../include/ml_thread.h"
#include "../src/ml_thread.c"

static void test_mutex_create_destroy(void) {
    ml_thread_mutex_t mtx;
    ASSERT_EQ(ml_thread_mutex_create(&mtx), ML_OK, "mutex create should succeed");
    ASSERT_EQ(ml_thread_mutex_destroy(&mtx), ML_OK, "mutex destroy should succeed");
}

static void test_mutex_lock_unlock(void) {
    ml_thread_mutex_t mtx;
    ml_thread_mutex_create(&mtx);
    ASSERT_EQ(ml_thread_mutex_lock(&mtx), ML_OK, "lock should succeed");
    ASSERT_EQ(ml_thread_mutex_unlock(&mtx), ML_OK, "unlock should succeed");
    ml_thread_mutex_destroy(&mtx);
}

static void test_mutex_double_lock_detected(void) {
    ml_thread_mutex_t mtx;
    ml_thread_mutex_create(&mtx);
    ml_thread_mutex_lock(&mtx);
    /* ERRORCHECK mutex: re-lock by same thread should fail */
    ml_int_t ret = ml_thread_mutex_lock(&mtx);
    ASSERT(ret != ML_OK, "double lock should be detected (ERRORCHECK)");
    ml_thread_mutex_unlock(&mtx);
    ml_thread_mutex_destroy(&mtx);
}

static void test_spinlock(void) {
    ml_spinlock_t spn;
    ASSERT_EQ(ml_thread_spinlock_create(&spn), ML_OK, "spinlock create");
    ASSERT_EQ(ml_thread_spinlock_lock(&spn), ML_OK, "spinlock lock");
    ASSERT_EQ(ml_thread_spinlock_unlock(&spn), ML_OK, "spinlock unlock");
    ASSERT_EQ(ml_thread_spinlock_destroy(&spn), ML_OK, "spinlock destroy");
}

static void test_rwlock(void) {
    ml_thread_rwlock_t rwl;
    ASSERT_EQ(ml_thread_rwlock_create(&rwl), ML_OK, "rwlock create");
    ASSERT_EQ(ml_thread_rwlock_rlock(&rwl), ML_OK, "read lock");
    ASSERT_EQ(ml_thread_rwlock_unlock(&rwl), ML_OK, "read unlock");
    ASSERT_EQ(ml_thread_rwlock_wlock(&rwl), ML_OK, "write lock");
    ASSERT_EQ(ml_thread_rwlock_unlock(&rwl), ML_OK, "write unlock");
    ASSERT_EQ(ml_thread_rwlock_destroy(&rwl), ML_OK, "rwlock destroy");
}

static void test_condvar(void) {
    ml_thread_mutex_t mtx;
    ml_thread_cond_t cond;
    ml_thread_mutex_create(&mtx);
    ASSERT_EQ(ml_thread_cond_create(&cond), ML_OK, "cond create");

    ml_thread_mutex_lock(&mtx);
    ASSERT_EQ(ml_thread_cond_signal(&cond), ML_OK, "cond signal");
    ml_thread_mutex_unlock(&mtx);

    ASSERT_EQ(ml_thread_cond_destroy(&cond), ML_OK, "cond destroy");
    ml_thread_mutex_destroy(&mtx);
}

void test_thread_suite(void) {
    TEST_SUITE("Thread Primitives");
    RUN_TEST("mutex create/destroy", test_mutex_create_destroy());
    RUN_TEST("mutex lock/unlock", test_mutex_lock_unlock());
    RUN_TEST("mutex double-lock detected", test_mutex_double_lock_detected());
    RUN_TEST("spinlock create/lock/unlock/destroy", test_spinlock());
    RUN_TEST("rwlock read/write lock/unlock", test_rwlock());
    RUN_TEST("condvar create/signal/destroy", test_condvar());
}
