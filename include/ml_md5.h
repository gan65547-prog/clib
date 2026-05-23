/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef __ML_MD5_H__
#define __ML_MD5_H__
#include <stdint.h>


#define ml_hash_finish_hex(ctx, output)                             \
    do {                                                            \
        char out_tmp[_Generic((ctx),                                \
            ml_hash_md5_t *: ML_HASH_MD5_DIGEST_SIZE)];           \
        _Generic((ctx),                                             \
            ml_hash_md5_t *: ml_hash_md5_finish)                  \
        (ctx, out_tmp, sizeof(out_tmp));                            \
        ml_hex_encode_binary(out_tmp, sizeof(out_tmp), output);    \
    } while (0)



/**
 * MD5 digest output size
 */
#define ML_HASH_MD5_DIGEST_SIZE 16

/**
 * MD5 digest hex representation size
 */
#define ML_HASH_MD5_DIGEST_HEX_SIZE 33 // 2 chars per byte + null


/**
 * MD5 hash context
 */
typedef struct ml_hash_md5_ctx
{
    struct ml_md5_s {
        uint32_t state[4];  /* A, B, C, D chaining variables */
        uint64_t total;     /* total bytes processed */
        uint8_t  buf[64];   /* input buffer */
        int      count;     /* bytes buffered */
    } priv;
} ml_hash_md5_t;

/* Hashing */
typedef struct ml_hash_md5_ctx ml_hash_md5_t;


void ml_hex_encode_binary(const void *input, size_t size, char *output);

void ml_hash_md5_init(ml_hash_md5_t *ctx);

/**
 * Update MD5 hash computation with new data
 * 
 * Updates the context with provided data which is used for the hash
 * calculation. Can be called repeatedly with new data. The final
 * hash represents the hash for the concatenation of all data.
 * 
 * \param[in,out] ctx    MD5 hash context to update
 * \param         data   Data to add
 * \param         size   Size of the data to add
 */
void ml_hash_md5_update(ml_hash_md5_t *ctx, const void *data, size_t size);

/**
 * Finish MD5 hash computation
 * 
 * Finishes the MD5 hash computation and provides the hash for the
 * concatenation of all provided data by previous calls to \ref ml_hash_md5_update.
 * The result is written to the buffer pointed to by output, which must be at
 * least \ref ML_HASH_MD5_DIGEST_SIZE big.
 * 
 * \param[in,out] ctx    MD5 hash context to finish
 * \param[out]    output Output buffer to write to
 * \param         size   Output buffer size
 */
void ml_hash_md5_finish(ml_hash_md5_t *ctx, void *output, size_t size);



#endif

