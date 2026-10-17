#include "test_framework.h"

/* Global test counters - defined once here */
int _tests_run = 0;
int _tests_pass = 0;
int _tests_fail = 0;
int _current_test_failed = 0;

/* Forward declarations */
void test_rbtree_suite(void);
void test_container_suite(void);
void test_pool_suite(void);
void test_string_suite(void);
void test_bitmap_suite(void);
void test_base64_suite(void);
void test_integration_suite(void);
void test_md5sum_suite(void);
void test_pkbuf_suite(void);
void test_mapkey_suite(void);
void test_ringbuf_suite(void);
void test_thread_suite(void);
void test_log_suite(void);
void test_event_suite(void);
void test_timer_suite(void);
void test_thread_pool_suite(void);
void test_json_suite(void);
void test_dictionary_suite(void);
void test_iniparser_suite(void);
void test_set_rbtree_suite(void);
void test_shm_pool_suite(void);
void test_zero_copy_suite(void);
void test_crypto_suite(void);
void test_queue_suite(void);
void test_http_parser_suite(void);
void test_websocket_suite(void);
void test_muxer_suite(void);
void test_codec_suite(void);
void test_camera_suite(void);
void test_rga_suite(void);
void test_cv_math_suite(void);
void test_pipeline_stage_suite(void);

int main(void) {
    printf("==============================================\n");
    printf("  clib - C Middleware Library Test Suite\n");
    printf("  Date: %s %s\n", __DATE__, __TIME__);
    printf("==============================================\n");

    test_rbtree_suite();
    test_container_suite();
    test_pool_suite();
    test_string_suite();
    test_bitmap_suite();
    test_base64_suite();
    test_integration_suite();
    test_md5sum_suite();
    test_pkbuf_suite();
    test_mapkey_suite();
    test_ringbuf_suite();
    test_thread_suite();
    test_log_suite();
    test_event_suite();
    test_timer_suite();
    test_thread_pool_suite();
    test_json_suite();
    test_dictionary_suite();
    test_iniparser_suite();
    test_set_rbtree_suite();
    test_shm_pool_suite();
    test_zero_copy_suite();
    test_crypto_suite();
    test_queue_suite();
    test_http_parser_suite();
    test_websocket_suite();
    test_muxer_suite();
    test_codec_suite();
    test_camera_suite();
    test_rga_suite();
    test_cv_math_suite();
    test_pipeline_stage_suite();

    PRINT_RESULTS();
    return _tests_fail > 0 ? 1 : 0;
}
