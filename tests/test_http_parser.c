/*
 * test_http_parser.c — Unit tests for ml_http module (parser + multipart).
 */
#include <string.h>
#include <strings.h>
#include "test_framework.h"
#include "../include/ml_http.h"
#include "../include/ml_pool.h"
#include "../src/ml_http.c"

static ml_pool_t g_pool;

/* ── request parsing ────────────────────────────────────────────────── */

static void test_parse_get_simple(void)
{
    const char *raw =
        "GET /api/v1/health HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);

    int n = ml_http_parse(&req, raw, strlen(raw));
    ASSERT(n > 0, "parse should succeed");
    ASSERT(req.m_method == ML_HTTP_GET, "method should be GET");
    ASSERT_STR_EQ(req.m_path, "/api/v1/health", "path should match");
    ASSERT_EQ(req.m_header_count, 1, "should have 1 header");
    ASSERT(req.m_content_length == 0, "no body expected");
    ml_http_request_free(&req);
}

static void test_parse_post_with_body(void)
{
    const char *raw =
        "POST /api/v1/plugins/start HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length:  15\r\n"
        "\r\n"
        "{\"name\":\"test\"}";

    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);

    int n = ml_http_parse(&req, raw, strlen(raw));
    ASSERT(n > 0, "parse should succeed");
    ASSERT(req.m_method == ML_HTTP_POST, "method should be POST");
    ASSERT_STR_EQ(req.m_path, "/api/v1/plugins/start", "path should match");
    ASSERT_EQ(req.m_content_length, 15, "Content-Length should be 15");
    ASSERT_PTR_NOT_NULL(req.m_body, "body should be present");
    ASSERT_EQ(req.m_body_len, 15, "body_len should be 15");
    ASSERT(0 == memcmp(req.m_body, "{\"name\":\"test\"}", 15),
           "body content should match");
    ml_http_request_free(&req);
}

static void test_parse_all_methods(void)
{
    const char *tests[] = {
        "GET / HTTP/1.1\r\n\r\n",
        "POST / HTTP/1.1\r\n\r\n",
        "PUT / HTTP/1.1\r\n\r\n",
        "DELETE / HTTP/1.1\r\n\r\n",
    };
    ml_http_method_e expected[] = {
        ML_HTTP_GET, ML_HTTP_POST, ML_HTTP_PUT, ML_HTTP_DELETE
    };
    int i;

    for (i = 0; i < 4; i++) {
        ml_http_request_t req;
        ml_http_request_init(&req, &g_pool);
        ml_int_t n = ml_http_parse(&req, tests[i], strlen(tests[i]));
        ASSERT(n > 0, "parse should succeed");
        ASSERT(req.m_method == expected[i], "method should match");
        ml_http_request_free(&req);
    }
}

static void test_parse_multiple_headers(void)
{
    const char *raw =
        "GET /test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Accept: application/json\r\n"
        "X-Custom: value\r\n"
        "\r\n";

    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);
    int n = ml_http_parse(&req, raw, strlen(raw));
    ASSERT(n > 0, "parse should succeed");
    ASSERT_EQ(req.m_header_count, 3, "should have 3 headers");
    ml_http_request_free(&req);
}

static void test_parse_incomplete_request(void)
{
    const char *raw = "GET /api";
    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);
    ml_int_t n = ml_http_parse(&req, raw, (ml_int_t)strlen(raw));
    ASSERT_EQ(n, 0, "incomplete should return 0 (need more data)");
    ml_http_request_free(&req);
}

static void test_parse_with_query(void)
{
    const char *raw =
        "GET /api/search?q=hello&page=1 HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);
    int n = ml_http_parse(&req, raw, strlen(raw));
    ASSERT(n > 0, "parse should succeed");
    ASSERT_STR_EQ(req.m_path, "/api/search", "path without query");
    ASSERT_STR_EQ(req.m_query, "q=hello&page=1", "query string");
    ASSERT_PTR_NOT_NULL(ml_http_query_get(&req, "q"), "query param q");
    ASSERT_PTR_NOT_NULL(ml_http_query_get(&req, "page"), "query param page");
    ASSERT_PTR_NULL(ml_http_query_get(&req, "missing"), "missing param → NULL");
    ml_http_request_free(&req);
}

static void test_parse_null_rejects(void)
{
    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);
    ASSERT(ml_http_parse(NULL, "GET / HTTP/1.1\r\n\r\n", 18) < 0,
           "NULL request should fail");
    ASSERT(ml_http_parse(&req, NULL, 10) < 0, "NULL data should fail");
    ASSERT(ml_http_parse(&req, "GET", 0) < 0, "zero length should fail");
    ml_http_request_free(&req);
}

/* ── header lookup ──────────────────────────────────────────────────── */

static void test_header_get_exact(void)
{
    const char *raw =
        "GET / HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "X-Request-ID: 12345\r\n"
        "\r\n";

    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);
    int n = ml_http_parse(&req, raw, strlen(raw));
    ASSERT(n > 0, "parse should succeed");

    const char *v = ml_http_header_get(&req, "Content-Type");
    ASSERT_PTR_NOT_NULL(v, "Content-Type should be found");
    ASSERT_STR_EQ(v, "application/json", "value should match");

    v = ml_http_header_get(&req, "X-Request-ID");
    ASSERT_PTR_NOT_NULL(v, "X-Request-ID should be found");
    ASSERT_STR_EQ(v, "12345", "value should match");

    v = ml_http_header_get(&req, "Not-Present");
    ASSERT_PTR_NULL(v, "missing header should return NULL");
    ml_http_request_free(&req);
}

static void test_header_get_case_insensitive(void)
{
    const char *raw =
        "GET / HTTP/1.1\r\n"
        "X-Test: 42\r\n"
        "\r\n";

    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);
    int n = ml_http_parse(&req, raw, strlen(raw));
    ASSERT(n > 0, "parse should succeed");

    ASSERT_PTR_NOT_NULL(ml_http_header_get(&req, "x-test"), "lowercase match");
    ASSERT_PTR_NOT_NULL(ml_http_header_get(&req, "X-TEST"), "uppercase match");
    ASSERT_PTR_NOT_NULL(ml_http_header_get(&req, "X-Test"), "mixed case match");
    ml_http_request_free(&req);
}

static void test_header_get_null_checks(void)
{
    ASSERT_PTR_NULL(ml_http_header_get(NULL, "X"), "NULL req → NULL");
    {
        ml_http_request_t req;
        ml_http_request_init(&req, &g_pool);
        ASSERT_PTR_NULL(ml_http_header_get(&req, NULL), "NULL name → NULL");
        ml_http_request_free(&req);
    }
}

/* ── method string ──────────────────────────────────────────────────── */

static void test_method_str(void)
{
    ASSERT_STR_EQ(ml_http_method_str(ML_HTTP_GET), "GET", "GET");
    ASSERT_STR_EQ(ml_http_method_str(ML_HTTP_POST), "POST", "POST");
    ASSERT_STR_EQ(ml_http_method_str(ML_HTTP_PUT), "PUT", "PUT");
    ASSERT_STR_EQ(ml_http_method_str(ML_HTTP_DELETE), "DELETE", "DELETE");
    ASSERT_STR_EQ(ml_http_method_str(ML_HTTP_UNKNOWN), "UNKNOWN", "UNKNOWN");
}

/* ── response builder ───────────────────────────────────────────────── */

static void test_response_write_200(void)
{
    ml_http_response_t resp;
    char buf[1024];
    char *body = "{\"status\":\"ok\"}";

    ml_memzero(&resp, sizeof(resp));
    resp.m_status       = 200;
    resp.m_content_type = "application/json";
    resp.m_cors_origin  = "*";
    resp.m_body         = body;
    resp.m_body_len     = strlen(body);

    int n = ml_http_response_write(&resp, buf, sizeof(buf));
    ASSERT(n > 0, "response should be written");
    ASSERT(strstr(buf, "HTTP/1.1 200 OK") != NULL, "status line");
    ASSERT(strstr(buf, "Content-Type: application/json") != NULL, "content-type");
    ASSERT(strstr(buf, "Content-Length: 15") != NULL, "content-length");
    ASSERT(strstr(buf, "Connection: close") != NULL, "connection header");
    ASSERT(strstr(buf, "Access-Control-Allow-Origin: *") != NULL, "cors header");
    ASSERT(strstr(buf, body) != NULL, "body present");
}

static void test_response_write_404(void)
{
    ml_http_response_t resp;
    char buf[512];

    ml_memzero(&resp, sizeof(resp));
    resp.m_status       = 404;
    resp.m_content_type = "application/json";
    resp.m_body         = NULL;
    resp.m_body_len     = 0;

    int n = ml_http_response_write(&resp, buf, sizeof(buf));
    ASSERT(n > 0, "response should be written");
    ASSERT(strstr(buf, "HTTP/1.1 404 Not Found") != NULL, "status line");
    ASSERT(strstr(buf, "Content-Length: 0") != NULL, "zero content-length");
}

static void test_response_write_null_checks(void)
{
    char buf[128];
    ASSERT_EQ(ml_http_response_write(NULL, buf, sizeof(buf)), 0,
              "NULL resp → 0");
    {
        ml_http_response_t resp;
        ml_memzero(&resp, sizeof(resp));
        resp.m_status = 200;
        ASSERT_EQ(ml_http_response_write(&resp, NULL, 128), 0,
                  "NULL buf → 0");
        ASSERT_EQ(ml_http_response_write(&resp, buf, 0), 0,
                  "zero buf_len → 0");
    }
}

static void test_json_response(void)
{
    char buf[512];
    int n = ml_http_json_response(200, "{\"ok\":true}", buf, sizeof(buf));
    ASSERT(n > 0, "json response should be written");
    ASSERT(strstr(buf, "200 OK") != NULL, "status");
    ASSERT(strstr(buf, "{\"ok\":true}") != NULL, "body");
}

static void test_error_response(void)
{
    char buf[512];
    int n = ml_http_error_response(500, "Internal Error", buf, sizeof(buf));
    ASSERT(n > 0, "error response should be written");
    ASSERT(strstr(buf, "500 Internal Server Error") != NULL, "status");
    ASSERT(strstr(buf, "\"code\":500") != NULL, "error code");
    ASSERT(strstr(buf, "\"message\":\"Internal Error\"") != NULL, "error msg");
}

/* ── multipart parser ───────────────────────────────────────────────── */

static void test_boundary_get_simple(void)
{
    const char *ct = "multipart/form-data; boundary=----WebKit123";
    const char *b = ml_http_boundary_get(ct);
    ASSERT_PTR_NOT_NULL(b, "boundary found");
    ASSERT_STR_EQ(b, "----WebKit123", "boundary value");
}

static void test_boundary_get_quoted(void)
{
    const char *ct = "multipart/form-data; boundary=\"----MyBoundary\"";
    const char *b = ml_http_boundary_get(ct);
    ASSERT_PTR_NOT_NULL(b, "boundary found in quotes");
}

static void test_boundary_get_missing(void)
{
    ASSERT_PTR_NULL(ml_http_boundary_get(NULL), "NULL → NULL");
    ASSERT_PTR_NULL(ml_http_boundary_get("text/plain"), "text/plain → NULL");
    ASSERT_PTR_NULL(ml_http_boundary_get("multipart/form-data"),
                    "no boundary param → NULL");
}

static void test_multipart_single_text_field(void)
{
    const char *body =
        "\r\n--boundary\r\n"
        "Content-Disposition: form-data; name=\"username\"\r\n"
        "\r\n"
        "alice\r\n"
        "--boundary--\r\n";

    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);
    req.m_body     = (char *)body;
    req.m_body_len = (int)strlen(body);

    ml_http_part_t parts[4];
    int n = ml_http_parse_multipart(&req, "boundary", parts, 4);
    ASSERT_EQ(n, 1, "one part");
    ASSERT_STR_EQ(parts[0].m_name, "username", "field name");
    ASSERT_EQ(parts[0].m_data_len, 5, "data len");
    ASSERT(0 == memcmp(parts[0].m_data, "alice", 5), "data match");
    ml_http_request_free(&req);
}

static void test_multipart_file_upload(void)
{
    const char *body =
        "\r\n--boundary\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hello world\n"
        "line two\n"
        "\r\n--boundary--\r\n";

    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);
    req.m_body     = (char *)body;
    req.m_body_len = (int)strlen(body);

    ml_http_part_t parts[4];
    int n = ml_http_parse_multipart(&req, "boundary", parts, 4);
    ASSERT_EQ(n, 1, "one part");
    ASSERT_STR_EQ(parts[0].m_name, "file", "field name");
    ASSERT_STR_EQ(parts[0].m_filename, "test.txt", "filename");
    ASSERT_STR_EQ(parts[0].m_content_type, "text/plain", "content type");
    ASSERT(0 == memcmp(parts[0].m_data, "hello world\nline two\n", 21),
           "file content match");
    ml_http_request_free(&req);
}

static void test_multipart_mixed_fields(void)
{
    const char *body =
        "\r\n--boundary\r\n"
        "Content-Disposition: form-data; name=\"name\"\r\n"
        "\r\n"
        "Alice\r\n"
        "--boundary\r\n"
        "Content-Disposition: form-data; name=\"age\"\r\n"
        "\r\n"
        "30\r\n"
        "--boundary--\r\n";

    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);
    req.m_body     = (char *)body;
    req.m_body_len = (int)strlen(body);

    ml_http_part_t parts[4];
    int n = ml_http_parse_multipart(&req, "boundary", parts, 4);
    ASSERT_EQ(n, 2, "two parts");
    ASSERT_STR_EQ(parts[0].m_name, "name", "first name");
    ASSERT(0 == memcmp(parts[0].m_data, "Alice", 5), "first data");
    ASSERT_STR_EQ(parts[1].m_name, "age", "second name");
    ASSERT(0 == memcmp(parts[1].m_data, "30", 2), "second data");
    ml_http_request_free(&req);
}

static void test_multipart_null_checks(void)
{
    ml_http_part_t parts[4];

    ASSERT(ml_http_parse_multipart(NULL, "b", parts, 4) < 0,
           "NULL request → -1");

    {
        ml_http_request_t req;
        ml_http_request_init(&req, &g_pool);
        req.m_body     = "data";
        req.m_body_len = 4;
        ASSERT(ml_http_parse_multipart(&req, NULL, parts, 4) < 0,
               "NULL boundary → -1");
        ASSERT(ml_http_parse_multipart(&req, "b", NULL, 4) < 0,
               "NULL parts → -1");
        ASSERT(ml_http_parse_multipart(&req, "b", parts, 0) < 0,
               "max_parts=0 → -1");
        ASSERT(ml_http_parse_multipart(&req, "b", parts, -1) < 0,
               "max_parts<0 → -1");
        ml_http_request_free(&req);
    }
}

static void test_multipart_empty_body(void)
{
    ml_http_request_t req;
    ml_http_request_init(&req, &g_pool);
    req.m_body     = "";
    req.m_body_len = 0;

    ml_http_part_t parts[4];
    int n = ml_http_parse_multipart(&req, "boundary", parts, 4);
    ASSERT(n < 0, "empty body → -1");
    ml_http_request_free(&req);
}

/* ── suite entry ────────────────────────────────────────────────────── */

void test_http_parser_suite(void)
{
    ml_pool_create(&g_pool, 0);

    TEST_SUITE("ml_http — HTTP/1.1 parser + multipart");

    RUN_TEST("parse GET simple",           { test_parse_get_simple(); });
    RUN_TEST("parse POST with body",       { test_parse_post_with_body(); });
    RUN_TEST("parse all methods",          { test_parse_all_methods(); });
    RUN_TEST("parse multiple headers",     { test_parse_multiple_headers(); });
    RUN_TEST("parse incomplete request",   { test_parse_incomplete_request(); });
    RUN_TEST("parse query string",         { test_parse_with_query(); });
    RUN_TEST("parse null rejects",         { test_parse_null_rejects(); });

    RUN_TEST("header lookup exact",        { test_header_get_exact(); });
    RUN_TEST("header lookup case-insens",  { test_header_get_case_insensitive(); });
    RUN_TEST("header lookup null checks",  { test_header_get_null_checks(); });

    RUN_TEST("method string",              { test_method_str(); });

    RUN_TEST("response write 200",         { test_response_write_200(); });
    RUN_TEST("response write 404",         { test_response_write_404(); });
    RUN_TEST("response write null checks", { test_response_write_null_checks(); });
    RUN_TEST("json response convenience",  { test_json_response(); });
    RUN_TEST("error response convenience", { test_error_response(); });

    RUN_TEST("boundary_get simple",        { test_boundary_get_simple(); });
    RUN_TEST("boundary_get quoted",        { test_boundary_get_quoted(); });
    RUN_TEST("boundary_get missing",       { test_boundary_get_missing(); });
    RUN_TEST("multipart single field",     { test_multipart_single_text_field(); });
    RUN_TEST("multipart file upload",      { test_multipart_file_upload(); });
    RUN_TEST("multipart mixed fields",     { test_multipart_mixed_fields(); });
    RUN_TEST("multipart null checks",      { test_multipart_null_checks(); });
    RUN_TEST("multipart empty body",       { test_multipart_empty_body(); });

    ml_pool_destroy(&g_pool);
}
