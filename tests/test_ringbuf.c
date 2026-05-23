/**
 * Test ring buffer core logic using heap-allocated memory
 * to bypass mmap dependency.
 *
 * The ring buffer is a single-producer / single-consumer lock-free FIFO.
 * m_length must be a power of 2.
 */
#include "test_framework.h"
#include "../include/ml_cints.h"
#include "../include/ml_ringbuf.h"
#include "../src/ml_ringbuf.c"

/* Helper: create a heap-backed ring buffer for testing
 * Layout: [ml_kbuff_t header][data buffer of ring_size bytes]
 */
static void ring_heap_create(ml_kring_t *kr, ml_uint_t ring_size) {
    ml_uint_t total = sizeof(ml_kbuff_t) + ring_size;
    ml_uint8_t *buf = (ml_uint8_t*)malloc(total);
    ASSERT_PTR_NOT_NULL(buf, "malloc ring buffer should succeed");

    ml_memzero(buf, total);
    kr->m_mmap.m_addr = buf;
    kr->m_kbuff = (ml_kbuff_t*)buf;
    kr->m_data  = buf + sizeof(ml_kbuff_t);
    kr->m_kbuff->m_length = ring_size;
    kr->m_kbuff->m_input  = 0;
    kr->m_kbuff->m_output = 0;
    pthread_mutex_init(&kr->m_kbuff->m_mutex, NULL);
}

static void ring_heap_destroy(ml_kring_t *kr) {
    pthread_mutex_destroy(&kr->m_kbuff->m_mutex);
    free(kr->m_mmap.m_addr);
    ml_memzero(kr, sizeof(*kr));
}

static void test_ring_create_empty(void) {
    ml_kring_t kr;
    ring_heap_create(&kr, 256);

    ASSERT(ml_kring_empty(&kr) == ML_TRUE, "new ring should be empty");
    ASSERT_EQ(ml_kring_touse(&kr), 0u, "bytes to use should be 0");
    ASSERT_EQ(ml_kring_vacant(&kr), 256u, "vacant should be ring size");

    ring_heap_destroy(&kr);
}

static void test_ring_input_output(void) {
    ml_kring_t kr;
    ring_heap_create(&kr, 256);

    const char *input = "Hello Ring Buffer!";
    ml_uint_t ilen = (ml_uint_t)strlen(input) + 1;
    ml_uint_t w = ml_kring_input(&kr, (void*)input, ilen);
    ASSERT_EQ(w, ilen, "should write all bytes");
    ASSERT_EQ(ml_kring_touse(&kr), ilen, "to-use should equal written");
    ASSERT(!ml_kring_empty(&kr), "ring should not be empty after write");

    char output[64];
    ml_memzero(output, sizeof(output));
    ml_uint_t r = ml_kring_output(&kr, output, sizeof(output));
    ASSERT_EQ(r, ilen, "should read back all bytes");
    ASSERT_EQ(strcmp(output, input), 0, "output should match input");
    ASSERT(ml_kring_empty(&kr), "ring should be empty after read");

    ring_heap_destroy(&kr);
}

static void test_ring_wrap_around(void) {
    ml_kring_t kr;
    ring_heap_create(&kr, 256);

    /* Write 200, read 100, write 150 (250 total, wraps at 256), read 250 */
    unsigned char wbuf[350], rbuf[300];
    for (int i = 0; i < 350; i++) wbuf[i] = (unsigned char)i;

    /* Write 200 */
    ml_uint_t w = ml_kring_input(&kr, wbuf, 200);
    ASSERT_EQ(w, 200u, "write 200 should succeed");

    /* Read 100 */
    ml_uint_t r = ml_kring_output(&kr, rbuf, 100);
    ASSERT_EQ(r, 100u, "read 100 should succeed");
    ASSERT_EQ(memcmp(rbuf, wbuf, 100), 0, "first 100 read = wbuf[0..99]");

    /* Write another 150 (total 250 in ring, wraps at index 256) */
    w = ml_kring_input(&kr, wbuf + 200, 150);
    ASSERT_EQ(w, 150u, "write 150 should succeed");

    /* Read all 250: 100 old + 150 new */
    r = ml_kring_output(&kr, rbuf, 250);
    ASSERT_EQ(r, 250u, "read 250 should succeed");

    /* First 100 should be remaining from original write */
    ASSERT_EQ(memcmp(rbuf, wbuf + 100, 100), 0,
              "first 100 of readback = wbuf[100..199]");
    /* Next 150 should be from second write */
    ASSERT_EQ(memcmp(rbuf + 100, wbuf + 200, 150), 0,
              "last 150 of readback = wbuf[200..349]");

    ASSERT(ml_kring_empty(&kr), "ring should be empty after full read");
    ring_heap_destroy(&kr);
}

static void test_ring_inall(void) {
    ml_kring_t kr;
    ring_heap_create(&kr, 256);

    unsigned char wbuf[300];
    for (int i = 0; i < 300; i++) wbuf[i] = (unsigned char)i;

    /* Write 150, then try inall for 50 (fits) and 200 (doesn't fit) */
    ml_kring_input(&kr, wbuf, 150);
    ASSERT_EQ(ml_kring_touse(&kr), 150u, "150 bytes in ring");

    /* inall 50 bytes - should fit (vacant = 256-150 = 106 >= 50) */
    ml_uint_t w = ml_kring_inall(&kr, wbuf + 150, 50);
    ASSERT_EQ(w, 50u, "inall 50 should fit");
    ASSERT_EQ(ml_kring_touse(&kr), 200u, "200 bytes in ring after inall");

    /* inall 200 bytes - should NOT fit (vacant = 256-200 = 56 < 200) */
    w = ml_kring_inall(&kr, wbuf + 200, 200);
    ASSERT_EQ(w, 0u, "inall 200 should fail (not enough space)");
    ASSERT_EQ(ml_kring_touse(&kr), 200u, "to-use unchanged after failed inall");

    ring_heap_destroy(&kr);
}

static void test_ring_vacant_full(void) {
    ml_kring_t kr;
    ring_heap_create(&kr, 256);

    unsigned char wbuf[256];
    ml_memzero(wbuf, sizeof(wbuf));

    /* Fill ring */
    ml_uint_t w = ml_kring_input(&kr, wbuf, 200);
    ASSERT_EQ(w, 200u, "write 200");

    ASSERT_EQ(ml_kring_vacant(&kr), 56u, "vacant = 256 - 200");
    ASSERT_EQ(ml_kring_touse(&kr), 200u, "to-use = 200");

    /* Read half */
    unsigned char rbuf[128];
    ml_kring_output(&kr, rbuf, 100);
    ASSERT_EQ(ml_kring_vacant(&kr), 156u, "vacant = 256 - 100");
    ASSERT_EQ(ml_kring_touse(&kr), 100u, "to-use = 100");

    ring_heap_destroy(&kr);
}

void test_ringbuf_suite(void) {
    TEST_SUITE("Ring Buffer (heap-backed)");
    RUN_TEST("create empty properties", test_ring_create_empty());
    RUN_TEST("basic input/output", test_ring_input_output());
    RUN_TEST("wrap-around", test_ring_wrap_around());
    RUN_TEST("inall (all-or-nothing write)", test_ring_inall());
    RUN_TEST("vacant/to-use counters", test_ring_vacant_full());
}
