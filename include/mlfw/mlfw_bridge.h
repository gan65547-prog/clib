/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_BRIDGE_H
#define MLFW_BRIDGE_H

#include "ml_types.h"
#include "mlfw_module.h"

/* Bridge event codes */
#define MLFW_BRIDGE_EVENT_FORWARD     0x8100
#define MLFW_BRIDGE_EVENT_PEER_LIST   0x8101
#define MLFW_BRIDGE_EVENT_PEER_STATS  0x8102

/* Max peers and buffer sizes */
#define MLFW_BRIDGE_MAX_PEERS         16
#define MLFW_BRIDGE_BUF_SIZE          65536
#define MLFW_BRIDGE_PORT_DEFAULT      9100

/* Wire header prepended to forwarded messages (network byte order) */
typedef struct {
    ml_uint16_t  m_node_src;
    ml_uint16_t  m_node_dst;
    ml_uint8_t   m_ttl;          /* hop limit, decremented at each forward */
} ml_bridge_wire_hdr_t;

#define ML_BRIDGE_SET_SRC(h, v)  (((h)->m_node_src) = htons(v))
#define ML_BRIDGE_SET_DST(h, v)  (((h)->m_node_dst) = htons(v))
#define ML_BRIDGE_SET_TTL(h, v)  (((h)->m_ttl) = (ml_uint8_t)(v))
#define ML_BRIDGE_GET_SRC(h)     ntohs((h)->m_node_src)
#define ML_BRIDGE_GET_DST(h)     ntohs((h)->m_node_dst)
#define ML_BRIDGE_GET_TTL(h)     ((h)->m_ttl)
#define ML_BRIDGE_DEC_TTL(h)     ((h)->m_ttl = (ml_uint8_t)((h)->m_ttl - 1))

#define ML_BRIDGE_DEFAULT_TTL    16

/* Payload for MLFW_BRIDGE_EVENT_FORWARD */
typedef struct {
    ml_uint16_t  m_node_dst;       /* target node ID */
    ml_uint16_t  m_pad;
    ml_uint_t    m_module_dst;     /* target module ID on remote node */
    ml_uint16_t  m_event;          /* event code for remote module */
    /* followed by payload data */
} ml_bridge_forward_t;

/* Peer descriptor */
typedef struct mlfw_connection_s mlfw_connection_t;

/* rx state machine states */
#define ML_BRIDGE_RX_HDR   0   /* reading wire header + msg header */
#define ML_BRIDGE_RX_BODY  1   /* reading payload */
#define ML_BRIDGE_RX_HS    2   /* reading 2-byte handshake (node_id) */

typedef struct {
    ml_uint16_t         m_node_id;
    char                m_addr[64];
    ml_uint16_t         m_port;
    ml_int_t            m_fd;
    mlfw_connection_t  *m_conn;
    ml_int_t            m_connected;
    ml_uint_t           m_msg_sent;
    ml_uint_t           m_msg_recv;

    /* rx state machine */
    ml_int_t            m_rx_state;    /* ML_BRIDGE_RX_HDR / _BODY */
    ml_int_t            m_rx_len;      /* bytes accumulated in m_rx_buf */
    ml_int_t            m_rx_expected; /* total bytes needed for current frame */
    ml_uint8_t          m_rx_buf[MLFW_BRIDGE_BUF_SIZE];

    /* tx async state */
    ml_int_t            m_tx_len;      /* bytes queued in m_tx_buf */
    ml_int_t            m_tx_off;      /* bytes already sent */
    ml_uint8_t          m_tx_buf[MLFW_BRIDGE_BUF_SIZE];
} ml_bridge_peer_t;

/* Bridge module (register in g_ml_modules[]) */
extern ml_module_t g_ml_module_bridge;

#endif
