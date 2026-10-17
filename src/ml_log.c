/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "ml_code.h"
#include "ml_log.h"
#include "ml_pool.h"





static void ml_log_lseek(ml_log_t * log)
{
    lseek(log->m_fd, 0, SEEK_SET);
}


static ml_uint_t ml_log_length(const char * path)
{
    struct stat sb;

    ml_memzero(&sb, sizeof(sb));

    if (0 == lstat(path, &sb))
        return sb.st_size;

    return 0;
}



void ml_log_write(ml_log_t * log, ml_uint_t size)
{
    int ret = 0;

    if (log->m_fd > 0) {
        ml_thread_mutex_lock(&log->m_mutex);

        if ( log->m_store >= log->m_length ) {
            ml_log_lseek(log);
            log->m_store = 0;
        }

        if (( ret = write(log->m_fd, log->m_buff, size) ) > 0 )
            log->m_store += ret;

        ml_thread_mutex_unlock(&log->m_mutex);
    }

}



char * ml_log_level(ml_int_t level)
{
    switch ( level ) {
        case ML_LOG_DEBUG :  { return "DEBUG"; } break;
        case ML_LOG_INFO  :  { return "INFO"; } break;
        case ML_LOG_IMPT  :  { return "IMPT"; } break;
        case ML_LOG_WARN  :  { return "WARN"; } break;
        case ML_LOG_ERROR :  { return "ERROR"; } break;
    }

    return "DEBUG";
}





char * ml_log_date(ml_log_t * log)
{
    time_t timep;
    struct tm tm_now, *now;
    ml_int_t n = 0, len = sizeof(log->m_date);
    
    timep = time(NULL);
    now   = localtime_r(&timep, &tm_now);

    n = snprintf(log->m_date, len-1, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d", \
                        (1900+now->tm_year), (1+now->tm_mon), now->tm_mday, \
                        now->tm_hour, now->tm_min, now->tm_sec);
    log->m_date[n] = 0;
    return log->m_date;
}







void ml_log_create(ml_log_t * log, const char * path, ml_uint_t deflv, ml_uint_t filesize, char * buf, ml_uint_t buflen)
{
    ml_int_t fd;
    mode_t mode = S_IRWXU | S_IRGRP | S_IROTH;

    log->m_level   = deflv;
    log->m_total   = buflen;
    log->m_length  = filesize;
    log->m_store   = ml_log_length(path);
    log->m_buff    = buf;

    log->m_fd = ((fd = open(path, O_CREAT | O_RDWR , mode)) > 0 ) ? fd : 0;

    if (fd > 0)
        lseek(fd, 0, SEEK_END);

    ml_thread_mutex_create(&log->m_mutex);
}



void ml_log_destroy(ml_log_t * log)
{
    if (log->m_fd)
        close(log->m_fd);
    ml_thread_mutex_destroy(&log->m_mutex);
}



