/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include "ml_types.h"
#include "ml_code.h"
#include "ml_mmap.h"


#include <errno.h>
#include <netdb.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>







ml_int_t  ml_mmap_connect(ml_mmap_t *mm, char * path, ml_int_t length)
{
    mm->m_path = path;
    mm->m_size = length;
    mm->m_fd = -1;
    mm->m_addr = NULL;

    mm->m_fd = open(mm->m_path, O_RDWR, 0644);
    if ( (-1) == mm->m_fd ) {
        printf("open %s error:%s\n", mm->m_path, strerror(errno));
        return ML_ENOREG;
    }

    /* 映射内存空间 */
    mm->m_addr = mmap(NULL, mm->m_size, PROT_READ | PROT_WRITE, MAP_SHARED, mm->m_fd, 0);
    if (mm->m_addr == MAP_FAILED) {
        printf("mmap %s error:%s\n", mm->m_path, strerror(errno));
        close(mm->m_fd);
        return ML_ERROR;
    }

    return ML_OK;
}





ml_int_t  ml_mmap_create(ml_mmap_t *mm, char * path, ml_int_t length)
{
    mm->m_path = path;
    mm->m_size = length;
    mm->m_fd = -1;
    mm->m_addr = NULL;

    mm->m_fd = open(mm->m_path, O_CREAT|O_RDWR|O_TRUNC, 0644);
    if ( (-1) == mm->m_fd ) {
        printf("open %s error:%s\n", mm->m_path, strerror(errno));
        return ML_ENOREG;
    }

    if ((-1) == ftruncate(mm->m_fd, mm->m_size)) {
        close(mm->m_fd);
        return ML_ERROR;
    }

    mm->m_addr = mmap(NULL, mm->m_size, PROT_WRITE|PROT_READ, MAP_SHARED, mm->m_fd, 0);
    if (mm->m_addr == MAP_FAILED) {
        printf("mmap %s error:%s\n", mm->m_path, strerror(errno));
        close(mm->m_fd);
        return ML_ERROR;
    }

    return ML_OK;
}




void ml_mmap_destroy(ml_mmap_t *mm)
{
    if (mm->m_addr && mm->m_addr != MAP_FAILED)
        munmap(mm->m_addr, mm->m_size);
    if (mm->m_fd >= 0)
        close(mm->m_fd);
    mm->m_addr = NULL;
    mm->m_fd = -1;
}




