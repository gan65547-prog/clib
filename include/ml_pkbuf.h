/*
 * ml_pkbuf — Network packet buffer with chain support.
 *
 * Provides a packet buffer structure (ml_pkbuf_t) for network protocol
 * processing, plus a lock-free SPSC ring (ml_pring_t) for queuing buffers.
 */
#ifndef __ML_PKBUF_H__
#define __ML_PKBUF_H__

#include "ml_types.h"
#include "ml_llist.h"
#include "ml_list.h"
#include "ml_pool.h"
#include "ml_inet.h"

typedef struct ml_pkbuf_s {
    llist_node_t       m_node;
    ml_list_t          m_list;

    ml_uint_t         m_event;
    ml_uint_t         m_resrv;

    ml_uint_t         m_ssid;
    ml_uint_t         m_dsid;

    ml_uint_t         m_blen;
    ml_uint_t         m_plen;

    ml_uint_t         m_ulen;
    ml_uint_t         m_proto;

    ml_uint_t         m_from;
    ml_uint_t         m_to;

    ml_uint8_t *      m_buff;
    /* application-layer data pointer */
    ml_uint8_t *      m_user;

    /* source address */
    ml_sockaddr_in_t  m_saddr;
    /* destination address */
    ml_sockaddr_in_t  m_daddr;

    void  *            m_info;
    void  *            m_data;
    void  *            m_parm;

    void (*f_handler)(struct ml_pkbuf_s *);
} ml_pkbuf_t;

/* pkbuf ring buffer — SPSC mode, len must be a power of 2 */
typedef struct ml_pring_s {
    ml_uint_t      m_in;
    ml_uint_t      m_out;
    ml_uint_t      m_len;
    ml_pkbuf_t **  m_ring;
} ml_pring_t;

#define ml_pring_init( r, b, l)                                \
    do {                                                        \
        (r)->m_in  = 0;                                         \
        (r)->m_out = 0;                                         \
        (r)->m_len = (l);                                       \
        (r)->m_ring = (b);                                      \
    } while(0)

#define ml_pring_empty(r)                                      (((r)->m_in) == ((r)->m_out))
#define ml_pring_input_final(r)                                (((r)->m_in)++)
#define ml_pring_output_final(r)                               (((r)->m_out)++)
#define ml_pring_input_get(r)                                  (r)->m_ring[((r)->m_in) & (((r)->m_len)-1)]
#define ml_pring_output_get(r)                                 (r)->m_ring[((r)->m_out) & (((r)->m_len)-1)]
#define ml_pring_output_set(r, b)                              ((r)->m_ring[((r)->m_out) & (((r)->m_len)-1)] = (b))
#define ml_pring_input_set(r, b)                               ((r)->m_ring[((r)->m_in) & (((r)->m_len)-1)] = (b))

/* Allocate a packet buffer with capacity for 'size' bytes of payload. */
ml_pkbuf_t * ml_pkbuf_alloc(ml_int_t size);
#define ml_pkbuf_free(a)        ml_free(a)

#endif
