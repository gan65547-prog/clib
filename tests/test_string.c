/**
 * Test string utility macros and functions from ml_string.h
 * Note: ml_strncpy etc. are macros wrapping standard libc functions.
 * The custom functions (ml_cpystrn, ml_strnstr, etc.) are in ml_string.c.
 */
#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_string.h"
#include "../src/ml_string.c"

static void test_strncpy_macro(void) {
    char dst[16];
    ml_memzero(dst, sizeof(dst));
    ml_strncpy(dst, "hello", sizeof(dst) - 1);
    ASSERT_EQ(strcmp(dst, "hello"), 0, "strncpy should copy string");
}

static void test_strncpy_truncation(void) {
    char dst[4];
    ml_memzero(dst, sizeof(dst));
    ml_strncpy(dst, "hello world", sizeof(dst) - 1);
    ASSERT_EQ(dst[0], 'h', "truncated dst[0] should be h");
    ASSERT_EQ(dst[1], 'e', "truncated dst[1] should be e");
    ASSERT_EQ(dst[2], 'l', "truncated dst[2] should be l");
    ASSERT_EQ(dst[3], '\0', "dst[3] should be null terminator");
}

static void test_snprintf_macro(void) {
    char buf[64];
    ml_snprintf(buf, sizeof(buf) - 1, "num=%d str=%s", 42, "test");
    ASSERT_EQ(strcmp(buf, "num=42 str=test"), 0, "snprintf should format correctly");
}

static void test_strncmp_macro(void) {
    int r = ml_strncmp("abcdef", "abcxyz", 3);
    ASSERT_EQ(r, 0, "first 3 chars should match");

    r = ml_strncmp("abc", "abd", 3);
    ASSERT(r < 0, "abc < abd lexicographically");
}

static void test_strlen_macro(void) {
    ASSERT_EQ(ml_strlen("hello"), 5, "strlen of hello is 5");
    ASSERT_EQ(ml_strlen(""), 0, "strlen of empty is 0");
}

static void test_cpystrn(void) {
    char dst[16];
    ml_memzero(dst, sizeof(dst));
    char *p = ml_cpystrn(dst, "testing123456789", 8);
    ASSERT_EQ(dst[7], '\0', "cpystrn should null-terminate at n-1");
    ASSERT_EQ(p, dst + 7, "cpystrn should return pointer to null byte");
}

static void test_sprintf_macro(void) {
    char buf[32];
    ml_sprintf(buf, sizeof(buf), "hello %s", "world");
    ASSERT_EQ(strcmp(buf, "hello world"), 0, "ml_sprintf should work");
}

void test_string_suite(void) {
    TEST_SUITE("String Utilities");
    RUN_TEST("strncpy macro", test_strncpy_macro());
    RUN_TEST("strncpy truncation", test_strncpy_truncation());
    RUN_TEST("snprintf macro", test_snprintf_macro());
    RUN_TEST("strncmp macro", test_strncmp_macro());
    RUN_TEST("strlen macro", test_strlen_macro());
    RUN_TEST("cpystrn custom", test_cpystrn());
    RUN_TEST("sprintf macro", test_sprintf_macro());
}
