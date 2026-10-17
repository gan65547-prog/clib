/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include "ml_cints.h"
#include "ml_code.h"
#include "ml_ringbuf.h"
#include "ml_pool.h"



ml_int_t ml_kring_create(const char * path, ml_uint_t length, ml_kring_t * kr)
{
    ml_int_t ret;
    ml_uint8_t * data;
    ml_uint_t  size = sizeof(ml_kbuff_t) + length;

    if (!path || !kr || length == 0 || (length & (length - 1)) != 0)
        return ML_EPARAM;

    ret = ml_file_map_create(&kr->m_mmap, path, size);

    if (ML_OK != ret)
        return ret;

    data = (ml_uint8_t*)kr->m_mmap.m_addr;
    kr->m_kbuff = (ml_kbuff_t*)data;
    kr->m_data  = data + sizeof(ml_kbuff_t);

    ret = pthread_mutexattr_init(&kr->m_kbuff->m_attr);
    if (ret) { ml_file_map_destroy(&kr->m_mmap); return ML_ERROR; }
    pthread_mutexattr_setpshared(&kr->m_kbuff->m_attr, PTHREAD_PROCESS_SHARED);
    ret = pthread_mutex_init(&kr->m_kbuff->m_mutex, &kr->m_kbuff->m_attr);
    if (ret) {
        pthread_mutexattr_destroy(&kr->m_kbuff->m_attr);
        ml_file_map_destroy(&kr->m_mmap);
        return ML_ERROR;
    }

    kr->m_kbuff->m_input  = 0;
    kr->m_kbuff->m_output = 0;
    kr->m_kbuff->m_length = length;

    return ML_OK;
}



ml_int_t ml_kring_lookup(const char * path,  ml_kring_t * kr)
{
    ml_int_t ret;
    ml_uint8_t * data;
    ml_uint_t  size;

    struct stat sb;

    if (!path || !kr)
        return ML_EPARAM;

    ml_memzero(&sb, sizeof(sb));

    if (0 != stat(path, &sb))
        return ML_ERROR;

    size = sb.st_size;

    ret = ml_file_map_connect(&kr->m_mmap, path, size);

    if (ML_OK != ret)
        return ret;

    data = (ml_uint8_t*)kr->m_mmap.m_addr;
    kr->m_kbuff = (ml_kbuff_t*)data;
    kr->m_data  = data + sizeof(ml_kbuff_t);

    return ML_OK;
}



void ml_kring_destroy(ml_kring_t * kr)
{
    if (!kr || !kr->m_kbuff) return;
    pthread_mutex_destroy(&kr->m_kbuff->m_mutex);
    pthread_mutexattr_destroy(&kr->m_kbuff->m_attr);
    ml_file_map_destroy(&kr->m_mmap);
}



ml_uint_t ml_kring_empty(ml_kring_t * kr)
{
    ml_uint_t ret;

    if (!kr || !kr->m_kbuff) return ML_TRUE;

    pthread_mutex_lock(&kr->m_kbuff->m_mutex);

    ret = (kr->m_kbuff->m_input == kr->m_kbuff->m_output) ? ML_TRUE : ML_FALSE;

    pthread_mutex_unlock(&kr->m_kbuff->m_mutex);

    return ret;
}




ml_uint_t ml_kring_input(ml_kring_t * kr, void * buf, ml_uint_t len)
{
    ml_uint_t l;
    ml_kbuff_t * fifo;
    ml_uint8_t * data;

    if (!kr || !kr->m_kbuff || !buf || len == 0) return 0;

    fifo = kr->m_kbuff;
    data = kr->m_data;

    pthread_mutex_lock(&kr->m_kbuff->m_mutex);

    len = ML_MIN(len, fifo->m_length - fifo->m_input + fifo->m_output);
    l = ML_MIN(len, fifo->m_length - (fifo->m_input & (fifo->m_length - 1)));

    ml_memcpy(data + (fifo->m_input & (fifo->m_length - 1)), buf, l);
    ml_memcpy(data, buf + l, len - l);

    fifo->m_input += len;

    pthread_mutex_unlock(&kr->m_kbuff->m_mutex);

    return len;
}



ml_uint_t ml_kring_output(ml_kring_t * kr, void * buf, ml_uint_t len)
{
    ml_uint_t l;
    ml_kbuff_t * fifo;
    ml_uint8_t * data;

    if (!kr || !kr->m_kbuff || !buf || len == 0) return 0;

    fifo = kr->m_kbuff;
    data = kr->m_data;

    pthread_mutex_lock(&kr->m_kbuff->m_mutex);

    len = ML_MIN(len, fifo->m_input - fifo->m_output);
    l = ML_MIN(len, fifo->m_length - (fifo->m_output & (fifo->m_length - 1)));
    
    ml_memcpy(buf, data + (fifo->m_output & (fifo->m_length - 1)), l);
    ml_memcpy(buf + l, data, len - l);

    fifo->m_output += len;
    
    pthread_mutex_unlock(&kr->m_kbuff->m_mutex);
    
    return len;
}



ml_uint_t ml_kring_inall(ml_kring_t * kr, void * buf, ml_uint_t len)
{
    ml_uint_t l, left;
    ml_kbuff_t * fifo;
    ml_uint8_t * data;

    if (!kr || !kr->m_kbuff || !buf || len == 0) return 0;

    fifo = kr->m_kbuff;
    data = kr->m_data;

    pthread_mutex_lock(&kr->m_kbuff->m_mutex);

    left = fifo->m_length - fifo->m_input + fifo->m_output;
    if (left > len) {
        l = ML_MIN(len, fifo->m_length - (fifo->m_input & (fifo->m_length - 1)));

        ml_memcpy(data + (fifo->m_input & (fifo->m_length - 1)), buf, l);
        ml_memcpy(data, buf + l, len - l);

        fifo->m_input += len;
    }
    else {
        len = 0;
    }

    pthread_mutex_unlock(&kr->m_kbuff->m_mutex);

    return len;
}






ml_uint_t ml_kring_touse(ml_kring_t * kr)
{
    ml_uint_t len;
    ml_kbuff_t * fifo;

    if (!kr || !kr->m_kbuff) return 0;

    fifo = kr->m_kbuff;
    
    pthread_mutex_lock(&kr->m_kbuff->m_mutex);
    /* 数据的长度 */
    len = fifo->m_input - fifo->m_output;

    pthread_mutex_unlock(&kr->m_kbuff->m_mutex);

    return len;
}

ml_uint_t ml_kring_vacant(ml_kring_t * kr)
{
    ml_uint_t len;
    ml_kbuff_t * fifo;

    if (!kr || !kr->m_kbuff) return 0;

    fifo = kr->m_kbuff;
    
    pthread_mutex_lock(&kr->m_kbuff->m_mutex);
    /* buffer中空余的长度 */
    len = fifo->m_length - fifo->m_input + fifo->m_output;

    pthread_mutex_unlock(&kr->m_kbuff->m_mutex);

    return len;

}





