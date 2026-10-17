/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

/*
 * ml_dictionary — hash table for string key/value pairs (MIT-style).
 * Adapted from N. Devillard's dictionary.c.
 */

#include "ml_dictionary.h"

#define ML_DICT_MAXVALSZ  1024
#define ML_DICT_MINSZ      128

/* ---- private ---- */

static char *dict_strdup(const char *s)
{
    char *t;
    size_t len;
    if (!s) return NULL;
    len = strlen(s) + 1;
    t = (char *)malloc(len);
    if (t) memcpy(t, s, len);
    return t;
}

static int dictionary_grow(ml_dictionary_t *d)
{
    char     **old_key, **old_val;
    unsigned  *old_hash;
    ssize_t    old_size, i;

    old_size = d->size;
    old_key  = d->key;
    old_val  = d->val;
    old_hash = d->hash;

    d->size *= 2;
    d->val  = (char **)calloc(d->size, sizeof(*d->val));
    d->key  = (char **)calloc(d->size, sizeof(*d->key));
    d->hash = (unsigned *)calloc(d->size, sizeof(*d->hash));
    d->n    = 0;

    if (!d->val || !d->key || !d->hash) {
        free(d->val);
        free(d->key);
        free(d->hash);
        d->val  = old_val;
        d->key  = old_key;
        d->hash = old_hash;
        d->size = old_size;
        return -1;
    }

    /* Re-insert entries at their new hash positions */
    for (i = 0; i < old_size; i++) {
        if (old_key[i]) {
            ssize_t j, start = (ssize_t)(old_hash[i] % (unsigned)d->size);
            for (j = 0; j < d->size; j++) {
                ssize_t idx = (start + j) % d->size;
                if (!d->key[idx]) {
                    d->key[idx]  = old_key[i];
                    d->val[idx]  = old_val[i];
                    d->hash[idx] = old_hash[i];
                    d->n++;
                    break;
                }
            }
        }
    }

    free(old_key);
    free(old_val);
    free(old_hash);
    return 0;
}

/* ---- public ---- */

unsigned ml_dictionary_hash(const char *key)
{
    size_t    len;
    unsigned  hash;
    size_t    i;

    if (!key) return 0;
    len = strlen(key);
    for (hash = 0, i = 0; i < len; i++) {
        hash += (unsigned)key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

ml_dictionary_t *ml_dictionary_new(size_t size)
{
    ml_dictionary_t *d;

    if (size < ML_DICT_MINSZ) size = ML_DICT_MINSZ;
    d = (ml_dictionary_t *)calloc(1, sizeof(*d));
    if (d) {
        d->size = (ssize_t)size;
        d->val  = (char **)calloc(size, sizeof(*d->val));
        d->key  = (char **)calloc(size, sizeof(*d->key));
        d->hash = (unsigned *)calloc(size, sizeof(*d->hash));
    }
    return d;
}

void ml_dictionary_delete(ml_dictionary_t *d)
{
    ssize_t i;
    if (!d) return;
    for (i = 0; i < d->size; i++) {
        free(d->key[i]);
        free(d->val[i]);
    }
    free(d->val);
    free(d->key);
    free(d->hash);
    free(d);
}

const char *ml_dictionary_get(const ml_dictionary_t *d, const char *key, const char *def)
{
    unsigned  hash;
    ssize_t   i, start;

    if (!d || !key || d->size == 0) return def;
    hash  = ml_dictionary_hash(key);
    start = (ssize_t)(hash % (unsigned)d->size);
    for (i = 0; i < d->size; i++) {
        ssize_t idx = (start + i) % d->size;
        if (!d->key[idx]) continue;
        if (hash == d->hash[idx] && !strcmp(key, d->key[idx]))
            return d->val[idx];
    }
    return def;
}

int ml_dictionary_set(ml_dictionary_t *d, const char *key, const char *val)
{
    ssize_t    i, start;
    unsigned   hash;

    if (!d || !key || d->size == 0) return -1;
    hash  = ml_dictionary_hash(key);
    start = (ssize_t)(hash % (unsigned)d->size);

    /* Check if key already exists */
    if (d->n > 0) {
        for (i = 0; i < d->size; i++) {
            ssize_t idx = (start + i) % d->size;
            if (!d->key[idx]) continue;
            if (hash == d->hash[idx] && !strcmp(key, d->key[idx])) {
                free(d->val[idx]);
                d->val[idx] = val ? dict_strdup(val) : NULL;
                return 0;
            }
        }
    }
    if (d->n >= d->size / 2) {
        if (dictionary_grow(d) != 0) return -1;
        start = (ssize_t)(hash % (unsigned)d->size);
    }
    /* Find empty slot via linear probing */
    for (i = 0; i < d->size; i++) {
        ssize_t idx = (start + i) % d->size;
        if (!d->key[idx]) {
            d->key[idx]  = dict_strdup(key);
            d->val[idx]  = val ? dict_strdup(val) : NULL;
            d->hash[idx] = hash;
            d->n++;
            return 0;
        }
    }
    return -1;
}

void ml_dictionary_unset(ml_dictionary_t *d, const char *key)
{
    unsigned  hash;
    ssize_t   i, start;

    if (!key || !d || d->size == 0) return;
    hash  = ml_dictionary_hash(key);
    start = (ssize_t)(hash % (unsigned)d->size);
    for (i = 0; i < d->size; i++) {
        ssize_t idx = (start + i) % d->size;
        if (!d->key[idx]) continue;
        if (hash == d->hash[idx] && !strcmp(key, d->key[idx])) {
            free(d->key[idx]);
            d->key[idx] = NULL;
            free(d->val[idx]);
            d->val[idx]  = NULL;
            d->hash[idx] = 0;
            d->n--;
            return;
        }
    }
}

void ml_dictionary_dump(const ml_dictionary_t *d, FILE *out)
{
    ssize_t i;
    if (!d || !out) return;
    if (d->n < 1) {
        fprintf(out, "empty dictionary\n");
        return;
    }
    for (i = 0; i < d->size; i++) {
        if (d->key[i])
            fprintf(out, "%20s\t[%s]\n", d->key[i], d->val[i] ? d->val[i] : "UNDEF");
    }
}
