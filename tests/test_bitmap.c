/**
 * Test bitmap operations - uses the real sws_bitmap.h API:
 * bitmap_init, bitmap_set, bitmap_test, bitmap_get
 */
#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_bitmap.h"    /* ml_bitmap_t, bitmap_init, bitmap_set, bitmap_test, bitmap_get */
#include "../src/ml_bitmap.c"

static void test_bitmap_set_test(void) {
    unsigned char bits_storage[8];
    ml_bitmap_t bmp;
    bmp.bits = bits_storage;
    bmp.btmp_bytes_len = sizeof(bits_storage);
    ml_bitmap_init(&bmp);

    /* Set some bits */
    ml_bitmap_set(&bmp, 0, 1);
    ml_bitmap_set(&bmp, 7, 1);
    ml_bitmap_set(&bmp, 15, 1);
    ml_bitmap_set(&bmp, 63, 1);

    ASSERT(ml_bitmap_test(&bmp, 0) != 0, "bit 0 should be set");
    ASSERT(ml_bitmap_test(&bmp, 7) != 0, "bit 7 should be set");
    ASSERT(ml_bitmap_test(&bmp, 15) != 0, "bit 15 should be set");
    ASSERT(ml_bitmap_test(&bmp, 63) != 0, "bit 63 should be set");
    ASSERT(ml_bitmap_test(&bmp, 1) == 0, "bit 1 should be clear");
    ASSERT(ml_bitmap_test(&bmp, 3) == 0, "bit 3 should be clear");
}

static void test_bitmap_clear(void) {
    unsigned char bits_storage[8];
    ml_bitmap_t bmp;
    bmp.bits = bits_storage;
    bmp.btmp_bytes_len = sizeof(bits_storage);
    ml_bitmap_init(&bmp);

    ml_bitmap_set(&bmp, 10, 1);
    ASSERT(ml_bitmap_test(&bmp, 10) != 0, "bit 10 should be set");
    ml_bitmap_set(&bmp, 10, 0);
    ASSERT(ml_bitmap_test(&bmp, 10) == 0, "bit 10 should be clear");
}

static void test_bitmap_find_free(void) {
    unsigned char bits_storage[4];
    ml_bitmap_t bmp;
    bmp.bits = bits_storage;
    bmp.btmp_bytes_len = sizeof(bits_storage);
    ml_bitmap_init(&bmp);

    /* Set first 5 bits */
    for (int i = 0; i < 5; i++) ml_bitmap_set(&bmp, i, 1);

    unsigned free_bit;
    int ret = ml_bitmap_get(&bmp, &free_bit);
    ASSERT_EQ(ret, 0, "bitmap_get should find free bit");
    ASSERT_EQ(free_bit, 5u, "first free bit should be 5");

    /* Set all bits, no free */
    for (int i = 0; i < 32; i++) ml_bitmap_set(&bmp, i, 1);
    ret = ml_bitmap_get(&bmp, &free_bit);
    ASSERT_EQ(ret, -1, "bitmap_get should return -1 when full");
}

static void test_bitmap_all_ones_pattern(void) {
    unsigned char bits_storage[2];
    ml_bitmap_t bmp;
    bmp.bits = bits_storage;
    bmp.btmp_bytes_len = sizeof(bits_storage);
    ml_bitmap_init(&bmp);

    /* Set alternating bits */
    for (int i = 0; i < 16; i += 2) ml_bitmap_set(&bmp, i, 1);
    for (int i = 0; i < 16; i += 2) ASSERT(ml_bitmap_test(&bmp, i) != 0, "even bit should be set");
    for (int i = 1; i < 16; i += 2) ASSERT(ml_bitmap_test(&bmp, i) == 0, "odd bit should be clear");
}

void test_bitmap_suite(void) {
    TEST_SUITE("Bitmap");
    RUN_TEST("set and test bits", test_bitmap_set_test());
    RUN_TEST("clear bits", test_bitmap_clear());
    RUN_TEST("find first free bit", test_bitmap_find_free());
    RUN_TEST("alternating pattern", test_bitmap_all_ones_pattern());
}
