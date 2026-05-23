/*
 * mlfw_http.h — Minimal HTTP/1.1 types and parser.
 *
 * Designed for embedded/admin API use — not a general-purpose server.
 * Supports: GET, POST, PUT, DELETE with JSON body.
 * Limits: 4096-byte headers, 65536-byte body.
 */
#ifndef MLFW_HTTP_H
#define MLFW_HTTP_H

#include "ml_types.h"

#define ML_HTTP_METHOD_LEN     8
#define ML_HTTP_PATH_LEN       256
#define ML_HTTP_HEADER_MAX     32
#define ML_HTTP_HEADER_NAME    64
#define ML_HTTP_HEADER_VALUE   256
#define ML_HTTP_MAX_HEADERS    4096
#define ML_HTTP_MAX_BODY       65536

typedef enum {
    ML_HTTP_GET = 0,
    ML_HTTP_POST,
    ML_HTTP_PUT,
    ML_HTTP_DELETE,
    ML_HTTP_UNKNOWN,
} ml_http_method_e;

typedef struct {
    char m_name[ML_HTTP_HEADER_NAME];
    char m_value[ML_HTTP_HEADER_VALUE];
} ml_http_header_t;

typedef struct {
    ml_http_method_e  m_method;
    char              m_path[ML_HTTP_PATH_LEN];
    char              m_query[ML_HTTP_PATH_LEN];   /* ?key=val part */
    ml_http_header_t  m_headers[ML_HTTP_HEADER_MAX];
    ml_int_t          m_header_count;
    ml_int_t          m_content_length;

    /* Raw body (if any), null-terminated for JSON strings */
    char             *m_body;
    ml_int_t          m_body_len;

    /* Internal parser state */
    ml_int_t          m_state;
    ml_int_t          m_offset;
    char              m_buf[ML_HTTP_MAX_HEADERS];
} ml_http_request_t;

/* ---- HTTP Response builder ---- */

typedef struct {
    ml_int_t   m_status;
    char      *m_content_type;
    char      *m_body;
    ml_int_t   m_body_len;
} ml_http_response_t;

/* ---- API ---- */

/* Initialise request object */
void ml_http_request_init(ml_http_request_t *r);

/*
 * Feed data into the parser. Returns:
 *   > 0  — request fully parsed, returns total bytes consumed
 *   0   — need more data (incomplete)
 *   < 0  — parse error (malformed request)
 */
ml_int_t ml_http_parse(ml_http_request_t *r, const char *data, ml_int_t len);

/* Serialise response into a buffer. Returns bytes written. */
ml_int_t ml_http_response_write(const ml_http_response_t *resp,
                                 char *buf, ml_int_t buf_len);

/* Convenience: write a simple JSON response */
ml_int_t ml_http_json_response(ml_int_t status, const char *json,
                                char *buf, ml_int_t buf_len);

/* Convenience: write an error response */
ml_int_t ml_http_error_response(ml_int_t status, const char *msg,
                                 char *buf, ml_int_t buf_len);

/* Look up header by name (case-insensitive). Returns NULL if not found. */
const char *ml_http_header_get(const ml_http_request_t *r, const char *name);

/* Extract a query parameter value from the request URL (?key=value&...).
 * Returns pointer to value in the request struct, or NULL if not found.
 * The returned pointer is valid for the lifetime of the request object. */
const char *ml_http_query_get(const ml_http_request_t *r, const char *key);

/* Method name to string */
const char *ml_http_method_str(ml_http_method_e method);

#endif
