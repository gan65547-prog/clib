/*
 * ml_connection — Connection abstraction for event-driven I/O.
 *
 * Wraps a file descriptor with read/write event handlers and alive/error
 * flags.  Used by the epoll event loop and TCP bridge.
 */
#ifndef __ML_CONNECTION_H__
#define __ML_CONNECTION_H__

#include "ml_types.h"

typedef struct ml_connection_s ml_connection_t;

typedef void (*ml_connection_handler_pt)(ml_connection_t *c);

struct ml_connection_s {
    ml_fd_t     m_fd;
    ml_int_t    m_alive;
    ml_int_t    m_error;

    ml_connection_handler_pt  f_rdev;
    ml_connection_handler_pt  f_wrev;
};

#endif
