/**
 * Test packet buffer (ml_pkbuf) allocation and ring buffer macros.
 */
#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_code.h"
#include "../include/ml_pool.h"
#include "../include/ml_pkbuf.h"
#include "../src/ml_pkbuf.c"

static ml_pool_t g_pool;

static void test_pkbuf_alloc(void) {
    ml_pkbuf_t *pkb = ml_pkbuf_alloc(128, &g_pool);
    ASSERT_PTR_NOT_NULL(pkb, "pkbuf alloc should succeed");

    ASSERT_EQ(pkb->m_blen, 128, "buffer length should be 128");
    ASSERT_EQ(pkb->m_plen, 0, "payload length should be 0");
    ASSERT_EQ(pkb->m_ulen, 0, "user length should be 0");
    ASSERT_EQ(pkb->m_proto, 0, "protocol should be 0");
    ASSERT_EQ(pkb->m_from, 0, "from should be 0");
    ASSERT_EQ(pkb->m_to, 0, "to should be 0");
    ASSERT_EQ(pkb->m_ssid, 0, "ssid should be 0");
    ASSERT_EQ(pkb->m_dsid, 0, "dsid should be 0");
    ASSERT_PTR_NOT_NULL(pkb->m_buff, "buff pointer should not be NULL");
    ASSERT_PTR_NULL(pkb->m_node.next, "node next should be NULL");
    ASSERT_PTR_NULL(pkb->f_handler, "handler should be NULL");

    /* m_buff should point to memory right after the header */
    ASSERT((void*)pkb->m_buff == (void*)(pkb + 1),
           "buff should point after header");

    ml_pkbuf_free(pkb);
}

static void test_pkbuf_write_read(void) {
    ml_pkbuf_t *pkb = ml_pkbuf_alloc(64, &g_pool);
    ASSERT_PTR_NOT_NULL(pkb, "pkbuf alloc should succeed");

    const char *test_data = "Hello Packet Buffer!";
    int len = (int)strlen(test_data) + 1;
    memcpy(pkb->m_buff, test_data, len);
    pkb->m_plen = len;

    ASSERT_EQ(strcmp((char*)pkb->m_buff, test_data), 0,
              "written data should match read data");
    ASSERT_EQ(pkb->m_plen, len, "payload length should match");

    ml_pkbuf_free(pkb);
}

static void test_pkbuf_multiple_alloc(void) {
    ml_pkbuf_t *bufs[10];
    int sizes[10];

    for (int i = 0; i < 10; i++) {
        sizes[i] = (i + 1) * 64;
        bufs[i] = ml_pkbuf_alloc(sizes[i], &g_pool);
        ASSERT_PTR_NOT_NULL(bufs[i], "each alloc should succeed");
        ASSERT_EQ(bufs[i]->m_blen, sizes[i], "size should match requested");
    }

    /* All buffers should be distinct */
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            ASSERT(bufs[i] != bufs[j], "each buffer should be unique");
        }
    }

    for (int i = 0; i < 10; i++)
        ml_pkbuf_free(bufs[i]);
}

static void test_pring_init(void) {
    ml_pring_t ring;
    ml_pkbuf_t *ring_buf[8];

    ml_pring_init(&ring, ring_buf, 8);

    ASSERT_EQ(ring.m_in, 0u, "initial in should be 0");
    ASSERT_EQ(ring.m_out, 0u, "initial out should be 0");
    ASSERT_EQ(ring.m_len, 8u, "length should be 8");
    ASSERT_EQ(ring.m_ring, ring_buf, "ring pointer should match");
    ASSERT(ml_pring_empty(&ring), "ring should be empty after init");
}

static void test_pring_input_output(void) {
    ml_pring_t ring;
    ml_pkbuf_t *ring_buf[4];
    ml_pkbuf_t *pkts[3];

    for (int i = 0; i < 3; i++) {
        pkts[i] = ml_pkbuf_alloc(32, &g_pool);
        ASSERT_PTR_NOT_NULL(pkts[i], "pkt alloc should succeed");
    }

    ml_pring_init(&ring, ring_buf, 4);
    ASSERT(ml_pring_empty(&ring), "should be empty");

    /* Insert 3 packets */
    ml_pring_input_set(&ring, pkts[0]);
    ml_pring_input_final(&ring);
    ASSERT(!ml_pring_empty(&ring), "should not be empty after input");

    ml_pring_input_set(&ring, pkts[1]);
    ml_pring_input_final(&ring);

    ml_pring_input_set(&ring, pkts[2]);
    ml_pring_input_final(&ring);

    /* Output should give them back in order */
    ml_pkbuf_t *out = ml_pring_output_get(&ring);
    ASSERT_EQ(out, pkts[0], "first output should be pkts[0]");
    ml_pring_output_final(&ring);

    out = ml_pring_output_get(&ring);
    ASSERT_EQ(out, pkts[1], "second output should be pkts[1]");
    ml_pring_output_final(&ring);

    out = ml_pring_output_get(&ring);
    ASSERT_EQ(out, pkts[2], "third output should be pkts[2]");
    ml_pring_output_final(&ring);

    ASSERT(ml_pring_empty(&ring), "should be empty after all outputs");

    for (int i = 0; i < 3; i++)
        ml_pkbuf_free(pkts[i]);
}

static void test_pring_wrap(void) {
    /* Test wrap-around: ring of size 8, write 4/read 2/write 4/read 6 */
    ml_pring_t ring;
    ml_pkbuf_t *ring_buf[8];
    ml_pkbuf_t *items[8];

    for (int i = 0; i < 8; i++) {
        items[i] = ml_pkbuf_alloc(16, &g_pool);
        ASSERT_PTR_NOT_NULL(items[i], "alloc should succeed");
        items[i]->m_ssid = (unsigned int)i;
    }

    ml_pring_init(&ring, ring_buf, 8);

    /* Write 4 */
    for (int i = 0; i < 4; i++) {
        ml_pring_input_set(&ring, items[i]);
        ml_pring_input_final(&ring);
    }
    /* Read 2 */
    for (int i = 0; i < 2; i++) {
        ml_pkbuf_t *o = ml_pring_output_get(&ring);
        ASSERT_EQ(o->m_ssid, (unsigned int)i, "first 2 reads should match");
        ml_pring_output_final(&ring);
    }
    /* Write 4 more (now in=8, out=2, indices 4-7 wrap around) */
    for (int i = 4; i < 8; i++) {
        ml_pring_input_set(&ring, items[i]);
        ml_pring_input_final(&ring);
    }
    /* Read 6 remaining */
    for (int i = 2; i < 8; i++) {
        ml_pkbuf_t *o = ml_pring_output_get(&ring);
        ASSERT_EQ(o->m_ssid, (unsigned int)i, "wrap-around reads should match");
        ml_pring_output_final(&ring);
    }

    ASSERT(ml_pring_empty(&ring), "should be empty at end");

    for (int i = 0; i < 8; i++)
        ml_pkbuf_free(items[i]);
}

void test_pkbuf_suite(void) {
    ml_pool_create(&g_pool, 0);

    TEST_SUITE("Packet Buffer");
    RUN_TEST("alloc and init", test_pkbuf_alloc());
    RUN_TEST("write and read data", test_pkbuf_write_read());
    RUN_TEST("multiple allocs distinct", test_pkbuf_multiple_alloc());
    RUN_TEST("ring init", test_pring_init());
    RUN_TEST("ring input/output", test_pring_input_output());
    RUN_TEST("ring wrap-around", test_pring_wrap());

    ml_pool_destroy(&g_pool);
}
