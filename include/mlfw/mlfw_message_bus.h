/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_MESSAGE_BUS_H
#define MLFW_MESSAGE_BUS_H

#include "ml_types.h"
#include "ml_container.h"
#include "mlfw_connection_pool.h"
#include "mlfw_message.h"
#include "mlfw_module.h"

/* Connection ID lookup tree (global, used by all modules) */
extern ml_container_t *g_ml_connection_tree;

/* Message bus module (register in g_ml_modules[]) */
extern ml_module_t g_ml_module_message_bus;

/* Lookup a connection by ID */
mlfw_connection_t *ml_conn_lookup(ml_uint_t id);

/* Insert/remove connection in the ID map */
#define ml_conn_insert(c)   mlfw_set_insert(g_ml_connection_tree, ML_SET_NUMBER, (c)->m_set)
#define ml_conn_remove(c)   mlfw_set_remove(g_ml_connection_tree, (c)->m_set)

#endif
