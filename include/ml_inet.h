/*
 * ml_inet — TCP socket address helpers.
 *
 * Defines ml_sockaddr_in_t (struct sockaddr_in compatible) and related types.
 */
#ifndef __ML_INET_H__
#define __ML_INET_H__

#include <stdint.h>
#include "ml_types.h"

typedef struct {
    ml_uint8_t  sin_len;
    ml_uint8_t  sin_family;
    uint16_t    sin_port;
    uint32_t    sin_addr;
    char        sin_zero[8];
} ml_sockaddr_in_t;

#endif
