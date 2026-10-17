/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_crypto — AES-256-GCM authenticated encryption (AEAD).
 *
 * Provides confidentiality + integrity via AES-256-GCM with 12-byte nonce
 * and 16-byte auth tag.  Supports Additional Authenticated Data (AAD) to
 * bind ciphertext to device/model metadata, preventing model-file swapping.
 *
 * File format (.enc):
 *   [ 4 bytes] magic:   "MLEN" (0x4D4C454E)
 *   [ 1 byte ] version: 1
 *   [ 1 byte ] cipher:  ML_CIPHER_AES_256_GCM
 *   [12 bytes] nonce:   randomly generated per encryption
 *   [16 bytes] auth_tag: GCM output
 *   [ N bytes] ciphertext
 */

#ifndef __ML_CRYPTO_H__
#define __ML_CRYPTO_H__

#include <stdint.h>
#include <stddef.h>

/* ── constants ────────────────────────────────────────────────────── */

#define ML_CRYPTO_MAGIC      0x4D4C454Eu   /* "MLEN" */
#define ML_CRYPTO_NONCE_LEN  12
#define ML_CRYPTO_TAG_LEN    16
#define ML_CRYPTO_KEY_LEN    32            /* AES-256 */

/* header: magic(4) + version(1) + cipher(1) + nonce(12) + tag(16) = 34 */
#define ML_CRYPTO_HEADER_SIZE 34

/* ── types ────────────────────────────────────────────────────────── */

typedef enum {
    ML_CIPHER_AES_256_GCM = 0,
} ml_cipher_type_t;

typedef struct ml_crypto_s ml_crypto_t;
typedef struct ml_pool_s   ml_pool_t;

/* ── return codes ─────────────────────────────────────────────────── */

#define ML_OK       0
#define ML_ERROR   -1
#define ML_ECRYPTO -2   /* auth-tag mismatch — ciphertext tampered */
#define ML_EINVAL  -3   /* invalid argument or bad file header     */
#define ML_ENOMEM  -4   /* allocation failed                       */

/* ── API ──────────────────────────────────────────────────────────── */

/* create / destroy */
ml_crypto_t *ml_crypto_create(ml_cipher_type_t type,
                              const uint8_t key[ML_CRYPTO_KEY_LEN],
                              ml_pool_t *pool);
void         ml_crypto_destroy(ml_crypto_t *c);

/* buffer encrypt — dst must be src_len + ML_CRYPTO_HEADER_SIZE bytes.
 * nonce is randomly generated internally.
 * on output *dst_len is set to the actual output size. */
int ml_crypto_encrypt(ml_crypto_t *c,
                      const uint8_t *src, size_t src_len,
                      uint8_t *dst, size_t *dst_len,
                      const uint8_t *aad, size_t aad_len);

/* buffer decrypt — parses header (nonce/tag), verifies auth tag.
 * returns ML_OK on success, ML_ECRYPTO on tag mismatch. */
int ml_crypto_decrypt(ml_crypto_t *c,
                      const uint8_t *src, size_t src_len,
                      uint8_t *dst, size_t *dst_len,
                      const uint8_t *aad, size_t aad_len);

/* file encrypt — writes .enc with header, internal random nonce */
int ml_crypto_encrypt_file(ml_crypto_t *c,
                           const void *data, size_t len,
                           const char *path,
                           const uint8_t *aad, size_t aad_len);

/* file decrypt — reads .enc, parses header, GCM decrypt + verify tag.
 * allocates *out_data from pool, caller must free with pool->f_free.
 * returns ML_OK / ML_ECRYPTO (tag mismatch) / ML_EINVAL (bad header). */
int ml_crypto_decrypt_file(ml_crypto_t *c,
                           const char *path,
                           void **out_data, size_t *out_len,
                           ml_pool_t *pool);

const char *ml_crypto_error(ml_crypto_t *c);

#endif /* __ML_CRYPTO_H__ */
