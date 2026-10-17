/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_http.c — HTTP/1.1 parser + response builder + multipart parser.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

#include "ml_http.h"
#include "ml_string.h"
#include "ml_code.h"
#include "ml_pool.h"

/* ── internal parser states ─────────────────────────────────────────── */

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
};

/* ── init ───────────────────────────────────────────────────────────── */

void
ml_http_request_init(ml_http_request_t *r)
{
    ml_memzero(r, sizeof(*r));
    r->m_buf = (char *)malloc(ML_HTTP_BUF_SIZE);
    if (r->m_buf)
        r->m_buf_size = ML_HTTP_BUF_SIZE;
}

void
ml_http_request_free(ml_http_request_t *r)
{
    if (!r) return;
    free(r->m_buf);
    r->m_buf      = NULL;
    r->m_buf_size = 0;
}

/* ── helpers ────────────────────────────────────────────────────────── */

static ml_http_method_e
parse_method(const char *s, size_t len)
{
    if (len == 3 && !ml_strncmp(s, "GET", 3))     return ML_HTTP_GET;
    if (len == 4 && !ml_strncmp(s, "POST", 4))    return ML_HTTP_POST;
    if (len == 3 && !ml_strncmp(s, "PUT", 3))     return ML_HTTP_PUT;
    if (len == 6 && !ml_strncmp(s, "DELETE", 6))  return ML_HTTP_DELETE;
    return ML_HTTP_UNKNOWN;
}

static int
parse_content_length(ml_http_request_t *r)
{
    const char *v = ml_http_header_get(r, "Content-Length");
    long val;
    char *end;

    if (!v) return -1;

    /* skip whitespace */
    while (*v == ' ' || *v == '\t') v++;
    if (*v < '0' || *v > '9') return -1;

    val = strtol(v, &end, 10);
    /* reject trailing non-whitespace garbage or overflow */
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return -1;
    if (val < 0 || val > ML_HTTP_MAX_BODY) return -1;

    return (int)val;
}

/* ── public query API ───────────────────────────────────────────────── */

const char *
ml_http_method_str(ml_http_method_e method)
{
    switch (method) {
    case ML_HTTP_GET:    return "GET";
    case ML_HTTP_POST:   return "POST";
    case ML_HTTP_PUT:    return "PUT";
    case ML_HTTP_DELETE: return "DELETE";
    default:             return "UNKNOWN";
    }
}

const char *
ml_http_header_get(const ml_http_request_t *r, const char *name)
{
    int i;
    if (!r || !name) return NULL;
    for (i = 0; i < r->m_header_count; i++) {
        if (0 == strcasecmp(r->m_headers[i].m_name, name))
            return r->m_headers[i].m_value;
    }
    return NULL;
}

const char *
ml_http_query_get(const ml_http_request_t *r, const char *key)
{
    const char *q;
    int key_len, i;

    if (!r || !key || !key[0] || !r->m_query[0])
        return NULL;

    key_len = (int)ml_strlen(key);
    q = r->m_query;

    while (*q) {
        while (*q == '&' || *q == ';')
            q++;

        if (!ml_strncmp(q, key, key_len) && q[key_len] == '=') {
            return q + key_len + 1;
        }

        for (i = 0; q[i] && q[i] != '&' && q[i] != ';'; i++)
            ;
        if (q[i])
            q += i + 1;
        else
            break;
    }

    return NULL;
}

/* ── request parser ─────────────────────────────────────────────────── */

int
ml_http_parse(ml_http_request_t *r, const char *data, size_t len)
{
    size_t i;
    char c;

    if (!r || !data || len == 0)
        return -1;

    for (i = 0; i < len; i++) {
        c = data[i];

        if (r->m_state < HTTP_STATE_BODY) {
            if (r->m_offset >= ML_HTTP_MAX_HEADERS - 1)
                return -1;
        }

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
                r->m_offset = 0;
                r->m_state  = HTTP_STATE_HEADER_DONE;
            }
            break;

        case HTTP_STATE_HEADER_VALUE:
            if (c == '\r' || c == '\n') {
                r->m_buf[r->m_offset - 1] = '\0';
                if (r->m_header_count < ML_HTTP_HEADER_MAX) {
                    char *v = r->m_buf;
                    while (*v == ' ') v++;
                    ml_strncpy(r->m_headers[r->m_header_count].m_value,
                               v, ML_HTTP_HEADER_VALUE - 1);
                    r->m_header_count++;
                }
                r->m_offset = 0;
                r->m_state  = (c == '\n') ? HTTP_STATE_HEADER_DONE
                                          : HTTP_STATE_HEADER_NAME;
            }
            break;

        case HTTP_STATE_HEADER_DONE:
            if (c == '\n') {
                { int cl = parse_content_length(r); r->m_content_length = cl > 0 ? (size_t)cl : 0; }
                if (r->m_content_length > 0) {
                    r->m_body_start = r->m_offset;
                    r->m_state      = HTTP_STATE_BODY;
                } else {
                    r->m_state = HTTP_STATE_DONE;
                    return (int)(i + 1);
                }
            }
            break;

        case HTTP_STATE_BODY:
            if (!r->m_buf || r->m_offset >= r->m_buf_size - 1)
                return -1;

            if ((r->m_offset - r->m_body_start) >= r->m_content_length) {
                r->m_buf[r->m_offset] = '\0';
                r->m_body     = r->m_buf + r->m_body_start;
                r->m_body_len = r->m_offset - r->m_body_start;
                r->m_state    = HTTP_STATE_DONE;
                return (int)(i + 1);
            }
            break;

        case HTTP_STATE_DONE:
            break;
        }
    }

    if (r->m_state == HTTP_STATE_DONE)
        return (int)i;

    return 0;
}

/* ── response builder ───────────────────────────────────────────────── */

static const char *
status_text(int status)
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

int
ml_http_response_write(const ml_http_response_t *resp,
                       char *buf, size_t buf_len)
{
    int n, off = 0;
    const char *conn;

    if (!resp || !buf || buf_len == 0)
        return 0;

    n = snprintf(buf + off, buf_len - off,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n",
        resp->m_status, status_text(resp->m_status),
        resp->m_content_type ? resp->m_content_type : "application/json",
        resp->m_body ? resp->m_body_len : 0);
    if (n < 0 || (size_t)n >= buf_len - off) return -1;
    off += n;

    if (resp->m_cors_origin) {
        n = snprintf(buf + off, buf_len - off,
                     "Access-Control-Allow-Origin: %s\r\n",
                     resp->m_cors_origin);
        if (n < 0 || (size_t)n >= buf_len - off) return -1;
        off += n;
    }

    conn = resp->m_connection ? resp->m_connection : "close";
    n = snprintf(buf + off, buf_len - off,
                 "Connection: %s\r\n", conn);
    if (n < 0 || (size_t)n >= buf_len - off) return -1;
    off += n;

    n = snprintf(buf + off, buf_len - off, "\r\n");
    if (n < 0 || (size_t)n >= buf_len - off) return -1;
    off += n;

    if (resp->m_body && resp->m_body_len > 0 && (size_t)off < buf_len) {
        size_t remain = buf_len - (size_t)off;
        size_t copy = (remain < resp->m_body_len) ? remain : resp->m_body_len;
        memcpy(buf + off, resp->m_body, copy);
        off += (int)copy;
    }

    return off;
}

int
ml_http_json_response(int status, const char *json,
                      char *buf, size_t buf_len)
{
    ml_http_response_t resp;
    ml_memzero(&resp, sizeof(resp));
    resp.m_status       = status;
    resp.m_content_type = "application/json";
    resp.m_body         = (char *)json;
    resp.m_body_len     = ml_strlen(json);
    return ml_http_response_write(&resp, buf, buf_len);
}

int
ml_http_error_response(int status, const char *msg,
                       char *buf, size_t buf_len)
{
    char json[256];
    char safe_msg[128];
    int i, j;

    for (i = 0, j = 0; msg && msg[i] && j < (int)sizeof(safe_msg) - 2; i++) {
        unsigned char c = (unsigned char)msg[i];
        if (c == '"' || c == '\\') {
            if (j + 2 >= (int)sizeof(safe_msg)) break;
            safe_msg[j++] = '\\';
            safe_msg[j++] = msg[i];
        } else if (c == '\n') {
            if (j + 2 >= (int)sizeof(safe_msg)) break;
            safe_msg[j++] = '\\';
            safe_msg[j++] = 'n';
        } else if (c == '\t') {
            if (j + 2 >= (int)sizeof(safe_msg)) break;
            safe_msg[j++] = '\\';
            safe_msg[j++] = 't';
        } else if (c == '\r') {
            continue;
        } else if (c < 0x20) {
            /* JSON escape other control chars as \\u00XX */
            if (j + 6 >= (int)sizeof(safe_msg)) break;
            safe_msg[j++] = '\\';
            safe_msg[j++] = 'u';
            safe_msg[j++] = '0';
            safe_msg[j++] = '0';
            safe_msg[j++] = "0123456789abcdef"[c >> 4];
            safe_msg[j++] = "0123456789abcdef"[c & 0x0F];
        } else {
            safe_msg[j++] = msg[i];
        }
    }
    safe_msg[j] = '\0';

    ml_snprintf(json, sizeof(json),
        "{\"error\":{\"code\":%d,\"message\":\"%s\"}}", status, safe_msg);
    return ml_http_json_response(status, json, buf, buf_len);
}

/* ── multipart form-data parser ─────────────────────────────────────── */

const char *
ml_http_boundary_get(const char *content_type)
{
    const char *p;

    if (!content_type) return NULL;

    /* case-insensitive prefix check */
    if (strncasecmp(content_type, "multipart/", 10) != 0)
        return NULL;

    p = strstr(content_type, "boundary=");
    if (!p) return NULL;

    p += 9;  /* skip "boundary=" */

    /* strip optional quotes */
    if (*p == '"') {
        p++;
        /* find closing quote */
        const char *end = strchr(p, '"');
        if (!end) return NULL;
        /* We need to return a mutable copy. Caller expects pointer into
         * the original content_type buffer — we can't modify it.  Return
         * p as-is; the caller strips quotes if present in path context.
         *
         * Actually we return p (after quote).  The full boundary with
         * possible trailing quote is returned.  Callers should handle this.
         */
        return p;  /* caller must handle the trailing quote */
    }

    return p;
}

/*
 * Find boundary in buffer, return pointer or NULL.
 * Boundary is preceded by \r\n (or start of body) and "--".
 */
static const char *
find_boundary(const char *data, size_t len, const char *boundary,
              int blen, size_t *p_offset)
{
    size_t i;

    if ((size_t)blen + 4 > len)
        return NULL;

    for (i = *p_offset; i + (size_t)blen + 4 <= len; i++) {
        /* must start with \r\n--boundary */
        if (data[i] == '\r' && data[i+1] == '\n' &&
            data[i+2] == '-' && data[i+3] == '-') {
            if (i + 4 + blen <= len &&
                !memcmp(data + i + 4, boundary, blen)) {
                *p_offset = i;
                return data + i;
            }
        }
    }
    return NULL;
}

int
ml_http_parse_multipart(ml_http_request_t *r, const char *boundary,
                        ml_http_part_t *parts, int max_parts)
{
    const char *data, *hdr, *body_start;
    size_t dlen;
    int blen, count = 0;
    size_t offset;

    if (!r || !r->m_body || !boundary || !parts || max_parts < 1)
        return -1;

    data = r->m_body;
    dlen = r->m_body_len;
    blen = (int)strlen(boundary);
    if (blen == 0) return -1;

    /* find first boundary */
    offset = 0;
    hdr = find_boundary(data, dlen, boundary, blen, &offset);
    if (!hdr) return -1;

    /* advance past "\r\n--boundary" */
    hdr += 4 + blen;
    offset = (size_t)(hdr - data);

    while (count < max_parts) {
        /* hdr now points right after boundary. Could be \r\n (next part)
         * or --\r\n (end marker). */
        if (offset + 1 < dlen && hdr[0] == '-' && hdr[1] == '-') {
            /* end boundary "--boundary--" */
            return count;
        }

        /* skip \r\n after boundary */
        if (offset + 1 < dlen && hdr[0] == '\r' && hdr[1] == '\n') {
            hdr += 2;
            offset += 2;
        }

        /* parse part headers until \r\n\r\n */
        ml_memzero(&parts[count], sizeof(ml_http_part_t));

        while (offset < dlen) {
            /* check for end of headers */
            if (offset + 1 < dlen && hdr[0] == '\r' && hdr[1] == '\n') {
                /* empty line → body follows */
                hdr += 2;
                offset += 2;
                break;
            }

            /* parse one header line: "Name: value\r\n" */
            const char *colon = NULL;
            const char *line_end = NULL;
            int hdr_len;

            /* find : and \r\n */
            {
                const char *p = hdr;
                while (p < data + dlen) {
                    if (*p == ':' && !colon) colon = p;
                    if (*p == '\r' && p[1] == '\n') { line_end = p; break; }
                    p++;
                }
                if (!colon || !line_end) return -1;
            }

            hdr_len = (int)(colon - hdr);

            /* Content-Disposition: form-data; name="field"; filename="f" */
            if (hdr_len == 19 &&
                !strncasecmp(hdr, "Content-Disposition", 19)) {
                const char *name_p, *fn_p;
                const char *val_start = colon + 1;

                /* skip whitespace */
                while (*val_start == ' ') val_start++;

                /* parse: form-data; name="xxx"; filename="yyy" */
                name_p = strstr(val_start, "name=\"");
                if (name_p) {
                    name_p += 6;
                    const char *name_end = strchr(name_p, '"');
                    if (name_end) {
                        int nlen = (int)(name_end - name_p);
                        if (nlen >= ML_HTTP_PART_NAME) nlen = ML_HTTP_PART_NAME - 1;
                        memcpy(parts[count].m_name, name_p, nlen);
                        parts[count].m_name[nlen] = '\0';
                    }
                }

                fn_p = strstr(val_start, "filename=\"");
                if (fn_p) {
                    fn_p += 10;
                    const char *fn_end = strchr(fn_p, '"');
                    if (fn_end) {
                        int fnlen = (int)(fn_end - fn_p);
                        if (fnlen >= ML_HTTP_PART_FILENAME)
                            fnlen = ML_HTTP_PART_FILENAME - 1;
                        memcpy(parts[count].m_filename, fn_p, fnlen);
                        parts[count].m_filename[fnlen] = '\0';
                    }
                }
            } else if (hdr_len == 12 &&
                       !strncasecmp(hdr, "Content-Type", 12)) {
                const char *val_start = colon + 1;
                while (*val_start == ' ') val_start++;
                int ctlen = (int)(line_end - val_start);
                if (ctlen >= ML_HTTP_PART_CTYPE) ctlen = ML_HTTP_PART_CTYPE - 1;
                memcpy(parts[count].m_content_type, val_start, ctlen);
                parts[count].m_content_type[ctlen] = '\0';
            }

            /* advance to next line */
            hdr = line_end + 2;
            offset = (size_t)(hdr - data);
        }

        /* hdr now points to body start. Find next boundary. */
        body_start = hdr;
        {
            size_t search_off = offset;
            const char *next = find_boundary(data, dlen, boundary, blen,
                                             &search_off);
            if (next) {
                size_t body_end = search_off;
                if (body_end > offset && data[body_end - 2] == '\r' &&
                    data[body_end - 1] == '\n')
                    body_end -= 2;

                parts[count].m_data     = (char *)body_start;
                parts[count].m_data_len = body_end - offset;

                /* advance to after boundary marker */
                hdr    = data + search_off + 4 + blen;
                offset = search_off + 4 + blen;
                count++;
            } else {
                /* no more boundaries — rest is body */
                parts[count].m_data     = (char *)body_start;
                parts[count].m_data_len = dlen - offset;
                count++;
                return count;
            }
        }
    }

    return count;
}
