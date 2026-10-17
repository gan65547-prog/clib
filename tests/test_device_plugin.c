/*
 * test_device_plugin.c — Device Management Plugin test.
 *
 * Tests: register → list → get → update → delete → null checks
 * Uses the module struct function pointers (not #include of .c file).
 */
#include "test_framework.h"
#include <string.h>
#include "../include/ml_types.h"
#include "../include/ml_code.h"
#include "../include/ml_config.h"
#include "../include/ml_pool.h"
#include "../include/ml_string.h"
#include "../include/ml_log.h"
#include "../../mlfw/include/mlfw/mlfw_module.h"
#include "../../mlfw/include/mlfw/mlfw_message.h"
#include "../../mlfw/plugins/device/ml_plugin_device.h"

/* ---- Stubs ---- */
static ml_pool_t g_test_pool;

/* ---- Test cases ---- */

static void test_device_register(void)
{
    ml_device_info_t dev;
    ml_int_t ret;

    ml_cycle_config_t cfg;
    ml_memzero(&cfg, sizeof(cfg));
    cfg.m_pool = &g_test_pool;
    ret = g_ml_module_device.f_config(&cfg);
    ASSERT_EQ(ret, ML_OK, "device module config");

    ret = g_ml_module_device.f_init(&cfg);
    ASSERT_EQ(ret, ML_OK, "device module init (free-list)");

    /* Register first device */
    ml_memzero(&dev, sizeof(dev));
    ml_strncpy(dev.m_name, "camera-01", ML_DEVICE_NAME_LEN - 1);
    ml_strncpy(dev.m_type, "ip_camera", ML_DEVICE_TYPE_LEN - 1);
    ml_strncpy(dev.m_addr, "192.168.1.100", ML_DEVICE_ADDR_LEN - 1);
    dev.m_status = ML_DEVICE_ONLINE;

    ret = ml_device_register(&dev);
    ASSERT_EQ(ret, ML_OK, "register device camera-01");

    /* Register second device */
    ml_strncpy(dev.m_name, "sensor-temp", ML_DEVICE_NAME_LEN - 1);
    ml_strncpy(dev.m_type, "temperature_sensor", ML_DEVICE_TYPE_LEN - 1);
    ml_strncpy(dev.m_addr, "192.168.1.101", ML_DEVICE_ADDR_LEN - 1);
    dev.m_status = ML_DEVICE_OFFLINE;

    ret = ml_device_register(&dev);
    ASSERT_EQ(ret, ML_OK, "register device sensor-temp");

    /* Register third device */
    ml_strncpy(dev.m_name, "gateway-main", ML_DEVICE_NAME_LEN - 1);
    ml_strncpy(dev.m_type, "gateway", ML_DEVICE_TYPE_LEN - 1);
    ml_strncpy(dev.m_addr, "10.0.0.1", ML_DEVICE_ADDR_LEN - 1);
    dev.m_status = ML_DEVICE_ONLINE;

    ret = ml_device_register(&dev);
    ASSERT_EQ(ret, ML_OK, "register device gateway-main");
}

static void test_device_update_existing(void)
{
    ml_device_info_t dev, out;
    ml_int_t ret;

    ml_memzero(&dev, sizeof(dev));
    ml_strncpy(dev.m_name, "camera-01", ML_DEVICE_NAME_LEN - 1);
    ml_strncpy(dev.m_type, "ip_camera", ML_DEVICE_TYPE_LEN - 1);
    ml_strncpy(dev.m_addr, "192.168.1.200", ML_DEVICE_ADDR_LEN - 1);
    dev.m_status = ML_DEVICE_OFFLINE;

    ret = ml_device_register(&dev);
    ASSERT_EQ(ret, ML_OK, "update existing device");

    ret = ml_device_get("camera-01", &out);
    ASSERT_EQ(ret, ML_OK, "get updated device");
    ASSERT_EQ(out.m_status, ML_DEVICE_OFFLINE, "status updated to offline");
}

static void test_device_list(void)
{
    ml_device_info_t buf[10];
    ml_int_t count = 0;
    ml_int_t ret;

    ret = ml_device_list(buf, 10, &count);
    ASSERT_EQ(ret, ML_OK, "list devices");
    ASSERT_EQ(count, 3, "device count is 3");
}

static void test_device_get(void)
{
    ml_device_info_t out;
    ml_int_t ret;

    ret = ml_device_get("sensor-temp", &out);
    ASSERT_EQ(ret, ML_OK, "get sensor-temp");
    ASSERT_EQ(out.m_status, ML_DEVICE_OFFLINE, "sensor status is offline");

    ret = ml_device_get("nonexistent", &out);
    ASSERT_EQ(ret, ML_ENOEXT, "get nonexistent returns ENOEXT");
}

static void test_device_delete(void)
{
    ml_device_info_t buf[10];
    ml_int_t count = 0;
    ml_int_t ret;

    ret = ml_device_delete("gateway-main");
    ASSERT_EQ(ret, ML_OK, "delete gateway-main");

    ret = ml_device_get("gateway-main", buf);
    ASSERT_EQ(ret, ML_ENOEXT, "deleted device not found");

    ret = ml_device_delete("nonexistent");
    ASSERT_EQ(ret, ML_ENOEXT, "delete nonexistent returns ENOEXT");

    ret = ml_device_list(buf, 10, &count);
    ASSERT_EQ(ret, ML_OK, "list after delete");
    ASSERT_EQ(count, 2, "count after delete is 2");
}

static void test_device_null_checks(void)
{
    ASSERT_EQ(ml_device_register(NULL), ML_EPARAM, "register NULL");
    ASSERT_EQ(ml_device_list(NULL, 0, NULL), ML_EPARAM, "list NULL params");
    ASSERT_EQ(ml_device_get(NULL, NULL), ML_EPARAM, "get NULL params");
    ASSERT_EQ(ml_device_get("test", NULL), ML_EPARAM, "get NULL out");
    ASSERT_EQ(ml_device_delete(NULL), ML_EPARAM, "delete NULL name");
}

static void test_device_message_handler(void)
{
    ml_message_header_t hdr;
    ml_int_t plen;

    /* Test LIST event via handler — does not check payload length */
    ml_memzero(&hdr, sizeof(hdr));
    ML_MSGHDR_SET_DEST(&hdr, MLFW_PLUGIN_DEVICE_ID);
    ML_MSGHDR_SET_RESP(&hdr, MLFW_DEVICE_EVENT_LIST);
    ML_MSGHDR_SET_LENGTH(&hdr, 0);

    {
        ml_uint8_t buf[sizeof(ml_message_header_t) + ML_DEVICE_MAX * sizeof(ml_device_info_t)];
        ml_memcpy(buf, &hdr, sizeof(hdr));

        ml_int_t result = g_ml_module_device.f_handler(
            &g_ml_module_device, NULL, buf,
            sizeof(buf), &plen);
        ASSERT_EQ(result, 1, "handler returns 1 on LIST");

        ml_message_header_t *rhdr = (ml_message_header_t *)buf;
        ASSERT_EQ(ML_MSGHDR_GET_CODE(rhdr), ML_OK, "handler LIST code is ML_OK");
        /* plen should include header + payload with device list */
        ASSERT_EQ(plen > (ml_int_t)sizeof(ml_message_header_t), 1,
                  "LIST response includes payload");
    }
}

static void test_device_fill_to_max(void)
{
    ml_device_info_t dev;
    ml_int_t i, ret;
    char name[ML_DEVICE_NAME_LEN];

    /*
     * We have 2 devices remaining (camera-01, sensor-temp).
     * gateway-main was deleted in the previous test.
     * Register up to ML_DEVICE_MAX, then overflow.
     */
    for (i = 2; i < ML_DEVICE_MAX; i++) {
        ml_memzero(&dev, sizeof(dev));
        ml_snprintf(name, sizeof(name), "device-%02d", i);
        ml_strncpy(dev.m_name, name, ML_DEVICE_NAME_LEN - 1);
        ml_strncpy(dev.m_type, "generic", ML_DEVICE_TYPE_LEN - 1);
        ml_strncpy(dev.m_addr, "0.0.0.0", ML_DEVICE_ADDR_LEN - 1);
        dev.m_status = ML_DEVICE_OFFLINE;

        ret = ml_device_register(&dev);
        ASSERT_EQ(ret, ML_OK, "register device up to max");
    }

    /* One more should fail (array full) */
    ml_memzero(&dev, sizeof(dev));
    ml_strncpy(dev.m_name, "overflow", ML_DEVICE_NAME_LEN - 1);
    ret = ml_device_register(&dev);
    ASSERT_EQ(ret, ML_ENOMEM, "register beyond ML_DEVICE_MAX fails");
}

static void test_device_module_check(void)
{
    ml_message_header_t hdr;

    ml_memzero(&hdr, sizeof(hdr));
    ML_MSGHDR_SET_DEST(&hdr, MLFW_PLUGIN_DEVICE_ID);

    ml_int_t match = g_ml_module_device.f_check(
        &g_ml_module_device, &hdr, sizeof(hdr));
    ASSERT_EQ(match, 1, "check matches device module ID");

    ML_MSGHDR_SET_DEST(&hdr, 0x9999);
    match = g_ml_module_device.f_check(
        &g_ml_module_device, &hdr, sizeof(hdr));
    ASSERT_EQ(match, 0, "check rejects wrong module ID");
}

static void cleanup(void)
{
    g_ml_module_device.f_exit();
}

void test_device_plugin_suite(void)
{
    ml_pool_create(&g_test_pool, 4096);

    TEST_SUITE("Device Management Plugin");
    RUN_TEST("register 3 devices", test_device_register());
    RUN_TEST("update existing device", test_device_update_existing());
    RUN_TEST("list all devices", test_device_list());
    RUN_TEST("get device by name", test_device_get());
    RUN_TEST("delete device", test_device_delete());
    RUN_TEST("null parameter checks", test_device_null_checks());
    RUN_TEST("message handler + check", test_device_message_handler());
    RUN_TEST("module check routing", test_device_module_check());
    RUN_TEST("fill to ML_DEVICE_MAX (64)", test_device_fill_to_max());

    cleanup();
    ml_pool_destroy(&g_test_pool);
}
