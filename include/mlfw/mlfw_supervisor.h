/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_SUPERVISOR_H
#define MLFW_SUPERVISOR_H

#include "ml_types.h"
#include "ml_code.h"
#include "mlfw_module.h"

/* Capacity limits */
#define MLFW_MAX_SESSIONS      32
#define MLFW_MAX_REGIONS      128
#define MLFW_SESSION_ID_LEN    64
#define MLFW_SESSION_LABEL_LEN 64

/* Pipeline crash recovery */
#define MLFW_PIPELINE_BACKOFF_BASE_MS  1000
#define MLFW_PIPELINE_BACKOFF_MAX_MS  30000

/* Session lifecycle states */
typedef enum {
    ML_SESSION_STATE_IDLE = 0,
    ML_SESSION_STATE_ACTIVE,
    ML_SESSION_STATE_PAUSED,
    ML_SESSION_STATE_COMPLETED,
    ML_SESSION_STATE_INTERRUPTED,
    ML_SESSION_STATE_RECOVERING,
} ml_session_state_e;

/* ---- Job session ---- */
typedef struct {
    char                m_id[MLFW_SESSION_ID_LEN];
    char                m_label[MLFW_SESSION_LABEL_LEN];
    ml_session_state_e  m_state;
    ml_int64_t          m_created_at;
    ml_int64_t          m_updated_at;
    ml_int_t            m_region_count;
} ml_job_session_t;

/* ---- Region session ---- */
typedef struct {
    char                m_id[MLFW_SESSION_ID_LEN];
    char                m_job_id[MLFW_SESSION_ID_LEN];
    ml_int_t            m_region_id;
    char                m_subject[64];
    ml_session_state_e  m_state;
    char                m_result_json[512];
    char                m_artifact_path[256];
} ml_region_session_t;

/* ---- Exponential backoff tracker ---- */
typedef struct {
    ml_int_t    m_restart_count;
    ml_int_t    m_backoff_ms;
    ml_int64_t  m_next_retry_at;
} ml_pipeline_backoff_t;

/* ---- Supervisor internal context ---- */
typedef struct {
    void                   *m_plugin;       /* ml_plugin_t * */
    ml_pipeline_backoff_t   m_backoff;
    ml_int_t                m_registered;
    ml_int64_t              m_last_heartbeat;
} ml_supervisor_ctx_t;

/* ---- Session Manager API ---- */

/* Job sessions */
ml_job_session_t    *ml_session_job_create(const char *label);
ml_int_t             ml_session_job_start(const char *session_id);
ml_int_t             ml_session_job_pause(const char *session_id);
ml_int_t             ml_session_job_stop(const char *session_id);
ml_int_t             ml_session_job_complete(const char *session_id);
ml_int_t             ml_session_job_mark_interrupted(const char *session_id);
ml_job_session_t    *ml_session_job_find(const char *session_id);
ml_int_t             ml_session_job_list(ml_job_session_t *buf, ml_int_t max);

/* Region sessions */
ml_region_session_t *ml_session_region_create(const char *job_id,
                                               ml_int_t region_id,
                                               const char *subject);
ml_int_t              ml_session_region_update(const char *region_id,
                                                const char *result_json);
ml_int_t              ml_session_region_set_artifact(const char *region_id,
                                                      const char *path);
ml_region_session_t  *ml_session_region_find(const char *region_id);
ml_int_t              ml_session_region_list_by_job(const char *job_id,
                                ml_region_session_t *buf, ml_int_t max);

/* Mark all active sessions as interrupted (called on pipeline crash) */
void ml_session_mark_all_interrupted(void);

/* Mark interrupted sessions as recovering (called on pipeline restart) */
void ml_session_mark_recovering(void);

/* Persistence */
ml_int_t ml_session_persist_save(void);
ml_int_t ml_session_persist_load(void);

/* Session manager lifecycle */
ml_int_t ml_session_manager_init(void);
void     ml_session_manager_exit(void);

/* ---- Supervisor API ---- */

/* Install and start the pipeline plugin */
ml_int_t ml_supervisor_pipeline_install(const char *binary_path,
                                         const char *config_path);
ml_int_t ml_supervisor_pipeline_start(void);
ml_int_t ml_supervisor_pipeline_stop(void);
ml_int_t ml_supervisor_pipeline_reload(void);

/* Crash recovery — called from timer / main loop */
void ml_supervisor_check_and_respawn(void);

/* Pipeline heartbeat handler */
void ml_supervisor_on_heartbeat(void);
void ml_supervisor_on_register(void);

/* Query */
ml_int_t ml_supervisor_is_registered(void);
ml_int_t ml_supervisor_get_backoff_ms(void);

/* Module extern */
extern ml_module_t g_ml_module_pipeline_supervisor;

#endif /* MLFW_SUPERVISOR_H */
