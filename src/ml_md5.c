/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * MD5 Message-Digest Algorithm (RFC 1321).
 * Clean-room implementation from the RFC specification.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ml_md5.h"

#define MD5_BLOCK_SIZE 64

/* Round functions from RFC 1321 section 3.4. */
#define F(x, y, z)  (((x) & (y)) | ((~(x)) & (z)))
#define G(x, y, z)  (((x) & (z)) | ((y) & (~(z))))
#define H(x, y, z)  ((x) ^ (y) ^ (z))
#define I(x, y, z)  ((y) ^ ((x) | (~(z))))

#define LEFTROTATE(x, n)  (((x) << (n)) | ((x) >> (32 - (n))))

/* Per-round step macro. */
#define STEP(fn, a, b, c, d, k, s, t)       \
    do {                                      \
        (a) += fn((b), (c), (d)) + (k) + (t); \
        (a) = LEFTROTATE((a), (s));            \
        (a) += (b);                            \
    } while (0)

static const uint32_t T[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

static void md5_transform(uint32_t state[4], const uint8_t block[MD5_BLOCK_SIZE])
{
    uint32_t a, b, c, d, m[16];

    /* Decode little-endian block into 16 x uint32_t. */
    for (int i = 0; i < 16; i++) {
        m[i] = (uint32_t)block[i * 4] |
               ((uint32_t)block[i * 4 + 1] << 8) |
               ((uint32_t)block[i * 4 + 2] << 16) |
               ((uint32_t)block[i * 4 + 3] << 24);
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];

    /* Round 1 */
    STEP(F, a, b, c, d, m[ 0],  7, T[ 0]);
    STEP(F, d, a, b, c, m[ 1], 12, T[ 1]);
    STEP(F, c, d, a, b, m[ 2], 17, T[ 2]);
    STEP(F, b, c, d, a, m[ 3], 22, T[ 3]);
    STEP(F, a, b, c, d, m[ 4],  7, T[ 4]);
    STEP(F, d, a, b, c, m[ 5], 12, T[ 5]);
    STEP(F, c, d, a, b, m[ 6], 17, T[ 6]);
    STEP(F, b, c, d, a, m[ 7], 22, T[ 7]);
    STEP(F, a, b, c, d, m[ 8],  7, T[ 8]);
    STEP(F, d, a, b, c, m[ 9], 12, T[ 9]);
    STEP(F, c, d, a, b, m[10], 17, T[10]);
    STEP(F, b, c, d, a, m[11], 22, T[11]);
    STEP(F, a, b, c, d, m[12],  7, T[12]);
    STEP(F, d, a, b, c, m[13], 12, T[13]);
    STEP(F, c, d, a, b, m[14], 17, T[14]);
    STEP(F, b, c, d, a, m[15], 22, T[15]);

    /* Round 2 */
    STEP(G, a, b, c, d, m[ 1],  5, T[16]);
    STEP(G, d, a, b, c, m[ 6],  9, T[17]);
    STEP(G, c, d, a, b, m[11], 14, T[18]);
    STEP(G, b, c, d, a, m[ 0], 20, T[19]);
    STEP(G, a, b, c, d, m[ 5],  5, T[20]);
    STEP(G, d, a, b, c, m[10],  9, T[21]);
    STEP(G, c, d, a, b, m[15], 14, T[22]);
    STEP(G, b, c, d, a, m[ 4], 20, T[23]);
    STEP(G, a, b, c, d, m[ 9],  5, T[24]);
    STEP(G, d, a, b, c, m[14],  9, T[25]);
    STEP(G, c, d, a, b, m[ 3], 14, T[26]);
    STEP(G, b, c, d, a, m[ 8], 20, T[27]);
    STEP(G, a, b, c, d, m[13],  5, T[28]);
    STEP(G, d, a, b, c, m[ 2],  9, T[29]);
    STEP(G, c, d, a, b, m[ 7], 14, T[30]);
    STEP(G, b, c, d, a, m[12], 20, T[31]);

    /* Round 3 */
    STEP(H, a, b, c, d, m[ 5],  4, T[32]);
    STEP(H, d, a, b, c, m[ 8], 11, T[33]);
    STEP(H, c, d, a, b, m[11], 16, T[34]);
    STEP(H, b, c, d, a, m[14], 23, T[35]);
    STEP(H, a, b, c, d, m[ 1],  4, T[36]);
    STEP(H, d, a, b, c, m[ 4], 11, T[37]);
    STEP(H, c, d, a, b, m[ 7], 16, T[38]);
    STEP(H, b, c, d, a, m[10], 23, T[39]);
    STEP(H, a, b, c, d, m[13],  4, T[40]);
    STEP(H, d, a, b, c, m[ 0], 11, T[41]);
    STEP(H, c, d, a, b, m[ 3], 16, T[42]);
    STEP(H, b, c, d, a, m[ 6], 23, T[43]);
    STEP(H, a, b, c, d, m[ 9],  4, T[44]);
    STEP(H, d, a, b, c, m[12], 11, T[45]);
    STEP(H, c, d, a, b, m[15], 16, T[46]);
    STEP(H, b, c, d, a, m[ 2], 23, T[47]);

    /* Round 4 */
    STEP(I, a, b, c, d, m[ 0],  6, T[48]);
    STEP(I, d, a, b, c, m[ 7], 10, T[49]);
    STEP(I, c, d, a, b, m[14], 15, T[50]);
    STEP(I, b, c, d, a, m[ 5], 21, T[51]);
    STEP(I, a, b, c, d, m[12],  6, T[52]);
    STEP(I, d, a, b, c, m[ 3], 10, T[53]);
    STEP(I, c, d, a, b, m[10], 15, T[54]);
    STEP(I, b, c, d, a, m[ 1], 21, T[55]);
    STEP(I, a, b, c, d, m[ 8],  6, T[56]);
    STEP(I, d, a, b, c, m[15], 10, T[57]);
    STEP(I, c, d, a, b, m[ 6], 15, T[58]);
    STEP(I, b, c, d, a, m[13], 21, T[59]);
    STEP(I, a, b, c, d, m[ 4],  6, T[60]);
    STEP(I, d, a, b, c, m[11], 10, T[61]);
    STEP(I, c, d, a, b, m[ 2], 15, T[62]);
    STEP(I, b, c, d, a, m[ 9], 21, T[63]);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void ml_hash_md5_init(ml_hash_md5_t *ctx)
{
    ctx->priv.state[0] = 0x67452301;
    ctx->priv.state[1] = 0xefcdab89;
    ctx->priv.state[2] = 0x98badcfe;
    ctx->priv.state[3] = 0x10325476;
    ctx->priv.count = 0;
    ctx->priv.total = 0;
}

void ml_hash_md5_update(ml_hash_md5_t *ctx, const void *data, size_t length)
{
    const uint8_t *p = (const uint8_t *)data;
    size_t remaining = length;

    ctx->priv.total += length;

    if (ctx->priv.count > 0) {
        size_t fill = MD5_BLOCK_SIZE - ctx->priv.count;
        if (remaining < fill) {
            memcpy(ctx->priv.buf + ctx->priv.count, p, remaining);
            ctx->priv.count += remaining;
            return;
        }
        memcpy(ctx->priv.buf + ctx->priv.count, p, fill);
        md5_transform(ctx->priv.state, ctx->priv.buf);
        p += fill;
        remaining -= fill;
        ctx->priv.count = 0;
    }

    while (remaining >= MD5_BLOCK_SIZE) {
        md5_transform(ctx->priv.state, p);
        p += MD5_BLOCK_SIZE;
        remaining -= MD5_BLOCK_SIZE;
    }

    if (remaining > 0) {
        memcpy(ctx->priv.buf + ctx->priv.count, p, remaining);
        ctx->priv.count += remaining;
    }
}

void ml_hash_md5_finish(ml_hash_md5_t *ctx, void *output, size_t size)
{
    uint8_t  pad[MD5_BLOCK_SIZE];
    uint8_t  len_buf[8];
    uint64_t bits;
    int      i;

    (void)size;
    assert(size >= ML_HASH_MD5_DIGEST_SIZE);

    /* Padding: append bit '1' (0x80 byte) then zeros. */
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;

    bits = ctx->priv.total * 8;

    /* Encode 64-bit bit-length in little-endian. */
    for (i = 0; i < 8; i++)
        len_buf[i] = (uint8_t)(bits >> (i * 8));

    /* Pad to 56 mod 64, then append 8-byte length. */
    if (ctx->priv.count < 56) {
        ml_hash_md5_update(ctx, pad, 56 - ctx->priv.count);
    } else {
        ml_hash_md5_update(ctx, pad, MD5_BLOCK_SIZE - ctx->priv.count);
        ml_hash_md5_update(ctx, pad, 56);
    }
    ml_hash_md5_update(ctx, len_buf, 8);

    /* Encode 4 state words into output in little-endian. */
    for (i = 0; i < 4; i++) {
        ((uint8_t *)output)[i * 4 + 0] = (uint8_t)(ctx->priv.state[i]);
        ((uint8_t *)output)[i * 4 + 1] = (uint8_t)(ctx->priv.state[i] >> 8);
        ((uint8_t *)output)[i * 4 + 2] = (uint8_t)(ctx->priv.state[i] >> 16);
        ((uint8_t *)output)[i * 4 + 3] = (uint8_t)(ctx->priv.state[i] >> 24);
    }
}

void ml_hex_encode_binary(const void *input, size_t size, char *output)
{
    const unsigned char *buffer = input;

    for (size_t i = 0; i < size; i++)
        sprintf(&output[i * 2], "%02x", buffer[i]);
}
