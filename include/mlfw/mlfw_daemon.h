/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_DAEMON_H
#define MLFW_DAEMON_H

#include "ml_types.h"
#include "ml_log.h"
#include "ml_pool.h"
#include "ml_event.h"
#include "ml_timer.h"
#include "mlfw_module.h"
#include "mlfw_process.h"

/*
 * External globals that the daemon provides and modules reference.
 * Define these in your application's main.c.
 */
extern ml_pool_t           g_ml_pool;
extern ml_log_t            g_ml_log;
extern ml_event_t         *g_ml_event;
extern ml_timer_tree_t    *g_ml_timer_tree;
extern ml_cycle_config_t  *g_ml_cycle_config;

/* ---- Daemon lifecycle functions ---- */

/* Parse JSON config from workspace, returns config object */
ml_int_t ml_daemon_config_load(ml_cycle_config_t *cfg);

/* Assign IDs to modules with m_no == MLFW_MODULE_ID_AUTO.
 * Detect duplicate manual IDs. Call BEFORE ml_daemon_modules_config(). */
ml_int_t ml_daemon_modules_id_assign(void);

/* Access the active module array (set by ml_daemon_run).
 * Use this instead of g_ml_modules to ensure message dispatch
 * and module iteration are consistent with daemon parameters. */
ml_module_t **ml_daemon_get_modules(void);

/* Look up a module by name. Searches cores first, then business modules.
 * Returns the module pointer, or NULL if not found.
 * Useful for runtime discovery of auto-assigned modules. */
ml_module_t *ml_module_lookup_by_name(const char *name);

/* Iterate all cores+business modules, call f_config */
ml_int_t ml_daemon_modules_config(void);

/* Iterate all cores+business modules, call f_init */
ml_int_t ml_daemon_modules_init(void);

/* Iterate reverse, call f_exit */
void ml_daemon_modules_exit(void);

/* Set file descriptor limit */
ml_int_t ml_daemon_set_ulimit(ml_int_t maxfd);

/* Create workspace directories */
ml_int_t ml_daemon_workspace_init(void);

/*
 * Main event loop — blocks until g_ml_quit is set.
 * Combines epoll + timer wheel into a unified wait.
 */
void ml_daemon_cycle(void);

/*
 * Complete lifecycle — call from main():
 *   ml_pool_create → ml_process_init → ml_daemon_workspace_init
 *   → ml_log_create → ml_daemon_config_load → ml_daemon_modules_config
 *   → ml_init_signals → ml_daemon_modules_init → ml_daemon_cycle
 *   → ml_daemon_modules_exit → ml_log_destroy → ml_pool_destroy
 */
ml_int_t ml_daemon_run(int argc, char **argv, ml_core_t **cores, ml_module_t **modules);

#endif
