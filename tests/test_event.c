/**
 * Test event-driven I/O module (epoll wrapper).
 * Linux-only: requires <sys/epoll.h>.
 */
#include "test_framework.h"

#ifdef __linux__
#include "../include/ml_types.h"
#include "../include/ml_code.h"
#include "../include/ml_event.h"
#include "../src/ml_event.c"

static void test_event_create_destroy(void) {
    ml_event_t ev;
    ml_epoll_event_t ee[16];

    ml_int_t ret = ml_event_create(&ev, ee, 16);
    ASSERT_EQ(ret, ML_OK, "event create should succeed");
    ASSERT(ev.m_fd > 0, "epoll fd should be valid");
    ASSERT_EQ(ev.m_len, 16, "event array length = 16");
    ASSERT(ev.f_insert != 0, "insert callback set");
    ASSERT(ev.f_remove != 0, "remove callback set");
    ASSERT(ev.f_cycle != 0, "cycle callback set");

    ml_event_destroy(&ev);
}

void test_event_suite(void) {
    TEST_SUITE("Event Driver (epoll)");
    RUN_TEST("create and destroy", test_event_create_destroy());
}
#else
void test_event_suite(void) {
    TEST_SUITE("Event Driver (epoll)");
    printf("  SKIP epoll not available on this platform\n");
}
#endif
