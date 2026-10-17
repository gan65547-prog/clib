/*
 * test_http.c — Unit tests for mlfw_http parser and response builder.
 */
#include <string.h>
#include <strings.h>
#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/mlfw/mlfw_http.h"
/* Source symbols come from test_http_parser.o which includes ml_http.c */

/* ---- Request parsing ---- */

static void test_parse_get_simple(void)
{
    const char *raw =
        "GET /api/v1/health HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    ml_http_request_t req;
    ml_http_request_init(&req);

    ml_int_t n = ml_http_parse(&req, raw, (ml_int_t)strlen(raw));
    ASSERT(n > 0, "parse should succeed");
    ASSERT(req.m_method == ML_HTTP_GET, "method should be GET");
    ASSERT_STR_EQ(req.m_path, "/api/v1/health", "path should match");
    ASSERT_EQ(req.m_header_count, 1, "should have 1 header");
    ASSERT(req.m_content_length < 0, "no body expected");
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
    ml_http_request_init(&req);

    ml_int_t n = ml_http_parse(&req, raw, (ml_int_t)strlen(raw));
    ASSERT(n > 0, "parse should succeed");
    ASSERT(req.m_method == ML_HTTP_POST, "method should be POST");
    ASSERT_STR_EQ(req.m_path, "/api/v1/plugins/start", "path should match");
    ASSERT_EQ(req.m_content_length, 15, "Content-Length should be 15");
    ASSERT_PTR_NOT_NULL(req.m_body, "body should be present");
    ASSERT_EQ(req.m_body_len, 15, "body_len should be 15");
    ASSERT(0 == memcmp(req.m_body, "{\"name\":\"test\"}", 15),
           "body content should match");
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
    ml_int_t i;

    for (i = 0; i < 4; i++) {
        ml_http_request_t req;
        ml_http_request_init(&req);
        ml_int_t n = ml_http_parse(&req, tests[i], (ml_int_t)strlen(tests[i]));
        ASSERT(n > 0, "parse should succeed");
        ASSERT(req.m_method == expected[i], "method should match");
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
    ml_http_request_init(&req);
    ml_int_t n = ml_http_parse(&req, raw, (ml_int_t)strlen(raw));
    ASSERT(n > 0, "parse should succeed");
    ASSERT_EQ(req.m_header_count, 3, "should have 3 headers");
}

static void test_parse_incomplete_request(void)
{
    const char *raw = "GET /api";
    ml_http_request_t req;
    ml_http_request_init(&req);
    ml_int_t n = ml_http_parse(&req, raw, (ml_int_t)strlen(raw));
    ASSERT_EQ(n, 0, "incomplete should return 0 (need more data)");
}

static void test_parse_null_rejects(void)
{
    ml_http_request_t req;
    ml_http_request_init(&req);
    ASSERT(ml_http_parse(NULL, "GET / HTTP/1.1\r\n\r\n", 18) < 0,
           "NULL request should fail");
    ASSERT(ml_http_parse(&req, NULL, 10) < 0, "NULL data should fail");
    ASSERT(ml_http_parse(&req, "GET", 0) < 0, "zero length should fail");
    ASSERT(ml_http_parse(&req, "GET", -1) < 0, "negative length should fail");
}

/* ---- Header lookup ---- */

static void test_header_get_exact(void)
{
    const char *raw =
        "GET / HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "X-Request-ID: 12345\r\n"
        "\r\n";

    ml_http_request_t req;
    ml_http_request_init(&req);
    ml_int_t n = ml_http_parse(&req, raw, (ml_int_t)strlen(raw));
    ASSERT(n > 0, "parse should succeed");

    const char *v = ml_http_header_get(&req, "Content-Type");
    ASSERT_PTR_NOT_NULL(v, "Content-Type should be found");
    ASSERT_STR_EQ(v, "application/json", "value should match");

    v = ml_http_header_get(&req, "X-Request-ID");
    ASSERT_PTR_NOT_NULL(v, "X-Request-ID should be found");
    ASSERT_STR_EQ(v, "12345", "value should match");

    v = ml_http_header_get(&req, "Not-Present");
    ASSERT_PTR_NULL(v, "missing header should return NULL");
}

static void test_header_get_case_insensitive(void)
{
    const char *raw =
        "GET / HTTP/1.1\r\n"
        "X-Test: 42\r\n"
        "\r\n";

    ml_http_request_t req;
    ml_http_request_init(&req);
    ml_int_t n = ml_http_parse(&req, raw, (ml_int_t)strlen(raw));
    ASSERT(n > 0, "parse should succeed");

    ASSERT_PTR_NOT_NULL(ml_http_header_get(&req, "x-test"),
                        "lowercase should match");
    ASSERT_PTR_NOT_NULL(ml_http_header_get(&req, "X-TEST"),
                        "uppercase should match");
    ASSERT_PTR_NOT_NULL(ml_http_header_get(&req, "X-Test"),
                        "mixed case should match");
}

static void test_header_get_null_checks(void)
{
    ASSERT_PTR_NULL(ml_http_header_get(NULL, "X"), "NULL req → NULL");
    {
        ml_http_request_t req;
        ml_http_request_init(&req);
        ASSERT_PTR_NULL(ml_http_header_get(&req, NULL), "NULL name → NULL");
    }
}

/* ---- Method string ---- */

static void test_method_str(void)
{
    ASSERT_STR_EQ(ml_http_method_str(ML_HTTP_GET), "GET", "GET");
    ASSERT_STR_EQ(ml_http_method_str(ML_HTTP_POST), "POST", "POST");
    ASSERT_STR_EQ(ml_http_method_str(ML_HTTP_PUT), "PUT", "PUT");
    ASSERT_STR_EQ(ml_http_method_str(ML_HTTP_DELETE), "DELETE", "DELETE");
    ASSERT_STR_EQ(ml_http_method_str(ML_HTTP_UNKNOWN), "UNKNOWN", "UNKNOWN");
}

/* ---- Response serialization ---- */

static void test_response_write_200(void)
{
    ml_http_response_t resp;
    char buf[1024];
    char *body = "{\"status\":\"ok\"}";

    ml_memzero(&resp, sizeof(resp));
    resp.m_status       = 200;
    resp.m_content_type = "application/json";
    resp.m_body         = body;
    resp.m_body_len     = (ml_int_t)strlen(body);

    ml_int_t n = ml_http_response_write(&resp, buf, sizeof(buf));
    ASSERT(n > 0, "response should be written");
    ASSERT(strstr(buf, "HTTP/1.1 200 OK") != NULL, "status line");
    ASSERT(strstr(buf, "Content-Type: application/json") != NULL, "content-type");
    ASSERT(strstr(buf, "Content-Length: 15") != NULL, "x-test");
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

    ml_int_t n = ml_http_response_write(&resp, buf, sizeof(buf));
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
        ASSERT_EQ(ml_http_response_write(&resp, buf, -1), 0,
                  "negative buf_len → 0");
    }
}

/* ---- Convenience: JSON response ---- */

static void test_json_response(void)
{
    char buf[512];
    ml_int_t n = ml_http_json_response(200, "{\"ok\":true}", buf, sizeof(buf));
    ASSERT(n > 0, "json response should be written");
    ASSERT(strstr(buf, "200 OK") != NULL, "status");
    ASSERT(strstr(buf, "{\"ok\":true}") != NULL, "body");
}

/* ---- Convenience: error response ---- */

static void test_error_response(void)
{
    char buf[512];
    ml_int_t n = ml_http_error_response(500, "Internal Error", buf, sizeof(buf));
    ASSERT(n > 0, "error response should be written");
    ASSERT(strstr(buf, "500 Internal Server Error") != NULL, "status");
    ASSERT(strstr(buf, "\"code\":500") != NULL, "error code");
    ASSERT(strstr(buf, "\"message\":\"Internal Error\"") != NULL, "error message");
}

void test_http_suite(void)
{
    TEST_SUITE("HTTP Parser & Response");

    RUN_TEST("parse GET simple", { test_parse_get_simple(); });
    RUN_TEST("parse POST with body", { test_parse_post_with_body(); });
    RUN_TEST("parse all methods", { test_parse_all_methods(); });
    RUN_TEST("parse multiple headers", { test_parse_multiple_headers(); });
    RUN_TEST("parse incomplete request", { test_parse_incomplete_request(); });
    RUN_TEST("parse null rejects", { test_parse_null_rejects(); });

    RUN_TEST("header lookup exact", { test_header_get_exact(); });
    RUN_TEST("header lookup case-insensitive", { test_header_get_case_insensitive(); });
    RUN_TEST("header lookup null checks", { test_header_get_null_checks(); });

    RUN_TEST("method string", { test_method_str(); });

    RUN_TEST("response write 200", { test_response_write_200(); });
    RUN_TEST("response write 404", { test_response_write_404(); });
    RUN_TEST("response write null checks", { test_response_write_null_checks(); });

    RUN_TEST("json response convenience", { test_json_response(); });
    RUN_TEST("error response convenience", { test_error_response(); });
}
