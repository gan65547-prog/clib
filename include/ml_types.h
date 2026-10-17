/*
 * ml_types — Core type definitions for clib.
 *
 * Defines the fundamental integer types (ml_int_t, ml_uint8_t, etc.),
 * atomic operations, and the container_of macro used throughout the library.
 */
#ifndef __ML_TYPES_H__
#define __ML_TYPES_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>

typedef unsigned int                  ml_uint_t;
typedef int                           ml_int_t;

typedef unsigned char                 ml_uint8_t;
typedef unsigned short                ml_uint16_t;
typedef uint64_t                      ml_uint64_t;
typedef int64_t                       ml_int64_t;

typedef unsigned int                  ml_addr4_t;
typedef int                           ml_fd_t;
typedef ml_uint64_t                  ml_uptime_t;
typedef ml_int64_t                   ml_long_t;
typedef ml_int64_t                   ml_atomic_int_t;
typedef ml_uint64_t                  ml_atomic_uint_t;
typedef volatile ml_uint64_t         ml_atomic_t;

typedef _Atomic unsigned char          ml_uatomic_t;

#ifndef container_of
#define container_of(ptr, type, member) ({                    \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#define ml_uatomic_init(a)                  \
            (*(a) = 0)

#define ml_uatomic_lock(a)                  \
    while (__atomic_test_and_set(a, __ATOMIC_ACQUIRE))

#define ml_uatomic_unlock(a)                \
    __atomic_clear(a, __ATOMIC_RELEASE)

#endif
