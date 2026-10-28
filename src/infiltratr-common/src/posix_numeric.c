// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "infiltratr/posix_numeric.h"
#include "posix_read_internal.h"

InfiltratrIoResult infiltratr_read_i64_file_ex(const char *path, int64_t *value)
{
    return infiltratr_posix_read_numeric_file(
        path, INFILTRATR_POSIX_NUMERIC_I64, value);
}

bool infiltratr_read_i64_file(const char *path, int64_t *value)
{
    return infiltratr_read_i64_file_ex(path, value) == INFILTRATR_IO_OK;
}
