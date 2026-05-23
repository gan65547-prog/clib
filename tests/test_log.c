/**
 * Test log module (file-based debug log).
 */
#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_code.h"
#include "../include/ml_log.h"
#include "../src/ml_log.c"

static void test_log_create_destroy(void) {
    ml_log_t log;
    char buffer[256];
    const char *path = "/tmp/clib_test_log.tmp";

    ml_log_create(&log, path, ML_LOG_DEBUG, 4096, buffer, sizeof(buffer));
    ASSERT(log.m_fd > 0, "log file should be opened");
    ASSERT_EQ(log.m_level, (ml_uint_t)ML_LOG_DEBUG, "level should be DEBUG");
    ASSERT_EQ(log.m_total, (ml_uint_t)256, "total buffer size");
    ASSERT_EQ(log.m_length, (ml_uint_t)4096, "file length limit");

    ml_log_destroy(&log);
    remove(path);
}

static void test_log_level_names(void) {
    ASSERT_EQ(strcmp(ml_log_level(ML_LOG_DEBUG), "DEBUG"), 0, "DEBUG level name");
    ASSERT_EQ(strcmp(ml_log_level(ML_LOG_INFO), "INFO"), 0, "INFO level name");
    ASSERT_EQ(strcmp(ml_log_level(ML_LOG_WARN), "WARN"), 0, "WARN level name");
    ASSERT_EQ(strcmp(ml_log_level(ML_LOG_ERROR), "ERROR"), 0, "ERROR level name");
}

static void test_log_date(void) {
    ml_log_t log;
    char buffer[256];
    const char *path = "/tmp/clib_test_log2.tmp";

    ml_log_create(&log, path, ML_LOG_DEBUG, 4096, buffer, sizeof(buffer));
    char *date = ml_log_date(&log);
    ASSERT_PTR_NOT_NULL(date, "date string should not be NULL");
    /* date format: YYYY-MM-DD HH:MM:SS (19 chars) */
    ASSERT(strlen(date) >= 19, "date should be at least 19 chars");

    ml_log_destroy(&log);
    remove(path);
}

static void test_log_write(void) {
    ml_log_t log;
    char buffer[256];
    const char *path = "/tmp/clib_test_log3.tmp";

    ml_log_create(&log, path, ML_LOG_DEBUG, 4096, buffer, sizeof(buffer));
    ml_memzero(buffer, sizeof(buffer));
    snprintf(buffer, sizeof(buffer) - 1, "test message\n");
    ml_log_write(&log, (ml_uint_t)strlen(buffer));

    /* Verify file has content */
    struct stat sb;
    ml_memzero(&sb, sizeof(sb));
    ASSERT(stat(path, &sb) == 0, "log file should exist after write");
    ASSERT(sb.st_size > 0, "log file should have content");

    ml_log_destroy(&log);
    remove(path);
}

void test_log_suite(void) {
    TEST_SUITE("Log");
    RUN_TEST("create and destroy", test_log_create_destroy());
    RUN_TEST("log level names", test_log_level_names());
    RUN_TEST("log date format", test_log_date());
    RUN_TEST("log write to file", test_log_write());
}
