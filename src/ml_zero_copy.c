/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

/*
 * ml_zero_copy — Kernel-assisted zero-copy I/O implementation.
 */
#define _GNU_SOURCE
#ifdef __linux__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#include "ml_types.h"
#include "ml_code.h"
#include "ml_zero_copy.h"

ml_int64_t ml_sendfile(ml_int_t out_fd, ml_int_t in_fd,
                       ml_int64_t *offset, ml_int64_t count)
{
    ssize_t total = 0;
    ssize_t n;
    off_t off = offset ? (off_t)*offset : (off_t)-1;

    if (out_fd < 0 || in_fd < 0 || count <= 0)
        return ML_EPARAM;

    while ((ml_int64_t)total < count) {
        size_t chunk = (size_t)(count - total);
        if (chunk > ML_ZERO_COPY_CHUNK)
            chunk = ML_ZERO_COPY_CHUNK;

        n = sendfile(out_fd, in_fd, offset ? &off : NULL, chunk);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            if (total > 0)
                break;  /* partial success */
            return ML_ERROR;
        }
        if (n == 0)
            break;  /* EOF */

        total += n;
    }

    if (offset)
        *offset = (ml_int64_t)off;

    return (ml_int64_t)total;
}

ml_int64_t ml_splice(ml_int_t in_fd, ml_int64_t *in_off,
                     ml_int_t out_fd, ml_int64_t *out_off,
                     ml_int64_t count, ml_uint_t flags)
{
    ssize_t total = 0;
    ssize_t n;
    loff_t *iptr = in_off ? (loff_t *)in_off : NULL;
    loff_t *optr = out_off ? (loff_t *)out_off : NULL;

    if (in_fd < 0 || out_fd < 0 || count <= 0)
        return ML_EPARAM;

    while ((ml_int64_t)total < count) {
        size_t chunk = (size_t)(count - total);
        if (chunk > ML_ZERO_COPY_CHUNK)
            chunk = ML_ZERO_COPY_CHUNK;

        n = splice(in_fd, iptr, out_fd, optr, chunk,
                   (unsigned int)(flags | SPLICE_F_MOVE));
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            if (total > 0)
                break;
            return ML_ERROR;
        }
        if (n == 0)
            break;

        total += n;
    }

    return (ml_int64_t)total;
}

ml_int64_t ml_copy_file_range(ml_int_t in_fd, ml_int64_t *in_off,
                              ml_int_t out_fd, ml_int64_t *out_off,
                              ml_int64_t count)
{
    ssize_t total = 0;
    ssize_t n;
    loff_t *iptr = in_off ? (loff_t *)in_off : NULL;
    loff_t *optr = out_off ? (loff_t *)out_off : NULL;

    if (in_fd < 0 || out_fd < 0 || count <= 0)
        return ML_EPARAM;

    while ((ml_int64_t)total < count) {
        size_t chunk = (size_t)(count - total);
        if (chunk > ML_ZERO_COPY_CHUNK)
            chunk = ML_ZERO_COPY_CHUNK;

        n = copy_file_range(in_fd, iptr, out_fd, optr, chunk, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            /* copy_file_range may not be supported on this filesystem;
               fall through to splice-based relay */
            break;
        }
        if (n == 0)
            break;

        total += n;
    }

    return total > 0 ? (ml_int64_t)total : ML_ERROR;
}

/*
 * Pipe relay: create a kernel pipe on first use, splice data
 * in_fd → pipe → out_fd.  Caches the pipe fds per-call (not thread-safe,
 * caller must serialize or use a pool).
 */
ml_int64_t ml_pipe_relay(ml_int_t in_fd, ml_int_t out_fd, ml_int64_t count)
{
    ml_int_t pipefd[2];
    ml_int64_t total = 0, n;

    if (in_fd < 0 || out_fd < 0 || count <= 0)
        return ML_EPARAM;

    if (pipe2(pipefd, O_NONBLOCK) < 0)
        return ML_ERROR;

    while (total < count) {
        ml_int64_t chunk = count - total;
        if (chunk > ML_ZERO_COPY_CHUNK)
            chunk = ML_ZERO_COPY_CHUNK;

        /* in_fd → pipe */
        n = splice(in_fd, NULL, pipefd[1], NULL, (size_t)chunk,
                   SPLICE_F_MOVE);
        if (n <= 0)
            break;

        /* pipe → out_fd */
        n = splice(pipefd[0], NULL, out_fd, NULL, (size_t)n,
                   SPLICE_F_MOVE);
        if (n <= 0)
            break;

        total += n;
    }

    close(pipefd[0]);
    close(pipefd[1]);

    return total > 0 ? total : ML_ERROR;
}

#else /* !__linux__ */

#include "ml_zero_copy.h"
#include "ml_code.h"

ml_int64_t ml_sendfile(ml_int_t out_fd, ml_int_t in_fd,
                       ml_int64_t *offset, ml_int64_t count)
{
    (void)out_fd; (void)in_fd; (void)offset; (void)count;
    return ML_ERROR;
}
ml_int64_t ml_splice(ml_int_t in_fd, ml_int64_t *in_off,
                     ml_int_t out_fd, ml_int64_t *out_off,
                     ml_int64_t count, ml_uint_t flags)
{
    (void)in_fd; (void)in_off; (void)out_fd; (void)out_off; (void)count; (void)flags;
    return ML_ERROR;
}
ml_int64_t ml_copy_file_range(ml_int_t in_fd, ml_int64_t *in_off,
                              ml_int_t out_fd, ml_int64_t *out_off,
                              ml_int64_t count)
{
    (void)in_fd; (void)in_off; (void)out_fd; (void)out_off; (void)count;
    return ML_ERROR;
}
ml_int64_t ml_pipe_relay(ml_int_t in_fd, ml_int_t out_fd, ml_int64_t count)
{
    (void)in_fd; (void)out_fd; (void)count;
    return ML_ERROR;
}

#endif
