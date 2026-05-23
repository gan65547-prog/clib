/*
 * test_ipc.c — Cross-Plugin IPC verification.
 *
 * Verifies that two plugins (ipc_ping + ipc_echo) can communicate
 * via the message bus dispatch mechanism:
 *   ping receives a SEND event → constructs message for echo →
 *   echo's f_handler is called → echo replies with same payload →
 *   ping stores the result for verification.
 *
 * Plugins are compiled as separate .o files and linked in,
 * matching how the real daemon loads them.
 */
#include "test_framework.h"
#include <string.h>

#include "../include/ml_types.h"
#include "../include/ml_code.h"
#include "../include/ml_config.h"
#include "../include/ml_pool.h"
#include "../include/ml_string.h"
#include "../include/ml_log.h"
#include "../include/mlfw/mlfw_module.h"
#include "../include/mlfw/mlfw_message.h"

#include "../../mlfw/plugins/ipc_echo/ml_plugin_ipc_echo.h"
#include "../../mlfw/plugins/ipc_ping/ml_plugin_ipc_ping.h"

/* ---- Stubs ---- */
static ml_pool_t g_test_pool;
static ml_log_t  g_test_log;

/* Daemon globals referenced by plugin modules */
ml_log_t g_ml_log;

/* ---- Helpers ---- */

static ml_int_t call_handler(ml_module_t *m, void *buf, ml_int_t blen, ml_int_t *plen)
{
    return m->f_handler(m, NULL, buf, blen, plen);
}

static ml_int_t call_check(ml_module_t *m, void *buf, ml_int_t blen)
{
    return m->f_check(m, buf, blen);
}

/* ---- Tests ---- */

static void test_ipc_init_both(void)
{
    ml_cycle_config_t cfg;
    ml_int_t ret;

    ml_memzero(&cfg, sizeof(cfg));
    cfg.m_pool = &g_test_pool;

    ret = g_ml_module_ipc_echo.f_config(&cfg);
    ASSERT_EQ(ret, ML_OK, "echo config");
    ret = g_ml_module_ipc_echo.f_init(&cfg);
    ASSERT_EQ(ret, ML_OK, "echo init");

    ret = g_ml_module_ipc_ping.f_config(&cfg);
    ASSERT_EQ(ret, ML_OK, "ping config");
    ret = g_ml_module_ipc_ping.f_init(&cfg);
    ASSERT_EQ(ret, ML_OK, "ping init");
}

static void test_ipc_ping_send_triggers_echo(void)
{
    ml_message_header_t hdr;
    const char *msg = "hello from ping!";
    ml_int_t msg_len = (ml_int_t)strlen(msg) + 1;
    ml_int_t plen;
    ml_int_t ret;

    ml_memzero(&hdr, sizeof(hdr));
    ML_MSGHDR_SET_DEST(&hdr, MLFW_PLUGIN_IPC_PING_ID);
    ML_MSGHDR_SET_RESP(&hdr, MLFW_IPC_PING_EVENT_SEND);
    ML_MSGHDR_SET_LENGTH(&hdr, msg_len);

    ml_uint8_t buf[sizeof(ml_message_header_t) + 256];
    ml_memcpy(buf, &hdr, sizeof(hdr));
    ml_memcpy(buf + sizeof(hdr), msg, msg_len);

    ret = call_handler(&g_ml_module_ipc_ping, buf, sizeof(buf), &plen);

    ASSERT_EQ(ret, 1, "ping handler processed");
    ASSERT_EQ(ml_ipc_ping_result(), ML_OK, "echo returned ML_OK");
    ASSERT(strcmp(ml_ipc_ping_response(), "hello from ping!") == 0,
           "echo response matches sent message");
}

static void test_ipc_echo_module_check(void)
{
    ml_message_header_t hdr;

    ml_memzero(&hdr, sizeof(hdr));
    ML_MSGHDR_SET_DEST(&hdr, MLFW_PLUGIN_IPC_ECHO_ID);
    ASSERT_EQ(call_check(&g_ml_module_ipc_echo, &hdr, sizeof(hdr)),
              1, "echo check matches own ID");

    ML_MSGHDR_SET_DEST(&hdr, 0x9999);
    ASSERT_EQ(call_check(&g_ml_module_ipc_echo, &hdr, sizeof(hdr)),
              0, "echo check rejects wrong ID");
}

static void test_ipc_ping_module_check(void)
{
    ml_message_header_t hdr;

    ml_memzero(&hdr, sizeof(hdr));
    ML_MSGHDR_SET_DEST(&hdr, MLFW_PLUGIN_IPC_PING_ID);
    ASSERT_EQ(call_check(&g_ml_module_ipc_ping, &hdr, sizeof(hdr)),
              1, "ping check matches own ID");

    ML_MSGHDR_SET_DEST(&hdr, 0x9999);
    ASSERT_EQ(call_check(&g_ml_module_ipc_ping, &hdr, sizeof(hdr)),
              0, "ping check rejects wrong ID");
}

static void test_ipc_direct_echo(void)
{
    ml_message_header_t hdr;
    const char *msg = "direct echo test";
    ml_int_t msg_len = (ml_int_t)strlen(msg) + 1;
    ml_int_t plen;
    ml_int_t ret;

    ml_memzero(&hdr, sizeof(hdr));
    ML_MSGHDR_SET_DEST(&hdr, MLFW_PLUGIN_IPC_ECHO_ID);
    ML_MSGHDR_SET_RESP(&hdr, MLFW_IPC_ECHO_EVENT_REQ);
    ML_MSGHDR_SET_LENGTH(&hdr, msg_len);

    ml_uint8_t buf[sizeof(ml_message_header_t) + 256];
    ml_memcpy(buf, &hdr, sizeof(hdr));
    ml_memcpy(buf + sizeof(hdr), msg, msg_len);

    ret = call_handler(&g_ml_module_ipc_echo, buf, sizeof(buf), &plen);

    ASSERT_EQ(ret, 1, "echo handler processed");
    ASSERT_EQ(ml_ipc_echo_last_len(), msg_len, "echo stored correct length");
    ASSERT(strcmp(ml_ipc_echo_last_msg(), msg) == 0,
           "echo stored correct message");
}

static void test_ipc_null_checks(void)
{
    ASSERT_EQ(call_check(&g_ml_module_ipc_ping, NULL, 0),
              0, "ping check NULL buf");
    ASSERT_EQ(call_check(&g_ml_module_ipc_echo, NULL, 0),
              0, "echo check NULL buf");
}


static void cleanup(void)
{
    g_ml_module_ipc_ping.f_exit();
    g_ml_module_ipc_echo.f_exit();
}

void test_ipc_suite(void)
{
    ml_pool_create(&g_test_pool, 4096);

    TEST_SUITE("IPC Cross-Plugin Communication");
    RUN_TEST("init both plugins", test_ipc_init_both());
    RUN_TEST("ping -> echo round trip", test_ipc_ping_send_triggers_echo());
    RUN_TEST("echo module check routing", test_ipc_echo_module_check());
    RUN_TEST("ping module check routing", test_ipc_ping_module_check());
    RUN_TEST("direct echo handler", test_ipc_direct_echo());
    RUN_TEST("null parameter checks", test_ipc_null_checks());

    ml_pool_destroy(&g_test_pool);
    cleanup();
}
