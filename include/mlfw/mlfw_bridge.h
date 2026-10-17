/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_BRIDGE_H
#define MLFW_BRIDGE_H

#include "mlfw_module.h"
#include "mlfw_connection_pool.h"

#define MLFW_BRIDGE_PORT_DEFAULT  19100
#define MLFW_BRIDGE_MAX_PEERS     16

typedef struct {
    ml_uint16_t         m_node_id;
    char                m_addr[64];
    ml_uint16_t         m_port;
    mlfw_connection_t  *m_conn;
    ml_int_t            m_state;
} ml_bridge_peer_t;

extern ml_module_t g_ml_module_bridge;

#endif /* MLFW_BRIDGE_H */
