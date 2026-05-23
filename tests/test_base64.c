/**
 * Test base64 encode/decode with correct 4-parameter API:
 * ml_base64_encode(inbuf, ilen, outbuf, olen)
 * ml_base64_decode(inbuf, ilen, outbuf, olen)
 */
#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_base64.h"
#include "../src/ml_base64.c"

static void test_base64_text_roundtrip(void) {
    const char *input = "Hello World!";
    int ilen = (int)strlen(input);
    char encoded[64];
    unsigned char decoded[64];

    ml_memzero(encoded, sizeof(encoded));
    ml_memzero(decoded, sizeof(decoded));

    int elen = ml_base64_encode((const unsigned char*)input, ilen, encoded, sizeof(encoded));
    ASSERT(elen > 0, "encode should return positive length");

    int dlen = ml_base64_decode(encoded, elen, decoded, sizeof(decoded));
    ASSERT(dlen > 0, "decode should return positive length");
    ASSERT_EQ(dlen, ilen, "decoded length should match input");
    ASSERT_EQ(strcmp((char*)decoded, input), 0, "round-trip should preserve data");
}

static void test_base64_binary_roundtrip(void) {
    unsigned char input[256];
    for (int i = 0; i < 256; i++) input[i] = (unsigned char)i;

    char encoded[512];
    unsigned char decoded[512];
    ml_memzero(encoded, sizeof(encoded));
    ml_memzero(decoded, sizeof(decoded));

    int elen = ml_base64_encode(input, 256, encoded, sizeof(encoded));
    ASSERT(elen > 0, "encode binary should succeed");

    int dlen = ml_base64_decode(encoded, elen, decoded, sizeof(decoded));
    ASSERT_EQ(dlen, 256, "decode binary should get 256 bytes");
    ASSERT_EQ(memcmp(decoded, input, 256), 0, "binary round-trip should match");
}

static void test_base64_encode_size(void) {
    /* Verify ML_BASE64_ENC_SIZE macro */
    ASSERT_EQ(ML_BASE64_ENC_SIZE(3), 5,  "enc size for 3 bytes = 4 chars + null");
    ASSERT_EQ(ML_BASE64_ENC_SIZE(6), 9,  "enc size for 6 bytes = 8 chars + null");
    ASSERT_EQ(ML_BASE64_ENC_SIZE(1), 5,  "enc size for 1 byte = 4 chars + null");
}

void test_base64_suite(void) {
    TEST_SUITE("Base64");
    RUN_TEST("text round-trip", test_base64_text_roundtrip());
    RUN_TEST("binary round-trip", test_base64_binary_roundtrip());
    RUN_TEST("encode size macro", test_base64_encode_size());
}
