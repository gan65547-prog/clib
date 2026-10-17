/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_PROCESS_H
#define MLFW_PROCESS_H

#include <signal.h>

#include "ml_types.h"
#include "ml_code.h"
#include "ml_config.h"
#include "ml_pool.h"

/* Globals */
extern volatile sig_atomic_t g_ml_quit;
extern volatile sig_atomic_t g_ml_resp;

typedef struct {
    ml_int_t     m_signo;
    char        *m_signame;
    void       (*f_handler)(ml_int_t signo, siginfo_t *info, void *ucontext);
} ml_signal_t;

/* Signal handling */
void ml_signal_handler(ml_int_t signo, siginfo_t *info, void *ucontext);
ml_int_t ml_init_signals(void);
int ml_init_proctitle(void);
void ml_setproctitle(char *title);

/* Process init */
ml_int_t ml_process_init(int argc, char **argv, ml_pool_t *pool);

/* Path accessors */
char *ml_process_workspace(void);
char *ml_process_tempspace(void);
char *ml_process_logspace(void);
char *ml_process_configfile(void);
char *ml_process_pluginspace(void);
char *ml_process_version(void);

/* Child process management (used by plugin module) */
void ml_children_get_status(void);
void ml_children_respawn(void);

#endif /* MLFW_PROCESS_H */
