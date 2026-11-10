// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file posix.h
 * @brief POSIX file, path and monotonic-clock adapters for the shared C core.
 *
 * The rich `*_ex` file readers preserve meaningful failure categories and are
 * preferred when a caller needs to distinguish missing data, permission
 * failures, truncation and invalid values. The boolean/value helpers preserve
 * the same complete-input semantics while collapsing those distinctions.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATR_COMMON_POSIX_H
#define INFILTRATR_COMMON_POSIX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INFILTRATR_IO_OK = 0,
    INFILTRATR_IO_INVALID_ARGUMENT,
    INFILTRATR_IO_NOT_FOUND,
    INFILTRATR_IO_PERMISSION_DENIED,
    INFILTRATR_IO_EMPTY,
    INFILTRATR_IO_TRUNCATED,
    INFILTRATR_IO_INVALID_VALUE,
    INFILTRATR_IO_ERROR
} InfiltratrIoResult;

typedef enum {
    INFILTRATR_ATOMIC_FILE_PRIVATE = 0,
    INFILTRATR_ATOMIC_FILE_PRESERVE_PERMISSIONS
} InfiltratrAtomicFileMode;

typedef bool (*InfiltratrAtomicFileWriter)(FILE *stream,
                                           const void *user_data);

const char *infiltratr_io_result_name(InfiltratrIoResult result);

bool infiltratr_realpath_copy(const char *path, char *destination, size_t size);

/**
 * Concatenate two byte-exact POSIX path fragments without inserting a
 * separator. Source strings must not overlap writable destination storage.
 * On an otherwise-valid size failure, destination is cleared.
 */
bool infiltratr_path_concat(char *destination, size_t size,
                            const char *base, const char *suffix);

/**
 * Join two POSIX path fragments at their boundary without normalizing either
 * fragment internally. A nonempty left fragment is copied verbatim; leading
 * slashes are removed from the right fragment, and one slash is inserted only
 * when the preserved left fragment does not already end in one. With an empty
 * left fragment, the right fragment is preserved verbatim (including a leading
 * slash). Source strings must not overlap the writable destination. On an
 * otherwise-valid size failure, destination is cleared.
 */
bool infiltratr_path_join(char *destination, size_t size,
                          const char *left, const char *right);

/**
 * Resolve the current POSIX user's home directory.
 *
 * A non-empty HOME environment value is authoritative. When HOME is unset or
 * empty, the implementation falls back to the passwd database for the real
 * user ID. Valid output storage is cleared on failure.
 */
bool infiltratr_posix_home_directory(char *destination, size_t size);

/**
 * Resolve XDG_CONFIG_HOME according to the XDG base-directory contract.
 *
 * An absolute non-empty XDG_CONFIG_HOME is used directly. Relative or missing
 * values fall back to `$HOME/.config`. Valid output storage is cleared on
 * failure.
 */
bool infiltratr_xdg_config_home(char *destination, size_t size);

/**
 * Resolve XDG_CONFIG_HOME into newly allocated storage.
 *
 * This variant has no project-imposed path-length ceiling. On success,
 * *destination owns a NUL-terminated path that the caller releases with
 * free(). On failure *destination is NULL.
 */
bool infiltratr_xdg_config_home_alloc(char **destination);

/**
 * Resolve XDG_DATA_HOME according to the XDG base-directory contract.
 *
 * An absolute non-empty XDG_DATA_HOME is used directly. Relative or missing
 * values fall back to `$HOME/.local/share`. Valid output storage is cleared on
 * failure.
 */
bool infiltratr_xdg_data_home(char *destination, size_t size);

/**
 * Create a POSIX directory path recursively.
 *
 * Existing directory components are accepted, including symlinks that resolve
 * to directories. Traversal is anchored one directory descriptor at a time so
 * already-resolved parent components are not re-resolved by later pathname
 * operations. The supplied permission bits are passed to mkdirat for newly
 * created components and remain subject to umask. Returns zero on success or
 * an errno-style failure code.
 */
int infiltratr_mkdir_parents(const char *path, unsigned int mode);

bool infiltratr_first_readable_path(const char *base,
                                    const char *const *suffixes,
                                    size_t suffix_count,
                                    char *destination, size_t size);

/**
 * Read one complete bounded text file with explicit failure reporting.
 *
 * `size` must be at least two bytes. With otherwise-valid arguments, `buffer`
 * is cleared before opening the path. Reads retry after `EINTR`, reserve one
 * byte for NUL termination and perform an additional one-byte probe when the
 * buffer fills so exact-fit input can be distinguished from truncation.
 * Embedded NUL bytes in the retained byte range are rejected as
 * `INFILTRATR_IO_INVALID_VALUE` rather than allowing later C-string handling
 * to hide trailing file content. Trailing CR/LF bytes in valid retained text
 * are removed using the known byte count rather than rediscovering its length.
 *
 * `length`, when non-NULL, is set to zero before argument validation and then
 * receives the retained post-line-ending length. A zero-byte file returns
 * `INFILTRATR_IO_EMPTY`. `INFILTRATR_IO_TRUNCATED` preserves the bounded
 * prefix in `buffer` so rich callers can inspect it.
 */
InfiltratrIoResult infiltratr_read_text_file_ex(const char *path,
                                                char *buffer, size_t size,
                                                size_t *length);

/**
 * Read one complete file into newly allocated NUL-terminated storage.
 *
 * The complete byte stream is retained with no fixed application-side size
 * limit. Embedded NUL bytes are preserved and included in `length`; callers
 * that require C text must reject them according to their own grammar. On
 * success the caller owns `*text` and must release it with free(). Empty files
 * return INFILTRATR_IO_EMPTY with `*text == NULL`. Output pointers are reset
 * before I/O begins.
 */
InfiltratrIoResult infiltratr_read_text_file_alloc(const char *path,
                                                   char **text,
                                                   size_t *length);

/**
 * Read and parse one complete unsigned decimal file value.
 *
 * The implementation reads to EOF with dynamically grown temporary storage;
 * there is no fixed text-length limit. Embedded NUL bytes are rejected rather
 * than allowing trailing file content to escape validation. I/O status is
 * propagated unchanged and `*value` changes only on successful parsing.
 */
InfiltratrIoResult infiltratr_read_u64_file_ex(const char *path,
                                               uint64_t *value);

/**
 * Read and parse one complete finite ASCII-decimal file value.
 *
 * The implementation reads to EOF with dynamically grown temporary storage;
 * there is no fixed text-length limit. Embedded NUL bytes are rejected.
 * Parsing uses the locale-independent core decimal grammar. I/O status is
 * propagated unchanged and `*value` changes only on success.
 */
InfiltratrIoResult infiltratr_read_double_file_ex(const char *path,
                                                  double *value);

/**
 * Read one complete bounded text file and remove trailing CR/LF bytes.
 *
 * This compatibility wrapper retains the boolean API but not the historical
 * silent-truncation shortcut: oversized, empty or failed reads return false and
 * leave valid output storage empty. Exact-fit input succeeds.
 */
bool infiltratr_read_text_file(const char *path, char *buffer, size_t size);

/** Parse one complete unsigned decimal file value with no fixed text limit. */
bool infiltratr_read_u64_file(const char *path, uint64_t *value);

uint64_t infiltratr_read_u64_or_zero(const char *path);

/** Parse one complete finite decimal file value with no fixed text limit. */
bool infiltratr_read_double_file(const char *path, double *value);

double infiltratr_read_double_or_nan(const char *path);

/**
 * Read the first successfully parsed unsigned attribute from ordered suffixes.
 *
 * Each non-NULL suffix is concatenated directly to `base`; no separator is
 * inserted. Candidate paths are allocated to their exact required length, so
 * the helper imposes no private path-length ceiling beyond the platform and
 * allocator. The first successful value is returned; false means none
 * succeeded or a candidate path could not be represented/allocated.
 */
bool infiltratr_read_first_u64(const char *base,
                               const char *const *suffixes,
                               size_t suffix_count, uint64_t *value);

int infiltratr_atomic_file_write(const char *path,
                                 InfiltratrAtomicFileMode mode,
                                 InfiltratrAtomicFileWriter writer,
                                 const void *user_data);

int infiltratr_atomic_file_write_bytes(const char *path,
                                       InfiltratrAtomicFileMode mode,
                                       const void *data, size_t length);

/**
 * Remove one POSIX directory entry and durably publish that removal.
 *
 * The parent directory is resolved before unlinking so allocation/path errors
 * cannot occur after the namespace change. If `missing_ok` is true, an absent
 * path is treated as success and no directory sync is required. Otherwise an
 * absent path returns ENOENT.
 *
 * A zero return means both unlink and parent-directory fsync completed. If the
 * directory sync fails, the unlink may already have happened; the sync error is
 * returned so callers can retain/reconstruct recovery state as appropriate.
 */
int infiltratr_unlink_durable(const char *path, bool missing_ok);

bool infiltratr_monotonic_nanoseconds(uint64_t *nanoseconds);

double infiltratr_monotonic_seconds(void);

/**
 * Create an absolute POSIX-clock deadline a whole number of milliseconds
 * after the current value of `clock_id`.
 *
 * The clock sample must use a normalized, non-negative `struct timespec`.
 * Zero milliseconds is valid and produces an immediate deadline. The caller's
 * output is unchanged on failure.
 *
 * @return zero on success, otherwise an errno-style error such as EINVAL,
 *         EOVERFLOW or the error reported by clock_gettime().
 */
int infiltratr_posix_deadline_after_milliseconds(int clock_id,
                                                  uint64_t milliseconds,
                                                  struct timespec *deadline);

/**
 * Return the upward-rounded whole milliseconds remaining until an absolute
 * POSIX-clock deadline.
 *
 * A reached or expired deadline returns zero milliseconds. Both the supplied
 * deadline and the sampled clock value must be normalized and non-negative.
 * The caller's output is unchanged on failure.
 *
 * @return zero on success, otherwise an errno-style error such as EINVAL,
 *         EOVERFLOW or the error reported by clock_gettime().
 */
int infiltratr_posix_deadline_remaining_milliseconds(
    int clock_id, const struct timespec *deadline,
    uint64_t *milliseconds);

#ifdef __cplusplus
}
#endif

#endif
