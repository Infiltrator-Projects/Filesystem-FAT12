// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal_posix.c
 * @brief POSIX temporal policy persistence and provider capability.
 */
#define _POSIX_C_SOURCE 200809L

#include "infiltratr/temporal_posix.h"

#include "infiltratr/arithmetic.h"
#include "infiltratr/config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define TEMPORAL_DOCUMENT_CAPACITY 1024U

static char *path_join_alloc(const char *left, const char *right)
{
    size_t left_length;
    size_t right_length;
    size_t length = 0U;
    size_t size = 0U;
    bool needs_separator;
    const char *right_start;
    char *joined;

    if (left == NULL || right == NULL) {
        errno = EINVAL;
        return NULL;
    }

    left_length = strlen(left);
    right_start = right;
    while (*right_start == '/' && left_length > 0U) {
        right_start++;
    }
    right_length = strlen(right_start);
    needs_separator =
        left_length > 0U && left[left_length - 1U] != '/';

    if (!infiltratr_size_add_checked(left_length, right_length, &length) ||
        (needs_separator &&
         !infiltratr_size_add_checked(length, 1U, &length)) ||
        !infiltratr_size_add_checked(length, 1U, &size)) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    joined = malloc(size);
    if (joined == NULL) {
        return NULL;
    }
    if (!infiltratr_path_join(joined, size, left, right_start)) {
        free(joined);
        errno = ENAMETOOLONG;
        return NULL;
    }
    return joined;
}

static char *temporal_policy_directory_alloc(void)
{
    char *config_home = NULL;
    char *directory;

    if (!infiltratr_xdg_config_home_alloc(&config_home)) {
        return NULL;
    }
    directory = path_join_alloc(config_home, "infiltrator");
    free(config_home);
    return directory;
}

static char *temporal_policy_path_alloc(void)
{
    char *directory = temporal_policy_directory_alloc();
    char *path;

    if (directory == NULL) {
        return NULL;
    }
    path = path_join_alloc(directory, "presentation.conf");
    free(directory);
    return path;
}

static bool copy_path(char *destination, size_t size, const char *source)
{
    size_t length;

    if (destination == NULL || size == 0U || source == NULL) {
        return false;
    }
    destination[0] = '\0';
    length = strlen(source);
    if (length >= size) {
        return false;
    }
    memcpy(destination, source, length + 1U);
    return true;
}

bool infiltratr_temporal_posix_policy_directory(char *destination,
                                                size_t size)
{
    char *directory;
    bool okay;

    if (destination == NULL || size == 0U) {
        return false;
    }
    destination[0] = '\0';

    directory = temporal_policy_directory_alloc();
    if (directory == NULL) {
        return false;
    }
    okay = copy_path(destination, size, directory);
    free(directory);
    return okay;
}

bool infiltratr_temporal_posix_policy_path(char *destination, size_t size)
{
    char *path;
    bool okay;

    if (destination == NULL || size == 0U) {
        return false;
    }
    destination[0] = '\0';

    path = temporal_policy_path_alloc();
    if (path == NULL) {
        return false;
    }
    okay = copy_path(destination, size, path);
    free(path);
    return okay;
}

static const char *provider_marker_value(void)
{
    const char *override =
        getenv("INFILTRATR_TEMPORAL_PROVIDER_MARKER_PATH");

    if (override != NULL && override[0] == '/') {
        return override;
    }
    return INFILTRATR_TEMPORAL_PROVIDER_MARKER;
}

bool infiltratr_temporal_posix_provider_marker_path(char *destination,
                                                    size_t size)
{
    return copy_path(destination, size, provider_marker_value());
}

static bool provider_document_valid(const char *text)
{
    const char *cursor;
    bool provider_seen = false;
    bool version_seen = false;
    bool contract_seen = false;

    if (text == NULL) {
        return false;
    }

    cursor = text;
    while (*cursor != '\0') {
        const char *newline = strchr(cursor, '\n');
        const size_t line_length =
            newline != NULL ? (size_t)(newline - cursor) : strlen(cursor);
        char line[256];
        char *key = NULL;
        char *value = NULL;
        InfiltratrConfigLineStatus status;

        if (line_length >= sizeof(line)) {
            return false;
        }

        memcpy(line, cursor, line_length);
        line[line_length] = '\0';
        status = infiltratr_config_parse_line(line, &key, &value);
        if (status == INFILTRATR_CONFIG_LINE_INVALID) {
            return false;
        }

        if (status == INFILTRATR_CONFIG_LINE_ENTRY) {
            if (strcmp(key, "provider") == 0) {
                if (provider_seen ||
                    strcmp(value, "system-settings") != 0) {
                    return false;
                }
                provider_seen = true;
            } else if (strcmp(key, "policy-version") == 0) {
                if (version_seen || strcmp(value, "3") != 0) {
                    return false;
                }
                version_seen = true;
            } else if (strcmp(key, "contract") == 0) {
                if (contract_seen ||
                    strcmp(value, "infiltratr-temporal-v3") != 0) {
                    return false;
                }
                contract_seen = true;
            } else {
                return false;
            }
        }

        if (newline == NULL) {
            break;
        }
        cursor = newline + 1;
    }

    return provider_seen && version_seen && contract_seen;
}

bool infiltratr_temporal_posix_provider_available(void)
{
    char text[TEMPORAL_DOCUMENT_CAPACITY];
    const InfiltratrIoResult result =
        infiltratr_read_text_file_ex(provider_marker_value(), text,
                                     sizeof(text), NULL);
    return result == INFILTRATR_IO_OK && provider_document_valid(text);
}

InfiltratrIoResult infiltratr_temporal_posix_policy_load(
    InfiltratrTemporalPolicyV3 *policy,
    bool *found)
{
    char *path = NULL;
    char text[TEMPORAL_DOCUMENT_CAPACITY];

    if (policy == NULL || found == NULL ||
        !infiltratr_temporal_policy_v3_default(policy)) {
        return INFILTRATR_IO_INVALID_ARGUMENT;
    }

    *found = false;
    path = temporal_policy_path_alloc();
    if (path == NULL) {
        return INFILTRATR_IO_ERROR;
    }

    const InfiltratrIoResult result =
        infiltratr_read_text_file_ex(path, text, sizeof(text), NULL);
    free(path);
    if (result == INFILTRATR_IO_NOT_FOUND) {
        return INFILTRATR_IO_OK;
    }
    if (result != INFILTRATR_IO_OK) {
        return result;
    }
    if (!infiltratr_temporal_policy_v3_parse(text, policy)) {
        (void)infiltratr_temporal_policy_v3_default(policy);
        return INFILTRATR_IO_INVALID_VALUE;
    }

    *found = true;
    return INFILTRATR_IO_OK;
}

int infiltratr_temporal_posix_policy_save(
    const InfiltratrTemporalPolicyV3 *policy)
{
    char *directory = NULL;
    char *path = NULL;
    char document[TEMPORAL_DOCUMENT_CAPACITY];
    size_t length = 0U;
    int failure;

    if (policy == NULL ||
        !infiltratr_temporal_policy_v3_serialize(
            policy, document, sizeof(document), &length)) {
        return EINVAL;
    }

    directory = temporal_policy_directory_alloc();
    path = temporal_policy_path_alloc();
    if (directory == NULL || path == NULL) {
        failure = errno != 0 ? errno : ENOMEM;
        free(directory);
        free(path);
        return failure;
    }

    failure = infiltratr_mkdir_parents(directory, 0700U);
    if (failure == 0) {
        failure = infiltratr_atomic_file_write_bytes(
            path, INFILTRATR_ATOMIC_FILE_PRIVATE, document, length);
    }

    free(directory);
    free(path);
    return failure;
}
