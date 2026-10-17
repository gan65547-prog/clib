/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifdef __linux__

#include <errno.h>
#include "ml_types.h"
#include "ml_code.h"
#include "ml_event.h"




ml_int_t ml_event_add(ml_event_t * ev, ml_connection_t * c, ml_int_t state)
{
    ml_int_t op;
    struct epoll_event  ee;


    ee.events = state;
    ee.data.ptr = (void *) c;


    if (c->m_alive)
        op = EPOLL_CTL_MOD;
    else
        op = EPOLL_CTL_ADD;

    if (-1 == epoll_ctl(ev->m_fd, op, c->m_fd, &ee)) {
        return ML_ERROR;
    }

    c->m_alive = 1;

    return ML_OK;
}




ml_int_t ml_event_del(ml_event_t * ev, ml_connection_t * c)
{
    struct epoll_event  ee;

    if (c->m_alive) {
        c->m_alive = 0;
        ee.events = 0;
        ee.data.ptr = NULL;

        if (-1 == epoll_ctl(ev->m_fd, EPOLL_CTL_DEL, c->m_fd, &ee)) {
            return ML_ERROR;
        }        
    }
    return ML_OK;

}







static void ml_event_wait(ml_event_t * ev, ml_int_t msec)
{
    ml_int_t n, i;
    ml_uint_t events, error;
    ml_connection_t * c;
    void (*f_handler)(ml_connection_t *);

    do {
        n = epoll_wait(ev->m_fd, ev->m_ev, ev->m_len, msec);
    } while (n < 0 && errno == EINTR);

    if (ev->f_call)
        ev->f_call(ev->m_param);

    if (n < 0) {
        return;
    }
    
    for (i = 0; i < n; i++) {
    
        c = (ml_connection_t*)ev->m_ev[i].data.ptr;
        if (!c) continue;
        events = ev->m_ev[i].events;
        error = 0;

        if (events & (EPOLLHUP | EPOLLERR)) {
            error = 1;
            events |= EPOLLIN;
        }

        if (events & (EPOLLIN)) {
            c->m_error = error;
            f_handler = c->f_rdev;
            if (f_handler) f_handler(c);
        }

        if ((events & EPOLLOUT) && c->m_alive) {
            c->m_error = error;
            f_handler = c->f_wrev;
            if (f_handler) f_handler(c);
        }

    }


}





ml_int_t ml_event_create(ml_event_t * ev, ml_epoll_event_t * ee, ml_int_t n)
{
    ev->m_ev  = ee;
    ev->m_len = n;
    ev->m_param = NULL;

    INIT_LIST_HEAD(&ev->m_first);
    INIT_LIST_HEAD(&ev->m_later);


    ev->m_fd = epoll_create1(0);
    ev->f_insert = ml_event_add;
    ev->f_remove = ml_event_del;
    ev->f_cycle  = ml_event_wait;
    ev->f_call   = NULL;

    return (ev->m_fd >= 0) ? ML_OK : ML_ERROR;
}



void ml_event_destroy(ml_event_t * ev)
{
    if (!ev) return;
    if (ev->m_fd >= 0)
        close(ev->m_fd);
    ev->m_fd = -1;
}

#endif /* __linux__ */

