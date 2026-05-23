/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

/*
 * mlfw_http.c — Minimal HTTP/1.1 parser implementation.
 *
 * Parses: request line → headers → body.
 * Supports keep-alive detection but does not manage connections.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <strings.h>

#include "ml_types.h"
#include "ml_code.h"
#include "ml_pool.h"
#include "ml_string.h"
#include "mlfw/mlfw_http.h"

/* Parser states */
enum {
    HTTP_STATE_METHOD = 0,
    HTTP_STATE_PATH,
    HTTP_STATE_QUERY,
    HTTP_STATE_VERSION,
    HTTP_STATE_HEADER_NAME,
    HTTP_STATE_HEADER_VALUE,
    HTTP_STATE_HEADER_DONE,
    HTTP_STATE_BODY,
    HTTP_STATE_DONE,
    HTTP_STATE_ERROR = -1,
};

void ml_http_request_init(ml_http_request_t *r)
{
    ml_memzero(r, sizeof(*r));
}

const char *ml_http_method_str(ml_http_method_e method)
{
    switch (method) {
    case ML_HTTP_GET:    return "GET";
    case ML_HTTP_POST:   return "POST";
    case ML_HTTP_PUT:    return "PUT";
    case ML_HTTP_DELETE: return "DELETE";
    default:             return "UNKNOWN";
    }
}

const char *ml_http_header_get(const ml_http_request_t *r, const char *name)
{
    ml_int_t i;
    if (!r || !name) return NULL;
    for (i = 0; i < r->m_header_count; i++) {
        if (0 == strcasecmp(r->m_headers[i].m_name, name))
            return r->m_headers[i].m_value;
    }
    return NULL;
}

const char *ml_http_query_get(const ml_http_request_t *r, const char *key)
{
    const char *q;
    ml_int_t key_len, i;

    if (!r || !key || !key[0] || !r->m_query[0])
        return NULL;

    key_len = (ml_int_t)ml_strlen(key);
    q = r->m_query;

    while (*q) {
        /* Skip leading & or ; separators */
        while (*q == '&' || *q == ';')
            q++;

        /* Check if this parameter matches the key */
        if (!ml_strncmp(q, key, key_len) && q[key_len] == '=') {
            /* Found — return pointer to value (after '=') */
            return q + key_len + 1;
        }

        /* Advance to next separator */
        for (i = 0; q[i] && q[i] != '&' && q[i] != ';'; i++)
            ;
        if (q[i])
            q += i + 1;
        else
            break;
    }

    return NULL;
}

/* ---- Internal parser helpers ---- */

static ml_http_method_e parse_method(const char *s, ml_int_t len)
{
    if (len == 3 && !ml_strncmp(s, "GET", 3))     return ML_HTTP_GET;
    if (len == 4 && !ml_strncmp(s, "POST", 4))    return ML_HTTP_POST;
    if (len == 3 && !ml_strncmp(s, "PUT", 3))     return ML_HTTP_PUT;
    if (len == 6 && !ml_strncmp(s, "DELETE", 6))  return ML_HTTP_DELETE;
    return ML_HTTP_UNKNOWN;
}

static ml_int_t parse_content_length(ml_http_request_t *r)
{
    const char *v = ml_http_header_get(r, "Content-Length");
    if (!v) return -1;
    return (ml_int_t)atoi(v);
}

/*
 * State-machine parser.  Processes data byte-by-byte through
 * request-line → headers → body states.
 *
 * Returns:
 *   > 0  — complete request parsed (bytes consumed)
 *   0    — need more data
 *   -1   — parse error
 */
ml_int_t ml_http_parse(ml_http_request_t *r, const char *data, ml_int_t len)
{
    ml_int_t i;
    char c;

    if (!r || !data || len <= 0)
        return -1;

    for (i = 0; i < len; i++) {
        c = data[i];

        /* Buffer overflow guard */
        if (r->m_offset >= ML_HTTP_MAX_HEADERS - 1)
            return -1;

        r->m_buf[r->m_offset++] = c;
        r->m_buf[r->m_offset]   = '\0';

        switch (r->m_state) {

        case HTTP_STATE_METHOD:
            if (c == ' ') {
                r->m_buf[r->m_offset - 1] = '\0';
                r->m_method = parse_method(r->m_buf, r->m_offset - 1);
                r->m_offset = 0;
                r->m_state  = HTTP_STATE_PATH;
            }
            break;

        case HTTP_STATE_PATH:
            if (c == ' ') {
                r->m_buf[r->m_offset - 1] = '\0';
                ml_strncpy(r->m_path, r->m_buf, sizeof(r->m_path) - 1);
                r->m_offset = 0;
                r->m_state  = HTTP_STATE_VERSION;
            } else if (c == '?') {
                r->m_buf[r->m_offset - 1] = '\0';
                ml_strncpy(r->m_path, r->m_buf, sizeof(r->m_path) - 1);
                r->m_offset = 0;
                r->m_state  = HTTP_STATE_QUERY;
            }
            break;

        case HTTP_STATE_QUERY:
            if (c == ' ') {
                r->m_buf[r->m_offset - 1] = '\0';
                ml_strncpy(r->m_query, r->m_buf, sizeof(r->m_query) - 1);
                r->m_offset = 0;
                r->m_state  = HTTP_STATE_VERSION;
            }
            break;

        case HTTP_STATE_VERSION:
            if (c == '\r') {
                r->m_offset = 0;
                r->m_state  = HTTP_STATE_HEADER_NAME;
            }
            break;

        case HTTP_STATE_HEADER_NAME:
            if (c == '\n' && r->m_offset == 1) {
                /* Skip \n of CRLF from previous line transition */
                r->m_offset = 0;
            } else if (c == ':') {
                r->m_buf[r->m_offset - 1] = '\0';
                if (r->m_header_count < ML_HTTP_HEADER_MAX) {
                    ml_strncpy(r->m_headers[r->m_header_count].m_name,
                               r->m_buf, ML_HTTP_HEADER_NAME - 1);
                }
                r->m_offset = 0;
                r->m_state  = HTTP_STATE_HEADER_VALUE;
            } else if (c == '\r') {
                /* Empty line → end of headers */
                r->m_offset = 0;
                r->m_state  = HTTP_STATE_HEADER_DONE;
            }
            break;

        case HTTP_STATE_HEADER_VALUE:
            if (c == '\r') {
                r->m_buf[r->m_offset - 1] = '\0';
                if (r->m_header_count < ML_HTTP_HEADER_MAX) {
                    /* Skip leading whitespace */
                    char *v = r->m_buf;
                    while (*v == ' ') v++;
                    ml_strncpy(r->m_headers[r->m_header_count].m_value,
                               v, ML_HTTP_HEADER_VALUE - 1);
                    r->m_header_count++;
                }
                r->m_offset = 0;
                /* Next char should be \n, then check for next header or body */
                r->m_state = HTTP_STATE_HEADER_NAME;
            }
            break;

        case HTTP_STATE_HEADER_DONE:
            if (c == '\n') {
                r->m_content_length = parse_content_length(r);
                if (r->m_content_length > 0) {
                    r->m_offset = 0;
                    r->m_state  = HTTP_STATE_BODY;
                } else {
                    r->m_offset = 0;
                    r->m_state  = HTTP_STATE_DONE;
                    return i + 1;
                }
            }
            break;

        case HTTP_STATE_BODY:
            if (r->m_offset >= (ml_int_t)sizeof(r->m_buf) - 1)
                return -1;  /* body too large for buffer */

            if (r->m_offset >= r->m_content_length) {
                r->m_buf[r->m_offset] = '\0';
                r->m_body     = r->m_buf;
                r->m_body_len = r->m_offset;
                r->m_state    = HTTP_STATE_DONE;
                return i + 1;
            }
            /*
             * Note: body bytes are collected in m_buf during the
             * BODY state. The data loop above writes each byte to
             * r->m_buf[r->m_offset++] before the switch. So by the
             * time we reach here, the byte is already stored.
             *
             * For the first body byte, we reset m_offset to 0
             * (in HEADER_DONE → BODY transition). Then each subsequent
             * byte increments m_offset and is stored in m_buf.
             */
            break;

        case HTTP_STATE_DONE:
            /* Multiple requests in same buffer — ignore trailing */
            break;
        }
    }

    if (r->m_state == HTTP_STATE_DONE)
        return i;  /* completed */

    return 0;  /* need more data */
}

/* ---- Response serialization ---- */

static const char *status_text(ml_int_t status)
{
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 500: return "Internal Server Error";
    default:  return "Unknown";
    }
}

ml_int_t ml_http_response_write(const ml_http_response_t *resp,
                                 char *buf, ml_int_t buf_len)
{
    ml_int_t n;

    if (!resp || !buf || buf_len <= 0)
        return 0;

    n = ml_snprintf(buf, buf_len,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        resp->m_status, status_text(resp->m_status),
        resp->m_content_type ? resp->m_content_type : "application/json",
        resp->m_body ? resp->m_body_len : 0);

    /* snprintf returns "would-be" length — clamp to actual buffer */
    if (n < 0) n = 0;
    if (n > buf_len) n = buf_len;

    if (resp->m_body && resp->m_body_len > 0 && n < buf_len) {
        ml_int_t remain = buf_len - n;
        ml_int_t copy = (remain < resp->m_body_len) ? remain : resp->m_body_len;
        ml_memcpy(buf + n, resp->m_body, copy);
        n += copy;
    }

    return n;
}

ml_int_t ml_http_json_response(ml_int_t status, const char *json,
                                char *buf, ml_int_t buf_len)
{
    ml_http_response_t resp;
    ml_memzero(&resp, sizeof(resp));
    resp.m_status       = status;
    resp.m_content_type = "application/json";
    resp.m_body         = (char *)json;
    resp.m_body_len     = (ml_int_t)ml_strlen(json);
    return ml_http_response_write(&resp, buf, buf_len);
}

ml_int_t ml_http_error_response(ml_int_t status, const char *msg,
                                 char *buf, ml_int_t buf_len)
{
    char json[256];
    ml_snprintf(json, sizeof(json),
        "{\"error\":{\"code\":%d,\"message\":\"%s\"}}", status, msg);
    return ml_http_json_response(status, json, buf, buf_len);
}
