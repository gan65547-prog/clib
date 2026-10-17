/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_http — Standalone HTTP/1.1 parser, response builder, and multipart parser.
 *
 * A self-contained HTTP module extracted from mlfw_http for general-purpose use.
 * Parses request-line → headers → body via a byte-by-byte state machine.
 * Supports multipart/form-data body parsing for file uploads.
 *
 * Limits: 4096-byte headers total, 65536-byte body, 32 headers per request,
 *         16 multipart parts, 256-char field names / filenames.
 */

#ifndef __ML_HTTP_H__
#define __ML_HTTP_H__

#include "ml_types.h"

/* ── constants ─────────────────────────────────────────────────────── */

#define ML_HTTP_METHOD_LEN      8
#define ML_HTTP_PATH_LEN        256
#define ML_HTTP_HEADER_MAX      32
#define ML_HTTP_HEADER_NAME     64
#define ML_HTTP_HEADER_VALUE    256
#define ML_HTTP_MAX_HEADERS     4096
#define ML_HTTP_MAX_BODY        65536

#define ML_HTTP_PART_MAX        16
#define ML_HTTP_PART_NAME       128
#define ML_HTTP_PART_FILENAME   256
#define ML_HTTP_PART_CTYPE      64

/* ── enums ──────────────────────────────────────────────────────────── */

typedef enum {
    ML_HTTP_GET = 0,
    ML_HTTP_POST,
    ML_HTTP_PUT,
    ML_HTTP_DELETE,
    ML_HTTP_UNKNOWN,
} ml_http_method_e;

/* ── types ──────────────────────────────────────────────────────────── */

typedef struct {
    char m_name[ML_HTTP_HEADER_NAME];
    char m_value[ML_HTTP_HEADER_VALUE];
} ml_http_header_t;

typedef struct {
    ml_http_method_e  m_method;
    char              m_path[ML_HTTP_PATH_LEN];
    char              m_query[ML_HTTP_PATH_LEN];
    ml_http_header_t  m_headers[ML_HTTP_HEADER_MAX];
    int               m_header_count;
    int               m_content_length;

    char             *m_body;
    int               m_body_len;

    /* internal parser state */
    int               m_state;
    int               m_offset;
    char              m_buf[ML_HTTP_MAX_BODY];
} ml_http_request_t;

typedef struct {
    int    m_status;
    char  *m_content_type;
    char  *m_body;
    int    m_body_len;
} ml_http_response_t;

/* multipart form-data part descriptor */
typedef struct {
    char   m_name[ML_HTTP_PART_NAME];
    char   m_filename[ML_HTTP_PART_FILENAME];
    char   m_content_type[ML_HTTP_PART_CTYPE];
    char  *m_data;           /* pointer into original buffer */
    int    m_data_len;
} ml_http_part_t;

/* ── request parsing ────────────────────────────────────────────────── */

void ml_http_request_init(ml_http_request_t *r);

/*
 * Feed data into the parser. Returns:
 *   > 0  — request fully parsed, returns total bytes consumed
 *   0    — need more data (incomplete)
 *   < 0  — parse error (malformed request)
 */
int ml_http_parse(ml_http_request_t *r, const char *data, int len);

/* case-insensitive header lookup, returns NULL if not found */
const char *ml_http_header_get(const ml_http_request_t *r, const char *name);

/* query parameter lookup (?key=value&...), returns pointer or NULL */
const char *ml_http_query_get(const ml_http_request_t *r, const char *key);

/* method enum → string */
const char *ml_http_method_str(ml_http_method_e method);

/* ── response building ──────────────────────────────────────────────── */

/* serialise response into buf, returns bytes written */
int ml_http_response_write(const ml_http_response_t *resp,
                           char *buf, int buf_len);

int ml_http_json_response(int status, const char *json,
                          char *buf, int buf_len);

int ml_http_error_response(int status, const char *msg,
                           char *buf, int buf_len);

/* ── multipart form-data ────────────────────────────────────────────── */

/*
 * Extract boundary string from a Content-Type header value.
 * Example: "multipart/form-data; boundary=----WebKit" → "----WebKit"
 * Returns pointer into `content_type`, or NULL if no boundary found.
 */
const char *ml_http_boundary_get(const char *content_type);

/*
 * Parse a multipart/form-data request body.
 * The request must already be fully parsed (ml_http_parse returned > 0).
 *
 * Returns number of parts parsed, or -1 on parse error.
 * Each part's m_data points into r->m_body — do not free separately.
 */
int ml_http_parse_multipart(ml_http_request_t *r, const char *boundary,
                            ml_http_part_t *parts, int max_parts);

#endif /* __ML_HTTP_H__ */
