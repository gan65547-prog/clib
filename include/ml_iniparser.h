/*
 * ml_iniparser — INI configuration file parser for clib.
 *
 * Reads standard INI-format files with [section] headers and key = value
 * pairs.  Internally uses ml_dictionary_t for storage.  Keys are stored in
 * lowercase as "section:key".  Section headers are stored as "section" with
 * a NULL value.
 *
 * Supports: comments (# ;), quoted values, line continuation with backslash,
 * and boolean shortcuts (true/false, yes/no, 1/0).
 *
 * Adapted from N. Devillard's iniparser.
 */
#ifndef __ML_INIPARSER_H__
#define __ML_INIPARSER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ml_dictionary.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Set a custom error callback. Pass NULL to restore the default (stderr). */
void ml_ini_set_error_callback(int (*errback)(const char *, ...));

/* Query: number of sections in the dictionary. */
int              ml_ini_get_nsec(const ml_dictionary_t *d);
/* Query: get the name of the n-th section (0-based). */
const char      *ml_ini_get_secname(const ml_dictionary_t *d, int n);

/* Debug: dump all entries as [key]=[value]. */
void             ml_ini_dump(const ml_dictionary_t *d, FILE *f);
/* Debug: dump in INI format with sections. */
void             ml_ini_dump_ini(const ml_dictionary_t *d, FILE *f);
/* Debug: dump a single section in INI format. */
void             ml_ini_dump_section_ini(const ml_dictionary_t *d, const char *s, FILE *f);

/* Get the number of keys in a section. */
int              ml_ini_get_sec_nkeys(const ml_dictionary_t *d, const char *s);
/* Get all keys in a section. keys[] must be pre-allocated (size = nkeys). */
const char     **ml_ini_get_sec_keys(const ml_dictionary_t *d, const char *s, const char **keys);

/* Typed getters. def is returned when the key is not found. */
const char      *ml_ini_get_string(const ml_dictionary_t *d, const char *key, const char *def);
int              ml_ini_get_int(const ml_dictionary_t *d, const char *key, int notfound);
long             ml_ini_get_long(const ml_dictionary_t *d, const char *key, long notfound);
double           ml_ini_get_double(const ml_dictionary_t *d, const char *key, double notfound);
int              ml_ini_get_bool(const ml_dictionary_t *d, const char *key, int notfound);

/* Check whether an entry exists in the dictionary. Returns 1 if found. */
int              ml_ini_find_entry(const ml_dictionary_t *d, const char *entry);

/* Modify: set or remove a key. Keys are lowercased automatically. */
int              ml_ini_set(ml_dictionary_t *d, const char *entry, const char *val);
void             ml_ini_unset(ml_dictionary_t *d, const char *entry);

/* Load an INI file from disk. Returns NULL on error (syntax or I/O). */
ml_dictionary_t *ml_ini_load(const char *ininame);

/* Free a dictionary loaded by ml_ini_load(). */
void             ml_ini_free(ml_dictionary_t *d);

#ifdef __cplusplus
}
#endif

#endif /* __ML_INIPARSER_H__ */
