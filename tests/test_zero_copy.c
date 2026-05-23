#include "test_framework.h"
#include "../include/ml_zero_copy.h"
#include "../include/ml_code.h"

void test_zero_copy_suite(void)
{
    TEST_SUITE("Zero-Copy I/O");

    RUN_TEST("sendfile rejects bad params", {
        ml_int64_t rc = ml_sendfile(-1, -1, NULL, 0);
        ASSERT(rc == ML_EPARAM || rc == ML_ERROR, "bad fds rejected");
    });

    RUN_TEST("splice rejects bad params", {
        ml_int64_t rc = ml_splice(-1, NULL, -1, NULL, 0, 0);
        ASSERT(rc == ML_EPARAM || rc == ML_ERROR, "bad fds rejected");
    });

    RUN_TEST("copy_file_range rejects bad params", {
        ml_int64_t rc = ml_copy_file_range(-1, NULL, -1, NULL, 0);
        ASSERT(rc == ML_EPARAM || rc == ML_ERROR, "bad fds rejected");
    });

    RUN_TEST("pipe_relay rejects bad params", {
        ml_int64_t rc = ml_pipe_relay(-1, -1, 0);
        ASSERT(rc == ML_EPARAM || rc == ML_ERROR, "bad fds rejected");
    });
}
