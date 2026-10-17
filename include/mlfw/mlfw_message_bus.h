/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_MESSAGE_BUS_H
#define MLFW_MESSAGE_BUS_H

#include "mlfw_module.h"
#include "mlfw_connection_pool.h"

extern ml_module_t g_ml_module_message_bus;

/* Connection management */
void ml_conn_insert(mlfw_connection_t *c);
void ml_conn_remove(mlfw_connection_t *c);
mlfw_connection_t *ml_conn_lookup(ml_uint_t id);

extern ml_container_t *g_ml_connection_tree;

#endif /* MLFW_MESSAGE_BUS_H */
