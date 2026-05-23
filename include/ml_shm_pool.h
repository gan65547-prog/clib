/*
 * ml_shm_pool — Shared memory pool for cross-process zero-copy data exchange.
 *
 * Uses POSIX shm_open + mmap MAP_SHARED for portable cross-process shared
 * memory. Inside the region a simple bump allocator with a spinlock
 * provides thread/process-safe allocation.
 *
 * Layout of the shared region:
 *   [ml_shm_header_t | data_area ...]
 */
#ifndef __ML_SHM_POOL_H__
#define __ML_SHM_POOL_H__

#include "ml_types.h"
#include "ml_config.h"

#define ML_SHM_POOL_MAGIC  0x6D6C7368  /* "mlsh" */

typedef struct {
    ml_uint_t       m_magic;
    ml_uint_t       m_version;
    ml_uint64_t     m_size;           /* total region size */
    ml_uint64_t     m_offset;         /* next free byte offset */
    ml_uatomic_t    m_lock;           /* cross-process spinlock */
    ml_int_t        m_refcount;       /* process attach count */
    char            m_name[ML_MAX_NAME_LENGTH];
} ml_shm_header_t;

typedef struct {
    ml_shm_header_t *m_header;
    ml_int_t         m_fd;            /* shm fd */
    void            *m_base;          /* mmap'd base address */
    ml_uint64_t      m_size;
    char             m_path[ML_MAX_PATH_LENGTH];
} ml_shm_pool_t;

/*
 * Create or attach a named shared-memory pool.
 *   name:  unique name (e.g., "mlfw_data_plane")
 *   size:  region size in bytes
 *   create: 1 = create new, 0 = attach existing
 * Returns ML_OK or error code.
 */
ml_int_t ml_shm_pool_open(ml_shm_pool_t *sp, const char *name,
                          ml_uint64_t size, ml_int_t create);

/* Detach and optionally unlink when refcount reaches 0. */
void     ml_shm_pool_close(ml_shm_pool_t *sp);

/* Allocate/free within the shared pool (simple bump allocator). */
void    *ml_shm_alloc(ml_shm_pool_t *sp, ml_uint64_t size);
void     ml_shm_free(ml_shm_pool_t *sp, void *p);

/* Query: bytes currently allocated, process attach count */
ml_uint64_t ml_shm_used(const ml_shm_pool_t *sp);
ml_int_t    ml_shm_refcount(const ml_shm_pool_t *sp);

#endif
