/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_PLUGIN_H
#define MLFW_PLUGIN_H

#include "ml_types.h"
#include "ml_code.h"
#include "mlfw_module.h"

#define MLFW_MAX_PROJECT    64
#define MLFW_PLUGIN_NAME_LEN 64
#define MLFW_PLUGIN_PATH_LEN 256

typedef enum {
    MLFW_PLUGIN_TYPE_NONE = 0,
    MLFW_PLUGIN_TYPE_CHILD,   /* fork + dlopen .so */
    MLFW_PLUGIN_TYPE_SCRIPT,  /* fork + exec script */
} mlfw_plugin_type_e;

typedef struct ml_plugin_s {
    ml_int_t             m_reserved;
    ml_int_t             m_id;
    ml_int_t             m_index;
    ml_int_t             m_state;
    ml_int_t             m_exit;
    ml_int_t             m_out;
    ml_int_t             m_start;
    ml_int_t             m_status;
    ml_int_t             m_restart;
    ml_int_t             m_max_restarts;
    ml_int_t             m_health;
    ml_int_t             m_cpu_period_us;
    ml_int_t             m_cpu_quota_us;
    ml_int_t             m_mem_max;
    char                 m_name[MLFW_PLUGIN_NAME_LEN];
    char                 m_plin[MLFW_PLUGIN_PATH_LEN];
    char                 m_path[MLFW_PLUGIN_PATH_LEN];
    char                 m_rcf[MLFW_PLUGIN_PATH_LEN];
    mlfw_plugin_type_e   m_type;
    pid_t                m_pid;
    void                *m_data;
    ml_module_t         *m_module;
    void               (*f_start)(struct ml_plugin_s *p);
    void               (*f_stop)(struct ml_plugin_s *p);
    ml_int_t           (*f_spawn)(struct ml_plugin_s *p);
    ml_int_t           (*f_cancel)(struct ml_plugin_s *p);
} ml_plugin_t;

typedef struct {
    char                 m_name[MLFW_PLUGIN_NAME_LEN];
    char                 m_path[MLFW_PLUGIN_PATH_LEN];
    char                 m_sofile[MLFW_PLUGIN_PATH_LEN];
    char                 m_script[MLFW_PLUGIN_PATH_LEN];
    ml_int_t             m_index;
    ml_int_t             m_type;
    ml_int_t             m_start_timeout_ms;
    ml_int_t             m_health;
    ml_int_t             m_cpu_period_us;
    ml_int_t             m_cpu_quota_us;
    ml_int_t             m_mem_max;
} ml_plugin_params_t;

ml_plugin_t *ml_plugin_lookup(const char *name);

/* Plugin management */
ml_int_t ml_plugin_install(ml_plugin_params_t *params);

/* Health checker */
void ml_plugin_health_init(ml_timer_tree_t *tree);
void ml_plugin_health_exit(void);

/* Cgroup integration (Linux only, stubbed on other platforms) */
ml_int_t ml_plugin_cgroup_create(const char *name, const void *cfg, ml_int_t pid);
void ml_plugin_cgroup_remove(const char *name);

#endif /* MLFW_PLUGIN_H */
