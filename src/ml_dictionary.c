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
    char     **new_val, **new_key;
    unsigned  *new_hash;

    new_val  = (char **)calloc(d->size * 2, sizeof(*d->val));
    new_key  = (char **)calloc(d->size * 2, sizeof(*d->key));
    new_hash = (unsigned *)calloc(d->size * 2, sizeof(*d->hash));
    if (!new_val || !new_key || !new_hash) {
        free(new_val);
        free(new_key);
        free(new_hash);
        return -1;
    }
    memcpy(new_val,  d->val,  d->size * sizeof(char *));
    memcpy(new_key,  d->key,  d->size * sizeof(char *));
    memcpy(new_hash, d->hash, d->size * sizeof(unsigned));
    free(d->val);
    free(d->key);
    free(d->hash);
    d->size *= 2;
    d->val  = new_val;
    d->key  = new_key;
    d->hash = new_hash;
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
    ssize_t   i;

    if (!d) return def;
    hash = ml_dictionary_hash(key);
    for (i = 0; i < d->size; i++) {
        if (!d->key[i]) continue;
        if (hash == d->hash[i] && !strcmp(key, d->key[i]))
            return d->val[i];
    }
    return def;
}

int ml_dictionary_set(ml_dictionary_t *d, const char *key, const char *val)
{
    ssize_t   i;
    unsigned  hash;

    if (!d || !key) return -1;
    hash = ml_dictionary_hash(key);

    if (d->n > 0) {
        for (i = 0; i < d->size; i++) {
            if (!d->key[i]) continue;
            if (hash == d->hash[i] && !strcmp(key, d->key[i])) {
                free(d->val[i]);
                d->val[i] = val ? dict_strdup(val) : NULL;
                return 0;
            }
        }
    }
    if (d->n == d->size) {
        if (dictionary_grow(d) != 0) return -1;
    }
    for (i = d->n; d->key[i]; ) {
        if (++i == d->size) i = 0;
    }
    d->key[i]  = dict_strdup(key);
    d->val[i]  = val ? dict_strdup(val) : NULL;
    d->hash[i] = hash;
    d->n++;
    return 0;
}

void ml_dictionary_unset(ml_dictionary_t *d, const char *key)
{
    unsigned  hash;
    ssize_t   i;

    if (!key || !d) return;
    hash = ml_dictionary_hash(key);
    for (i = 0; i < d->size; i++) {
        if (!d->key[i]) continue;
        if (hash == d->hash[i] && !strcmp(key, d->key[i]))
            break;
    }
    if (i >= d->size) return;
    free(d->key[i]);
    d->key[i] = NULL;
    free(d->val[i]);
    d->val[i]  = NULL;
    d->hash[i] = 0;
    d->n--;
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
