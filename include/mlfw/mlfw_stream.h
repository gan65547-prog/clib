/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef MLFW_STREAM_H
#define MLFW_STREAM_H

#include "ml_types.h"
#include "ml_code.h"

#ifdef __linux__

#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#define ml_socklen_t        socklen_t
#define ml_sockaddr_t       struct sockaddr
#define ml_sockaddr_in_t    struct sockaddr_in
#define ml_sockaddr_un_t    struct sockaddr_un

static __inline__ ml_int_t ml_sock_set_blocking(ml_int_t fd, ml_int_t blk)
{
    ml_int_t flag = fcntl(fd, F_GETFL, 0);
    if (blk) flag &= ~O_NONBLOCK;
    else     flag |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flag);
}

#define ML_SOCK_NONBLK(s)  ml_sock_set_blocking(s, 0)

/* Stream send/receive with full-buffer guarantees */
ml_int_t ml_stream_recv(ml_int_t fd, void *buf, ml_int_t size, ml_int_t *len);
ml_int_t ml_stream_send(ml_int_t fd, const void *buf, ml_int_t size, ml_int_t *len);
ml_int_t ml_stream_recv_timeout(ml_int_t fd, void *buf, ml_int_t size, ml_int_t *len, ml_int_t msec);
void     ml_stream_close(ml_int_t *fd);
void     ml_stream_setopt(ml_int_t fd);

/* Unix Domain Socket operations */
ml_int_t ml_unix_create(const char *path, ml_int_t maxls, ml_int_t *fd);
ml_int_t ml_unix_connect(const char *path, ml_int_t *fd, ml_int_t tmo);
ml_int_t ml_unix_register(const char *path, ml_int_t *fd, ml_uint_t sid, ml_int_t msec);
ml_int_t ml_unix_sendmsg(const char *path, const void *data, ml_int_t size, void *buf, ml_int_t blen, ml_int_t *len, ml_int_t msec);
void     ml_unix_destroy(ml_int_t *fd);

/* TCP socket operations */
ml_int_t ml_tcp_connect(const char *addr4, ml_uint16_t port, ml_int_t *fd, ml_int_t tmo);
ml_int_t ml_tcp_create(const char *addr4, ml_uint16_t port, ml_uint_t maxls, ml_int_t *fd);

/* Convenience macros */
#define ml_unix_nonblock(s)       ML_SOCK_NONBLK(s)
#define ml_unix_sendto(fd,b,sz,l) ml_stream_send(fd,b,sz,l)
#define ml_unix_rcvfrom(fd,b,sz,l) ml_stream_recv(fd,b,sz,l)
#define ml_unix_rcvtime(fd,b,sz,l,t) ml_stream_recv_timeout(fd,b,sz,l,t)

#endif /* __linux__ */
#endif /* MLFW_STREAM_H */
