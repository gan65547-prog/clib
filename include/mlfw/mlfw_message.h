/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_MESSAGE_H
#define MLFW_MESSAGE_H

#include "ml_types.h"

/* Message header */
typedef struct {
    ml_uint16_t  m_dest;
    ml_uint16_t  m_source;
    ml_uint16_t  m_resp;
    ml_uint16_t  m_length;
    ml_int_t     m_code;
} ml_message_header_t;

/* Header field accessors */
#define ML_MSGHDR_SET_DEST(h, v)   ((h)->m_dest   = (ml_uint16_t)(v))
#define ML_MSGHDR_GET_DEST(h)      ((h)->m_dest)
#define ML_MSGHDR_SET_SOURCE(h, v) ((h)->m_source = (ml_uint16_t)(v))
#define ML_MSGHDR_GET_SOURCE(h)    ((h)->m_source)
#define ML_MSGHDR_SET_RESP(h, v)   ((h)->m_resp   = (ml_uint16_t)(v))
#define ML_MSGHDR_GET_RESP(h)      ((h)->m_resp)
#define ML_MSGHDR_SET_LENGTH(h, v) ((h)->m_length = (ml_uint16_t)(v))
#define ML_MSGHDR_GET_LENGTH(h)    ((h)->m_length)
#define ML_MSGHDR_SET_CODE(h, v)   ((h)->m_code   = (ml_int_t)(v))
#define ML_MSGHDR_GET_CODE(h)      ((h)->m_code)

/* Registration events */
#define MLFW_REGIST_EVENT_ADD      0x8001
#define MLFW_REGIST_EVENT_DEL      0x8002

/* Bridge events */
#define MLFW_BRIDGE_EVENT_FORWARD  0xA001

/* Plugin events */
#define MLFW_PROJECT_EVENT_INLOAD  0xB001
#define MLFW_PROJECT_EVENT_UNLOAD  0xB002
#define MLFW_PROJECT_EVENT_ARRAY   0xB003
#define MLFW_PROJECT_EVENT_SPAWN   0xB004
#define MLFW_PROJECT_EVENT_CANCEL  0xB005
#define MLFW_PROJECT_EVENT_LEVEL   0xB006
#define MLFW_PROJECT_EVENT_RESET   0xB007

/* Pipeline control events (supervisor ↔ pipeline) */
#define MLFW_PIPELINE_EVENT_HEARTBEAT  0xC001
#define MLFW_PIPELINE_EVENT_REGISTER   0xC002
#define MLFW_PIPELINE_EVENT_STATUS     0xC003
#define MLFW_PIPELINE_EVENT_START      0xC004
#define MLFW_PIPELINE_EVENT_STOP       0xC005
#define MLFW_PIPELINE_EVENT_RELOAD     0xC006

/* Session management events */
#define MLFW_SESSION_EVENT_CREATE      0xD001
#define MLFW_SESSION_EVENT_START       0xD002
#define MLFW_SESSION_EVENT_STOP        0xD003
#define MLFW_SESSION_EVENT_QUERY       0xD004
#define MLFW_SESSION_EVENT_UPDATE      0xD005

#endif /* MLFW_MESSAGE_H */
