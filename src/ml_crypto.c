/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * AES-256-GCM implementation via OpenSSL EVP API.
 */

#include "ml_crypto.h"
#include "ml_pool.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

struct ml_crypto_s {
    ml_cipher_type_t  type;
    uint8_t           key[ML_CRYPTO_KEY_LEN];
    ml_pool_t        *pool;
    char              error[256];
};

/* ── helpers ──────────────────────────────────────────────────────── */

static void
set_error(ml_crypto_t *c, const char *msg)
{
    if (c && msg) {
        strncpy(c->error, msg, sizeof(c->error) - 1);
        c->error[sizeof(c->error) - 1] = '\0';
    }
}

static void
write_le32(uint8_t *dst, uint32_t v)
{
    dst[0] = (uint8_t)(v);
    dst[1] = (uint8_t)(v >> 8);
    dst[2] = (uint8_t)(v >> 16);
    dst[3] = (uint8_t)(v >> 24);
}

static uint32_t
read_le32(const uint8_t *src)
{
    return (uint32_t)src[0]
        | ((uint32_t)src[1] << 8)
        | ((uint32_t)src[2] << 16)
        | ((uint32_t)src[3] << 24);
}

/* ── create / destroy ─────────────────────────────────────────────── */

ml_crypto_t *
ml_crypto_create(ml_cipher_type_t type,
                 const uint8_t key[ML_CRYPTO_KEY_LEN],
                 ml_pool_t *pool)
{
    ml_crypto_t *c;

    (void)type; /* only AES-256-GCM supported today */

    if (!key || !pool) return NULL;

    c = pool->f_alloc(pool, sizeof(*c));
    if (!c) return NULL;
    memset(c, 0, sizeof(*c));

    c->type = ML_CIPHER_AES_256_GCM;
    c->pool = pool;
    memcpy(c->key, key, ML_CRYPTO_KEY_LEN);
    c->error[0] = '\0';

    return c;
}

void
ml_crypto_destroy(ml_crypto_t *c)
{
    if (!c) return;
    /* zero key before free — volatile pointer prevents compiler elision */
    {
        volatile uint8_t *kp = c->key;
        size_t i;
        for (i = 0; i < ML_CRYPTO_KEY_LEN; i++) kp[i] = 0;
    }
    c->pool->f_free(c->pool, c);
}

const char *
ml_crypto_error(ml_crypto_t *c)
{
    if (!c) return "NULL crypto context";
    return c->error;
}

/* ── buffer encrypt ───────────────────────────────────────────────── */

int
ml_crypto_encrypt(ml_crypto_t *c,
                  const uint8_t *src, size_t src_len,
                  uint8_t *dst, size_t *dst_len,
                  const uint8_t *aad, size_t aad_len)
{
    EVP_CIPHER_CTX *ctx = NULL;
    uint8_t         nonce[ML_CRYPTO_NONCE_LEN];
    int             outl = 0, finl = 0, ret = ML_ERROR;

    if (!c || !src || !dst || !dst_len) {
        set_error(c, "NULL argument");
        return ML_EINVAL;
    }

    if (src_len > (size_t)INT_MAX || aad_len > (size_t)INT_MAX) {
        set_error(c, "input too large");
        return ML_EINVAL;
    }

    /* generate random nonce */
    if (RAND_bytes(nonce, sizeof(nonce)) != 1) {
        set_error(c, "RAND_bytes failed");
        return ML_ERROR;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        set_error(c, "EVP_CIPHER_CTX_new failed");
        return ML_ERROR;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, c->key, nonce) != 1) {
        set_error(c, "EVP_EncryptInit_ex failed");
        goto done;
    }

    /* AAD */
    if (aad && aad_len > 0) {
        if (EVP_EncryptUpdate(ctx, NULL, &outl, aad, (int)aad_len) != 1) {
            set_error(c, "EVP_EncryptUpdate AAD failed");
            goto done;
        }
    }

    /* build header: magic(4) + version(1) + cipher(1) + nonce(12) + tag(16) */
    write_le32(dst, ML_CRYPTO_MAGIC);
    dst[4] = 1; /* version */
    dst[5] = (uint8_t)c->type;
    memcpy(dst + 6, nonce, ML_CRYPTO_NONCE_LEN);
    /* tag will be filled at dst + 18 */

    /* encrypt payload — ciphertext goes after header */
    if (EVP_EncryptUpdate(ctx, dst + ML_CRYPTO_HEADER_SIZE, &outl,
                          src, (int)src_len) != 1) {
        set_error(c, "EVP_EncryptUpdate payload failed");
        goto done;
    }

    if (EVP_EncryptFinal_ex(ctx, dst + ML_CRYPTO_HEADER_SIZE + outl, &finl) != 1) {
        set_error(c, "EVP_EncryptFinal_ex failed");
        goto done;
    }

    /* get auth tag */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, ML_CRYPTO_TAG_LEN,
                            dst + 18) != 1) {
        set_error(c, "EVP_CTRL_GCM_GET_TAG failed");
        goto done;
    }

    *dst_len = ML_CRYPTO_HEADER_SIZE + (size_t)(outl + finl);
    ret = ML_OK;

done:
    EVP_CIPHER_CTX_free(ctx);
    return ret;
}

/* ── buffer decrypt ───────────────────────────────────────────────── */

int
ml_crypto_decrypt(ml_crypto_t *c,
                  const uint8_t *src, size_t src_len,
                  uint8_t *dst, size_t *dst_len,
                  const uint8_t *aad, size_t aad_len)
{
    EVP_CIPHER_CTX *ctx = NULL;
    const uint8_t  *nonce, *tag, *ciphertext;
    size_t          ct_len;
    uint32_t        magic;
    int             outl = 0, finl = 0, ret = ML_ERROR;

    if (!c || !src || !dst || !dst_len) {
        set_error(c, "NULL argument");
        return ML_EINVAL;
    }

    if (aad_len > (size_t)INT_MAX) {
        set_error(c, "AAD too large");
        return ML_EINVAL;
    }

    /* parse header */
    if (src_len < ML_CRYPTO_HEADER_SIZE) {
        set_error(c, "data too short for crypto header");
        return ML_EINVAL;
    }

    magic = read_le32(src);
    if (magic != ML_CRYPTO_MAGIC) {
        set_error(c, "bad crypto magic");
        return ML_EINVAL;
    }
    if (src[4] != 1) {
        set_error(c, "unsupported crypto version");
        return ML_EINVAL;
    }

    nonce      = src + 6;
    tag        = src + 18;
    ciphertext = src + ML_CRYPTO_HEADER_SIZE;
    ct_len     = src_len - ML_CRYPTO_HEADER_SIZE;

    if (ct_len > (size_t)INT_MAX) {
        set_error(c, "ciphertext too large");
        return ML_EINVAL;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        set_error(c, "EVP_CIPHER_CTX_new failed");
        return ML_ERROR;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, c->key, nonce) != 1) {
        set_error(c, "EVP_DecryptInit_ex failed");
        goto done;
    }

    /* AAD */
    if (aad && aad_len > 0) {
        if (EVP_DecryptUpdate(ctx, NULL, &outl, aad, (int)aad_len) != 1) {
            set_error(c, "EVP_DecryptUpdate AAD failed");
            goto done;
        }
    }

    /* decrypt payload */
    if (EVP_DecryptUpdate(ctx, dst, &outl, ciphertext, (int)ct_len) != 1) {
        set_error(c, "EVP_DecryptUpdate payload failed");
        goto done;
    }

    /* set expected auth tag before finalising */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, ML_CRYPTO_TAG_LEN,
                            (void *)tag) != 1) {
        set_error(c, "EVP_CTRL_GCM_SET_TAG failed");
        goto done;
    }

    /* Finalise — returns <=0 if tag mismatch (tampered ciphertext) */
    if (EVP_DecryptFinal_ex(ctx, dst + outl, &finl) <= 0) {
        set_error(c, "auth tag mismatch — ciphertext tampered");
        ret = ML_ECRYPTO;
        /* zero the partially-decrypted output — it's garbage */
        memset(dst, 0, outl);
        goto done;
    }

    *dst_len = (size_t)(outl + finl);
    ret = ML_OK;

done:
    EVP_CIPHER_CTX_free(ctx);
    return ret;
}

/* ── file encrypt ─────────────────────────────────────────────────── */

int
ml_crypto_encrypt_file(ml_crypto_t *c,
                       const void *data, size_t len,
                       const char *path,
                       const uint8_t *aad, size_t aad_len)
{
    uint8_t *enc_buf = NULL;
    size_t   enc_len = 0;
    FILE    *fp      = NULL;

    if (!c || !data || !path) {
        set_error(c, "NULL argument");
        return ML_EINVAL;
    }

    enc_buf = c->pool->f_alloc(c->pool, len + ML_CRYPTO_HEADER_SIZE);
    if (!enc_buf) {
        set_error(c, "allocation failed");
        return ML_ENOMEM;
    }

    if (ml_crypto_encrypt(c, data, len, enc_buf, &enc_len, aad, aad_len) != ML_OK) {
        c->pool->f_free(c->pool, enc_buf);
        return ML_ERROR;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        set_error(c, "cannot open output file");
        c->pool->f_free(c->pool, enc_buf);
        return ML_ERROR;
    }

    if (fwrite(enc_buf, 1, enc_len, fp) != enc_len) {
        set_error(c, "file write failed");
        fclose(fp);
        c->pool->f_free(c->pool, enc_buf);
        return ML_ERROR;
    }

    fclose(fp);
    c->pool->f_free(c->pool, enc_buf);
    return ML_OK;
}

/* ── file decrypt ─────────────────────────────────────────────────── */

int
ml_crypto_decrypt_file(ml_crypto_t *c,
                       const char *path,
                       void **out_data, size_t *out_len,
                       ml_pool_t *pool)
{
    uint8_t *enc_buf = NULL;
    uint8_t *dec_buf = NULL;
    size_t   enc_len, dec_len = 0;
    long     fsize;
    FILE    *fp = NULL;
    int      ret = ML_ERROR;

    if (!c || !path || !out_data || !out_len || !pool) {
        set_error(c, "NULL argument");
        return ML_EINVAL;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        set_error(c, "cannot open input file");
        return ML_ERROR;
    }

    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize < (long)ML_CRYPTO_HEADER_SIZE) {
        set_error(c, "file too small");
        fclose(fp);
        return ML_EINVAL;
    }

    enc_buf = c->pool->f_alloc(c->pool, (size_t)fsize);
    if (!enc_buf) {
        set_error(c, "allocation failed");
        fclose(fp);
        return ML_ENOMEM;
    }

    enc_len = fread(enc_buf, 1, (size_t)fsize, fp);
    fclose(fp);

    if (enc_len != (size_t)fsize) {
        set_error(c, "file read error");
        c->pool->f_free(c->pool, enc_buf);
        return ML_ERROR;
    }

    /* allocate decryption buffer from caller's pool */
    dec_buf = pool->f_alloc(pool, enc_len);
    if (!dec_buf) {
        set_error(c, "allocation failed");
        c->pool->f_free(c->pool, enc_buf);
        return ML_ENOMEM;
    }

    ret = ml_crypto_decrypt(c, enc_buf, enc_len, dec_buf, &dec_len, NULL, 0);
    c->pool->f_free(c->pool, enc_buf);

    if (ret != ML_OK) {
        pool->f_free(pool, dec_buf);
        return ret;
    }

    *out_data = dec_buf;
    *out_len  = dec_len;
    return ML_OK;
}
