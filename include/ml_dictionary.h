/*
 * ml_dictionary — Hash table for string key/value pairs.
 *
 * Provides a simple hash-table with O(1) average insert, lookup, and delete.
 * Keys and values are copied internally; the caller retains ownership of
 * the passed strings.  The table automatically grows when full.
 *
 * Thread safety: NOT thread-safe.  External synchronization is required
 * if accessed from multiple threads.
 *
 * Adapted from N. Devillard's dictionary implementation.
 */
#ifndef __ML_DICTIONARY_H__
#define __ML_DICTIONARY_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ml_pool_s ml_pool_t;

typedef struct ml_dictionary_s {
    int          n;
    ssize_t      size;
    char       **val;
    char       **key;
    unsigned    *hash;
    ml_pool_t   *pool;
} ml_dictionary_t;

/* Hash function used internally. Exposed for pre-computation if needed. */
unsigned         ml_dictionary_hash(const char *key);

/* Create a new dictionary. Minimum size is 128 entries.
 * pool: optional memory pool (NULL → malloc/free). */
ml_dictionary_t *ml_dictionary_new(size_t size, ml_pool_t *pool);

/* Delete a dictionary, freeing all keys, values, and internal arrays. */
void             ml_dictionary_delete(ml_dictionary_t *d);

/* Retrieve a value by key. Returns def if key is not found or d is NULL. */
const char      *ml_dictionary_get(const ml_dictionary_t *d, const char *key, const char *def);

/* Set a key/value pair. Overwrites if key exists. Returns 0 on success, -1 on error. */
int              ml_dictionary_set(ml_dictionary_t *d, const char *key, const char *val);

/* Remove a key and its value. No-op if key is not found. */
void             ml_dictionary_unset(ml_dictionary_t *d, const char *key);

/* Debug: print all key/value pairs to the given stream. */
void             ml_dictionary_dump(const ml_dictionary_t *d, FILE *out);

#ifdef __cplusplus
}
#endif

#endif /* __ML_DICTIONARY_H__ */
