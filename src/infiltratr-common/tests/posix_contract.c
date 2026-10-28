// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "infiltratr/posix.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_all(int descriptor, const char *text, size_t length)
{
    size_t written = 0U;
    while (written < length) {
        const ssize_t amount = write(descriptor, text + written,
                                     length - written);
        assert(amount > 0);
        written += (size_t)amount;
    }
}

static void test_paths(void)
{
    char path[16];
    assert(infiltratr_path_concat(path, sizeof(path), "/sys/", "dev"));
    assert(strcmp(path, "/sys/dev") == 0);

    char too_small[5] = "xxxx";
    assert(!infiltratr_path_concat(too_small, sizeof(too_small),
                                   "/sys", "/device"));
    assert(strcmp(too_small, "") == 0);

    assert(infiltratr_path_join(path, sizeof(path), "/sys/", "/dev"));
    assert(strcmp(path, "/sys/dev") == 0);
    assert(infiltratr_path_join(path, sizeof(path), "", "/dev"));
    assert(strcmp(path, "/dev") == 0);
    assert(infiltratr_path_join(path, sizeof(path), "a///", "///b"));
    assert(strcmp(path, "a///b") == 0);

    char resolved[1] = {'x'};
    assert(!infiltratr_realpath_copy(".", resolved, sizeof(resolved)));
    assert(resolved[0] == '\0');
}

static void test_user_paths_and_directories(void)
{
    const char *old_home_value = getenv("HOME");
    const char *old_config_value = getenv("XDG_CONFIG_HOME");
    const char *old_data_value = getenv("XDG_DATA_HOME");
    char *old_home = old_home_value ? strdup(old_home_value) : NULL;
    char *old_config = old_config_value ? strdup(old_config_value) : NULL;
    char *old_data = old_data_value ? strdup(old_data_value) : NULL;

    assert(setenv("HOME", "/tmp/infiltratr-home", 1) == 0);
    assert(unsetenv("XDG_CONFIG_HOME") == 0);
    assert(unsetenv("XDG_DATA_HOME") == 0);

    char path[256];
    assert(infiltratr_posix_home_directory(path, sizeof(path)));
    assert(strcmp(path, "/tmp/infiltratr-home") == 0);
    assert(infiltratr_xdg_config_home(path, sizeof(path)));
    assert(strcmp(path, "/tmp/infiltratr-home/.config") == 0);
    assert(infiltratr_xdg_data_home(path, sizeof(path)));
    assert(strcmp(path, "/tmp/infiltratr-home/.local/share") == 0);

    assert(setenv("XDG_CONFIG_HOME", "/tmp/infiltratr-config", 1) == 0);
    assert(setenv("XDG_DATA_HOME", "/tmp/infiltratr-data", 1) == 0);
    assert(infiltratr_xdg_config_home(path, sizeof(path)));
    assert(strcmp(path, "/tmp/infiltratr-config") == 0);
    assert(infiltratr_xdg_data_home(path, sizeof(path)));
    assert(strcmp(path, "/tmp/infiltratr-data") == 0);

    char *allocated_config = NULL;
    assert(infiltratr_xdg_config_home_alloc(&allocated_config));
    assert(allocated_config != NULL);
    assert(strcmp(allocated_config, "/tmp/infiltratr-config") == 0);
    free(allocated_config);

    char *long_config = malloc(5002U);
    assert(long_config != NULL);
    long_config[0] = '/';
    memset(long_config + 1U, 'x', 5000U);
    long_config[5001U] = '\0';
    assert(setenv("XDG_CONFIG_HOME", long_config, 1) == 0);
    allocated_config = NULL;
    assert(infiltratr_xdg_config_home_alloc(&allocated_config));
    assert(allocated_config != NULL);
    assert(strcmp(allocated_config, long_config) == 0);
    free(allocated_config);
    free(long_config);

    assert(setenv("XDG_CONFIG_HOME", "relative-config", 1) == 0);
    assert(infiltratr_xdg_config_home(path, sizeof(path)));
    assert(strcmp(path, "/tmp/infiltratr-home/.config") == 0);

    char tiny[2] = "x";
    assert(!infiltratr_xdg_config_home(tiny, sizeof(tiny)));
    assert(tiny[0] == '\0');

    char root[] = "infiltratr-mkdir-XXXXXX";
    assert(mkdtemp(root));
    char nested[256];
    assert(snprintf(nested, sizeof(nested), "%s/a/b/c", root) > 0);
    assert(infiltratr_mkdir_parents(nested, 0700U) == 0);
    assert(infiltratr_mkdir_parents(nested, 0700U) == 0);
    struct stat status;
    assert(stat(nested, &status) == 0 && S_ISDIR(status.st_mode));

    char symlink_target[256];
    char symlink_path[256];
    char symlink_child[256];
    assert(snprintf(symlink_target, sizeof(symlink_target), "%s/target", root) > 0);
    assert(mkdir(symlink_target, 0700) == 0);
    assert(snprintf(symlink_path, sizeof(symlink_path), "%s/link", root) > 0);
    assert(symlink("target", symlink_path) == 0);
    assert(snprintf(symlink_child, sizeof(symlink_child), "%s/link/child", root) > 0);
    assert(infiltratr_mkdir_parents(symlink_child, 0700U) == 0);
    assert(stat(symlink_child, &status) == 0 && S_ISDIR(status.st_mode));
    assert(rmdir(symlink_child) == 0);
    assert(unlink(symlink_path) == 0);
    assert(rmdir(symlink_target) == 0);

    char child[256];
    assert(snprintf(child, sizeof(child), "%s/a/b", root) > 0);
    assert(rmdir(nested) == 0);
    assert(rmdir(child) == 0);
    assert(snprintf(child, sizeof(child), "%s/a", root) > 0);
    assert(rmdir(child) == 0);
    assert(rmdir(root) == 0);

    if (old_home) {
        assert(setenv("HOME", old_home, 1) == 0);
        free(old_home);
    } else {
        assert(unsetenv("HOME") == 0);
    }
    if (old_config) {
        assert(setenv("XDG_CONFIG_HOME", old_config, 1) == 0);
        free(old_config);
    } else {
        assert(unsetenv("XDG_CONFIG_HOME") == 0);
    }
    if (old_data) {
        assert(setenv("XDG_DATA_HOME", old_data, 1) == 0);
        free(old_data);
    } else {
        assert(unsetenv("XDG_DATA_HOME") == 0);
    }
}

static void test_first_readable_path(void)
{
    char directory[] = "infiltratr-readable-path-XXXXXX";
    assert(mkdtemp(directory));
    char readable[256];
    assert(snprintf(readable, sizeof(readable), "%s/present", directory) > 0);
    FILE *file = fopen(readable, "wb");
    assert(file);
    assert(fputs("value\n", file) >= 0);
    assert(fclose(file) == 0);

    static const char *const suffixes[] = {"/missing", "/present"};
    char selected[256] = "old";
    assert(infiltratr_first_readable_path(directory, suffixes, 2U,
                                          selected, sizeof(selected)));
    assert(strcmp(selected, readable) == 0);

    static const char *const absent[] = {"/missing", NULL};
    assert(!infiltratr_first_readable_path(directory, absent, 2U,
                                           selected, sizeof(selected)));
    assert(strcmp(selected, "") == 0);
    assert(unlink(readable) == 0);
    assert(rmdir(directory) == 0);
}

static void test_long_first_u64_path(void)
{
    char root[] = "infiltratr-long-u64-XXXXXX";
    assert(mkdtemp(root));

    char paths[7][1024];
    assert(snprintf(paths[0], sizeof(paths[0]), "%s", root) > 0);
    for (size_t depth = 1U; depth < 7U; ++depth) {
        char component[82];
        memset(component, (int)('a' + (int)depth), 80U);
        component[80] = (char)('0' + (int)depth);
        component[81] = '\0';
        const size_t parent_length = strlen(paths[depth - 1U]);
        const size_t component_length = strlen(component);
        assert(parent_length + 1U + component_length + 1U <=
               sizeof(paths[depth]));
        memcpy(paths[depth], paths[depth - 1U], parent_length);
        paths[depth][parent_length] = '/';
        memcpy(paths[depth] + parent_length + 1U, component,
               component_length + 1U);
        assert(mkdir(paths[depth], 0700) == 0);
    }
    assert(strlen(paths[6]) > 512U);

    char file_path[1024];
    const int written = snprintf(file_path, sizeof(file_path),
                                 "%s/value", paths[6]);
    assert(written > 0 && (size_t)written < sizeof(file_path));
    FILE *file = fopen(file_path, "wb");
    assert(file);
    assert(fputs("42\n", file) >= 0);
    assert(fclose(file) == 0);

    static const char *const suffixes[] = {"/missing", "/value"};
    uint64_t value = 0U;
    assert(infiltratr_read_first_u64(paths[6], suffixes, 2U, &value));
    assert(value == 42U);

    assert(unlink(file_path) == 0);
    for (size_t depth = 7U; depth-- > 1U;)
        assert(rmdir(paths[depth]) == 0);
    assert(rmdir(root) == 0);
}

static void test_text_io(void)
{
    char empty_name[] = "infiltratr-contract-empty-XXXXXX";
    const int empty_fd = mkstemp(empty_name);
    assert(empty_fd >= 0);
    assert(close(empty_fd) == 0);

    char read_buffer[8] = "old";
    size_t length = 99U;
    assert(infiltratr_read_text_file_ex(empty_name, read_buffer,
                                        sizeof(read_buffer), &length) ==
           INFILTRATR_IO_EMPTY);
    assert(strcmp(read_buffer, "") == 0);
    assert(length == 0U);
    assert(!infiltratr_read_text_file(empty_name, read_buffer,
                                      sizeof(read_buffer)));
    assert(unlink(empty_name) == 0);

    char exact_name[] = "infiltratr-contract-exact-XXXXXX";
    const int exact_fd = mkstemp(exact_name);
    assert(exact_fd >= 0);
    static const char exact_text[] = "1234567";
    write_all(exact_fd, exact_text, sizeof(exact_text) - 1U);
    assert(close(exact_fd) == 0);
    assert(infiltratr_read_text_file_ex(exact_name, read_buffer,
                                        sizeof(read_buffer), &length) ==
           INFILTRATR_IO_OK);
    assert(strcmp(read_buffer, exact_text) == 0);
    assert(length == sizeof(exact_text) - 1U);
    assert(infiltratr_read_text_file(exact_name, read_buffer,
                                     sizeof(read_buffer)));
    assert(strcmp(read_buffer, exact_text) == 0);
    assert(unlink(exact_name) == 0);

    char truncated_name[] = "infiltratr-contract-trunc-XXXXXX";
    const int truncated_fd = mkstemp(truncated_name);
    assert(truncated_fd >= 0);
    static const char truncated_text[] = "12345678";
    write_all(truncated_fd, truncated_text, sizeof(truncated_text) - 1U);
    assert(close(truncated_fd) == 0);
    assert(infiltratr_read_text_file_ex(truncated_name, read_buffer,
                                        sizeof(read_buffer), &length) ==
           INFILTRATR_IO_TRUNCATED);
    assert(strcmp(read_buffer, "1234567") == 0);
    assert(length == 7U);
    strcpy(read_buffer, "old");
    assert(!infiltratr_read_text_file(truncated_name, read_buffer,
                                      sizeof(read_buffer)));
    assert(strcmp(read_buffer, "") == 0);
    assert(unlink(truncated_name) == 0);

    char allocated_name[] = "infiltratr-contract-allocated-XXXXXX";
    const int allocated_fd = mkstemp(allocated_name);
    assert(allocated_fd >= 0);
    char large_text[12000];
    memset(large_text, 'q', sizeof(large_text));
    large_text[sizeof(large_text) - 1U] = '\n';
    write_all(allocated_fd, large_text, sizeof(large_text));
    assert(close(allocated_fd) == 0);
    char *allocated = (char *)0x1;
    length = 99U;
    assert(infiltratr_read_text_file_alloc(allocated_name, &allocated, &length) ==
           INFILTRATR_IO_OK);
    assert(allocated != NULL);
    assert(length == sizeof(large_text));
    assert(memcmp(allocated, large_text, sizeof(large_text)) == 0);
    assert(allocated[length] == '\0');
    free(allocated);
    assert(unlink(allocated_name) == 0);

    allocated = (char *)0x1;
    length = 99U;
    assert(infiltratr_read_text_file_alloc("does-not-exist-infiltratr",
                                           &allocated, &length) ==
           INFILTRATR_IO_NOT_FOUND);
    assert(allocated == NULL);
    assert(length == 0U);

    length = 99U;
    assert(infiltratr_read_text_file_ex("does-not-exist-infiltratr", read_buffer,
                                        sizeof(read_buffer), &length) ==
           INFILTRATR_IO_NOT_FOUND);
    assert(strcmp(read_buffer, "") == 0);
    assert(length == 0U);

    char invalid_buffer = 'Q';
    length = 99U;
    assert(infiltratr_read_text_file_ex("x", &invalid_buffer, 1U, &length) ==
           INFILTRATR_IO_INVALID_ARGUMENT);
    assert(invalid_buffer == 'Q');
    assert(length == 0U);

    char nul_text_name[] = "infiltratr-contract-text-nul-XXXXXX";
    const int nul_text_fd = mkstemp(nul_text_name);
    assert(nul_text_fd >= 0);
    static const char bounded_nul_text[] = {'a', 'b', '\0', 'c', '\n'};
    write_all(nul_text_fd, bounded_nul_text, sizeof(bounded_nul_text));
    assert(close(nul_text_fd) == 0);
    strcpy(read_buffer, "old");
    length = 99U;
    assert(infiltratr_read_text_file_ex(
               nul_text_name, read_buffer, sizeof(read_buffer), &length) ==
           INFILTRATR_IO_INVALID_VALUE);
    assert(strcmp(read_buffer, "") == 0);
    assert(length == 0U);
    assert(unlink(nul_text_name) == 0);
}

static void test_typed_io(void)
{
    char number_name[] = "infiltratr-contract-number-XXXXXX";
    int descriptor = mkstemp(number_name);
    assert(descriptor >= 0);

    char long_number[700];
    memset(long_number, ' ', 300U);
    memcpy(long_number + 300U, "18446744073709551615", 20U);
    memset(long_number + 320U, ' ', 300U);
    long_number[620U] = '\n';
    write_all(descriptor, long_number, 621U);
    assert(close(descriptor) == 0);

    uint64_t unsigned_value = 7U;
    assert(infiltratr_read_u64_file_ex(number_name, &unsigned_value) ==
           INFILTRATR_IO_OK);
    assert(unsigned_value == UINT64_MAX);
    unsigned_value = 7U;
    assert(infiltratr_read_u64_file(number_name, &unsigned_value));
    assert(unsigned_value == UINT64_MAX);
    assert(unlink(number_name) == 0);

    char double_name[] = "infiltratr-contract-double-XXXXXX";
    descriptor = mkstemp(double_name);
    assert(descriptor >= 0);
    char long_double[700];
    memset(long_double, ' ', 300U);
    memcpy(long_double + 300U, "1.25", 4U);
    memset(long_double + 304U, ' ', 300U);
    long_double[604U] = '\n';
    write_all(descriptor, long_double, 605U);
    assert(close(descriptor) == 0);
    double double_value = 0.0;
    assert(infiltratr_read_double_file_ex(double_name, &double_value) ==
           INFILTRATR_IO_OK);
    assert(double_value == 1.25);
    assert(unlink(double_name) == 0);

    char invalid_name[] = "infiltratr-contract-invalid-XXXXXX";
    descriptor = mkstemp(invalid_name);
    assert(descriptor >= 0);
    static const char invalid_text[] = "12x\n";
    write_all(descriptor, invalid_text, sizeof(invalid_text) - 1U);
    assert(close(descriptor) == 0);

    unsigned_value = 99U;
    assert(infiltratr_read_u64_file_ex(invalid_name, &unsigned_value) ==
           INFILTRATR_IO_INVALID_VALUE);
    assert(unsigned_value == 99U);

    double_value = 9.0;
    assert(infiltratr_read_double_file_ex(invalid_name, &double_value) ==
           INFILTRATR_IO_INVALID_VALUE);
    assert(double_value == 9.0);
    assert(unlink(invalid_name) == 0);

    char nul_name[] = "infiltratr-contract-nul-XXXXXX";
    descriptor = mkstemp(nul_name);
    assert(descriptor >= 0);
    static const char nul_text[] = {'1', '\0', '2'};
    write_all(descriptor, nul_text, sizeof(nul_text));
    assert(close(descriptor) == 0);
    unsigned_value = 99U;
    assert(infiltratr_read_u64_file_ex(nul_name, &unsigned_value) ==
           INFILTRATR_IO_INVALID_VALUE);
    assert(unsigned_value == 99U);
    assert(unlink(nul_name) == 0);
}

static bool fail_after_partial_write(FILE *stream, const void *user_data)
{
    (void)user_data;
    assert(fputs("incomplete", stream) >= 0);
    errno = ENOSPC;
    return false;
}

static void expect_contents(const char *path, const char *expected)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    char contents[128];
    const size_t length = fread(contents, 1U, sizeof(contents) - 1U, file);
    assert(fclose(file) == 0);
    contents[length] = '\0';
    assert(strcmp(contents, expected) == 0);
}

static void expect_no_atomic_temporaries(const char *directory)
{
    DIR *stream = opendir(directory);
    assert(stream);
    struct dirent *entry;
    while ((entry = readdir(stream)))
        assert(strncmp(entry->d_name, ".infiltratr-write-", 18U) != 0);
    assert(closedir(stream) == 0);
}

static void test_atomic_replacement(void)
{
    char directory[] = "infiltratr-atomic-file-XXXXXX";
    assert(mkdtemp(directory));
    char path[256];
    assert(snprintf(path, sizeof(path), "%s/state.conf", directory) > 0);

    static const char first[] = "first complete value\n";
    assert(infiltratr_atomic_file_write_bytes(
               path, INFILTRATR_ATOMIC_FILE_PRIVATE,
               first, sizeof(first) - 1U) == 0);
    expect_contents(path, first);
    struct stat status;
    assert(stat(path, &status) == 0);
    assert((status.st_mode & 0777) == 0600);

    assert(chmod(path, 0640) == 0);
    static const char second[] = "replacement\n";
    assert(infiltratr_atomic_file_write_bytes(
               path, INFILTRATR_ATOMIC_FILE_PRESERVE_PERMISSIONS,
               second, sizeof(second) - 1U) == 0);
    expect_contents(path, second);
    assert(stat(path, &status) == 0);
    assert((status.st_mode & 0777) == 0640);

    assert(infiltratr_atomic_file_write(
               path, INFILTRATR_ATOMIC_FILE_PRIVATE,
               fail_after_partial_write, NULL) == ENOSPC);
    expect_contents(path, second);
    expect_no_atomic_temporaries(directory);

    char victim[256];
    char link_path[256];
    assert(snprintf(victim, sizeof(victim), "%s/victim", directory) > 0);
    assert(snprintf(link_path, sizeof(link_path), "%s/link", directory) > 0);
    FILE *victim_stream = fopen(victim, "wb");
    assert(victim_stream);
    assert(fputs("must-not-change", victim_stream) >= 0);
    assert(fclose(victim_stream) == 0);
    assert(symlink("victim", link_path) == 0);

    static const char replacement[] = "replacement-of-link\n";
    assert(infiltratr_atomic_file_write_bytes(
               link_path, INFILTRATR_ATOMIC_FILE_PRIVATE,
               replacement, sizeof(replacement) - 1U) == 0);
    expect_contents(victim, "must-not-change");
    struct stat link_status;
    assert(lstat(link_path, &link_status) == 0);
    assert(S_ISREG(link_status.st_mode));
    expect_contents(link_path, replacement);
    assert(unlink(link_path) == 0);
    assert(unlink(victim) == 0);

    assert(infiltratr_atomic_file_write_bytes(
               NULL, INFILTRATR_ATOMIC_FILE_PRIVATE,
               first, sizeof(first) - 1U) == EINVAL);
    assert(infiltratr_atomic_file_write_bytes(
               path, INFILTRATR_ATOMIC_FILE_PRIVATE, NULL, 1U) == EINVAL);
    assert(unlink(path) == 0);
    assert(rmdir(directory) == 0);
}

static void test_durable_unlink(void)
{
    assert(infiltratr_unlink_durable(NULL, false) == EINVAL);
    assert(infiltratr_unlink_durable("", false) == EINVAL);

    char directory[] = "infiltratr-durable-unlink-XXXXXX";
    assert(mkdtemp(directory));
    char path[256];
    assert(snprintf(path, sizeof(path), "%s/recovery.state", directory) > 0);

    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fputs("pending\n", file) >= 0);
    assert(fclose(file) == 0);

    assert(infiltratr_unlink_durable(path, false) == 0);
    errno = 0;
    assert(access(path, F_OK) != 0);
    assert(errno == ENOENT);

    assert(infiltratr_unlink_durable(path, false) == ENOENT);
    assert(infiltratr_unlink_durable(path, true) == 0);
    assert(rmdir(directory) == 0);
}

static void test_results_and_clock(void)
{
    assert(strcmp(infiltratr_io_result_name(INFILTRATR_IO_OK), "ok") == 0);
    assert(strcmp(infiltratr_io_result_name(INFILTRATR_IO_TRUNCATED),
                  "truncated") == 0);
    assert(strcmp(infiltratr_io_result_name((InfiltratrIoResult)999),
                  "unknown") == 0);

    assert(!infiltratr_monotonic_nanoseconds(NULL));
    uint64_t nanoseconds = 0U;
    assert(infiltratr_monotonic_nanoseconds(&nanoseconds));
    assert(nanoseconds > 0U);
    assert(infiltratr_monotonic_seconds() > 0.0);

    assert(infiltratr_posix_deadline_after_milliseconds(
               CLOCK_MONOTONIC, 1U, NULL) == EINVAL);

    struct timespec deadline = {0};
    assert(infiltratr_posix_deadline_after_milliseconds(
               CLOCK_MONOTONIC, 1000U, &deadline) == 0);
    uint64_t remaining = UINT64_MAX;
    assert(infiltratr_posix_deadline_remaining_milliseconds(
               CLOCK_MONOTONIC, &deadline, &remaining) == 0);
    assert(remaining > 0U && remaining <= 1000U);

    struct timespec immediate = {0};
    assert(infiltratr_posix_deadline_after_milliseconds(
               CLOCK_MONOTONIC, 0U, &immediate) == 0);
    remaining = UINT64_MAX;
    assert(infiltratr_posix_deadline_remaining_milliseconds(
               CLOCK_MONOTONIC, &immediate, &remaining) == 0);
    assert(remaining == 0U);

    const struct timespec malformed = {
        .tv_sec = 0,
        .tv_nsec = 1000000000L
    };
    remaining = 77U;
    assert(infiltratr_posix_deadline_remaining_milliseconds(
               CLOCK_MONOTONIC, &malformed, &remaining) == EINVAL);
    assert(remaining == 77U);
}

int main(void)
{
    test_paths();
    test_user_paths_and_directories();
    test_first_readable_path();
    test_long_first_u64_path();
    test_text_io();
    test_typed_io();
    test_atomic_replacement();
    test_durable_unlink();
    test_results_and_clock();

    puts("Libraries POSIX API contract tests passed.");
    return 0;
}
