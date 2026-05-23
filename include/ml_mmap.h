/*
 * ml_mmap — Memory-mapped file I/O.
 *
 * Wraps mmap(2) for creating and attaching to file-backed shared memory
 * regions.  Used by the ring-buffer and shared-memory-pool modules.
 */
#ifndef __ML_MMAP_H__
#define __ML_MMAP_H__

#include "ml_types.h"

/* File-backed shared memory region */
typedef struct ml_mmap_s {
    ml_int_t     m_fd;
    ml_int_t     m_size;
    char     *    m_path;
    void     *    m_addr;
} ml_mmap_t;

/* Create a new mmap region backed by the file at path. */
ml_int_t  ml_mmap_create(ml_mmap_t *mm, char * path, ml_int_t length);
/* Unmap and close the region. */
void ml_mmap_destroy(ml_mmap_t *fm);
/* Attach to an existing mmap region. */
ml_int_t  ml_mmap_connect(ml_mmap_t *fm, char * path, ml_int_t length);

/* Aliases used by the ring buffer module */
typedef ml_mmap_t ml_file_map_t;
#define ml_file_map_create(mm, path, len)  ml_mmap_create(mm, path, len)
#define ml_file_map_connect(mm, path, len) ml_mmap_connect(mm, path, len)
#define ml_file_map_destroy(mm)            ml_mmap_destroy(mm)

#endif
