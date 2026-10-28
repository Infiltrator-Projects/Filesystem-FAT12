// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file posix_io.h
 * @brief Exact EINTR-safe POSIX descriptor I/O.
 *
 * These helpers complete the requested byte count or fail; short transfers are
 * retried and EINTR is transparent. Individual system calls are capped at
 * SSIZE_MAX so oversized size_t requests never rely on implementation-defined
 * transfer counts. A zero-byte transfer before completion is treated as EIO
 * rather than as a successful partial operation. Invalid descriptors/buffers
 * set EINVAL. Positioned offsets are checked against the native signed off_t
 * range and set EOVERFLOW rather than silently narrowing a uint64_t offset.
 * For nonzero positioned transfers, the complete requested byte range is
 * validated before any I/O occurs, so an impossible tail cannot cause a
 * predictable partial read or write before EOVERFLOW is reported.
 */
#ifndef INFILTRATR_COMMON_POSIX_IO_H
#define INFILTRATR_COMMON_POSIX_IO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Read exactly `count` sequential bytes; return 0 or -1 with errno set. */
int infiltratr_read_full(int descriptor, void *buffer, size_t count);
/** Write exactly `count` sequential bytes; return 0 or -1 with errno set. */
int infiltratr_write_full(int descriptor, const void *buffer, size_t count);
/** Read exactly `count` bytes beginning at `offset` without changing file position. */
int infiltratr_pread_full(int descriptor, void *buffer, size_t count,
                          uint64_t offset);
/** Write exactly `count` bytes beginning at `offset` without changing file position. */
int infiltratr_pwrite_full(int descriptor, const void *buffer, size_t count,
                           uint64_t offset);

#ifdef __cplusplus
}
#endif
#endif
