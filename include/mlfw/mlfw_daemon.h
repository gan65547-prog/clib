/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_DAEMON_H
#define MLFW_DAEMON_H

#include "ml_code.h"
#include "ml_config.h"
#include "ml_log.h"
#include "ml_event.h"
#include "ml_timer.h"
#include "mlfw_module.h"

/* Framework globals */
extern ml_pool_t          g_ml_pool;
extern ml_log_t           g_ml_log;
extern ml_event_t        *g_ml_event;
extern ml_timer_tree_t   *g_ml_timer_tree;
extern ml_cycle_config_t *g_ml_cycle_config;

/* Daemon API */
ml_int_t ml_daemon_run(int argc, char **argv, ml_core_t **cores,
                        ml_module_t **modules);
ml_module_t **ml_daemon_get_modules(void);
ml_module_t *ml_module_lookup_by_name(const char *name);

ml_int_t ml_daemon_set_ulimit(ml_int_t maxfd);
ml_int_t ml_daemon_workspace_init(void);
ml_int_t ml_daemon_config_load(ml_cycle_config_t *cfg);
ml_int_t ml_daemon_modules_id_assign(void);
ml_int_t ml_daemon_modules_config(void);
ml_int_t ml_daemon_modules_init(void);
void ml_daemon_modules_exit(void);
void ml_daemon_cycle(void);

#endif /* MLFW_DAEMON_H */
