/*
 * ml_event — epoll-based event loop (Linux) with select fallback.
 *
 * On Linux this wraps epoll(7).  On non-Linux platforms the struct is
 * forward-declared so that framework headers compile, but event-driven
 * I/O is not available.
 *
 * Defines EPOLL* constants as ML_EPOLL* aliases for portability.
 */
#ifndef __ML_EVENT_H__
#define __ML_EVENT_H__

#include "ml_types.h"

/* Forward declaration for non-Linux platforms */
typedef struct ml_event_s ml_event_t;
typedef struct ml_connection_s ml_connection_t;

#ifdef __linux__

#include "ml_inet.h"
#include "ml_list.h"
#include "ml_connection.h"
#include <sys/epoll.h>
#include <unistd.h>

#define ML_EPOLLET                 EPOLLET
#define ML_EPOLLIN                 EPOLLIN
#define ML_EPOLLOUT                EPOLLOUT
#define ML_EPOLLHUP                EPOLLHUP
#define ML_EPOLLERR                EPOLLERR
#define ML_EPOLLRDHUP              EPOLLRDHUP

typedef struct epoll_event          ml_epoll_event_t;
typedef struct ml_event_s          ml_event_t;

typedef ml_int_t (*ml_event_insert_pt)(ml_event_t * ev, ml_connection_t * c, ml_int_t state);
typedef ml_int_t (*ml_event_remove_pt)(ml_event_t * ev, ml_connection_t * c);
typedef void (*ml_event_wait_pt)(ml_event_t * ev, ml_int_t msec);
typedef void  (*ml_event_callback_pt)(void * p);

struct ml_event_s {
    ml_int_t     m_fd;
    ml_int_t     m_len;
    void *        m_param;

    ml_list_t    m_first;
    ml_list_t    m_later;

    ml_epoll_event_t *  m_ev;
    ml_event_insert_pt     f_insert;
    ml_event_remove_pt     f_remove;
    ml_event_wait_pt       f_cycle;
    ml_event_callback_pt   f_call;
};

/* Create an event loop with capacity for n events. */
ml_int_t ml_event_create(ml_event_t * ev, ml_epoll_event_t * ee, ml_int_t n);
/* Destroy the event loop and release resources. */
void ml_event_destroy(ml_event_t * ev);
/* Add a connection to the event loop with the given epoll state flags. */
ml_int_t ml_event_add(ml_event_t * ev, ml_connection_t * c, ml_int_t state);
/* Remove a connection from the event loop. */
ml_int_t ml_event_del(ml_event_t * ev, ml_connection_t * c);

#endif /* __linux__ */

#endif /* __ML_EVENT_H__ */
