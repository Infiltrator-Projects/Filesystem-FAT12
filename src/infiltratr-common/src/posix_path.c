// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file posix_path.c
 * @brief POSIX lexical-path primitive implementation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/posix_path.h"

#include <string.h>

const char *infiltratr_path_basename(const char *path)
{
    if (!path) return "";
    const char *separator = strrchr(path, '/');
    return separator ? separator + 1 : path;
}

bool infiltratr_path_dirname(const char *path, char *buffer, size_t size)
{
    if (buffer && size > 0U) buffer[0] = '\0';
    if (!path || !buffer || size == 0U) return false;

    size_t end = strlen(path);
    if (end == 0U) {
        if (size < 2U) return false;
        memcpy(buffer, ".", 2U);
        return true;
    }

    while (end > 1U && path[end - 1U] == '/') end--;

    size_t separator = end;
    while (separator > 0U && path[separator - 1U] != '/') separator--;
    if (separator == 0U) {
        if (size < 2U) return false;
        memcpy(buffer, ".", 2U);
        return true;
    }

    size_t parent_length = separator - 1U;
    while (parent_length > 0U && path[parent_length - 1U] == '/')
        parent_length--;

    if (parent_length == 0U) {
        if (size < 2U) return false;
        memcpy(buffer, "/", 2U);
        return true;
    }
    if (parent_length + 1U > size) return false;
    memcpy(buffer, path, parent_length);
    buffer[parent_length] = '\0';
    return true;
}
