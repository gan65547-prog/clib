/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

/*
 * app.c — Example mlfw application.
 *
 * This is ALL you need to write to build a daemon on the mlfw framework.
 * Define your core modules and business modules in NULL-terminated arrays,
 * link with main.o + libclib, and you're done.
 *
 * Build:
 *   make -C ..                        # builds libclib + main.o
 *   gcc -o myapp app.c ../src/mlfw/main.o -L.. -lclib -lpthread -lm
 */

#include "ml_code.h"
#include "mlfw_daemon.h"
#include "mlfw_message_bus.h"
#include "mlfw_plugin.h"

/* ---- Example business module ---- */

static ml_int_t example_config(ml_cycle_config_t *cfg)
{
    (void)cfg;
    ml_log_print(&g_ml_log, ML_LOG_INFO, "%s", "example module config");
    return ML_OK;
}

static ml_int_t example_init(ml_cycle_config_t *cfg)
{
    (void)cfg;
    ml_log_print(&g_ml_log, ML_LOG_INFO, "%s", "example module init");
    return ML_OK;
}

static void example_exit(void)
{
    ml_log_print(&g_ml_log, ML_LOG_INFO, "%s", "example module exit");
}

static ml_int_t example_check(ml_module_t *m, void *buf, ml_int_t blen)
{
    (void)m; (void)buf; (void)blen;
    return 0; /* this module doesn't handle messages directly */
}

static ml_int_t example_handler(ml_module_t *m, void *conn, void *buf,
                                 ml_int_t blen, ml_int_t *plen)
{
    (void)m; (void)conn; (void)buf; (void)blen;
    *plen = 0;
    return 0;
}

static ml_module_t g_example_module = {
    0,          /* version */
    0x2000,     /* module ID */
    "example",  /* name */
    example_config,
    example_init,
    example_exit,
    example_handler,
    example_check,
};

/* ---- Module arrays (these are what mlfw main() consumes) ---- */

ml_core_t *g_ml_cores[] = {
    /* Core framework modules */
    /* Add your core modules here, NULL-terminated */
    NULL
};

ml_module_t *g_ml_modules[] = {
#ifdef __linux__
    &g_ml_module_message_bus,  /* Unix socket IPC (required on Linux) */
    &g_ml_module_plugin,       /* Plugin manager (optional)           */
#endif
    &g_example_module,         /* Your business module */
    NULL
};
