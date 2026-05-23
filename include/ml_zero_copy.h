/*
 * ml_zero_copy — Kernel-assisted zero-copy I/O wrappers.
 *
 * Wraps sendfile(2), splice(2), and copy_file_range(2) with
 * fallback to read/write when kernel support is unavailable.
 */
#ifndef __ML_ZERO_COPY_H__
#define __ML_ZERO_COPY_H__

#include "ml_types.h"

/*
 * Send file data to socket, zero-copy. Kernel doesn't touch userspace.
 *   out_fd:  socket or other writable fd
 *   in_fd:   regular file or mmap-capable fd
 *   offset:  input file offset (NULL = use current position)
 *   count:   bytes to transfer
 * Returns bytes transferred, or ML_ERROR on failure.
 */
ml_int64_t ml_sendfile(ml_int_t out_fd, ml_int_t in_fd, ml_int64_t *offset, ml_int64_t count);

/*
 * Splice data between two fds via kernel pipe. At least one fd must be a pipe.
 * Returns bytes transferred, or ML_ERROR on failure.
 */
ml_int64_t ml_splice(ml_int_t in_fd, ml_int64_t *in_off,
                     ml_int_t out_fd, ml_int64_t *out_off,
                     ml_int64_t count, ml_uint_t flags);

/*
 * Copy a range of bytes between files inside the kernel (no userspace copy).
 * Both fds must refer to regular files on the same filesystem for zero-copy;
 * falls back to kernel buffer copy across filesystems.
 */
ml_int64_t ml_copy_file_range(ml_int_t in_fd, ml_int64_t *in_off,
                              ml_int_t out_fd, ml_int64_t *out_off,
                              ml_int64_t count);

/*
 * Convenience: relay data from one fd to another using a kernel pipe + splice.
 * Creates a pipe on first call and caches it (thread-local).
 * Returns total bytes transferred, or ML_ERROR.
 */
ml_int64_t ml_pipe_relay(ml_int_t in_fd, ml_int_t out_fd, ml_int64_t count);

/* Maximum bytes to transfer in a single splice() call (Linux default) */
#define ML_ZERO_COPY_CHUNK  (16 * 1024 * 1024)  /* 16 MiB */

#endif
