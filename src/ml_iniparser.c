/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

/*
 * ml_iniparser — INI file parser for clib (MIT-style).
 * Adapted from N. Devillard's iniparser.c.
 *
 * Sections stored as "section" keys with NULL value.
 * Key/value pairs stored as "section:key" = "value".
 */

#include <ctype.h>
#include <stdarg.h>
#include "ml_iniparser.h"

#define ASCIILINESZ  1024
#define INI_INVALID_KEY  ((char *)-1)

typedef enum {
    LINE_UNPROCESSED,
    LINE_ERROR,
    LINE_EMPTY,
    LINE_COMMENT,
    LINE_SECTION,
    LINE_VALUE
} line_status_t;

/* ---- private helpers ---- */

static const char *strlwc(const char *in, char *out, unsigned len)
{
    unsigned i;
    if (!in || !out || len == 0) return NULL;
    for (i = 0; in[i] != '\0' && i < len - 1; i++)
        out[i] = (char)tolower((int)in[i]);
    out[i] = '\0';
    return out;
}

static char *xstrdup(const char *s)
{
    char *t;
    size_t len;
    if (!s) return NULL;
    len = strlen(s) + 1;
    t = (char *)malloc(len);
    if (t) memcpy(t, s, len);
    return t;
}

static unsigned strstrip(char *s)
{
    char *last, *dest = s;
    if (!s) return 0;
    last = s + strlen(s);
    while (isspace((int)*s) && *s) s++;
    while (last > s && isspace((int)*(last - 1))) last--;
    *last = (char)0;
    memmove(dest, s, last - s + 1);
    return (unsigned)(last - s);
}

static int default_error_callback(const char *format, ...)
{
    int ret;
    va_list argptr;
    va_start(argptr, format);
    ret = vfprintf(stderr, format, argptr);
    va_end(argptr);
    return ret;
}

static int (*ini_error_callback)(const char *, ...) = NULL;

void ml_ini_set_error_callback(int (*errback)(const char *, ...))
{
    ini_error_callback = errback ? errback : default_error_callback;
}

static void ini_error(const char *fmt, ...)
{
    va_list argptr;
    if (!ini_error_callback) ini_error_callback = default_error_callback;
    va_start(argptr, fmt);
    ini_error_callback(fmt, argptr);
    va_end(argptr);
}

/* ---- query ---- */

int ml_ini_get_nsec(const ml_dictionary_t *d)
{
    int i, nsec;
    if (!d) return -1;
    nsec = 0;
    for (i = 0; i < d->size; i++) {
        if (!d->key[i]) continue;
        if (!strchr(d->key[i], ':')) nsec++;
    }
    return nsec;
}

const char *ml_ini_get_secname(const ml_dictionary_t *d, int n)
{
    int i, foundsec;
    if (!d || n < 0) return NULL;
    foundsec = 0;
    for (i = 0; i < d->size; i++) {
        if (!d->key[i]) continue;
        if (!strchr(d->key[i], ':')) {
            if (foundsec >= n) break;
            foundsec++;
        }
    }
    return (i < d->size) ? d->key[i] : NULL;
}

/* ---- dump/debug ---- */

void ml_ini_dump(const ml_dictionary_t *d, FILE *f)
{
    int i;
    if (!d || !f) return;
    for (i = 0; i < d->size; i++) {
        if (!d->key[i]) continue;
        fprintf(f, "[%s]=[%s]\n", d->key[i], d->val[i] ? d->val[i] : "UNDEF");
    }
}

void ml_ini_dump_ini(const ml_dictionary_t *d, FILE *f)
{
    int i, nsec;
    const char *secname;
    if (!d || !f) return;
    nsec = ml_ini_get_nsec(d);
    if (nsec < 1) {
        for (i = 0; i < d->size; i++) {
            if (!d->key[i]) continue;
            fprintf(f, "%s = %s\n", d->key[i], d->val[i]);
        }
        return;
    }
    for (i = 0; i < nsec; i++) {
        secname = ml_ini_get_secname(d, i);
        ml_ini_dump_section_ini(d, secname, f);
    }
    fprintf(f, "\n");
}

void ml_ini_dump_section_ini(const ml_dictionary_t *d, const char *s, FILE *f)
{
    int j, seclen;
    char keym[ASCIILINESZ + 1];
    if (!d || !f) return;
    if (!ml_ini_find_entry(d, s)) return;
    seclen = (int)strlen(s);
    fprintf(f, "\n[%s]\n", s);
    snprintf(keym, sizeof(keym), "%s:", s);
    for (j = 0; j < d->size; j++) {
        if (!d->key[j]) continue;
        if (!strncmp(d->key[j], keym, seclen + 1))
            fprintf(f, "%-30s = %s\n", d->key[j] + seclen + 1,
                    d->val[j] ? d->val[j] : "");
    }
    fprintf(f, "\n");
}

int ml_ini_get_sec_nkeys(const ml_dictionary_t *d, const char *s)
{
    int seclen, nkeys, j;
    char keym[ASCIILINESZ + 1];
    nkeys = 0;
    if (!d || !ml_ini_find_entry(d, s)) return 0;
    seclen = (int)strlen(s);
    strlwc(s, keym, sizeof(keym));
    keym[seclen] = ':';
    for (j = 0; j < d->size; j++) {
        if (!d->key[j]) continue;
        if (!strncmp(d->key[j], keym, seclen + 1)) nkeys++;
    }
    return nkeys;
}

const char **ml_ini_get_sec_keys(const ml_dictionary_t *d, const char *s, const char **keys)
{
    int i, j, seclen;
    char keym[ASCIILINESZ + 1];
    if (!d || !keys || !ml_ini_find_entry(d, s)) return NULL;
    seclen = (int)strlen(s);
    strlwc(s, keym, sizeof(keym));
    keym[seclen] = ':';
    i = 0;
    for (j = 0; j < d->size; j++) {
        if (!d->key[j]) continue;
        if (!strncmp(d->key[j], keym, seclen + 1)) keys[i++] = d->key[j];
    }
    return keys;
}

/* ---- typed getters ---- */

const char *ml_ini_get_string(const ml_dictionary_t *d, const char *key, const char *def)
{
    char tmp[ASCIILINESZ + 1];
    if (!d || !key) return def;
    return ml_dictionary_get(d, strlwc(key, tmp, sizeof(tmp)), def);
}

long ml_ini_get_long(const ml_dictionary_t *d, const char *key, long notfound)
{
    const char *str = ml_ini_get_string(d, key, INI_INVALID_KEY);
    if (str == INI_INVALID_KEY) return notfound;
    return strtol(str, NULL, 0);
}

int ml_ini_get_int(const ml_dictionary_t *d, const char *key, int notfound)
{
    return (int)ml_ini_get_long(d, key, notfound);
}

double ml_ini_get_double(const ml_dictionary_t *d, const char *key, double notfound)
{
    const char *str = ml_ini_get_string(d, key, INI_INVALID_KEY);
    if (str == INI_INVALID_KEY) return notfound;
    return atof(str);
}

int ml_ini_get_bool(const ml_dictionary_t *d, const char *key, int notfound)
{
    const char *c = ml_ini_get_string(d, key, INI_INVALID_KEY);
    if (c == INI_INVALID_KEY) return notfound;
    if (c[0] == 'y' || c[0] == 'Y' || c[0] == '1' || c[0] == 't' || c[0] == 'T') return 1;
    if (c[0] == 'n' || c[0] == 'N' || c[0] == '0' || c[0] == 'f' || c[0] == 'F') return 0;
    return notfound;
}

/* ---- modify ---- */

int ml_ini_find_entry(const ml_dictionary_t *d, const char *entry)
{
    return (ml_ini_get_string(d, entry, INI_INVALID_KEY) != INI_INVALID_KEY) ? 1 : 0;
}

int ml_ini_set(ml_dictionary_t *d, const char *entry, const char *val)
{
    char tmp[ASCIILINESZ + 1];
    return ml_dictionary_set(d, strlwc(entry, tmp, sizeof(tmp)), val);
}

void ml_ini_unset(ml_dictionary_t *d, const char *entry)
{
    char tmp[ASCIILINESZ + 1];
    ml_dictionary_unset(d, strlwc(entry, tmp, sizeof(tmp)));
}

/* ---- line parser ---- */

static line_status_t ini_parse_line(const char *input_line, char *section, char *key, char *value)
{
    line_status_t sta;
    char *line;
    size_t len;

    line = xstrdup(input_line);
    len  = strstrip(line);
    sta  = LINE_UNPROCESSED;

    if (len < 1) {
        sta = LINE_EMPTY;
    } else if (line[0] == '#' || line[0] == ';') {
        sta = LINE_COMMENT;
    } else if (line[0] == '[' && line[len - 1] == ']') {
        sscanf(line, "[%1024[^]]", section);
        strstrip(section);
        strlwc(section, section, (unsigned)len);
        sta = LINE_SECTION;
    } else if (sscanf(line, "%1024[^=] = \"%1024[^\"]\"", key, value) == 2 ||
               sscanf(line, "%1024[^=] = '%1024[^\']'",   key, value) == 2) {
        strstrip(key);
        strlwc(key, key, (unsigned)len);
        sta = LINE_VALUE;
    } else if (sscanf(line, "%1024[^=] = %1024[^;#]", key, value) == 2) {
        strstrip(key);
        strlwc(key, key, (unsigned)len);
        strstrip(value);
        if (!strcmp(value, "\"\"") || !strcmp(value, "''")) value[0] = 0;
        sta = LINE_VALUE;
    } else if (sscanf(line, "%1024[^=] = %1024[^;#]", key, value) == 2 ||
               sscanf(line, "%1024[^=] %1024[=]", key, value) == 2) {
        strstrip(key);
        strlwc(key, key, (unsigned)len);
        value[0] = 0;
        sta = LINE_VALUE;
    } else {
        sta = LINE_ERROR;
    }
    free(line);
    return sta;
}

/* ---- load / free ---- */

ml_dictionary_t *ml_ini_load(const char *ininame)
{
    FILE *in;
    char line[ASCIILINESZ + 1];
    char section[ASCIILINESZ + 1];
    char key[ASCIILINESZ + 1];
    char tmp[(ASCIILINESZ * 2) + 1];
    char val[ASCIILINESZ + 1];
    int last = 0, len, lineno = 0, errs = 0, mem_err = 0;
    ml_dictionary_t *dict;

    in = fopen(ininame, "r");
    if (!in) {
        ini_error("ml_ini: cannot open %s\n", ininame);
        return NULL;
    }
    dict = ml_dictionary_new(0, NULL);
    if (!dict) { fclose(in); return NULL; }

    memset(line, 0, sizeof(line));
    memset(section, 0, sizeof(section));
    memset(key, 0, sizeof(key));
    memset(val, 0, sizeof(val));

    while (fgets(line + last, ASCIILINESZ - last, in)) {
        lineno++;
        len = (int)strlen(line) - 1;
        if (len < 0) continue;
        if (line[len] != '\n' && !feof(in)) {
            ini_error("ml_ini: line too long in %s (%d)\n", ininame, lineno);
            ml_dictionary_delete(dict);
            fclose(in);
            return NULL;
        }
        while (len >= 0 && (line[len] == '\n' || isspace(line[len])))
            line[len--] = 0;
        if (len < 0) len = 0;

        if (line[len] == '\\') { last = len; continue; }
        last = 0;

        switch (ini_parse_line(line, section, key, val)) {
        case LINE_EMPTY:
        case LINE_COMMENT:
            break;
        case LINE_SECTION:
            mem_err = ml_dictionary_set(dict, section, NULL);
            break;
        case LINE_VALUE:
            snprintf(tmp, sizeof(tmp), "%s:%s", section, key);
            mem_err = ml_dictionary_set(dict, tmp, val);
            break;
        case LINE_ERROR:
            ini_error("ml_ini: syntax error in %s (%d):\n-> %s\n", ininame, lineno, line);
            errs++;
            break;
        default:
            break;
        }
        memset(line, 0, sizeof(line));
        last = 0;
        if (mem_err < 0) { ini_error("ml_ini: memory allocation failure\n"); break; }
    }
    if (errs) { ml_dictionary_delete(dict); dict = NULL; }
    fclose(in);
    return dict;
}

void ml_ini_free(ml_dictionary_t *d)
{
    ml_dictionary_delete(d);
}
