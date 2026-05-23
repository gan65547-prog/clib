/*
 * ml_ringbuf — Lock-free SPSC ring buffer for cross-process communication.
 *
 * Design: single producer / single consumer lock-free ring backed by mmap'd
 * shared memory.  Requires m_length to be a power of 2.
 *
 * Use ml_kring_create to initialise a new ring, or ml_kring_lookup to attach
 * to one that already exists.
 */
#ifndef __ML_RINGBUF_H__
#define __ML_RINGBUF_H__

#include <pthread.h>
#include "ml_types.h"
#include "ml_mmap.h"

typedef struct {
    pthread_mutex_t     m_mutex;
    pthread_mutexattr_t m_attr;
    ml_uint64_t        m_input;
    ml_uint64_t        m_output;
    ml_uint_t          m_length;
} ml_kbuff_t;

typedef struct {
    ml_file_map_t  m_mmap;
    ml_kbuff_t    *m_kbuff;
    ml_uint8_t    *m_data;
} ml_kring_t;

/* Create a new ring buffer backed by a file at path. */
ml_int_t  ml_kring_create(char *path, ml_uint_t length, ml_kring_t *kr);
/* Attach to an existing ring buffer. */
ml_int_t  ml_kring_lookup(char *path, ml_kring_t *kr);
/* Destroy the ring buffer and release resources. */
void       ml_kring_destroy(ml_kring_t *kr);
/* Returns non-zero if the ring is empty. */
ml_uint_t ml_kring_empty(ml_kring_t *kr);
/* Write up to len bytes into the ring. Returns bytes actually written. */
ml_uint_t ml_kring_input(ml_kring_t *kr, void *buf, ml_uint_t len);
/* Read up to len bytes from the ring. Returns bytes actually read. */
ml_uint_t ml_kring_output(ml_kring_t *kr, void *buf, ml_uint_t len);
/* Write len bytes, or nothing if not enough space (all-or-nothing). */
ml_uint_t ml_kring_inall(ml_kring_t *kr, void *buf, ml_uint_t len);
/* Bytes currently available for reading. */
ml_uint_t ml_kring_touse(ml_kring_t *kr);
/* Free space available for writing. */
ml_uint_t ml_kring_vacant(ml_kring_t *kr);

#endif
