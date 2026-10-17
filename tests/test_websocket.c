/*
 * test_websocket.c — Unit tests for ml_websocket protocol module.
 */
#include <string.h>
#include "test_framework.h"
#include "../include/ml_websocket.h"
#include "../include/ml_base64.h"
#include "../src/ml_websocket.c"

/* ── frame parsing ──────────────────────────────────────────────────── */

static void test_parse_text_unmasked(void)
{
    /* server→client text frame: "Hello" (unmasked) */
    unsigned char raw[] = {
        0x81, 0x05,           /* FIN + TEXT, len=5 */
        'H','e','l','l','o'
    };
    ml_ws_frame_t f;

    int n = ml_ws_parse_frame((char *)raw, sizeof(raw), &f);
    ASSERT_EQ(n, 7, "consumed 7 bytes");
    ASSERT(f.fin == 1, "FIN set");
    ASSERT_EQ(f.opcode, ML_WS_OPCODE_TEXT, "opcode TEXT");
    ASSERT(f.masked == 0, "unmasked");
    ASSERT_EQ(f.payload_len, 5, "payload_len 5");
    ASSERT(0 == memcmp(f.payload, "Hello", 5), "payload matches");
}

static void test_parse_text_masked(void)
{
    /* client→server masked text: "Hello" */
    unsigned char raw[] = {
        0x81, 0x85,           /* FIN+TEXT, MASKED, len=5 */
        0x37, 0xFA, 0x21, 0x3D,  /* mask key */
        'H'^0x37, 'e'^0xFA, 'l'^0x21, 'l'^0x3D, 'o'^0x37
    };
    ml_ws_frame_t f;

    int n = ml_ws_parse_frame((char *)raw, sizeof(raw), &f);
    ASSERT_EQ(n, 11, "consumed 11 bytes");
    ASSERT_EQ(f.opcode, ML_WS_OPCODE_TEXT, "opcode TEXT");
    ASSERT(f.masked == 1, "masked");
    ASSERT_EQ(f.payload_len, 5, "payload_len 5");
    /* after unmasking, payload should be "Hello" */
    ASSERT(0 == memcmp(f.payload, "Hello", 5), "payload unmasked to Hello");
}

static void test_parse_binary(void)
{
    unsigned char raw[] = {
        0x82, 0x04,           /* FIN+BINARY, len=4 */
        0x00, 0x01, 0x02, 0x03
    };
    ml_ws_frame_t f;

    int n = ml_ws_parse_frame((char *)raw, sizeof(raw), &f);
    ASSERT_EQ(n, 6, "consumed 6 bytes");
    ASSERT_EQ(f.opcode, ML_WS_OPCODE_BINARY, "opcode BINARY");
    ASSERT_EQ(f.payload_len, 4, "payload_len 4");
    ASSERT(f.payload[0] == 0 && f.payload[3] == 3, "binary content");
}

static void test_parse_ping_pong_close(void)
{
    unsigned char ping[] = { 0x89, 0x00 };  /* FIN+PING, len=0 */
    unsigned char pong[] = { 0x8A, 0x00 };  /* FIN+PONG, len=0 */
    unsigned char close[] = { 0x88, 0x00 }; /* FIN+CLOSE, len=0 */
    ml_ws_frame_t f;

    ASSERT_EQ(ml_ws_parse_frame((char *)ping, 2, &f), 2, "ping parsed");
    ASSERT_EQ(f.opcode, ML_WS_OPCODE_PING, "opcode PING");

    ASSERT_EQ(ml_ws_parse_frame((char *)pong, 2, &f), 2, "pong parsed");
    ASSERT_EQ(f.opcode, ML_WS_OPCODE_PONG, "opcode PONG");

    ASSERT_EQ(ml_ws_parse_frame((char *)close, 2, &f), 2, "close parsed");
    ASSERT_EQ(f.opcode, ML_WS_OPCODE_CLOSE, "opcode CLOSE");
}

static void test_parse_16bit_length(void)
{
    /* frame with 126 extended length: 200 bytes */
    unsigned char raw[204];
    memset(raw, 0, sizeof(raw));
    raw[0] = 0x82;  /* FIN+BINARY */
    raw[1] = 0x7E;  /* 126 marker */
    raw[2] = 0x00;  /* length hi */
    raw[3] = 0xC8;  /* length lo = 200 */
    memset(raw + 4, 0xAB, 200);

    ml_ws_frame_t f;
    int n = ml_ws_parse_frame((char *)raw, 204, &f);
    ASSERT_EQ(n, 204, "200-byte payload");
    ASSERT_EQ(f.payload_len, 200, "payload_len 200");
}

static void test_parse_incomplete(void)
{
    unsigned char raw[] = { 0x81, 0x05, 'H','e' };  /* len=5 but only 2 bytes */

    ml_ws_frame_t f;
    int n = ml_ws_parse_frame((char *)raw, sizeof(raw), &f);
    ASSERT_EQ(n, 0, "incomplete → 0");
}

static void test_parse_incomplete_header(void)
{
    unsigned char raw[] = { 0x81 };  /* only 1 byte */
    ml_ws_frame_t f;
    ASSERT_EQ(ml_ws_parse_frame((char *)raw, 1, &f), -1, "too short → -1");
}

static void test_parse_null_checks(void)
{
    ml_ws_frame_t f;
    ASSERT_EQ(ml_ws_parse_frame(NULL, 10, &f), -1, "NULL data → -1");
    ASSERT_EQ(ml_ws_parse_frame((char *)"x", 10, NULL), -1, "NULL frame → -1");
}

/* ── frame encoding ─────────────────────────────────────────────────── */

static void test_encode_text_unmasked(void)
{
    char buf[128];
    int n = ml_ws_encode_frame(ML_WS_OPCODE_TEXT, "Hello", 5, 0, buf, sizeof(buf));
    ASSERT_EQ(n, 7, "encoded 7 bytes");
    ASSERT(buf[0] == (char)0x81, "FIN+TEXT byte");
    ASSERT(buf[1] == (char)0x05, "len=5");
    ASSERT(0 == memcmp(buf + 2, "Hello", 5), "payload");
}

static void test_encode_text_masked(void)
{
    char buf[128];
    int n = ml_ws_encode_frame(ML_WS_OPCODE_TEXT, "Hello", 5, 1, buf, sizeof(buf));
    ASSERT_EQ(n, 11, "encoded 11 bytes (2 hdr + 4 mask + 5 payload)");
    ASSERT((buf[1] & 0x80) != 0, "mask bit set");

    /* apply mask again to recover original */
    uint8_t mask[4];
    memcpy(mask, buf + 2, 4);
    ml_ws_mask(buf + 6, 5, mask);
    ASSERT(0 == memcmp(buf + 6, "Hello", 5), "round-trip unmask");
}

static void test_encode_close_with_code(void)
{
    char buf[128];
    int n = ml_ws_encode_close(1000, "bye", 0, buf, sizeof(buf));
    ASSERT(n > 2, "close frame > header only");

    ml_ws_frame_t f;
    int parsed = ml_ws_parse_frame(buf, n, &f);
    ASSERT_EQ(parsed, n, "close frame parseable");
    ASSERT_EQ(f.opcode, ML_WS_OPCODE_CLOSE, "opcode CLOSE");
    ASSERT(f.payload_len >= 2, "at least 2-byte status code");
    /* status code 1000 = 0x03E8 in big-endian */
    ASSERT((unsigned char)f.payload[0] == 0x03, "status hi byte");
    ASSERT((unsigned char)f.payload[1] == 0xE8, "status lo byte");
}

static void test_encode_overflow(void)
{
    char buf[4];
    int n = ml_ws_encode_frame(ML_WS_OPCODE_TEXT, "HelloWorld", 10, 0, buf, 4);
    ASSERT(n < 0, "overflow → -1");
}

static void test_encode_null_payload(void)
{
    char buf[128];
    /* ping with no payload is valid */
    int n = ml_ws_encode_frame(ML_WS_OPCODE_PING, NULL, 0, 0, buf, sizeof(buf));
    ASSERT_EQ(n, 2, "empty ping = 2-byte header");
}

/* ── frame round-trip ───────────────────────────────────────────────── */

static void test_roundtrip_masked(void)
{
    char buf[256];
    const char *msg = "Round-trip test with masking!";

    int n = ml_ws_encode_frame(ML_WS_OPCODE_TEXT, msg, (int)strlen(msg),
                                1, buf, sizeof(buf));
    ASSERT(n > 0, "encoded");

    ml_ws_frame_t f;
    int parsed = ml_ws_parse_frame(buf, n, &f);
    ASSERT_EQ(parsed, n, "parsed all bytes");
    ASSERT_EQ(f.opcode, ML_WS_OPCODE_TEXT, "opcode preserved");
    ASSERT_EQ(f.payload_len, (int)strlen(msg), "length preserved");
    ASSERT(0 == memcmp(f.payload, msg, f.payload_len), "content round-trip intact");
}

static void test_roundtrip_unmasked(void)
{
    char buf[256];
    int i;
    unsigned char data[128];
    for (i = 0; i < 128; i++) data[i] = (unsigned char)(i * 3);

    int n = ml_ws_encode_frame(ML_WS_OPCODE_BINARY, (char *)data, 128,
                                0, buf, sizeof(buf));
    ASSERT(n > 0, "encoded");

    ml_ws_frame_t f;
    int parsed = ml_ws_parse_frame(buf, n, &f);
    ASSERT_EQ(parsed, n, "parsed all bytes");
    ASSERT_EQ(f.payload_len, 128, "length preserved");
    ASSERT(0 == memcmp(f.payload, data, 128), "binary round-trip");
}

static void test_roundtrip_empty_close(void)
{
    char buf[128];
    int n = ml_ws_encode_frame(ML_WS_OPCODE_CLOSE, NULL, 0, 0, buf, sizeof(buf));
    ASSERT(n > 0, "encoded");

    ml_ws_frame_t f;
    int parsed = ml_ws_parse_frame(buf, n, &f);
    ASSERT_EQ(parsed, n, "parsed");
    ASSERT_EQ(f.opcode, ML_WS_OPCODE_CLOSE, "CLOSE round-trip");
}

/* ── masking idempotent ─────────────────────────────────────────────── */

static void test_mask_idempotent(void)
{
    char msg[] = "Test Message";
    uint8_t mask[4] = { 0x12, 0x34, 0x56, 0x78 };
    char original[16];

    memcpy(original, msg, sizeof(msg));
    ml_ws_mask(msg, (int)strlen(msg), mask);
    ASSERT(memcmp(msg, original, strlen(original)) != 0, "masked != original");
    ml_ws_mask(msg, (int)strlen(msg), mask);
    ASSERT(0 == memcmp(msg, original, strlen(original)),
           "double-mask = original");
}

/* ── accept key computation ─────────────────────────────────────────── */

static void test_accept_key_known_vector(void)
{
    /* RFC 6455 §4.2.2 example */
    const char *key = "dGhlIHNhbXBsZSBub25jZQ==";
    char accept[29];

    char *result = ml_ws_accept_key(key, accept);
    ASSERT_PTR_NOT_NULL(result, "accept key computed");
    ASSERT_STR_EQ(result, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", "RFC example match");
}

static void test_accept_key_null_checks(void)
{
    char buf[29];
    ASSERT_PTR_NULL(ml_ws_accept_key(NULL, buf), "NULL key → NULL");
    ASSERT_PTR_NULL(ml_ws_accept_key("dGhl", NULL), "NULL out → NULL");
}

/* ── handshake messages ─────────────────────────────────────────────── */

static void test_server_handshake(void)
{
    char buf[256];
    int n = ml_ws_server_handshake("dGhlIHNhbXBsZSBub25jZQ==", buf, sizeof(buf));
    ASSERT(n > 0, "handshake written");
    ASSERT(strstr(buf, "HTTP/1.1 101") != NULL, "101 status");
    ASSERT(strstr(buf, "Upgrade: websocket") != NULL, "Upgrade header");
    ASSERT(strstr(buf, "Connection: Upgrade") != NULL, "Connection header");
    ASSERT(strstr(buf, "Sec-WebSocket-Accept:") != NULL, "Accept header");
}

static void test_client_handshake(void)
{
    char buf[512];
    int n = ml_ws_client_handshake("example.com:8080", "/ws/chat",
                                    "dGhlIHNhbXBsZSBub25jZQ==",
                                    buf, sizeof(buf));
    ASSERT(n > 0, "handshake written");
    ASSERT(strstr(buf, "GET /ws/chat HTTP/1.1") != NULL, "request line");
    ASSERT(strstr(buf, "Host: example.com:8080") != NULL, "Host header");
    ASSERT(strstr(buf, "Upgrade: websocket") != NULL, "Upgrade header");
    ASSERT(strstr(buf, "Sec-WebSocket-Version: 13") != NULL, "Version header");
}

static void test_handshake_overflow(void)
{
    char tiny[8];
    ASSERT(ml_ws_server_handshake("key", tiny, sizeof(tiny)) < 0,
           "too small → -1");
}

/* ── generate key ──────────────────────────────────────────────────── */

static void test_generate_key(void)
{
    char key[25];
    ml_ws_generate_key(key);
    ASSERT_EQ((int)strlen(key), 24, "key is 24 chars");
    /* should be valid base64 */
    int i;
    for (i = 0; i < 24; i++) {
        int c = (unsigned char)key[i];
        ASSERT((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=',
               "valid base64 char");
    }
}

static void test_generate_key_accept_roundtrip(void)
{
    char key[25], accept[29];
    ml_ws_generate_key(key);
    char *result = ml_ws_accept_key(key, accept);
    ASSERT_PTR_NOT_NULL(result, "accept key from generated key");
    ASSERT_EQ((int)strlen(result), 28, "accept key is 28 chars");
}

/* ── opcode names ───────────────────────────────────────────────────── */

static void test_opcode_names(void)
{
    ASSERT_STR_EQ(ml_ws_opcode_name(ML_WS_OPCODE_TEXT), "text", "TEXT");
    ASSERT_STR_EQ(ml_ws_opcode_name(ML_WS_OPCODE_BINARY), "binary", "BINARY");
    ASSERT_STR_EQ(ml_ws_opcode_name(ML_WS_OPCODE_CLOSE), "close", "CLOSE");
    ASSERT_STR_EQ(ml_ws_opcode_name(ML_WS_OPCODE_PING), "ping", "PING");
    ASSERT_STR_EQ(ml_ws_opcode_name(ML_WS_OPCODE_PONG), "pong", "PONG");
    ASSERT_STR_EQ(ml_ws_opcode_name(ML_WS_OPCODE_CONT), "continuation", "CONT");
    ASSERT_STR_EQ(ml_ws_opcode_name(99), "unknown", "unknown opcode");
}

/* ── suite entry ────────────────────────────────────────────────────── */

void test_websocket_suite(void)
{
    TEST_SUITE("ml_websocket — RFC 6455");

    RUN_TEST("parse text unmasked",         { test_parse_text_unmasked(); });
    RUN_TEST("parse text masked",           { test_parse_text_masked(); });
    RUN_TEST("parse binary",                { test_parse_binary(); });
    RUN_TEST("parse ping/pong/close",       { test_parse_ping_pong_close(); });
    RUN_TEST("parse 16-bit length",         { test_parse_16bit_length(); });
    RUN_TEST("parse incomplete frame",      { test_parse_incomplete(); });
    RUN_TEST("parse incomplete header",     { test_parse_incomplete_header(); });
    RUN_TEST("parse null checks",           { test_parse_null_checks(); });

    RUN_TEST("encode text unmasked",        { test_encode_text_unmasked(); });
    RUN_TEST("encode text masked",          { test_encode_text_masked(); });
    RUN_TEST("encode close with code",      { test_encode_close_with_code(); });
    RUN_TEST("encode overflow",             { test_encode_overflow(); });
    RUN_TEST("encode null payload",         { test_encode_null_payload(); });

    RUN_TEST("roundtrip masked text",       { test_roundtrip_masked(); });
    RUN_TEST("roundtrip unmasked binary",   { test_roundtrip_unmasked(); });
    RUN_TEST("roundtrip empty close",       { test_roundtrip_empty_close(); });

    RUN_TEST("mask XOR idempotent",         { test_mask_idempotent(); });

    RUN_TEST("accept key RFC vector",       { test_accept_key_known_vector(); });
    RUN_TEST("accept key null checks",      { test_accept_key_null_checks(); });

    RUN_TEST("server handshake",            { test_server_handshake(); });
    RUN_TEST("client handshake",            { test_client_handshake(); });
    RUN_TEST("handshake overflow",          { test_handshake_overflow(); });

    RUN_TEST("generate key",                { test_generate_key(); });
    RUN_TEST("generate key + accept r-t",   { test_generate_key_accept_roundtrip(); });

    RUN_TEST("opcode names",                { test_opcode_names(); });
}
