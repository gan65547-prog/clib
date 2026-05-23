/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_PLUGIN_H
#define MLFW_PLUGIN_H

#include "ml_types.h"
#include "ml_config.h"
#include "ml_timer.h"
#include "mlfw_module.h"

#define MLFW_MAX_PROJECT  32

/* Plugin types */
#define MLFW_PLUGIN_TYPE_NONE   0
#define MLFW_PLUGIN_TYPE_CHILD  1   /* fork() + dlopen() .so plugin */
#define MLFW_PLUGIN_TYPE_SCRIPT 2   /* shell script plugin */

#define ML_PLUGIN_DEP_MAX  8    /* max dependencies per plugin */

/*
 * Plugin descriptor.
 * Represent one installed plugin/bundle.
 */
typedef struct ml_plugin_s {
    void          *m_data;    /* free-list pointer */
    ml_int_t       m_out;     /* default timeout (ms) */
    ml_int_t       m_type;    /* MLFW_PLUGIN_TYPE_* */
    ml_int_t       m_pid;     /* child PID */
    ml_int_t       m_index;   /* index passed to plugin */
    ml_int_t       m_start;   /* start level (1-15, smaller starts first) */
    ml_uint_t      m_state:1; /* running */
    ml_uint_t      m_exit:1;  /* exit flag for respawn */
    ml_uint_t      m_health:1; /* enable health check monitoring */

    /* Resource limits (0 = use defaults) */
    ml_int_t       m_cpu_quota_us;   /* CPU quota in µs per period */
    ml_int_t       m_cpu_period_us;  /* CPU period in µs (default 100000) */
    ml_int64_t     m_mem_max;        /* memory.max in bytes */

    /* Dependency graph */
    ml_int_t       m_dep_count;      /* number of dependencies */
    char           m_deps[ML_PLUGIN_DEP_MAX][ML_MAX_NAME_LENGTH];

    /* Runtime state (managed by health checker) */
    ml_int_t       m_fail_count;     /* consecutive health failures */
    ml_int_t       m_restart_count;  /* total restart count */

    ml_timer_node_t m_timer;

    char m_name[ML_MAX_NAME_LENGTH];
    char m_vers[ML_MAX_NAME_LENGTH];
    char m_path[ML_MAX_PATH_LENGTH];
    char m_rcf[ML_MAX_PATH_LENGTH];   /* script path */
    char m_plin[ML_MAX_PATH_LENGTH];  /* plugin .so path */

    ml_int_t (*f_spawn)(struct ml_plugin_s *);
    ml_int_t (*f_cancel)(struct ml_plugin_s *);
} ml_plugin_t;

/* Plugin module extern (for embedding in g_ml_modules[]) */
extern ml_module_t g_ml_module_plugin;

/* ---- Child process management ---- */
void ml_children_get_status(void);
void ml_children_respawn(void);

/* ---- Resource limits (cgroup v2) ---- */
ml_int_t ml_plugin_cgroup_create(const char *name, const void *cfg, ml_int_t pid);
void     ml_plugin_cgroup_remove(const char *name);

/* ---- Health check ---- */
ml_int_t ml_plugin_health_init(struct ml_timer_tree_s *timer_tree);
void     ml_plugin_health_exit(void);

/* ---- Dependency orchestration ---- */
ml_int_t ml_plugin_startup_order(ml_int_t *order, ml_int_t *count);
ml_int_t ml_plugin_startup_all(void);
ml_int_t ml_plugin_shutdown_all(void);

/* Internal: access to plugin array and lookup (used by health/orchestrate) */
extern ml_plugin_t *g_plugin_array;
ml_plugin_t *ml_plugin_lookup(const char *name);

/*
 * Plugin install parameters — passed to ml_plugin_inload().
 * Replaces the JSON-object approach for direct C API usage.
 */
typedef struct ml_plugin_params_s {
    ml_int_t  m_type;                    /* MLFW_PLUGIN_TYPE_CHILD or SCRIPT */
    ml_int_t  m_index;                   /* instance index */
    ml_int_t  m_health;                  /* enable health check (default 1 for CHILD) */
    ml_int_t  m_start_timeout_ms;       /* startup handshake timeout (default 3000) */
    ml_int_t  m_cpu_quota_us;           /* CPU quota in µs per period (0 = no limit) */
    ml_int_t  m_cpu_period_us;          /* CPU period in µs (default 100000) */
    ml_int64_t m_mem_max;              /* memory.max in bytes (0 = no limit) */
    char      m_name[ML_MAX_NAME_LENGTH];
    char      m_path[ML_MAX_PATH_LENGTH];  /* plugin workspace dir */
    char      m_script[ML_MAX_PATH_LENGTH]; /* shell script path (type=SCRIPT) */
    char      m_sofile[ML_MAX_PATH_LENGTH]; /* .so file path (type=CHILD) */
} ml_plugin_params_t;

/* ---- Plugin operations (called via message bus or direct API) ---- */
ml_int_t ml_plugin_inload(void *json_obj);    /* install (json_obj or ml_plugin_params_t*) */
ml_int_t ml_plugin_unload(void *json_obj);    /* uninstall by name (const char*) */
ml_int_t ml_plugin_spawn(void *json_obj);     /* start by name (const char*) */
ml_int_t ml_plugin_cancel(void *json_obj);    /* stop by name (const char*) */
ml_int_t ml_plugin_level(void *json_obj);     /* change start priority */
ml_int_t ml_plugin_reset(void *json_obj);     /* restart by name (const char*) */
ml_int_t ml_plugin_array(void *json_obj, void *buf, ml_int_t len, ml_int_t *size); /* list */

/* Direct API — use these in tests or when not going through message bus */
ml_int_t ml_plugin_install(ml_plugin_params_t *params);
ml_int_t ml_plugin_uninstall(const char *name);
ml_int_t ml_plugin_start(const char *name);
ml_int_t ml_plugin_stop(const char *name);

#endif
