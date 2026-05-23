/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_MESSAGE_H
#define MLFW_MESSAGE_H

#include "ml_types.h"

/* Timer event commands */
#define MLFW_TIMER_EVENT_SID     7861
#define MLFW_TIMER_EVENT_ADD     0x7861
#define MLFW_TIMER_EVENT_DEL     0x7862
#define MLFW_TIMER_EVENT_GET     0x7863
#define MLFW_TIMER_EVENT_WKP     0x7864
#define MLFW_TIMER_EVENT_OUT     0x7865

/* Action/config event commands */
#define MLFW_ACTION_EVENT_ADD    0x8050

/* Connection registration */
#define MLFW_REGIST_EVENT_ADD    0x8080

/* Plugin/Project event commands */
#define MLFW_PROJECT_EVENT_INLOAD   0x8060
#define MLFW_PROJECT_EVENT_UNLOAD   0x8061
#define MLFW_PROJECT_EVENT_ARRAY    0x8062
#define MLFW_PROJECT_EVENT_SPAWN    0x8063
#define MLFW_PROJECT_EVENT_CANCEL   0x8064
#define MLFW_PROJECT_EVENT_LEVEL    0x8065
#define MLFW_PROJECT_EVENT_RESET    0x8066

#define MLFW_SIMPLEX_EVENT_SID   0x7771

/*
 * Message header for Unix Domain Socket IPC.
 * All multi-byte fields are network byte order.
 */
typedef struct ml_message_header_s {
    ml_uint_t    m_source;
    ml_uint_t    m_dest;
    ml_uint16_t  m_code;
    ml_uint16_t  m_resp;
    ml_uint16_t  m_type;
    ml_uint16_t  m_length;
} ml_message_header_t;

/* Timer info payload */
typedef struct ml_timer_info_s {
    ml_uint_t  m_info;
    ml_uint_t  m_out;
} ml_timer_info_t;

#include <arpa/inet.h>

#define ML_MSGHDR_SET_SOURCE(h, s)   (((h)->m_source) = htonl(s))
#define ML_MSGHDR_SET_DEST(h, s)     (((h)->m_dest)   = htonl(s))
#define ML_MSGHDR_SET_CODE(h, s)     (((h)->m_code)   = htons(s))
#define ML_MSGHDR_SET_TYPE(h, s)     (((h)->m_type)   = htons(s))
#define ML_MSGHDR_SET_RESP(h, s)     (((h)->m_resp)   = htons(s))
#define ML_MSGHDR_SET_LENGTH(h, s)   (((h)->m_length) = htons(s))

#define ML_MSGHDR_GET_SOURCE(h)      ntohl((h)->m_source)
#define ML_MSGHDR_GET_DEST(h)        ntohl((h)->m_dest)
#define ML_MSGHDR_GET_CODE(h)        ntohs((h)->m_code)
#define ML_MSGHDR_GET_TYPE(h)        ntohs((h)->m_type)
#define ML_MSGHDR_GET_RESP(h)        ntohs((h)->m_resp)
#define ML_MSGHDR_GET_LENGTH(h)      ntohs((h)->m_length)

#define ML_TIMER_SET_INFO(t, s)      (((t)->m_info) = htonl(s))
#define ML_TIMER_SET_OUT(t, s)       (((t)->m_out)  = htonl(s))
#define ML_TIMER_GET_INFO(t)         ntohl((t)->m_info)
#define ML_TIMER_GET_OUT(t)          ntohl((t)->m_out)

#endif
