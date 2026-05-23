/*
 * ml_json — lightweight JSON parser and builder for clib.
 *
 * Parse JSON strings into a linked tree of ml_json_t nodes, or build trees
 * programmatically and print them back to JSON text.  Supports objects,
 * arrays, strings, numbers, booleans, and null.
 *
 * Thread-safe as long as different trees are not shared across threads.
 */
#ifndef __ML_JSON_H__
#define __ML_JSON_H__

#include "ml_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Node types (renamed under ml_ prefix, adapted from cJSON) */
#define ML_JSON_FALSE  0
#define ML_JSON_TRUE   1
#define ML_JSON_NULL   2
#define ML_JSON_NUMBER 3
#define ML_JSON_STRING 4
#define ML_JSON_ARRAY  5
#define ML_JSON_OBJECT 6

#define ML_JSON_IS_REFERENCE 256

typedef struct ml_json_s {
    struct ml_json_s *next, *prev;
    struct ml_json_s *child;
    int     type;
    char   *valuestring;
    int     valueint;
    double  valuedouble;
    char   *string;
} ml_json_t;

/* ---- lifecycle ---- */

/* Parse a JSON string. Returns NULL on invalid input or NULL pointer. */
ml_json_t  *ml_json_parse(const char *value);

/* Parse one JSON value from a stream, advancing *end_ptr past it. */
ml_json_t  *ml_json_parse_stream(const char *value, char **end_ptr);

/* Print a tree to a newly-allocated JSON string (formatted). */
char       *ml_json_print(ml_json_t *item);

/* Print a tree to a compact (no whitespace) JSON string. */
char       *ml_json_print_unformatted(ml_json_t *item);

/* Free a string returned by ml_json_print / ml_json_print_unformatted. */
void        ml_json_free_str(char *v);

/* Recursively delete a JSON tree. */
void        ml_json_delete(ml_json_t *c);

/* ---- query ---- */

/* Number of elements in a JSON array. */
int         ml_json_get_array_size(ml_json_t *array);

/* Get the n-th element of a JSON array (0-based). Returns NULL if out of bounds. */
ml_json_t  *ml_json_get_array_item(ml_json_t *array, int item);

/* Get a child object by key. Returns NULL if not found. */
ml_json_t  *ml_json_get_object_item(ml_json_t *object, const char *string);

/* Convenience: get typed value from an object key, returning def if missing. */
const char *ml_json_get_string(ml_json_t *obj, const char *key, const char *def);
int         ml_json_get_int(ml_json_t *obj, const char *key, int def);
double      ml_json_get_double(ml_json_t *obj, const char *key, double def);
int         ml_json_get_bool(ml_json_t *obj, const char *key, int def);

/* ---- create ---- */

ml_json_t  *ml_json_create_null(void);
ml_json_t  *ml_json_create_true(void);
ml_json_t  *ml_json_create_false(void);
ml_json_t  *ml_json_create_bool(int b);
ml_json_t  *ml_json_create_number(double num);
ml_json_t  *ml_json_create_string(const char *string);
ml_json_t  *ml_json_create_array(void);
ml_json_t  *ml_json_create_object(void);

/* ---- modify ---- */

void ml_json_add_item_to_array(ml_json_t *array, ml_json_t *item);
void ml_json_add_item_to_object(ml_json_t *object, const char *string, ml_json_t *item);
ml_json_t *ml_json_detach_item_from_array(ml_json_t *array, int which);
void ml_json_delete_item_from_array(ml_json_t *array, int which);
ml_json_t *ml_json_detach_item_from_object(ml_json_t *object, const char *string);
void ml_json_delete_item_from_object(ml_json_t *object, const char *string);

/* Convenience macros for adding typed values to an object */
#define ml_json_add_null_to_object(obj, name)   ml_json_add_item_to_object(obj, name, ml_json_create_null())
#define ml_json_add_true_to_object(obj, name)   ml_json_add_item_to_object(obj, name, ml_json_create_true())
#define ml_json_add_false_to_object(obj, name)  ml_json_add_item_to_object(obj, name, ml_json_create_false())
#define ml_json_add_number_to_object(obj, name, n) ml_json_add_item_to_object(obj, name, ml_json_create_number(n))
#define ml_json_add_string_to_object(obj, name, s) ml_json_add_item_to_object(obj, name, ml_json_create_string(s))

#ifdef __cplusplus
}
#endif

#endif /* __ML_JSON_H__ */
