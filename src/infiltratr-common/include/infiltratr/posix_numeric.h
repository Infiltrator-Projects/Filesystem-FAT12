// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file posix_numeric.h
 * @brief Typed signed numeric POSIX file readers.
 *
 * This module completes the signed side of the typed-reader family declared in
 * posix.h while keeping the richer result code available to callers that must
 * distinguish I/O, truncation and parse failures.
 */
#ifndef INFILTRATR_COMMON_POSIX_NUMERIC_H
#define INFILTRATR_COMMON_POSIX_NUMERIC_H

#include "infiltratr/posix.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read and parse one complete signed decimal file value.
 *
 * File status is propagated unchanged. Successfully read text that is not a
 * complete signed base-10 integer returns INFILTRATR_IO_INVALID_VALUE. Caller
 * output changes only on success.
 */
InfiltratrIoResult infiltratr_read_i64_file_ex(const char *path,
                                               int64_t *value);

/** Boolean compatibility wrapper around infiltratr_read_i64_file_ex(). */
bool infiltratr_read_i64_file(const char *path, int64_t *value);

#ifdef __cplusplus
}
#endif
#endif
