/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_MODULE_H
#define MLFW_MODULE_H

#include "ml_types.h"
#include "ml_code.h"
#include "ml_config.h"

#define MLFW_MODULE_ID_AUTO      0x0000
#define MLFW_MODULE_ID_AUTO_BASE 0x4000
#define MLFW_MODULE_ID_AUTO_MAX  0x7FFF

/* Forward declaration */
struct ml_module_s;
typedef struct ml_module_s ml_module_t;

typedef ml_module_t ml_core_t;  /* alias — cores use identical struct */

struct ml_module_s {
    ml_int_t        m_reserved;
    ml_int_t        m_no;
    const char     *m_name;
    ml_int_t      (*f_config)(ml_cycle_config_t *cfg);
    ml_int_t      (*f_init)(ml_cycle_config_t *cfg);
    void          (*f_exit)(void);
    ml_int_t      (*f_handler)(ml_module_t *m, void *conn,
                                void *buf, ml_int_t blen, ml_int_t *plen);
    ml_int_t      (*f_check)(ml_module_t *m, void *buf, ml_int_t blen);
    const char    *m_dep_names[8];
    ml_int_t        m_dep_count;
};

#endif /* MLFW_MODULE_H */
