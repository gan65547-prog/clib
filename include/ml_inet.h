/*
 * ml_inet — TCP socket address helpers.
 *
 * Defines ml_sockaddr_in_t (layout-compatible with struct sockaddr_in)
 * and related types.
 */
#ifndef __ML_INET_H__
#define __ML_INET_H__

#include <netinet/in.h>
#include <stdint.h>
#include "ml_types.h"

/* Layout-compatible with struct sockaddr_in on both Linux and macOS.
 * sin_len is present only on BSD/macOS; sa_family_t is unsigned short
 * on Linux and unsigned char on macOS — we match the platform. */
typedef struct {
#ifdef __APPLE__
    uint8_t       sin_len;
#endif
    sa_family_t   sin_family;
    in_port_t     sin_port;
    struct in_addr sin_addr;
    uint8_t       sin_zero[8];
} ml_sockaddr_in_t;

/* Compile-time check: must be cast-compatible with sockaddr_in */
#if defined(__linux__)
_Static_assert(sizeof(ml_sockaddr_in_t) == sizeof(struct sockaddr_in),
    "ml_sockaddr_in_t size must match sockaddr_in");
#endif

#endif
