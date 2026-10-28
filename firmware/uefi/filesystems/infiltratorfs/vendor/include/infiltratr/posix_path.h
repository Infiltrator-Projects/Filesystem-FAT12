// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file posix_path.h
 * @brief Small POSIX lexical-path primitives shared by native consumers.
 *
 * These helpers operate on POSIX path syntax only and perform no filesystem
 * access. They are separate from the portable target because slash semantics
 * are part of the POSIX provider contract.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATR_COMMON_POSIX_PATH_H
#define INFILTRATR_COMMON_POSIX_PATH_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return the final lexical component of a POSIX path.
 *
 * The returned pointer aliases `path` and must not be freed. NULL maps to the
 * empty string. A trailing slash therefore has an empty final component, which
 * matches the simple `strrchr(path, '/') + 1` semantics used by consumers.
 */
const char *infiltratr_path_basename(const char *path);

/**
 * Copy the lexical parent directory of a POSIX path into caller storage.
 *
 * Trailing separators are ignored. A path with no separator yields "." and a
 * path directly below the root yields "/". Repeated separators immediately
 * before the final component are collapsed in the returned parent. NULL input,
 * missing output storage or truncation returns false; valid output storage is
 * cleared on failure.
 */
bool infiltratr_path_dirname(const char *path, char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif
