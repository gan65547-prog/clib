/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_MODULE_H
#define MLFW_MODULE_H

#include "ml_types.h"
#include "ml_log.h"
#include "ml_pool.h"

/* Auto-assignment: set m_no to MLFW_MODULE_ID_AUTO (0) and the daemon
 * will assign a unique ID from [MLFW_MODULE_ID_AUTO_BASE, MLFW_MODULE_ID_AUTO_MAX).
 * Use a fixed ID when other modules need to address this one at compile time. */
#define MLFW_MODULE_ID_AUTO        0
#define MLFW_MODULE_ID_AUTO_BASE   0x9000
#define MLFW_MODULE_ID_AUTO_MAX    0xFF00  /* exclusive upper bound (256 slots) */

/* Forward declarations */
typedef struct ml_module_s       ml_module_t;
typedef struct ml_cycle_config_s ml_cycle_config_t;

/* Module lifecycle callbacks */
typedef ml_int_t (*ml_module_config_pt)(ml_cycle_config_t *cycle);
typedef ml_int_t (*ml_module_init_pt)(ml_cycle_config_t *cycle);
typedef void      (*ml_module_exit_pt)(void);
typedef ml_int_t (*ml_module_handler_pt)(ml_module_t *m, void *conn, void *buf, ml_int_t blen, ml_int_t *plen);
typedef ml_int_t (*ml_module_check_pt)(ml_module_t *m, void *buf, ml_int_t blen);

/*
 * Global configuration context.
 * Created once at startup, passed to all modules during config/init.
 */
struct ml_cycle_config_s {
    char   *m_path;   /* Config file path */
    void   *m_obj;    /* Parsed config object (JSON or other) */
    ml_log_t  *m_log;
    ml_pool_t *m_pool;
};

/*
 * Module descriptor.
 * Each module (core or business) provides one of these as a static global.
 * Modules are registered via NULL-terminated arrays (g_ml_cores[] / g_ml_modules[]).
 */
struct ml_module_s {
    ml_int_t  m_sn;         /* Sequence number */
    ml_int_t  m_no;         /* Module number (message bus address).
                                Set to MLFW_MODULE_ID_AUTO for auto-assignment. */
    char     *m_name;       /* Human-readable name */

    ml_module_config_pt  f_config;   /* Parse config from ml_cycle_config_t */
    ml_module_init_pt    f_init;     /* One-time initialisation */
    ml_module_exit_pt    f_exit;     /* Cleanup on shutdown */

    ml_module_handler_pt f_handler;  /* Handle incoming message */
    ml_module_check_pt   f_check;    /* Returns true if this module handles the message */

    /* Init-order dependencies: names of modules that must be initialised
     * before this one. NULL or m_dep_count == 0 means no dependencies.
     * Placed at end so existing designated initializers continue to work. */
    char    **m_dep_names;
    ml_int_t  m_dep_count;
};

/* Convenience typedef: core modules share the same struct */
typedef ml_module_t ml_core_t;

/*
 * Framework globals — declared by the daemon, used by modules.
 * Expected to be defined in the application's main file.
 */
extern ml_module_t      *g_ml_modules[];   /* Business modules, NULL-terminated */
extern ml_core_t        *g_ml_cores[];     /* Core modules, NULL-terminated */
extern ml_cycle_config_t *g_ml_cycle_config;
extern ml_log_t           g_ml_log;

#endif
