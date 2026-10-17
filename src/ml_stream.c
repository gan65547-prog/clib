/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>

#ifndef SUN_LEN
#define SUN_LEN(su) (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

#include "ml_code.h"
#include "ml_pool.h"
#include "ml_stream.h"

void ml_stream_setopt(ml_int_t sockfd)
{
    struct linger timeout = { .l_onoff = 1, .l_linger = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (char *)&timeout, sizeof(timeout));

    ml_int_t flag = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (char *)&flag, sizeof(flag));

#ifdef __linux__
    flag = 300;
    setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (char *)&flag, sizeof(flag));

    flag = 3;
    setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (char *)&flag, sizeof(flag));

    flag = 10;
    setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (char *)&flag, sizeof(flag));
#endif
}

ml_int_t ml_stream_recv(ml_int_t fd, void *buf, ml_int_t size, ml_int_t *len)
{
    ml_uint8_t *pkg = (ml_uint8_t *)buf;
    ml_int_t n, cnt = 0, status = ML_OK;

    *len = 0;
    while (size) {
        n = recv(fd, pkg, size, 0);
        if (0 == n) {
            status = ML_ESOCKT;
            break;
        } else if (-1 == n) {
            if (errno == EAGAIN) { status = ML_EAGAIN; break; }
            else continue;
        } else {
            cnt  += n;
            pkg  += n;
            size -= n;
        }
    }
    *len = cnt;
    return status;
}

ml_int_t ml_stream_send(ml_int_t fd, const void *buf, ml_int_t size, ml_int_t *len)
{
    ml_uint8_t *value = (ml_uint8_t *)buf;
    ml_int_t n, cnt = 0, status = ML_OK;

    *len = 0;
    while (size) {
        n = send(fd, value, size, 0);
        if (0 == n) {
            status = ML_ESOCKT;
            break;
        } else if (-1 == n) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN) { status = ML_EAGAIN; break; }
            status = ML_ESOCKT;
            break;
        } else {
            cnt   += n;
            value += n;
            size  -= n;
        }
    }
    *len = cnt;
    return status;
}

ml_int_t ml_stream_recv_timeout(ml_int_t fd, void *buf, ml_int_t size, ml_int_t *len, ml_int_t msec)
{
    fd_set rset;
    struct timeval tv;
    ml_int_t ret;

    *len = 0;
    FD_ZERO(&rset);
    FD_SET(fd, &rset);
    tv.tv_sec  = msec / 1000;
    tv.tv_usec = (msec % 1000) * 1000;

    ret = select(fd + 1, &rset, NULL, NULL, &tv);
    if (ret == 0)      return ML_EAGAIN;
    else if (ret == -1) return ML_ERROR;

    if (FD_ISSET(fd, &rset))
        return ml_stream_recv(fd, buf, size, len);
    return ML_ERROR;
}

void ml_stream_close(ml_int_t *fd)
{
    if (*fd > 0) {
        close(*fd);
        *fd = -1;
    }
}

/* ---- Unix Domain Socket ---- */

ml_int_t ml_unix_sendto(ml_int_t fd, const void *buf, ml_int_t size, ml_int_t *len)
{
    return ml_stream_send(fd, buf, size, len);
}

ml_int_t ml_unix_rcvfrom(ml_int_t fd, void *buf, ml_int_t size, ml_int_t *len)
{
    return ml_stream_recv(fd, buf, size, len);
}

ml_int_t ml_unix_nonblock(ml_int_t fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return ML_ERROR;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return ML_ERROR;
    return ML_OK;
}

void ml_unix_destroy(ml_int_t *fd)
{
    ml_stream_close(fd);
}

ml_int_t ml_unix_create(const char *path, ml_int_t maxls, ml_int_t *fd)
{
    ml_sockaddr_un_t addr;
    ml_int_t sock, flag = 1, len = sizeof(int);

    unlink(path);
    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return ML_ESOCKT;

    ml_memzero(&addr, sizeof(addr));
    addr.sun_family = PF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, len);
    if (bind(sock, (ml_sockaddr_t *)&addr, SUN_LEN(&addr)) < 0) {
        close(sock);
        return ML_ESOCKT;
    }

    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
    if (listen(sock, maxls) < 0) {
        close(sock);
        return ML_ESOCKT;
    }

    *fd = sock;
    return ML_OK;
}

ml_int_t ml_unix_connect(const char *path, ml_int_t *fd, ml_int_t tmo)
{
    ml_sockaddr_un_t addr;
    ml_int_t sock, ret, flag = 1;
    fd_set rset, wset;
    struct timeval tv;

    ml_memzero(&addr, sizeof(addr));
    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock <= 0) return ML_ERROR;

    addr.sun_family = PF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);

    ret = connect(sock, (ml_sockaddr_t *)&addr, SUN_LEN(&addr));
    if (ret < 0 && errno != EINPROGRESS) {
        close(sock);
        return ML_ESOCKT;
    }

    if (tmo <= 0) { *fd = sock; return ML_OK; }

    FD_ZERO(&rset); FD_ZERO(&wset);
    FD_SET(sock, &rset); FD_SET(sock, &wset);
    tv.tv_sec = tmo / 1000;
    tv.tv_usec = (tmo % 1000) * 1000;

    ret = select(sock + 1, &rset, &wset, NULL, &tv);
    if (ret == 0) { close(sock); return ML_ESOCKT; }

    if (FD_ISSET(sock, &rset) || FD_ISSET(sock, &wset)) {
        int error = 0;
        socklen_t slen = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &slen) < 0 || error != 0) {
            close(sock);
            return ML_ESOCKT;
        }
    } else {
        close(sock);
        return ML_ESOCKT;
    }

    *fd = sock;
    return ML_OK;
}

ml_int_t ml_unix_sendmsg(const char *path, const void *data, ml_int_t size,
                          void *buf, ml_int_t blen, ml_int_t *len, ml_int_t msec)
{
    ml_int_t sockfd, ret, l;

    ret = ml_unix_connect(path, &sockfd, msec);
    if (ret != ML_OK) return ML_ESOCKT;

    ret = ml_stream_send(sockfd, data, size, &l);
    if (ret != ML_OK) { ml_stream_close(&sockfd); return ML_ESOCKT; }

    if (buf && blen)
        ml_stream_recv_timeout(sockfd, buf, blen, len, msec);

    ml_stream_close(&sockfd);
    return ML_OK;
}

/* ---- TCP Socket ---- */

ml_int_t ml_tcp_connect(const char *addr4, ml_uint16_t port, ml_int_t *fd, ml_int_t tmo)
{
    ml_sockaddr_in_t addr;
    ml_int_t sock, ret, flag = 1;
    fd_set rset, wset;
    struct timeval tv;

    ml_memzero(&addr, sizeof(addr));
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock <= 0) return ML_ESOCKT;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(addr4);

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);

    ret = connect(sock, (ml_sockaddr_t *)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) { close(sock); return ML_ESOCKT; }

    if (tmo <= 0) { *fd = sock; return ML_OK; }

    FD_ZERO(&rset); FD_ZERO(&wset);
    FD_SET(sock, &rset); FD_SET(sock, &wset);
    tv.tv_sec = tmo / 1000;
    tv.tv_usec = (tmo % 1000) * 1000;

    ret = select(sock + 1, &rset, &wset, NULL, &tv);
    if (ret == 0) { close(sock); return ML_ESOCKT; }

    if (FD_ISSET(sock, &rset) || FD_ISSET(sock, &wset)) {
        int error = 0;
        socklen_t slen = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &slen) < 0 || error != 0) {
            close(sock);
            return ML_ESOCKT;
        }
    } else {
        close(sock);
        return ML_ESOCKT;
    }

    *fd = sock;
    return ML_OK;
}

ml_int_t ml_tcp_create(const char *addr4, ml_uint16_t port, ml_uint_t maxls, ml_int_t *fd)
{
    ml_sockaddr_in_t addr;
    ml_int_t sock, on = 1, flag = 1;

    ml_memzero(&addr, sizeof(addr));
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return ML_ESOCKT;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = (addr4 && *addr4) ? inet_addr(addr4) : INADDR_ANY;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);

    if (bind(sock, (ml_sockaddr_t *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return ML_ESOCKT;
    }
    if (listen(sock, maxls) < 0) {
        close(sock);
        return ML_ESOCKT;
    }

    *fd = sock;
    return ML_OK;
}

