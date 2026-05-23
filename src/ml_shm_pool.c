/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

/*
 * ml_shm_pool — Shared memory pool implementation.
 *
 * Uses POSIX shm_open(3) + mmap(2) for portable cross-process shared memory.
 * Simple bump allocator: suitable for pre-allocating buffers at startup.
 * Thread/process-safe via embedded spinlock (ml_uatomic_t).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "ml_types.h"
#include "ml_code.h"
#include "ml_pool.h"
#include "ml_config.h"
#include "ml_string.h"
#include "ml_shm_pool.h"

#define ML_SHM_POOL_ALIGN  8

static ml_uint64_t ml_shm_align64(ml_uint64_t d)
{
    return (d + (ML_SHM_POOL_ALIGN - 1)) & ~((ml_uint64_t)(ML_SHM_POOL_ALIGN - 1));
}

ml_int_t ml_shm_pool_open(ml_shm_pool_t *sp, const char *name,
                          ml_uint64_t size, ml_int_t create)
{
    ml_int_t fd;
    ml_uint64_t total;
    ml_shm_header_t *hdr;
    char shm_name[ML_MAX_PATH_LENGTH];
    ml_int_t flags = O_RDWR;

    if (!sp || !name || !name[0] || size == 0)
        return ML_EPARAM;

    ml_memzero(sp, sizeof(ml_shm_pool_t));

    /* Build shm path: /<name> */
    ml_snprintf(shm_name, sizeof(shm_name), "/%s", name);

    total = ml_shm_align64(sizeof(ml_shm_header_t) + size);

    if (create) {
        flags |= O_CREAT;

        /* Check if already exists — clean up stale shm from previous run */
        {
            ml_int_t probe = shm_open(shm_name, O_RDONLY, 0600);
            if (probe >= 0) {
                close(probe);
                shm_unlink(shm_name);
            }
        }

        fd = shm_open(shm_name, flags | O_EXCL, 0600);
        if (fd < 0)
            return ML_ERROR;

        if (ftruncate(fd, (off_t)total) < 0) {
            close(fd);
            shm_unlink(shm_name);
            return ML_ERROR;
        }
    } else {
        fd = shm_open(shm_name, flags, 0600);
        if (fd < 0)
            return ML_ENOEXT;

        {
            struct stat st;
            if (fstat(fd, &st) < 0) {
                close(fd);
                return ML_ERROR;
            }
            total = (ml_uint64_t)st.st_size;
        }
    }

    sp->m_base = mmap(NULL, (size_t)total,
                      PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sp->m_base == MAP_FAILED) {
        close(fd);
        if (create) shm_unlink(shm_name);
        return ML_ENOMEM;
    }

    sp->m_fd   = fd;
    sp->m_size = total;
    ml_strncpy(sp->m_path, shm_name, sizeof(sp->m_path) - 1);

    hdr = (ml_shm_header_t *)sp->m_base;

    if (create) {
        ml_memzero(hdr, sizeof(ml_shm_header_t));
        hdr->m_magic    = ML_SHM_POOL_MAGIC;
        hdr->m_version  = 1;
        hdr->m_size     = total;
        hdr->m_offset   = ml_shm_align64(sizeof(ml_shm_header_t));
        hdr->m_refcount = 0;
        ml_uatomic_init(&hdr->m_lock);
        ml_strncpy(hdr->m_name, name, sizeof(hdr->m_name) - 1);
    } else {
        if (hdr->m_magic != ML_SHM_POOL_MAGIC) {
            munmap(sp->m_base, (size_t)total);
            close(fd);
            return ML_ERROR;
        }
    }

    /* Increment attach count under lock */
    ml_uatomic_lock(&hdr->m_lock);
    hdr->m_refcount++;
    ml_uatomic_unlock(&hdr->m_lock);

    sp->m_header = hdr;
    return ML_OK;
}

void ml_shm_pool_close(ml_shm_pool_t *sp)
{
    ml_int_t remaining;

    if (!sp || !sp->m_header)
        return;

    ml_uatomic_lock(&sp->m_header->m_lock);
    remaining = --sp->m_header->m_refcount;
    ml_uatomic_unlock(&sp->m_header->m_lock);

    if (sp->m_base)
        munmap(sp->m_base, (size_t)sp->m_size);
    if (sp->m_fd >= 0)
        close(sp->m_fd);

    if (remaining <= 0 && sp->m_path[0])
        shm_unlink(sp->m_path);

    ml_memzero(sp, sizeof(ml_shm_pool_t));
}

/* Bump allocator — lock-free path for common case, spinlock for safety */

void *ml_shm_alloc(ml_shm_pool_t *sp, ml_uint64_t size)
{
    ml_shm_header_t *hdr;
    ml_uint64_t off, aligned, new_off;

    if (!sp || !sp->m_header || size == 0)
        return NULL;

    hdr     = sp->m_header;
    aligned = ml_shm_align64(size);

    ml_uatomic_lock(&hdr->m_lock);
    off = hdr->m_offset;
    new_off = off + aligned;

    if (new_off > hdr->m_size) {
        ml_uatomic_unlock(&hdr->m_lock);
        return NULL;  /* out of shared memory */
    }

    hdr->m_offset = new_off;
    ml_uatomic_unlock(&hdr->m_lock);

    return (ml_uint8_t *)sp->m_base + off;
}

void ml_shm_free(ml_shm_pool_t *sp, void *p)
{
    /* Bump allocator: individual frees are no-ops.
     * Memory is reclaimed when the entire region is destroyed. */
    (void)sp;
    (void)p;
}

ml_uint64_t ml_shm_used(const ml_shm_pool_t *sp)
{
    if (!sp || !sp->m_header)
        return 0;
    return sp->m_header->m_offset - ml_shm_align64(sizeof(ml_shm_header_t));
}

ml_int_t ml_shm_refcount(const ml_shm_pool_t *sp)
{
    if (!sp || !sp->m_header)
        return 0;
    return sp->m_header->m_refcount;
}
