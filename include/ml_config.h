/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_config.h — Framework configuration constants.
 *
 * Sizing limits and general-purpose macros used throughout clib and mlfw.
 * Runtime paths (workspace, tempspace, log, plugins) are managed by
 * mlfw_process.c and overridden via CLI flags (-w, -m, -l, -p).
 */

#ifndef __ML_CONFIG_H__
#define __ML_CONFIG_H__

/* Log buffer sizes */
#define ML_LOG_BUFF_LENGTH                     1024
#define ML_LOG_FILE_LENGTH                     (1 * 1024 * 1024)

/* Path and name length limits */
#define ML_MAX_DIR_LENGTH                      64
#define ML_MAX_SKEY_LENGTH                     64
#define ML_MAX_PATH_LENGTH                     4096
#define ML_MAX_NAME_LENGTH                     64
#define ML_MAX_DATE_LENGTH                     64
#define ML_MAX_FSYNC_COUNT                     64

/* Network address string lengths */
#define ML_IP4_ADDR_LENGTH                     32
#define ML_IP6_ADDR_LENGTH                     64

/* Backward-compatibility aliases */
#define MAX_DIR_LENGTH                         ML_MAX_DIR_LENGTH
#define MAX_FSYNC_COUNT                        ML_MAX_FSYNC_COUNT
#define MAX_SKEY_LENGTH                        ML_MAX_SKEY_LENGTH
#define MAX_PATH_LENGTH                        ML_MAX_PATH_LENGTH
#define MAX_NAME_LENGTH                        ML_MAX_NAME_LENGTH
#define IP4_ADDR_LENGTH                        ML_IP4_ADDR_LENGTH
#define IP6_ADDR_LENGTH                        ML_IP6_ADDR_LENGTH

/* Forward declarations for cycle config */
typedef struct ml_pool_s    ml_pool_t;
typedef struct ml_log_s     ml_log_t;

typedef struct {
    ml_pool_t      *m_pool;
    ml_log_t       *m_log;
    char           *m_path;
    void           *m_obj;
} ml_cycle_config_t;

/* General-purpose macros */
#define ML_TRUE                                1
#define ML_FALSE                               0
#define ML_MIN(a, b)                           ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a < _b ? _a : _b; })
#define ML_MAX(a, b)                           ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a > _b ? _a : _b; })

#endif
