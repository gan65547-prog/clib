/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * AES-256-GCM crypto tests — round-trip, tamper detection, file I/O, AAD.
 */
#include "test_framework.h"
#include "../include/ml_crypto.h"
#include "../include/ml_pool.h"
#include "../src/ml_crypto.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 32-byte test key */
static const uint8_t test_key[ML_CRYPTO_KEY_LEN] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
};

static ml_pool_t g_pool;

/* ── setup / teardown ─────────────────────────────────────────────── */

static ml_crypto_t *create_ctx(void)
{
    return ml_crypto_create(ML_CIPHER_AES_256_GCM, test_key, &g_pool);
}

/* ── round-trip tests ─────────────────────────────────────────────── */

static void test_roundtrip_small(void)
{
    ml_crypto_t *c = create_ctx();
    const uint8_t plain[] = "Hello, World!";
    uint8_t enc[sizeof(plain) + ML_CRYPTO_HEADER_SIZE];
    uint8_t dec[sizeof(plain)];
    size_t elen = 0, dlen = 0;

    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");

    ASSERT_EQ(ml_crypto_encrypt(c, plain, sizeof(plain), enc, &elen, NULL, 0),
              ML_OK, "encrypt small");
    ASSERT(elen >= ML_CRYPTO_HEADER_SIZE + sizeof(plain), "encrypt output size");

    ASSERT_EQ(ml_crypto_decrypt(c, enc, elen, dec, &dlen, NULL, 0),
              ML_OK, "decrypt small");
    ASSERT_EQ(dlen, sizeof(plain), "decrypt output size matches");
    ASSERT(memcmp(plain, dec, sizeof(plain)) == 0, "round-trip data matches");

    ml_crypto_destroy(c);
}

static void test_roundtrip_1mb(void)
{
    ml_crypto_t *c = create_ctx();
    const size_t  size = 1024 * 1024;
    uint8_t      *plain, *enc, *dec;
    size_t        elen = 0, dlen = 0;
    size_t        i;

    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");

    plain = g_pool.f_alloc(&g_pool, size);
    enc   = g_pool.f_alloc(&g_pool, size + ML_CRYPTO_HEADER_SIZE);
    dec   = g_pool.f_alloc(&g_pool, size);

    ASSERT_PTR_NOT_NULL(plain, "alloc plain");
    ASSERT_PTR_NOT_NULL(enc,   "alloc enc");
    ASSERT_PTR_NOT_NULL(dec,   "alloc dec");

    /* fill with repeatable pattern */
    for (i = 0; i < size; i++) plain[i] = (uint8_t)(i * 7 + 13);

    ASSERT_EQ(ml_crypto_encrypt(c, plain, size, enc, &elen, NULL, 0),
              ML_OK, "encrypt 1MB");
    ASSERT_EQ(ml_crypto_decrypt(c, enc, elen, dec, &dlen, NULL, 0),
              ML_OK, "decrypt 1MB");
    ASSERT_EQ(dlen, size, "decrypt size matches");
    ASSERT(memcmp(plain, dec, size) == 0, "1MB round-trip data matches");

    g_pool.f_free(&g_pool, plain);
    g_pool.f_free(&g_pool, enc);
    g_pool.f_free(&g_pool, dec);
    ml_crypto_destroy(c);
}

static void test_roundtrip_empty(void)
{
    ml_crypto_t *c = create_ctx();
    const uint8_t plain[] = "";
    uint8_t enc[ML_CRYPTO_HEADER_SIZE + 1];
    uint8_t dec[1];
    size_t elen = 0, dlen = 0;

    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");

    ASSERT_EQ(ml_crypto_encrypt(c, plain, 0, enc, &elen, NULL, 0),
              ML_OK, "encrypt empty");

    ASSERT_EQ(ml_crypto_decrypt(c, enc, elen, dec, &dlen, NULL, 0),
              ML_OK, "decrypt empty");
    ASSERT_EQ(dlen, (size_t)0, "decrypt empty gives 0 bytes");

    ml_crypto_destroy(c);
}

/* ── tamper detection ─────────────────────────────────────────────── */

static void test_tamper_ciphertext(void)
{
    ml_crypto_t *c = create_ctx();
    const uint8_t plain[] = "sensitive model data 123456";
    uint8_t enc[sizeof(plain) + ML_CRYPTO_HEADER_SIZE];
    uint8_t dec[sizeof(plain)];
    size_t elen = 0, dlen = 0;

    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");
    ASSERT_EQ(ml_crypto_encrypt(c, plain, sizeof(plain), enc, &elen, NULL, 0),
              ML_OK, "encrypt");

    /* flip one bit in ciphertext */
    enc[ML_CRYPTO_HEADER_SIZE + 3] ^= 0x01;

    ASSERT_EQ(ml_crypto_decrypt(c, enc, elen, dec, &dlen, NULL, 0),
              ML_ECRYPTO, "tampered ciphertext → ML_ECRYPTO");

    ml_crypto_destroy(c);
}

static void test_wrong_key(void)
{
    ml_crypto_t *c1, *c2;
    uint8_t wrong_key[ML_CRYPTO_KEY_LEN];
    const uint8_t plain[] = "top secret payload";
    uint8_t enc[sizeof(plain) + ML_CRYPTO_HEADER_SIZE];
    uint8_t dec[sizeof(plain)];
    size_t elen = 0, dlen = 0;

    c1 = create_ctx();
    ASSERT_PTR_NOT_NULL(c1, "create c1");

    ASSERT_EQ(ml_crypto_encrypt(c1, plain, sizeof(plain), enc, &elen, NULL, 0),
              ML_OK, "encrypt");

    /* create second context with different key */
    memset(wrong_key, 0xFF, sizeof(wrong_key));
    c2 = ml_crypto_create(ML_CIPHER_AES_256_GCM, wrong_key, &g_pool);
    ASSERT_PTR_NOT_NULL(c2, "create c2 with wrong key");

    ASSERT_EQ(ml_crypto_decrypt(c2, enc, elen, dec, &dlen, NULL, 0),
              ML_ECRYPTO, "wrong key → ML_ECRYPTO");

    ml_crypto_destroy(c1);
    ml_crypto_destroy(c2);
}

static void test_tamper_tag(void)
{
    ml_crypto_t *c = create_ctx();
    const uint8_t plain[] = "another secret";
    uint8_t enc[sizeof(plain) + ML_CRYPTO_HEADER_SIZE];
    uint8_t dec[sizeof(plain)];
    size_t elen = 0, dlen = 0;

    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");
    ASSERT_EQ(ml_crypto_encrypt(c, plain, sizeof(plain), enc, &elen, NULL, 0),
              ML_OK, "encrypt");

    /* flip one bit in auth tag (bytes 18-33) */
    enc[20] ^= 0x01;

    ASSERT_EQ(ml_crypto_decrypt(c, enc, elen, dec, &dlen, NULL, 0),
              ML_ECRYPTO, "tampered tag → ML_ECRYPTO");

    ml_crypto_destroy(c);
}

/* ── header validation ────────────────────────────────────────────── */

static void test_bad_magic(void)
{
    ml_crypto_t *c = create_ctx();
    const uint8_t plain[] = "test";
    uint8_t enc[sizeof(plain) + ML_CRYPTO_HEADER_SIZE];
    uint8_t dec[sizeof(plain)];
    size_t elen = 0, dlen = 0;

    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");
    ASSERT_EQ(ml_crypto_encrypt(c, plain, sizeof(plain), enc, &elen, NULL, 0),
              ML_OK, "encrypt");

    /* corrupt magic bytes */
    enc[0] = 0xDE;
    enc[1] = 0xAD;

    ASSERT_EQ(ml_crypto_decrypt(c, enc, elen, dec, &dlen, NULL, 0),
              ML_EINVAL, "bad magic → ML_EINVAL");

    ml_crypto_destroy(c);
}

static void test_bad_version(void)
{
    ml_crypto_t *c = create_ctx();
    const uint8_t plain[] = "test";
    uint8_t enc[sizeof(plain) + ML_CRYPTO_HEADER_SIZE];
    uint8_t dec[sizeof(plain)];
    size_t elen = 0, dlen = 0;

    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");
    ASSERT_EQ(ml_crypto_encrypt(c, plain, sizeof(plain), enc, &elen, NULL, 0),
              ML_OK, "encrypt");

    /* corrupt version byte */
    enc[4] = 99;

    ASSERT_EQ(ml_crypto_decrypt(c, enc, elen, dec, &dlen, NULL, 0),
              ML_EINVAL, "bad version → ML_EINVAL");

    ml_crypto_destroy(c);
}

static void test_data_too_short(void)
{
    ml_crypto_t *c = create_ctx();
    uint8_t dec[1];
    size_t dlen = 0;
    uint8_t short_buf[10] = {0};

    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");
    ASSERT_EQ(ml_crypto_decrypt(c, short_buf, sizeof(short_buf), dec, &dlen, NULL, 0),
              ML_EINVAL, "too short → ML_EINVAL");

    ml_crypto_destroy(c);
}

/* ── AAD tests ────────────────────────────────────────────────────── */

static void test_aad_roundtrip(void)
{
    ml_crypto_t *c = create_ctx();
    const uint8_t plain[] = "model data";
    const uint8_t aad[]   = "device=rk3588-cx&model=v2";
    uint8_t enc[sizeof(plain) + ML_CRYPTO_HEADER_SIZE];
    uint8_t dec[sizeof(plain)];
    size_t elen = 0, dlen = 0;

    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");
    ASSERT_EQ(ml_crypto_encrypt(c, plain, sizeof(plain), enc, &elen, aad, sizeof(aad)),
              ML_OK, "encrypt with AAD");

    ASSERT_EQ(ml_crypto_decrypt(c, enc, elen, dec, &dlen, aad, sizeof(aad)),
              ML_OK, "decrypt with same AAD");
    ASSERT(memcmp(plain, dec, sizeof(plain)) == 0, "AAD round-trip matches");

    ml_crypto_destroy(c);
}

static void test_aad_mismatch(void)
{
    ml_crypto_t *c = create_ctx();
    const uint8_t plain[] = "model data";
    const uint8_t aad_enc[] = "device=rk3588-cx&model=v2";
    const uint8_t aad_dec[] = "device=rk3588-cx&model=v3"; /* different version */
    uint8_t enc[sizeof(plain) + ML_CRYPTO_HEADER_SIZE];
    uint8_t dec[sizeof(plain)];
    size_t elen = 0, dlen = 0;

    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");
    ASSERT_EQ(ml_crypto_encrypt(c, plain, sizeof(plain), enc, &elen, aad_enc, strlen((char*)aad_enc)),
              ML_OK, "encrypt with AAD");

    ASSERT_EQ(ml_crypto_decrypt(c, enc, elen, dec, &dlen, aad_dec, strlen((char*)aad_dec)),
              ML_ECRYPTO, "AAD mismatch → ML_ECRYPTO");

    ml_crypto_destroy(c);
}

/* ── file I/O tests ───────────────────────────────────────────────── */

static void test_file_roundtrip(void)
{
    ml_crypto_t *c = create_ctx();
    const uint8_t plain[] = "This is a model file content.\nWith multiple lines.\n";
    const char   *path   = "/tmp/test_crypto_file.bin";
    void         *out    = NULL;
    size_t        out_len = 0;

    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");

    ASSERT_EQ(ml_crypto_encrypt_file(c, plain, sizeof(plain), path, NULL, 0),
              ML_OK, "encrypt file");

    ASSERT_EQ(ml_crypto_decrypt_file(c, path, &out, &out_len, &g_pool),
              ML_OK, "decrypt file");

    ASSERT_EQ(out_len, sizeof(plain), "file decrypt size matches");
    ASSERT(memcmp(plain, out, sizeof(plain)) == 0, "file round-trip matches");

    g_pool.f_free(&g_pool, out);
    remove(path);
    ml_crypto_destroy(c);
}

static void test_file_bad_magic(void)
{
    ml_crypto_t *c = create_ctx();
    void *out = NULL;
    size_t out_len = 0;
    FILE *fp;

    /* write a file with bad magic */
    fp = fopen("/tmp/test_crypto_bad.bin", "wb");
    ASSERT_PTR_NOT_NULL(fp, "create test file");
    uint8_t bad[64] = {0xBA, 0xAD, 0xF0, 0x0D}; /* bad magic */
    fwrite(bad, 1, sizeof(bad), fp);
    fclose(fp);

    ASSERT_EQ(ml_crypto_decrypt_file(c, "/tmp/test_crypto_bad.bin", &out, &out_len, &g_pool),
              ML_EINVAL, "bad file magic → ML_EINVAL");

    remove("/tmp/test_crypto_bad.bin");
    ml_crypto_destroy(c);
}

/* ── error message ────────────────────────────────────────────────── */

static void test_error_message(void)
{
    ml_crypto_t *c = create_ctx();
    ASSERT_PTR_NOT_NULL(c, "create crypto ctx");

    /* trigger an error */
    uint8_t dec[10];
    size_t dlen = 0;
    uint8_t junk[100] = {0};
    ml_crypto_decrypt(c, junk, sizeof(junk), dec, &dlen, NULL, 0);

    const char *err = ml_crypto_error(c);
    ASSERT_PTR_NOT_NULL(err, "error string not NULL");
    ASSERT(strlen(err) > 0, "error string non-empty");

    ml_crypto_destroy(c);
}

/* ── suite entry ──────────────────────────────────────────────────── */

void test_crypto_suite(void)
{
    ml_pool_create(&g_pool, 0);

    TEST_SUITE("ml_crypto — AES-256-GCM");

    RUN_TEST("roundtrip_small",    test_roundtrip_small());
    RUN_TEST("roundtrip_1mb",      test_roundtrip_1mb());
    RUN_TEST("roundtrip_empty",    test_roundtrip_empty());
    RUN_TEST("tamper_ciphertext",  test_tamper_ciphertext());
    RUN_TEST("wrong_key",          test_wrong_key());
    RUN_TEST("tamper_tag",         test_tamper_tag());
    RUN_TEST("bad_magic",          test_bad_magic());
    RUN_TEST("bad_version",        test_bad_version());
    RUN_TEST("data_too_short",     test_data_too_short());
    RUN_TEST("aad_roundtrip",      test_aad_roundtrip());
    RUN_TEST("aad_mismatch",       test_aad_mismatch());
    RUN_TEST("file_roundtrip",     test_file_roundtrip());
    RUN_TEST("file_bad_magic",     test_file_bad_magic());
    RUN_TEST("error_message",      test_error_message());

    ml_pool_destroy(&g_pool);
}
