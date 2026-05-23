#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ml_memzero(buf, n) (void) memset(buf, 0, n)

extern int _tests_run;
extern int _tests_pass;
extern int _tests_fail;
extern int _current_test_failed;

#define TEST_SUITE(name) \
    do { printf("\n=== %s ===\n", name); } while(0)

#define RUN_TEST(name, body) \
    do { \
        _tests_run++; \
        printf("  RUN  %s ... ", name); \
        _current_test_failed = 0; \
        body; \
        if (!_current_test_failed) { \
            _tests_pass++; \
            printf("PASS\n"); \
        } \
    } while(0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("FAIL\n    [%s:%d] %s\n", __FILE__, __LINE__, msg); \
            _tests_fail++; \
            _current_test_failed = 1; \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            printf("FAIL\n    [%s:%d] %s: expected %lld got %lld\n", \
                   __FILE__, __LINE__, msg, (long long)(b), (long long)(a)); \
            _tests_fail++; \
            _current_test_failed = 1; \
            return; \
        } \
    } while(0)

#define ASSERT_PTR_NOT_NULL(p, msg) \
    ASSERT((p) != NULL, msg)

#define ASSERT_PTR_NULL(p, msg) \
    ASSERT((p) == NULL, msg)

#define ASSERT_STR_EQ(a, b, msg) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            printf("FAIL\n    [%s:%d] %s: expected \"%s\" got \"%s\"\n", \
                   __FILE__, __LINE__, msg, (b), (a)); \
            _tests_fail++; \
            _current_test_failed = 1; \
            return; \
        } \
    } while(0)

#define PRINT_RESULTS() \
    do { \
        printf("\n========================================\n"); \
        printf("  TOTAL: %d  PASS: %d  FAIL: %d\n", \
               _tests_run, _tests_pass, _tests_fail); \
        printf("========================================\n"); \
    } while(0)

#endif /* TEST_FRAMEWORK_H */
