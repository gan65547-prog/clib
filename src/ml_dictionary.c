/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

/*
 * ml_dictionary — hash table for string key/value pairs (MIT-style).
 * Adapted from N. Devillard's dictionary.c.
 */

#include "ml_dictionary.h"
#include "ml_pool.h"

#define ML_DICT_MAXVALSZ  1024
#define ML_DICT_MINSZ      128

static char ml_dict_tombstone_marker;
#define ML_DICT_TOMBSTONE ((char *)&ml_dict_tombstone_marker)

/* ---- private ---- */

static void *dict_alloc(ml_pool_t *pool, size_t size)
{
    if (pool)
        return pool->f_alloc(pool, size);
    return malloc(size);
}

static void dict_free(ml_pool_t *pool, void *p)
{
    if (!p) return;
    if (pool)
        pool->f_free(pool, p);
    else
        free(p);
}

static char *dict_strdup(ml_pool_t *pool, const char *s)
{
    char *t;
    size_t len;
    if (!s) return NULL;
    len = strlen(s) + 1;
    t = (char *)dict_alloc(pool, len);
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
    d->val  = (char **)dict_alloc(d->pool, (size_t)d->size * sizeof(*d->val));
    d->key  = (char **)dict_alloc(d->pool, (size_t)d->size * sizeof(*d->key));
    d->hash = (unsigned *)dict_alloc(d->pool, (size_t)d->size * sizeof(*d->hash));
    d->n    = 0;

    if (!d->val || !d->key || !d->hash) {
        dict_free(d->pool, d->val);
        dict_free(d->pool, d->key);
        dict_free(d->pool, d->hash);
        d->val  = old_val;
        d->key  = old_key;
        d->hash = old_hash;
        d->size = old_size;
        return -1;
    }

    memset(d->val,  0, (size_t)d->size * sizeof(*d->val));
    memset(d->key,  0, (size_t)d->size * sizeof(*d->key));
    memset(d->hash, 0, (size_t)d->size * sizeof(*d->hash));

    for (i = 0; i < old_size; i++) {
        if (old_key[i] && old_key[i] != ML_DICT_TOMBSTONE) {
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

    dict_free(d->pool, old_key);
    dict_free(d->pool, old_val);
    dict_free(d->pool, old_hash);
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

ml_dictionary_t *ml_dictionary_new(size_t size, ml_pool_t *pool)
{
    ml_dictionary_t *d;

    if (size < ML_DICT_MINSZ) size = ML_DICT_MINSZ;
    d = (ml_dictionary_t *)dict_alloc(pool, sizeof(*d));
    if (d) {
        memset(d, 0, sizeof(*d));
        d->pool = pool;
        d->size = (ssize_t)size;
        d->val  = (char **)dict_alloc(pool, size * sizeof(*d->val));
        d->key  = (char **)dict_alloc(pool, size * sizeof(*d->key));
        d->hash = (unsigned *)dict_alloc(pool, size * sizeof(*d->hash));
        if (!d->val || !d->key || !d->hash) {
            dict_free(pool, d->val);
            dict_free(pool, d->key);
            dict_free(pool, d->hash);
            dict_free(pool, d);
            return NULL;
        }
        memset(d->val,  0, size * sizeof(*d->val));
        memset(d->key,  0, size * sizeof(*d->key));
        memset(d->hash, 0, size * sizeof(*d->hash));
    }
    return d;
}

void ml_dictionary_delete(ml_dictionary_t *d)
{
    ssize_t i;
    if (!d) return;
    for (i = 0; i < d->size; i++) {
        if (d->key[i] && d->key[i] != ML_DICT_TOMBSTONE)
            dict_free(d->pool, d->key[i]);
        if (d->val[i] && d->val[i] != ML_DICT_TOMBSTONE)
            dict_free(d->pool, d->val[i]);
    }
    dict_free(d->pool, d->val);
    dict_free(d->pool, d->key);
    dict_free(d->pool, d->hash);
    dict_free(d->pool, d);
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
        if (!d->key[idx]) break;
        if (d->key[idx] == ML_DICT_TOMBSTONE) continue;
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

    {
        ssize_t first_tombstone = -1;

        for (i = 0; i < d->size; i++) {
            ssize_t idx = (start + i) % d->size;
            if (!d->key[idx]) {
                if (first_tombstone < 0) first_tombstone = idx;
                break;
            }
            if (d->key[idx] == ML_DICT_TOMBSTONE) {
                if (first_tombstone < 0) first_tombstone = idx;
                continue;
            }
            if (hash == d->hash[idx] && !strcmp(key, d->key[idx])) {
                dict_free(d->pool, d->val[idx]);
                d->val[idx] = val ? dict_strdup(d->pool, val) : NULL;
                return 0;
            }
        }

        if (d->n >= d->size / 2) {
            if (dictionary_grow(d) != 0) return -1;
            start = (ssize_t)(hash % (unsigned)d->size);
            first_tombstone = -1;
            for (i = 0; i < d->size; i++) {
                ssize_t idx = (start + i) % d->size;
                if (!d->key[idx]) { if (first_tombstone < 0) first_tombstone = idx; break; }
                if (d->key[idx] == ML_DICT_TOMBSTONE) {
                    if (first_tombstone < 0) first_tombstone = idx;
                    continue;
                }
            }
        }

        if (first_tombstone >= 0) {
            d->key[first_tombstone]  = dict_strdup(d->pool, key);
            d->val[first_tombstone]  = val ? dict_strdup(d->pool, val) : NULL;
            d->hash[first_tombstone] = hash;
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
        if (!d->key[idx]) return;
        if (d->key[idx] == ML_DICT_TOMBSTONE) continue;
        if (hash == d->hash[idx] && !strcmp(key, d->key[idx])) {
            dict_free(d->pool, d->key[idx]);
            d->key[idx] = ML_DICT_TOMBSTONE;
            dict_free(d->pool, d->val[idx]);
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
        if (d->key[i] && d->key[i] != ML_DICT_TOMBSTONE)
            fprintf(out, "%20s\t[%s]\n", d->key[i], d->val[i] ? d->val[i] : "UNDEF");
    }
}
