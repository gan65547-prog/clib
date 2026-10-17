/*
 * ml_log — Ring-buffer file logger.
 *
 * Writes log messages to a fixed-size file-backed ring buffer.  When the
 * buffer is full, the oldest entries are overwritten.  The ml_log_print
 * macro auto-appends timestamp, level, function name, and line number.
 */
#ifndef __ML_LOG_H__
#define __ML_LOG_H__

#include "ml_types.h"
#include "ml_thread.h"

/* Log severity levels */
enum {
    ML_LOG_DEBUG  = 0,
    ML_LOG_INFO,
    ML_LOG_IMPT,
    ML_LOG_WARN,
    ML_LOG_ERROR,
};

typedef struct ml_log_s {
    ml_int_t            m_fd;
    ml_uint_t           m_level;
    ml_uint_t           m_length;
    ml_uint_t           m_total;
    ml_uint_t           m_store;
    ml_uint_t           m_size;
    char               *m_buff;
    char                m_date[64];
    ml_thread_mutex_t   m_mutex;
} ml_log_t;

/* Create a log backed by the file at path, with minimum severity deflv.
 * The caller provides the ring buffer (buf, buflen).  filesize controls
 * how often the file is trimmed. */
void ml_log_create(ml_log_t * log, const char * path, ml_uint_t deflv,
                   ml_uint_t filesize, char * buf, ml_uint_t buflen);

/* Flush and close the log. */
void ml_log_destroy(ml_log_t * log);

/* Format the current date string. Returns pointer to log's internal buffer. */
const char * ml_log_date(ml_log_t * log);

/* Convert a log level integer to a human-readable string. */
const char * ml_log_level(ml_int_t level);

/* Low-level: write a pre-formatted buffer to the log. */
void ml_log_write(ml_log_t * log, ml_uint_t size);

/* High-level log macro.  Auto-appends timestamp, level, function, and line.
 * Example: ml_log_print(&log, ML_LOG_INFO, "server started on port %d", port); */
#define ml_log_print(log, level, fmt, ...)                                                     \
    do {                                                                                        \
        if ((level) >= ((log)->m_level))  {                                                     \
            char * _x_buffer_ = ((log)->m_buff);                                                \
            ml_uint_t _x_size = 0, _x_len = ((log)->m_total) - 1;                              \
            _x_size = snprintf(_x_buffer_, _x_len, "[%s] [%s] [%s:%d] " fmt,                    \
                                    ml_log_level(level), ml_log_date(log) ,                   \
                                    __FUNCTION__, __LINE__, ##__VA_ARGS__);                     \
            if ( _x_size < _x_len ) { _x_buffer_[_x_size++] = '\n'; }                           \
            else { _x_buffer_[_x_len-1] = '\n'; _x_size = _x_len; }                             \
            ml_log_write((log),  _x_size);                                                     \
        }                                                                                       \
    } while (0)

#endif
