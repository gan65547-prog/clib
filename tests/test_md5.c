/**
 * Test MD5 hash with known RFC 1321 test vectors.
 */
#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_md5.h"
#include "../src/ml_md5.c"

static void test_md5_empty(void) {
    ml_hash_md5_t ctx;
    unsigned char digest[ML_HASH_MD5_DIGEST_SIZE];
    char hex[ML_HASH_MD5_DIGEST_HEX_SIZE];

    ml_hash_md5_init(&ctx);
    ml_hash_md5_finish(&ctx, digest, sizeof(digest));
    ml_hex_encode_binary(digest, ML_HASH_MD5_DIGEST_SIZE, hex);

    ASSERT_EQ(strcmp(hex, "d41d8cd98f00b204e9800998ecf8427e"), 0,
              "MD5('') = d41d8cd98f00b204e9800998ecf8427e");
}

static void test_md5_a(void) {
    ml_hash_md5_t ctx;
    unsigned char digest[ML_HASH_MD5_DIGEST_SIZE];
    char hex[ML_HASH_MD5_DIGEST_HEX_SIZE];

    ml_hash_md5_init(&ctx);
    ml_hash_md5_update(&ctx, "a", 1);
    ml_hash_md5_finish(&ctx, digest, sizeof(digest));
    ml_hex_encode_binary(digest, ML_HASH_MD5_DIGEST_SIZE, hex);

    ASSERT_EQ(strcmp(hex, "0cc175b9c0f1b6a831c399e269772661"), 0,
              "MD5('a') = 0cc175b9c0f1b6a831c399e269772661");
}

static void test_md5_abc(void) {
    ml_hash_md5_t ctx;
    unsigned char digest[ML_HASH_MD5_DIGEST_SIZE];
    char hex[ML_HASH_MD5_DIGEST_HEX_SIZE];

    ml_hash_md5_init(&ctx);
    ml_hash_md5_update(&ctx, "abc", 3);
    ml_hash_md5_finish(&ctx, digest, sizeof(digest));
    ml_hex_encode_binary(digest, ML_HASH_MD5_DIGEST_SIZE, hex);

    ASSERT_EQ(strcmp(hex, "900150983cd24fb0d6963f7d28e17f72"), 0,
              "MD5('abc') = 900150983cd24fb0d6963f7d28e17f72");
}

static void test_md5_message_digest(void) {
    ml_hash_md5_t ctx;
    unsigned char digest[ML_HASH_MD5_DIGEST_SIZE];
    char hex[ML_HASH_MD5_DIGEST_HEX_SIZE];
    const char *input = "message digest";

    ml_hash_md5_init(&ctx);
    ml_hash_md5_update(&ctx, input, strlen(input));
    ml_hash_md5_finish(&ctx, digest, sizeof(digest));
    ml_hex_encode_binary(digest, ML_HASH_MD5_DIGEST_SIZE, hex);

    ASSERT_EQ(strcmp(hex, "f96b697d7cb7938d525a2f31aaf161d0"), 0,
              "MD5('message digest') = f96b697d7cb7938d525a2f31aaf161d0");
}

static void test_md5_incremental_update(void) {
    ml_hash_md5_t ctx;
    unsigned char digest1[ML_HASH_MD5_DIGEST_SIZE], digest2[ML_HASH_MD5_DIGEST_SIZE];
    char hex1[ML_HASH_MD5_DIGEST_HEX_SIZE], hex2[ML_HASH_MD5_DIGEST_HEX_SIZE];

    /* All at once */
    ml_hash_md5_init(&ctx);
    ml_hash_md5_update(&ctx, "hello world", 11);
    ml_hash_md5_finish(&ctx, digest1, sizeof(digest1));

    /* Chunked */
    ml_hash_md5_init(&ctx);
    ml_hash_md5_update(&ctx, "hello", 5);
    ml_hash_md5_update(&ctx, " ", 1);
    ml_hash_md5_update(&ctx, "world", 5);
    ml_hash_md5_finish(&ctx, digest2, sizeof(digest2));

    ml_hex_encode_binary(digest1, ML_HASH_MD5_DIGEST_SIZE, hex1);
    ml_hex_encode_binary(digest2, ML_HASH_MD5_DIGEST_SIZE, hex2);

    ASSERT_EQ(strcmp(hex1, hex2), 0,
              "chunked update should match single update");
}

static void test_md5_context_reuse(void) {
    ml_hash_md5_t ctx;
    unsigned char digest[ML_HASH_MD5_DIGEST_SIZE];
    char hex1[ML_HASH_MD5_DIGEST_HEX_SIZE], hex2[ML_HASH_MD5_DIGEST_HEX_SIZE];

    /* First hash */
    ml_hash_md5_init(&ctx);
    ml_hash_md5_update(&ctx, "test", 4);
    ml_hash_md5_finish(&ctx, digest, sizeof(digest));
    ml_hex_encode_binary(digest, ML_HASH_MD5_DIGEST_SIZE, hex1);

    /* Reuse context for second hash */
    ml_hash_md5_init(&ctx);
    ml_hash_md5_update(&ctx, "test", 4);
    ml_hash_md5_finish(&ctx, digest, sizeof(digest));
    ml_hex_encode_binary(digest, ML_HASH_MD5_DIGEST_SIZE, hex2);

    ASSERT_EQ(strcmp(hex1, hex2), 0,
              "context reuse should produce same hash");
}

static void test_md5_large_data(void) {
    ml_hash_md5_t ctx;
    unsigned char digest[ML_HASH_MD5_DIGEST_SIZE];
    char hex[ML_HASH_MD5_DIGEST_HEX_SIZE];

    /* 1000 'A' characters */
    ml_hash_md5_init(&ctx);
    for (int i = 0; i < 100; i++)
        ml_hash_md5_update(&ctx, "AAAAAAAAAA", 10);
    ml_hash_md5_finish(&ctx, digest, sizeof(digest));
    ml_hex_encode_binary(digest, ML_HASH_MD5_DIGEST_SIZE, hex);

    ASSERT_EQ(strlen(hex), 32u, "MD5 hex string should be 32 chars");
}

static void test_hex_encode(void) {
    unsigned char binary[] = { 0x00, 0x11, 0x22, 0x33, 0xAB, 0xCD, 0xEF };
    char hex[32];

    ml_hex_encode_binary(binary, sizeof(binary), hex);
    ASSERT_EQ(strcmp(hex, "00112233abcdef"), 0,
              "hex encode should produce lowercase hex");
}

void test_md5sum_suite(void) {
    TEST_SUITE("MD5 Hash");
    RUN_TEST("MD5 empty string", test_md5_empty());
    RUN_TEST("MD5 'a'", test_md5_a());
    RUN_TEST("MD5 'abc'", test_md5_abc());
    RUN_TEST("MD5 'message digest'", test_md5_message_digest());
    RUN_TEST("incremental update", test_md5_incremental_update());
    RUN_TEST("context reuse", test_md5_context_reuse());
    RUN_TEST("large data (1000 bytes)", test_md5_large_data());
    RUN_TEST("hex encode binary", test_hex_encode());
}
