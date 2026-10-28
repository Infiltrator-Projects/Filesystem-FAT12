// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file posix.c
 * @brief POSIX implementation of shared file, path and clock primitives.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#if defined(__linux__)
#define _GNU_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "infiltratr/posix.h"
#include "infiltratr/arithmetic.h"
#include "infiltratr/core.h"
#include "posix_read_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    const void *data;
    size_t length;
} InfiltratrAtomicBytes;

const char *infiltratr_io_result_name(InfiltratrIoResult result)
{
    switch (result) {
    case INFILTRATR_IO_OK:
        return "ok";
    case INFILTRATR_IO_INVALID_ARGUMENT:
        return "invalid-argument";
    case INFILTRATR_IO_NOT_FOUND:
        return "not-found";
    case INFILTRATR_IO_PERMISSION_DENIED:
        return "permission-denied";
    case INFILTRATR_IO_EMPTY:
        return "empty";
    case INFILTRATR_IO_TRUNCATED:
        return "truncated";
    case INFILTRATR_IO_INVALID_VALUE:
        return "invalid-value";
    case INFILTRATR_IO_ERROR:
        return "io-error";
    }
    return "unknown";
}

bool infiltratr_realpath_copy(const char *path, char *destination, size_t size)
{
    if (!path || !destination || size == 0U) return false;
    destination[0] = '\0';

    char *resolved = realpath(path, NULL);
    if (!resolved) return false;
    const size_t length = strlen(resolved);
    if (length >= size) {
        free(resolved);
        return false;
    }
    memcpy(destination, resolved, length + 1U);
    free(resolved);
    return true;
}

bool infiltratr_path_concat(char *destination, size_t size,
                            const char *base, const char *suffix)
{
    if (!destination || size == 0U || !base || !suffix) return false;
    const size_t base_length = strlen(base);
    const size_t suffix_length = strlen(suffix);
    if (base_length >= size || suffix_length > size - base_length - 1U) {
        destination[0] = '\0';
        return false;
    }
    memcpy(destination, base, base_length);
    memcpy(destination + base_length, suffix, suffix_length + 1U);
    return true;
}

bool infiltratr_path_join(char *destination, size_t size,
                          const char *left, const char *right)
{
    if (!destination || size == 0U || !left || !right) return false;
    const size_t left_length = strlen(left);
    const bool needs_separator = left_length > 0U && left[left_length - 1U] != '/';
    const char *right_start = right;
    while (*right_start == '/' && left_length > 0U) right_start++;
    const size_t right_length = strlen(right_start);
    const size_t separator_length = needs_separator ? 1U : 0U;
    if (left_length >= size || separator_length > size - left_length - 1U ||
        right_length > size - left_length - separator_length - 1U) {
        destination[0] = '\0';
        return false;
    }
    memcpy(destination, left, left_length);
    size_t offset = left_length;
    if (needs_separator) destination[offset++] = '/';
    memcpy(destination + offset, right_start, right_length + 1U);
    return true;
}

static bool copy_path_value(const char *value,
                            char *destination, size_t size)
{
    if (!destination || size == 0U) return false;
    destination[0] = '\0';
    if (!value || !value[0]) return false;
    const size_t length = strlen(value);
    if (length >= size) return false;
    memcpy(destination, value, length + 1U);
    return true;
}

static char *posix_home_directory_alloc_internal(void)
{
    const char *environment_home = getenv("HOME");
    if (environment_home && environment_home[0])
        return strdup(environment_home);

    long suggested = sysconf(_SC_GETPW_R_SIZE_MAX);
    size_t capacity = suggested > 0 ? (size_t)suggested : 16384U;
    if (capacity < 1024U) capacity = 1024U;

    char *storage = NULL;
    struct passwd entry;
    struct passwd *result = NULL;
    for (;;) {
        char *grown = realloc(storage, capacity);
        if (!grown) {
            free(storage);
            return NULL;
        }
        storage = grown;
        const int failure =
            getpwuid_r(getuid(), &entry, storage, capacity, &result);
        if (failure == 0) break;
        if (failure != ERANGE || capacity > SIZE_MAX / 2U) {
            free(storage);
            return NULL;
        }
        capacity *= 2U;
    }

    char *home = NULL;
    if (result && result->pw_dir && result->pw_dir[0])
        home = strdup(result->pw_dir);
    free(storage);
    return home;
}

bool infiltratr_posix_home_directory(char *destination, size_t size)
{
    if (!destination || size == 0U) return false;
    destination[0] = '\0';

    char *home = posix_home_directory_alloc_internal();
    if (!home) return false;
    const bool okay = copy_path_value(home, destination, size);
    free(home);
    return okay;
}

static char *path_join_alloc_internal(const char *left, const char *right)
{
    if (!left || !right) return NULL;

    const size_t left_length = strlen(left);
    const char *right_start = right;
    while (*right_start == '/' && left_length > 0U) right_start++;
    const size_t right_length = strlen(right_start);
    const bool needs_separator =
        left_length > 0U && left[left_length - 1U] != '/';
    size_t size = 0U;
    size_t length = 0U;

    if (!infiltratr_size_add_checked(left_length, right_length, &length) ||
        (needs_separator &&
         !infiltratr_size_add_checked(length, 1U, &length)) ||
        !infiltratr_size_add_checked(length, 1U, &size)) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    char *joined = malloc(size);
    if (!joined) return NULL;
    if (!infiltratr_path_join(joined, size, left, right_start)) {
        free(joined);
        errno = ENAMETOOLONG;
        return NULL;
    }
    return joined;
}

static char *xdg_home_alloc(const char *environment_name,
                            const char *fallback_suffix)
{
    const char *configured = getenv(environment_name);
    if (configured && configured[0] == '/')
        return strdup(configured);

    char *home = posix_home_directory_alloc_internal();
    if (!home) return NULL;
    char *resolved = path_join_alloc_internal(home, fallback_suffix);
    free(home);
    return resolved;
}

static bool xdg_home(const char *environment_name, const char *fallback_suffix,
                     char *destination, size_t size)
{
    if (!destination || size == 0U) return false;
    destination[0] = '\0';

    char *resolved = xdg_home_alloc(environment_name, fallback_suffix);
    if (!resolved) return false;
    const bool okay = copy_path_value(resolved, destination, size);
    free(resolved);
    return okay;
}

bool infiltratr_xdg_config_home(char *destination, size_t size)
{
    return xdg_home("XDG_CONFIG_HOME", ".config", destination, size);
}

bool infiltratr_xdg_config_home_alloc(char **destination)
{
    if (!destination) return false;
    *destination = xdg_home_alloc("XDG_CONFIG_HOME", ".config");
    return *destination != NULL;
}

bool infiltratr_xdg_data_home(char *destination, size_t size)
{
    return xdg_home("XDG_DATA_HOME", ".local/share", destination, size);
}

static int open_directory_relative(int directory_fd,
                                   const char *component)
{
    int flags = O_CLOEXEC | O_DIRECTORY;
#if defined(O_PATH)
    flags |= O_PATH;
#elif defined(O_SEARCH)
    flags |= O_SEARCH;
#else
    flags |= O_RDONLY;
#endif
    return openat(directory_fd, component, flags);
}

static int open_directory_anchor(const char *path)
{
    int flags = O_CLOEXEC | O_DIRECTORY;
#if defined(O_PATH)
    flags |= O_PATH;
#elif defined(O_SEARCH)
    flags |= O_SEARCH;
#else
    flags |= O_RDONLY;
#endif
    return open(path, flags);
}

int infiltratr_mkdir_parents(const char *path, unsigned int mode)
{
    char *copy;
    char *cursor;
    int directory_fd;
    int failure = 0;
    const mode_t permissions = (mode_t)(mode & 07777U);

    if (!path || !path[0]) return EINVAL;

    copy = strdup(path);
    if (!copy) return errno ? errno : ENOMEM;

    size_t length = strlen(copy);
    while (length > 1U && copy[length - 1U] == '/')
        copy[--length] = '\0';

    directory_fd = open_directory_anchor(copy[0] == '/' ? "/" : ".");
    if (directory_fd < 0) {
        failure = errno;
        free(copy);
        return failure;
    }

    cursor = copy + (copy[0] == '/' ? 1 : 0);
    while (*cursor && failure == 0) {
        char *separator;
        int next_fd;

        while (*cursor == '/') ++cursor;
        if (!*cursor) break;

        separator = strchr(cursor, '/');
        if (separator) *separator = '\0';

        if (mkdirat(directory_fd, cursor, permissions) != 0 &&
            errno != EEXIST) {
            failure = errno;
        } else {
            next_fd = open_directory_relative(directory_fd, cursor);
            if (next_fd < 0) {
                failure = errno;
            } else {
                if (close(directory_fd) != 0) {
                    failure = errno;
                    (void)close(next_fd);
                } else {
                    directory_fd = next_fd;
                }
            }
        }

        if (!separator) break;
        cursor = separator + 1;
    }

    if (failure == 0 && close(directory_fd) != 0)
        failure = errno;
    else if (failure != 0)
        (void)close(directory_fd);

    free(copy);
    return failure;
}

bool infiltratr_first_readable_path(const char *base,
                                    const char *const *suffixes,
                                    size_t suffix_count,
                                    char *destination, size_t size)
{
    if (!destination || size == 0U) return false;
    destination[0] = '\0';
    if (!base || !suffixes) return false;

    for (size_t index = 0U; index < suffix_count; index++) {
        const char *suffix = suffixes[index];
        if (!suffix ||
            !infiltratr_path_concat(destination, size, base, suffix))
            continue;
        if (access(destination, R_OK) == 0) return true;
    }
    destination[0] = '\0';
    return false;
}

InfiltratrIoResult infiltratr_read_text_file_ex(const char *path,
                                                char *buffer, size_t size,
                                                size_t *length)
{
    if (length) *length = 0U;
    if (!path || !buffer || size < 2U) return INFILTRATR_IO_INVALID_ARGUMENT;
    buffer[0] = '\0';

    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0)
        return infiltratr_posix_io_result_from_errno(errno);

    size_t used = 0U;
    InfiltratrIoResult result = INFILTRATR_IO_OK;
    while (used + 1U < size) {
        ssize_t amount;
        do {
            amount = read(descriptor, buffer + used, size - used - 1U);
        } while (amount < 0 && errno == EINTR);

        if (amount < 0) {
            result = infiltratr_posix_io_result_from_errno(errno);
            break;
        }
        if (amount == 0) break;
        used += (size_t)amount;
    }

    if (result == INFILTRATR_IO_OK && used + 1U == size) {
        char extra = '\0';
        ssize_t amount;
        do {
            amount = read(descriptor, &extra, 1U);
        } while (amount < 0 && errno == EINTR);
        if (amount < 0)
            result = infiltratr_posix_io_result_from_errno(errno);
        else if (amount > 0)
            result = INFILTRATR_IO_TRUNCATED;
    }

    const int close_result = close(descriptor);
    if (result == INFILTRATR_IO_OK && close_result != 0)
        result = infiltratr_posix_io_result_from_errno(errno);

    buffer[used] = '\0';
    if ((result == INFILTRATR_IO_OK ||
         result == INFILTRATR_IO_TRUNCATED) &&
        memchr(buffer, '\0', used) != NULL) {
        buffer[0] = '\0';
        if (length) *length = 0U;
        return INFILTRATR_IO_INVALID_VALUE;
    }

    size_t retained = used;
    while (retained > 0U &&
           (buffer[retained - 1U] == '\r' ||
            buffer[retained - 1U] == '\n')) {
        retained--;
    }
    buffer[retained] = '\0';
    if (length) *length = retained;
    if (result != INFILTRATR_IO_OK) return result;
    return used == 0U ? INFILTRATR_IO_EMPTY : INFILTRATR_IO_OK;
}

InfiltratrIoResult infiltratr_read_u64_file_ex(const char *path,
                                               uint64_t *value)
{
    return infiltratr_posix_read_numeric_file(
        path, INFILTRATR_POSIX_NUMERIC_U64, value);
}

InfiltratrIoResult infiltratr_read_double_file_ex(const char *path,
                                                  double *value)
{
    return infiltratr_posix_read_numeric_file(
        path, INFILTRATR_POSIX_NUMERIC_DOUBLE, value);
}

InfiltratrIoResult infiltratr_read_text_file_alloc(const char *path,
                                                   char **text,
                                                   size_t *length)
{
    return infiltratr_posix_read_alloc(path, text, length);
}

bool infiltratr_read_text_file(const char *path, char *buffer, size_t size)
{
    if (!path || !buffer || size < 2U) return false;
    const InfiltratrIoResult status =
        infiltratr_read_text_file_ex(path, buffer, size, NULL);
    if (status == INFILTRATR_IO_OK) return true;
    buffer[0] = '\0';
    return false;
}

bool infiltratr_read_u64_file(const char *path, uint64_t *value)
{
    return infiltratr_read_u64_file_ex(path, value) == INFILTRATR_IO_OK;
}

uint64_t infiltratr_read_u64_or_zero(const char *path)
{
    uint64_t value = 0U;
    (void)infiltratr_read_u64_file(path, &value);
    return value;
}

bool infiltratr_read_double_file(const char *path, double *value)
{
    return infiltratr_read_double_file_ex(path, value) == INFILTRATR_IO_OK;
}

double infiltratr_read_double_or_nan(const char *path)
{
    double value = NAN;
    (void)infiltratr_read_double_file(path, &value);
    return value;
}

static char *path_concat_alloc(const char *base, const char *suffix)
{
    if (!base || !suffix) return NULL;

    const size_t base_length = strlen(base);
    const size_t suffix_length = strlen(suffix);
    size_t length = 0U;
    size_t size = 0U;
    if (!infiltratr_size_add_checked(base_length, suffix_length, &length) ||
        !infiltratr_size_add_checked(length, 1U, &size)) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    char *path = malloc(size);
    if (!path) return NULL;
    memcpy(path, base, base_length);
    memcpy(path + base_length, suffix, suffix_length + 1U);
    return path;
}

bool infiltratr_read_first_u64(const char *base,
                               const char *const *suffixes,
                               size_t suffix_count, uint64_t *value)
{
    if (!base || !suffixes || !value) return false;

    for (size_t index = 0U; index < suffix_count; index++) {
        if (!suffixes[index]) continue;
        char *path = path_concat_alloc(base, suffixes[index]);
        if (!path) return false;
        const bool read = infiltratr_read_u64_file(path, value);
        free(path);
        if (read) return true;
    }
    return false;
}

static char *atomic_parent_directory(const char *path)
{
    const char *separator = strrchr(path, '/');
    if (!separator) return strdup(".");
    if (separator == path) return strdup("/");

    const size_t length = (size_t)(separator - path);
    char *parent = malloc(length + 1U);
    if (!parent) return NULL;
    memcpy(parent, path, length);
    parent[length] = '\0';
    return parent;
}

static char *atomic_temporary_template(const char *parent)
{
    static const char suffix[] = ".infiltratr-write-XXXXXX";
    const size_t parent_length = strlen(parent);
    const bool needs_separator = parent_length == 0U ||
                                 parent[parent_length - 1U] != '/';
    if (parent_length > SIZE_MAX - sizeof(suffix) - 1U) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    const size_t size = parent_length + (needs_separator ? 1U : 0U) +
                        sizeof(suffix);
    char *temporary = malloc(size);
    if (!temporary) return NULL;
    const int written = snprintf(temporary, size, "%s%s%s", parent,
                                 needs_separator ? "/" : "", suffix);
    if (written < 0 || (size_t)written >= size) {
        free(temporary);
        errno = ENAMETOOLONG;
        return NULL;
    }
    return temporary;
}

static const char *atomic_entry_name(const char *path)
{
    const char *separator = strrchr(path, '/');
    return separator != NULL ? separator + 1 : path;
}

static int atomic_open_directory(const char *path)
{
    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return -1;

    struct stat status;
    if (fstat(descriptor, &status) != 0) {
        const int failure = errno;
        (void)close(descriptor);
        errno = failure;
        return -1;
    }
    if (!S_ISDIR(status.st_mode)) {
        (void)close(descriptor);
        errno = ENOTDIR;
        return -1;
    }
    return descriptor;
}

static int atomic_completed_mode_at(int parent_descriptor,
                                    const char *entry_name,
                                    InfiltratrAtomicFileMode mode,
                                    mode_t *permissions)
{
    if (!permissions) return EINVAL;
    if (mode == INFILTRATR_ATOMIC_FILE_PRIVATE) {
        *permissions = S_IRUSR | S_IWUSR;
        return 0;
    }
    if (mode != INFILTRATR_ATOMIC_FILE_PRESERVE_PERMISSIONS) return EINVAL;

    struct stat status;
    if (fstatat(parent_descriptor, entry_name, &status,
                AT_SYMLINK_NOFOLLOW) == 0) {
        *permissions = S_ISREG(status.st_mode)
            ? status.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)
            : S_IRUSR | S_IWUSR;
        return 0;
    }
    if (errno != ENOENT) return errno;
    *permissions = S_IRUSR | S_IWUSR;
    return 0;
}

#if !defined(__linux__)
static int atomic_mark_close_on_exec(int descriptor)
{
    const int flags = fcntl(descriptor, F_GETFD);
    if (flags < 0 || fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) != 0)
        return errno;
    return 0;
}
#endif

static int atomic_create_temporary(char *temporary)
{
#if defined(__linux__)
    return mkostemp(temporary, O_CLOEXEC);
#else
    const int descriptor = mkstemp(temporary);
    if (descriptor < 0) return -1;

    const int failure = atomic_mark_close_on_exec(descriptor);
    if (failure != 0) {
        (void)close(descriptor);
        (void)unlink(temporary);
        errno = failure;
        return -1;
    }
    return descriptor;
#endif
}

static int atomic_verify_temporary(int parent_descriptor,
                                   const char *entry_name,
                                   int descriptor)
{
    struct stat opened;
    struct stat anchored;

    if (fstat(descriptor, &opened) != 0) return errno;
    if (fstatat(parent_descriptor, entry_name, &anchored,
                AT_SYMLINK_NOFOLLOW) != 0) {
        return errno;
    }
    if (!S_ISREG(opened.st_mode) || !S_ISREG(anchored.st_mode) ||
        opened.st_dev != anchored.st_dev ||
        opened.st_ino != anchored.st_ino) {
        return EIO;
    }
    return 0;
}

static int atomic_finish_stream(FILE *stream, bool content_complete)
{
    int failure = content_complete ? 0 : (errno ? errno : EIO);
    if (failure == 0 && ferror(stream)) failure = EIO;
    if (failure == 0 && fflush(stream) != 0) failure = errno;
    if (failure == 0 && fsync(fileno(stream)) != 0) failure = errno;
    if (fclose(stream) != 0 && failure == 0) failure = errno;
    return failure;
}

int infiltratr_atomic_file_write(const char *path,
                                 InfiltratrAtomicFileMode mode,
                                 InfiltratrAtomicFileWriter writer,
                                 const void *user_data)
{
    if (!path || !*path || !writer) return EINVAL;

    char *parent = atomic_parent_directory(path);
    if (!parent) return errno ? errno : ENOMEM;
    const char *entry_name = atomic_entry_name(path);

    const int parent_descriptor = atomic_open_directory(parent);
    if (parent_descriptor < 0) {
        const int failure = errno;
        free(parent);
        return failure;
    }

    mode_t permissions = 0;
    int failure = atomic_completed_mode_at(
        parent_descriptor, entry_name, mode, &permissions);
    char *temporary = NULL;
    const char *temporary_name = NULL;
    int descriptor = -1;
    bool temporary_anchored = false;

    if (failure == 0) {
        temporary = atomic_temporary_template(parent);
        if (!temporary) failure = errno ? errno : ENOMEM;
    }
    if (failure == 0) {
        descriptor = atomic_create_temporary(temporary);
        if (descriptor < 0) failure = errno;
    }
    if (failure == 0) {
        temporary_name = atomic_entry_name(temporary);
        failure = atomic_verify_temporary(
            parent_descriptor, temporary_name, descriptor);
        temporary_anchored = failure == 0;
    }
    if (failure == 0 && fchmod(descriptor, permissions) != 0)
        failure = errno;

    FILE *stream = NULL;
    if (failure == 0) {
        stream = fdopen(descriptor, "wb");
        if (!stream) failure = errno;
    }
    if (!stream) {
        if (descriptor >= 0) (void)close(descriptor);
    } else {
        errno = 0;
        const bool content_complete = writer(stream, user_data);
        failure = atomic_finish_stream(stream, content_complete);
    }

    if (failure == 0 &&
        renameat(parent_descriptor, temporary_name,
                 parent_descriptor, entry_name) != 0) {
        failure = errno;
    }
    if (failure == 0 && fsync(parent_descriptor) != 0) failure = errno;

    if (failure != 0 && temporary_anchored)
        (void)unlinkat(parent_descriptor, temporary_name, 0);

    if (close(parent_descriptor) != 0 && failure == 0) failure = errno;
    free(temporary);
    free(parent);
    return failure;
}

static bool atomic_write_bytes(FILE *stream, const void *user_data)
{
    const InfiltratrAtomicBytes *bytes = user_data;
    return bytes->length == 0U ||
           fwrite(bytes->data, 1U, bytes->length, stream) == bytes->length;
}

int infiltratr_atomic_file_write_bytes(const char *path,
                                       InfiltratrAtomicFileMode mode,
                                       const void *data, size_t length)
{
    if (!data && length != 0U) return EINVAL;
    InfiltratrAtomicBytes bytes = {.data = data, .length = length};
    return infiltratr_atomic_file_write(path, mode, atomic_write_bytes, &bytes);
}

int infiltratr_unlink_durable(const char *path, bool missing_ok)
{
    if (!path || !*path) return EINVAL;

    char *parent = atomic_parent_directory(path);
    if (!parent) return errno ? errno : ENOMEM;
    const char *entry_name = atomic_entry_name(path);

    const int parent_descriptor = atomic_open_directory(parent);
    if (parent_descriptor < 0) {
        const int failure = errno;
        free(parent);
        if (failure == ENOENT && missing_ok) return 0;
        return failure;
    }

    if (unlinkat(parent_descriptor, entry_name, 0) != 0) {
        const int failure = errno;
        (void)close(parent_descriptor);
        free(parent);
        if (failure == ENOENT && missing_ok) return 0;
        return failure;
    }

    int failure = 0;
    if (fsync(parent_descriptor) != 0) failure = errno;
    if (close(parent_descriptor) != 0 && failure == 0) failure = errno;
    free(parent);
    return failure;
}

static bool posix_timespec_is_normalized_nonnegative(
    const struct timespec *value)
{
    if (!value || value->tv_nsec < 0 || value->tv_nsec >= 1000000000L)
        return false;
    if ((time_t)-1 < (time_t)0 && (intmax_t)value->tv_sec < 0)
        return false;
    return true;
}

static uintmax_t posix_time_t_maximum(void)
{
    if ((time_t)-1 > (time_t)0) return (uintmax_t)(time_t)-1;

    const unsigned int time_bits = (unsigned int)(sizeof(time_t) * CHAR_BIT);
    const unsigned int uintmax_bits =
        (unsigned int)(sizeof(uintmax_t) * CHAR_BIT);
    if (time_bits >= uintmax_bits) return UINTMAX_MAX >> 1U;
    return (UINTMAX_C(1) << (time_bits - 1U)) - 1U;
}

int infiltratr_posix_deadline_after_milliseconds(int clock_id,
                                                  uint64_t milliseconds,
                                                  struct timespec *deadline)
{
    if (!deadline) return EINVAL;

    struct timespec now = {0};
    if (clock_gettime((clockid_t)clock_id, &now) != 0)
        return errno ? errno : EIO;
    if (!posix_timespec_is_normalized_nonnegative(&now)) return EINVAL;

    uintmax_t extra_seconds = (uintmax_t)(milliseconds / UINT64_C(1000));
    uint64_t nanoseconds =
        (uint64_t)now.tv_nsec +
        (milliseconds % UINT64_C(1000)) * UINT64_C(1000000);
    if (nanoseconds >= UINT64_C(1000000000)) {
        nanoseconds -= UINT64_C(1000000000);
        extra_seconds++;
    }

    const uintmax_t current_seconds = (uintmax_t)now.tv_sec;
    const uintmax_t maximum_seconds = posix_time_t_maximum();
    if (current_seconds > maximum_seconds ||
        extra_seconds > maximum_seconds - current_seconds)
        return EOVERFLOW;

    struct timespec result = {
        .tv_sec = (time_t)(current_seconds + extra_seconds),
        .tv_nsec = (long)nanoseconds
    };
    *deadline = result;
    return 0;
}

int infiltratr_posix_deadline_remaining_milliseconds(
    int clock_id, const struct timespec *deadline, uint64_t *milliseconds)
{
    if (!deadline || !milliseconds ||
        !posix_timespec_is_normalized_nonnegative(deadline))
        return EINVAL;

    struct timespec now = {0};
    if (clock_gettime((clockid_t)clock_id, &now) != 0)
        return errno ? errno : EIO;
    if (!posix_timespec_is_normalized_nonnegative(&now)) return EINVAL;

    if (deadline->tv_sec < now.tv_sec ||
        (deadline->tv_sec == now.tv_sec &&
         deadline->tv_nsec <= now.tv_nsec)) {
        *milliseconds = 0U;
        return 0;
    }

    uintmax_t seconds =
        (uintmax_t)deadline->tv_sec - (uintmax_t)now.tv_sec;
    long nanoseconds = deadline->tv_nsec - now.tv_nsec;
    if (nanoseconds < 0) {
        if (seconds == 0U) return EINVAL;
        seconds--;
        nanoseconds += 1000000000L;
    }

    if (seconds > UINT64_MAX / UINT64_C(1000)) return EOVERFLOW;
    uint64_t result = (uint64_t)seconds * UINT64_C(1000);
    const uint64_t fractional =
        ((uint64_t)nanoseconds + UINT64_C(999999)) / UINT64_C(1000000);
    if (fractional > UINT64_MAX - result) return EOVERFLOW;
    result += fractional;

    *milliseconds = result;
    return 0;
}

bool infiltratr_monotonic_nanoseconds(uint64_t *nanoseconds)
{
    if (!nanoseconds) return false;
    *nanoseconds = 0U;

    struct timespec timestamp = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0 ||
        timestamp.tv_sec < 0 || timestamp.tv_nsec < 0 ||
        timestamp.tv_nsec >= 1000000000L)
        return false;

    const uint64_t seconds = (uint64_t)timestamp.tv_sec;
    const uint64_t fractional = (uint64_t)timestamp.tv_nsec;
    if (seconds > (UINT64_MAX - fractional) / 1000000000U)
        return false;

    *nanoseconds = seconds * 1000000000U + fractional;
    return true;
}

double infiltratr_monotonic_seconds(void)
{
    struct timespec timestamp = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0 ||
        timestamp.tv_sec < 0 || timestamp.tv_nsec < 0 ||
        timestamp.tv_nsec >= 1000000000L)
        return 0.0;
    return (double)timestamp.tv_sec +
           (double)timestamp.tv_nsec / 1000000000.0;
}
