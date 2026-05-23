/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

/*
 * ml_json — lightweight JSON parser for clib.
 *
 * Adapted from cJSON v1.x by Dave Gamble (MIT license).
 * Renamed under the ml_ prefix and integrated with clib types.
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

#include "ml_json.h"

/* ---- Allocator hooks (default: malloc/free) ---- */

static void *(*json_malloc)(size_t sz) = malloc;
static void  (*json_free)(void *ptr)   = free;

/* ---- Internal helpers ---- */

static int json_strcasecmp(const char *s1, const char *s2)
{
    if (!s1) return (s1 == s2) ? 0 : 1;
    if (!s2) return 1;
    for (; tolower(*s1) == tolower(*s2); ++s1, ++s2)
        if (*s1 == 0) return 0;
    return tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
}

static char *json_strdup(const char *str)
{
    size_t len;
    char *copy;
    len = strlen(str) + 1;
    if (!(copy = (char *)json_malloc(len))) return 0;
    memcpy(copy, str, len);
    return copy;
}

/* ---- Internal constructor ---- */
static ml_json_t *json_new_item(void)
{
    ml_json_t *node = (ml_json_t *)json_malloc(sizeof(ml_json_t));
    if (node) memset(node, 0, sizeof(ml_json_t));
    return node;
}

/* ---- Delete ---- */
void ml_json_delete(ml_json_t *c)
{
    ml_json_t *next;
    while (c) {
        next = c->next;
        if (!(c->type & ML_JSON_IS_REFERENCE) && c->child)
            ml_json_delete(c->child);
        if (!(c->type & ML_JSON_IS_REFERENCE) && c->valuestring)
            json_free(c->valuestring);
        if (c->string) json_free(c->string);
        json_free(c);
        c = next;
    }
}

/* ---- Number parsing ---- */
static char **parse_number(ml_json_t *item, char **num)
{
    double n = 0, sign = 1, scale = 0;
    int subscale = 0, signsubscale = 1;

    if (**num == '-') { sign = -1; (*num)++; }
    if (**num == '0') (*num)++;
    if (**num >= '1' && **num <= '9') {
        do { n = (n * 10.0) + (**num - '0'); (*num)++; }
        while (**num >= '0' && **num <= '9');
    }
    if (**num == '.' && (*num)[1] >= '0' && (*num)[1] <= '9') {
        (*num)++;
        do { n = (n * 10.0) + (**num - '0'); scale--; (*num)++; }
        while (**num >= '0' && **num <= '9');
    }
    if (**num == 'e' || **num == 'E') {
        (*num)++;
        if (**num == '+') (*num)++;
        else if (**num == '-') { signsubscale = -1; (*num)++; }
        while (**num >= '0' && **num <= '9') {
            subscale = (subscale * 10) + (**num - '0'); (*num)++;
        }
    }
    n = sign * n * pow(10.0, (scale + subscale * signsubscale));
    item->valuedouble = n;
    item->valueint = (int)n;
    item->type = ML_JSON_NUMBER;
    return num;
}

/* ---- Number printing ---- */
static char *print_number(ml_json_t *item)
{
    char *str;
    double d = item->valuedouble;
    if (fabs(((double)item->valueint) - d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN) {
        str = (char *)json_malloc(21);
        if (str) sprintf(str, "%d", item->valueint);
    } else {
        str = (char *)json_malloc(64);
        if (str) {
            if (fabs(floor(d) - d) <= DBL_EPSILON)       sprintf(str, "%.0f", d);
            else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9) sprintf(str, "%e", d);
            else                                           sprintf(str, "%f", d);
        }
    }
    return str;
}

/* ---- String parsing ---- */
static const unsigned char first_byte_mark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

static char **parse_string(ml_json_t *item, char **str)
{
    char *ptr = *str + 1, *ptr2, *out;
    int len = 0;
    unsigned uc, uc2;

    if (**str != '\"') return NULL;
    while (*ptr != '\"' && *ptr && ++len)
        if (*ptr++ == '\\') ptr++;

    out = (char *)json_malloc(len + 1);
    if (!out) return 0;

    ptr = *str + 1; ptr2 = out;
    while (*ptr != '\"' && *ptr) {
        if (*ptr != '\\') *ptr2++ = *ptr++;
        else {
            ptr++;
            switch (*ptr) {
            case 'b': *ptr2++ = '\b'; break;
            case 'f': *ptr2++ = '\f'; break;
            case 'n': *ptr2++ = '\n'; break;
            case 'r': *ptr2++ = '\r'; break;
            case 't': *ptr2++ = '\t'; break;
            case 'u':
                sscanf(ptr + 1, "%4x", &uc); ptr += 4;
                if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0) break;
                if (uc >= 0xD800 && uc <= 0xDBFF) {
                    if (ptr[1] != '\\' || ptr[2] != 'u') break;
                    sscanf(ptr + 3, "%4x", &uc2); ptr += 6;
                    if (uc2 < 0xDC00 || uc2 > 0xDFFF) break;
                    uc = 0x10000 | ((uc & 0x3FF) << 10) | (uc2 & 0x3FF);
                }
                len = 4;
                if (uc < 0x80) len = 1;
                else if (uc < 0x800) len = 2;
                else if (uc < 0x10000) len = 3;
                ptr2 += len;
                switch (len) {
                case 4: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6; /* fall through */
                case 3: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6; /* fall through */
                case 2: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6; /* fall through */
                case 1: *--ptr2 = (uc | first_byte_mark[len]);
                }
                ptr2 += len;
                break;
            default: *ptr2++ = *ptr; break;
            }
            ptr++;
        }
    }
    *ptr2 = 0;
    if (*ptr == '\"') ptr++;
    item->valuestring = out;
    item->type = ML_JSON_STRING;
    *str = ptr;
    return str;
}

/* ---- String printing ---- */
static char *print_string_ptr(const char *str)
{
    const char *ptr;
    char *ptr2, *out;
    int len = 0;
    unsigned char token;

    if (!str) return json_strdup("");
    ptr = str;
    while ((token = *ptr) && ++len) {
        if (strchr("\"\\\b\f\n\r\t", token)) len++;
        else if (token < 32) len += 5;
        ptr++;
    }
    out = (char *)json_malloc(len + 3);
    if (!out) return 0;

    ptr2 = out; ptr = str;
    *ptr2++ = '\"';
    while (*ptr) {
        if ((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\')
            *ptr2++ = *ptr++;
        else {
            *ptr2++ = '\\';
            switch (token = *ptr++) {
            case '\\': *ptr2++ = '\\'; break;
            case '\"': *ptr2++ = '\"'; break;
            case '\b': *ptr2++ = 'b';  break;
            case '\f': *ptr2++ = 'f';  break;
            case '\n': *ptr2++ = 'n';  break;
            case '\r': *ptr2++ = 'r';  break;
            case '\t': *ptr2++ = 't';  break;
            default: sprintf(ptr2, "u%04x", token); ptr2 += 5; break;
            }
        }
    }
    *ptr2++ = '\"'; *ptr2++ = 0;
    return out;
}

static char *print_string(ml_json_t *item) { return print_string_ptr(item->valuestring); }

/* ---- Forward declarations ---- */
static char **parse_value(ml_json_t *item, char **value);
static char  *print_value(ml_json_t *item, int depth, int fmt);
static char **parse_array(ml_json_t *item, char **value);
static char  *print_array(ml_json_t *item, int depth, int fmt);
static char **parse_object(ml_json_t *item, char **value);
static char  *print_object(ml_json_t *item, int depth, int fmt);

/* ---- Skip whitespace ---- */
static inline char **skip(char **in)
{
    if (in && *in)
        while (isspace(**in)) (*in)++;
    return in;
}

/* ---- Parse entry points ---- */
ml_json_t *ml_json_parse(const char *value)
{
    ml_json_t *c;
    char **end_ptr;
    if (!value) return NULL;
    c = json_new_item();
    if (!c) return NULL;
    end_ptr = (char **)&value;
    if (!parse_value(c, skip(end_ptr))) { ml_json_delete(c); return NULL; }
    return c;
}

ml_json_t *ml_json_parse_stream(const char *value, char **end_ptr)
{
    if (!end_ptr) return NULL;
    ml_json_t *c = json_new_item();
    if (!c) return 0;
    *end_ptr = (char *)value;
    if (!parse_value(c, skip(end_ptr))) { ml_json_delete(c); return 0; }
    return c;
}

char *ml_json_print(ml_json_t *item)          { return print_value(item, 0, 1); }
char *ml_json_print_unformatted(ml_json_t *item) { return print_value(item, 0, 0); }
void  ml_json_free_str(char *v)               { json_free(v); }

/* ---- Stream comparison ---- */
static int stream_cmp(char **stream, const char *str)
{
    while (**stream == *str) { (*stream)++; str++; }
    if (*str == '\0') return 0;
    return **stream - *str;
}

/* ---- Value parser ---- */
static char **parse_value(ml_json_t *item, char **value)
{
    if (!stream_cmp(value, "null"))   { item->type = ML_JSON_NULL;   return value; }
    if (!stream_cmp(value, "false"))  { item->type = ML_JSON_FALSE;  return value; }
    if (!stream_cmp(value, "true"))   { item->type = ML_JSON_TRUE;   item->valueint = 1; return value; }

    switch (**value) {
    case '\"':          return parse_string(item, value);
    case '-':
    case '0'...'9':     return parse_number(item, value);
    case '[':           return parse_array(item, value);
    case '{':           return parse_object(item, value);
    }
    return NULL;
}

static char *print_value(ml_json_t *item, int depth, int fmt)
{
    char *out = 0;
    if (!item) return 0;
    switch ((item->type) & 255) {
    case ML_JSON_NULL:   out = json_strdup("null");  break;
    case ML_JSON_FALSE:  out = json_strdup("false"); break;
    case ML_JSON_TRUE:   out = json_strdup("true");  break;
    case ML_JSON_NUMBER: out = print_number(item);   break;
    case ML_JSON_STRING: out = print_string(item);   break;
    case ML_JSON_ARRAY:  out = print_array(item, depth, fmt);  break;
    case ML_JSON_OBJECT: out = print_object(item, depth, fmt); break;
    }
    return out;
}

/* ---- Array parsing ---- */
static char **parse_array(ml_json_t *item, char **value)
{
    ml_json_t *child;
    if (**value != '[') return NULL;

    item->type = ML_JSON_ARRAY;
    (*value)++;
    skip(value);
    if (**value == ']') { (*value)++; return value; }

    item->child = child = json_new_item();
    if (!item->child) return 0;
    if (!skip(parse_value(child, value))) return NULL;

    while (**value == ',') {
        (*value)++;
        skip(value);
        if (**value == ']') { (*value)++; break; }
        ml_json_t *new_item;
        if (!(new_item = json_new_item())) return 0;
        child->next = new_item; new_item->prev = child; child = new_item;
        if (!skip(parse_value(child, value))) return 0;
    }
    if (**value != ']') return NULL;
    (*value)++;
    return value;
}

static char *print_array(ml_json_t *item, int depth, int fmt)
{
    char **entries;
    char *out = 0, *ptr, *ret;
    int len = 5;
    ml_json_t *child = item->child;
    int numentries = 0, i = 0, fail = 0;

    while (child) { numentries++; child = child->next; }
    entries = (char **)json_malloc(numentries * sizeof(char *));
    if (!entries) return 0;
    memset(entries, 0, numentries * sizeof(char *));

    child = item->child;
    while (child && !fail) {
        ret = print_value(child, depth + 1, fmt);
        entries[i++] = ret;
        if (ret) len += strlen(ret) + 2 + (fmt ? 1 : 0);
        else fail = 1;
        child = child->next;
    }

    if (!fail) out = (char *)json_malloc(len);
    if (!out) fail = 1;

    if (fail) {
        for (i = 0; i < numentries; i++) if (entries[i]) json_free(entries[i]);
        json_free(entries);
        return 0;
    }

    *out = '[';
    ptr = out + 1; *ptr = 0;
    for (i = 0; i < numentries; i++) {
        strcpy(ptr, entries[i]); ptr += strlen(entries[i]);
        if (i != numentries - 1) { *ptr++ = ','; if (fmt) *ptr++ = ' '; *ptr = 0; }
        json_free(entries[i]);
    }
    json_free(entries);
    *ptr++ = ']'; *ptr++ = 0;
    return out;
}

/* ---- Object parsing ---- */
static char **parse_object(ml_json_t *item, char **value)
{
    ml_json_t *child;
    if (**value != '{') return NULL;

    item->type = ML_JSON_OBJECT;
    (*value)++;
    skip(value);
    if (**value == '}') { (*value)++; return value; }

    item->child = child = json_new_item();
    if (!item->child) return 0;
    if (!skip(parse_string(child, value))) return 0;
    child->string = child->valuestring; child->valuestring = 0;
    if (**value != ':') return NULL;
    (*value)++;
    if (!skip(parse_value(child, skip(value)))) return 0;

    while (**value == ',') {
        (*value)++;
        skip(value);
        if (**value == '}') { (*value)++; break; }
        ml_json_t *new_item;
        if (!(new_item = json_new_item())) return 0;
        child->next = new_item; new_item->prev = child; child = new_item;
        if (!skip(parse_string(child, value))) return 0;
        child->string = child->valuestring; child->valuestring = 0;
        if (**value != ':') return NULL;
        (*value)++;
        if (!skip(parse_value(child, skip(value)))) return 0;
    }
    if (**value != '}') return NULL;
    (*value)++;
    return value;
}

static char *print_object(ml_json_t *item, int depth, int fmt)
{
    char **entries = 0, **names = 0;
    char *out = 0, *ptr, *ret, *str;
    int len = 7, i = 0, j;
    ml_json_t *child = item->child;
    int numentries = 0, fail = 0;

    while (child) { numentries++; child = child->next; }
    entries = (char **)json_malloc(numentries * sizeof(char *));
    if (!entries) return 0;
    names = (char **)json_malloc(numentries * sizeof(char *));
    if (!names) { json_free(entries); return 0; }
    memset(entries, 0, sizeof(char *) * numentries);
    memset(names, 0, sizeof(char *) * numentries);

    child = item->child; depth++; if (fmt) len += depth;
    while (child) {
        names[i] = str = print_string_ptr(child->string);
        entries[i++] = ret = print_value(child, depth, fmt);
        if (str && ret) len += strlen(ret) + strlen(str) + 2 + (fmt ? 2 + depth : 0);
        else fail = 1;
        child = child->next;
    }

    if (!fail) out = (char *)json_malloc(len);
    if (!out) fail = 1;

    if (fail) {
        for (i = 0; i < numentries; i++) { if (names[i]) json_free(names[i]); if (entries[i]) json_free(entries[i]); }
        json_free(names); json_free(entries);
        return 0;
    }

    *out = '{'; ptr = out + 1; if (fmt) *ptr++ = '\n'; *ptr = 0;

    for (i = 0; i < numentries; i++) {
        if (fmt) for (j = 0; j < depth; j++) *ptr++ = '\t';
        strcpy(ptr, names[i]); ptr += strlen(names[i]);
        *ptr++ = ':'; if (fmt) *ptr++ = '\t';
        strcpy(ptr, entries[i]); ptr += strlen(entries[i]);
        if (i != numentries - 1) *ptr++ = ',';
        if (fmt) *ptr++ = '\n';
        *ptr = 0;
        json_free(names[i]); json_free(entries[i]);
    }
    json_free(names); json_free(entries);
    if (fmt) for (i = 0; i < depth - 1; i++) *ptr++ = '\t';
    *ptr++ = '}'; *ptr++ = 0;
    return out;
}

/* ---- Query functions ---- */
int ml_json_get_array_size(ml_json_t *array) {
    ml_json_t *c; int i = 0;
    if (!array) return 0;
    c = array->child;
    while (c) { i++; c = c->next; }
    return i;
}

ml_json_t *ml_json_get_array_item(ml_json_t *array, int item) {
    ml_json_t *c;
    if (!array) return NULL;
    c = array->child;
    while (c && item > 0) { item--; c = c->next; }
    return c;
}

ml_json_t *ml_json_get_object_item(ml_json_t *object, const char *string) {
    ml_json_t *c;
    if (!object) return NULL;
    c = object->child;
    while (c && json_strcasecmp(c->string, string)) c = c->next;
    return c;
}

/* ---- Typed convenience getters ---- */
const char *ml_json_get_string(ml_json_t *obj, const char *key, const char *def)
{
    ml_json_t *item = ml_json_get_object_item(obj, key);
    if (item && item->type == ML_JSON_STRING && item->valuestring)
        return item->valuestring;
    return def;
}

int ml_json_get_int(ml_json_t *obj, const char *key, int def)
{
    ml_json_t *item = ml_json_get_object_item(obj, key);
    if (item && item->type == ML_JSON_NUMBER)
        return item->valueint;
    return def;
}

double ml_json_get_double(ml_json_t *obj, const char *key, double def)
{
    ml_json_t *item = ml_json_get_object_item(obj, key);
    if (item && item->type == ML_JSON_NUMBER)
        return item->valuedouble;
    return def;
}

int ml_json_get_bool(ml_json_t *obj, const char *key, int def)
{
    ml_json_t *item = ml_json_get_object_item(obj, key);
    if (item) {
        if (item->type == ML_JSON_TRUE)  return 1;
        if (item->type == ML_JSON_FALSE) return 0;
    }
    return def;
}

/* ---- Helper for list append ---- */
static void suffix_object(ml_json_t *prev, ml_json_t *item) { prev->next = item; item->prev = prev; }

/* ---- Add items ---- */
void ml_json_add_item_to_array(ml_json_t *array, ml_json_t *item) {
    ml_json_t *c = array->child;
    if (!item) return;
    if (!c) { array->child = item; }
    else { while (c && c->next) c = c->next; suffix_object(c, item); }
}

void ml_json_add_item_to_object(ml_json_t *object, const char *string, ml_json_t *item) {
    if (!item) return;
    if (item->string) json_free(item->string);
    item->string = json_strdup(string);
    ml_json_add_item_to_array(object, item);
}

/* ---- Detach/delete from arrays ---- */
ml_json_t *ml_json_detach_item_from_array(ml_json_t *array, int which) {
    ml_json_t *c = array->child;
    while (c && which > 0) { c = c->next; which--; }
    if (!c) return 0;
    if (c->prev) c->prev->next = c->next;
    if (c->next) c->next->prev = c->prev;
    if (c == array->child) array->child = c->next;
    c->prev = c->next = 0;
    return c;
}

void ml_json_delete_item_from_array(ml_json_t *array, int which) {
    ml_json_delete(ml_json_detach_item_from_array(array, which));
}

ml_json_t *ml_json_detach_item_from_object(ml_json_t *object, const char *string) {
    int i = 0;
    ml_json_t *c = object->child;
    while (c && json_strcasecmp(c->string, string)) { i++; c = c->next; }
    if (c) return ml_json_detach_item_from_array(object, i);
    return 0;
}

void ml_json_delete_item_from_object(ml_json_t *object, const char *string) {
    ml_json_delete(ml_json_detach_item_from_object(object, string));
}

/* ---- Create functions ---- */
ml_json_t *ml_json_create_null(void)    { ml_json_t *item = json_new_item(); if (item) item->type = ML_JSON_NULL;   return item; }
ml_json_t *ml_json_create_true(void)    { ml_json_t *item = json_new_item(); if (item) item->type = ML_JSON_TRUE;   return item; }
ml_json_t *ml_json_create_false(void)   { ml_json_t *item = json_new_item(); if (item) item->type = ML_JSON_FALSE;  return item; }
ml_json_t *ml_json_create_bool(int b)   { ml_json_t *item = json_new_item(); if (item) item->type = b ? ML_JSON_TRUE : ML_JSON_FALSE; return item; }
ml_json_t *ml_json_create_number(double num) {
    ml_json_t *item = json_new_item();
    if (item) { item->type = ML_JSON_NUMBER; item->valuedouble = num; item->valueint = (int)num; }
    return item;
}
ml_json_t *ml_json_create_string(const char *string) {
    ml_json_t *item = json_new_item();
    if (item) { item->type = ML_JSON_STRING; item->valuestring = json_strdup(string); }
    return item;
}
ml_json_t *ml_json_create_array(void)   { ml_json_t *item = json_new_item(); if (item) item->type = ML_JSON_ARRAY;  return item; }
ml_json_t *ml_json_create_object(void)  { ml_json_t *item = json_new_item(); if (item) item->type = ML_JSON_OBJECT; return item; }
