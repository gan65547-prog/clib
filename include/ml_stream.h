/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_stream — Socket stream utilities (TCP / Unix Domain Socket).
 *
 * Thread safety: Thread-safe as long as each fd is accessed by a single
 * thread.  Shared-fd access must be externally synchronized.
 */

#ifndef ML_STREAM_H
#define ML_STREAM_H

#include "ml_types.h"
#include "ml_code.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sockaddr_un ml_sockaddr_un_t;
typedef struct sockaddr    ml_sockaddr_t;
typedef socklen_t          ml_socklen_t;

typedef struct sockaddr_in ml_sockaddr_in_t;

void ml_stream_setopt(ml_int_t sockfd);
ml_int_t ml_stream_recv(ml_int_t fd, void *buf, ml_int_t size, ml_int_t *len);
ml_int_t ml_stream_send(ml_int_t fd, const void *buf, ml_int_t size, ml_int_t *len);
ml_int_t ml_stream_recv_timeout(ml_int_t fd, void *buf, ml_int_t size,
                                 ml_int_t *len, ml_int_t msec);
void ml_stream_close(ml_int_t *fd);

/* Unix Domain Socket */
void ml_unix_destroy(ml_int_t *fd);
ml_int_t ml_unix_create(const char *path, ml_int_t maxls, ml_int_t *fd);
ml_int_t ml_unix_connect(const char *path, ml_int_t *fd, ml_int_t tmo);
ml_int_t ml_unix_sendmsg(const char *path, const void *data, ml_int_t size,
                          void *buf, ml_int_t blen, ml_int_t *len, ml_int_t msec);
ml_int_t ml_unix_sendto(ml_int_t fd, const void *buf, ml_int_t size, ml_int_t *len);
ml_int_t ml_unix_rcvfrom(ml_int_t fd, void *buf, ml_int_t size, ml_int_t *len);
ml_int_t ml_unix_nonblock(ml_int_t fd);

/* TCP Socket */
ml_int_t ml_tcp_connect(const char *addr4, ml_uint16_t port, ml_int_t *fd,
                         ml_int_t tmo);
ml_int_t ml_tcp_create(const char *addr4, ml_uint16_t port, ml_uint_t maxls,
                        ml_int_t *fd);

#ifdef __cplusplus
}
#endif

#endif /* ML_STREAM_H */
