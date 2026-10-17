/*
 * test_plugin.c — Full plugin lifecycle test.
 *
 * Tests: install → start → stop → uninstall
 * Uses the direct ml_plugin_* API (not message bus).
 *
 * Linux-only: requires fork, signals, workspace paths.
 */
#include "test_framework.h"
#include <unistd.h>
#include <sys/stat.h>
#include "../include/ml_types.h"
#include "../include/ml_code.h"
#include "../include/ml_config.h"
#include "../include/ml_pool.h"
#include "../include/ml_string.h"
#include "../include/ml_log.h"
#include "../include/ml_timer.h"
#include "../../mlfw/include/mlfw/mlfw_module.h"
#include "../../mlfw/include/mlfw/mlfw_plugin.h"
#include "../../mlfw/include/mlfw/mlfw_process.h"
#include "../../mlfw/src/mlfw_plugin.c"

/* ---- Stubs for external dependencies ---- */

static ml_pool_t     g_test_pool;
static ml_log_t      g_test_log;
static char          g_test_workspace[256];
static char          g_test_tempspace[256];
static char          g_test_pluginspace[256];

/* Daemon globals referenced by plugin modules */
ml_timer_tree_t  *g_ml_timer_tree = NULL;

char *ml_process_workspace(void)  { return g_test_workspace; }
char *ml_process_tempspace(void)  { return g_test_tempspace; }
char *ml_process_pluginspace(void){ return g_test_pluginspace; }
char *ml_process_logspace(void)   { return g_test_tempspace; }
char *ml_process_configfile(void) { return "mlfw.conf"; }
char *ml_process_version(void)    { return "1.0.0"; }
void  ml_setproctitle(char *t)    { (void)t; }

/* cgroup stub (no-op in test environment) */
ml_int_t ml_plugin_cgroup_create(const char *n, const void *c, ml_int_t p)
{ (void)n; (void)c; (void)p; return ML_OK; }
void     ml_plugin_cgroup_remove(const char *n)
{ (void)n; }

/* health check stubs */
ml_int_t ml_plugin_health_init(ml_timer_tree_t *timer_tree)
{ (void)timer_tree; return ML_OK; }
void     ml_plugin_health_exit(void) {}

/* ---- Test cases ---- */

static void test_plugin_install(void)
{
    ml_cycle_config_t cfg;
    ml_plugin_params_t params;
    ml_int_t ret;
    char script_path[512];

    ml_memzero(&cfg, sizeof(cfg));
    cfg.m_pool = &g_test_pool;
    cfg.m_log  = &g_test_log;

    ret = ml_plugin_module_config(&cfg);
    ASSERT_EQ(ret, ML_OK, "module config");

    ret = ml_plugin_module_init(&cfg);
    ASSERT_EQ(ret, ML_OK, "module init (free-list)");

    /* Prepare plugin install params */
    ml_snprintf(script_path, sizeof(script_path), "%s/test_plugin.sh",
                g_test_pluginspace);

    ml_memzero(&params, sizeof(params));
    params.m_type  = MLFW_PLUGIN_TYPE_SCRIPT;
    params.m_index = 1;
    ml_strncpy(params.m_name, "test_plugin", sizeof(params.m_name) - 1);
    ml_strncpy(params.m_path, g_test_workspace, sizeof(params.m_path) - 1);
    ml_strncpy(params.m_script, script_path, sizeof(params.m_script) - 1);

    ret = ml_plugin_install(&params);
    ASSERT_EQ(ret, ML_OK, "plugin install");
}

static void test_plugin_duplicate_rejected(void)
{
    ml_plugin_params_t params;
    ml_int_t ret;

    ml_memzero(&params, sizeof(params));
    params.m_type = MLFW_PLUGIN_TYPE_SCRIPT;
    ml_strncpy(params.m_name, "test_plugin", sizeof(params.m_name) - 1);
    ml_strncpy(params.m_script, "/dev/null", sizeof(params.m_script) - 1);

    ret = ml_plugin_install(&params);
    ASSERT_EQ(ret, ML_EEXIST, "duplicate install rejected");
}

static void test_plugin_start_stop(void)
{
    ml_int_t ret;

    ret = ml_plugin_start("test_plugin");
    ASSERT_EQ(ret, ML_OK, "plugin start (script spawn)");

    /* Script plugins are async (system() call), wait briefly */
    usleep(100000);

    ret = ml_plugin_stop("test_plugin");
    ASSERT_EQ(ret, ML_OK, "plugin stop (script cancel)");

    /* Verify not-found handling */
    ret = ml_plugin_start("nonexistent_plugin");
    ASSERT_EQ(ret, ML_ENOEXT, "start nonexistent returns ENOEXT");

    ret = ml_plugin_stop("nonexistent_plugin");
    ASSERT_EQ(ret, ML_ENOEXT, "stop nonexistent returns ENOEXT");
}

static void test_plugin_uninstall(void)
{
    ml_int_t ret;
    ml_int_t size;
    ml_plugin_params_t params;
    char buf[512];
    char script_path[512];

    /* Install another plugin */
    ml_snprintf(script_path, sizeof(script_path), "%s/test_plugin.sh",
                g_test_pluginspace);
    ml_memzero(&params, sizeof(params));
    params.m_type  = MLFW_PLUGIN_TYPE_SCRIPT;
    params.m_index = 2;
    ml_strncpy(params.m_name, "plugin_b", sizeof(params.m_name) - 1);
    ml_strncpy(params.m_path, g_test_workspace, sizeof(params.m_path) - 1);
    ml_strncpy(params.m_script, script_path, sizeof(params.m_script) - 1);

    ret = ml_plugin_install(&params);
    ASSERT_EQ(ret, ML_OK, "install plugin_b");

    /* Start it */
    ret = ml_plugin_start("plugin_b");
    ASSERT_EQ(ret, ML_OK, "start plugin_b");
    usleep(50000);

    /* Uninstall (should stop first) */
    ret = ml_plugin_uninstall("plugin_b");
    ASSERT_EQ(ret, ML_OK, "uninstall plugin_b (auto-stop)");

    /* Verify removed */
    ret = ml_plugin_start("plugin_b");
    ASSERT_EQ(ret, ML_ENOEXT, "plugin_b should be gone after uninstall");

    /* Array should still have test_plugin */
    ret = ml_plugin_array(NULL, buf, sizeof(buf), &size);
    ASSERT_EQ(ret, ML_OK, "plugin array list");
}

static void test_plugin_reset(void)
{
    ml_int_t ret;

    ret = ml_plugin_start("test_plugin");
    ASSERT_EQ(ret, ML_OK, "start before reset");
    usleep(50000);

    ret = ml_plugin_reset((void *)"test_plugin");
    ASSERT_EQ(ret, ML_OK, "plugin restart (reset)");
    usleep(50000);

    ret = ml_plugin_stop("test_plugin");
    ASSERT_EQ(ret, ML_OK, "stop after reset");
}

static void test_plugin_null_checks(void)
{
    ASSERT_EQ(ml_plugin_install(NULL), ML_EPARAM, "NULL params");
    ASSERT_EQ(ml_plugin_uninstall(NULL), ML_EPARAM, "uninstall NULL name");
    ASSERT_EQ(ml_plugin_start(NULL), ML_EPARAM, "start NULL name");
    ASSERT_EQ(ml_plugin_stop(NULL), ML_EPARAM, "stop NULL name");
    ASSERT_EQ(ml_plugin_inload(NULL), ML_EPARAM, "inload NULL");
}

static void cleanup(void)
{
    ml_int_t i;
    /* Stop any remaining plugins */
    ml_plugin_stop("test_plugin");
    usleep(50000);
    ml_plugin_uninstall("test_plugin");
    ml_plugin_module_exit();
}

void test_plugin_suite(void)
{
    char cwd[256];
    char cmd[512];

    /* Set up workspace directories for the test */
    getcwd(cwd, sizeof(cwd));
    ml_snprintf(g_test_workspace, sizeof(g_test_workspace), "%s/_plugin_test", cwd);
    ml_snprintf(g_test_tempspace, sizeof(g_test_tempspace), "%s/_plugin_test/tmp", cwd);
    ml_snprintf(g_test_pluginspace, sizeof(g_test_pluginspace), "%s/_plugin_test/plugins", cwd);

    mkdir(g_test_workspace, 0755);
    mkdir(g_test_tempspace, 0755);
    mkdir(g_test_pluginspace, 0755);

    /* Copy the test script into pluginspace so spawn can find it */
    ml_snprintf(cmd, sizeof(cmd), "cp %s/test_plugin.sh %s/test_plugin.sh 2>/dev/null",
                cwd, g_test_pluginspace);
    system(cmd);

    ml_pool_create(&g_test_pool, 4096);

    TEST_SUITE("Plugin Lifecycle");
    RUN_TEST("install → init free-list", test_plugin_install());
    RUN_TEST("duplicate install rejected", test_plugin_duplicate_rejected());
    RUN_TEST("start → stop script plugin", test_plugin_start_stop());
    RUN_TEST("install → start → uninstall", test_plugin_uninstall());
    RUN_TEST("start → reset (restart)", test_plugin_reset());
    RUN_TEST("null parameter checks", test_plugin_null_checks());

    cleanup();
    ml_pool_destroy(&g_test_pool);
    rmdir(g_test_pluginspace);
    rmdir(g_test_tempspace);
    rmdir(g_test_workspace);
}
