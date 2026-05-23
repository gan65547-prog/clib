/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_PROCESS_H
#define MLFW_PROCESS_H

#include <signal.h>
#include "ml_types.h"

/* Signal handler descriptor */
typedef struct ml_signal_s {
    ml_int_t  m_signo;
    char     *m_signame;
    void    (*f_handler)(ml_int_t signo, siginfo_t *info, void *ucontext);
} ml_signal_t;

/* Application quit / respawn flags */
extern volatile sig_atomic_t g_ml_quit;
extern volatile sig_atomic_t g_ml_resp;

/* Saved command-line args */
extern ml_int_t  g_ml_argc;
extern char    **g_ml_argv;
extern char    **g_ml_os_argv;

/* ---- Process lifecycle ---- */
ml_int_t ml_process_init(int argc, char **argv, ml_pool_t *pool);
ml_int_t ml_init_signals(void);
void     ml_signal_handler(ml_int_t signo, siginfo_t *info, void *ucontext);
int      ml_init_proctitle(void);
void     ml_setproctitle(char *title);

/* ---- Workspace paths (set by ml_process_init) ---- */
char *ml_process_workspace(void);
char *ml_process_tempspace(void);
char *ml_process_logspace(void);
char *ml_process_configfile(void);
char *ml_process_pluginspace(void);
char *ml_process_version(void);

#endif
