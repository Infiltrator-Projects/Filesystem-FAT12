// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "infiltratr/posix_numeric.h"
#include "infiltratr/posix_io.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char path[] = "infiltratr-posix-numeric-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);

    char text[700];
    memset(text, ' ', 300U);
    memcpy(text + 300U, "-9223372036854775808", 20U);
    memset(text + 320U, ' ', 300U);
    text[620U] = '\n';
    assert(infiltratr_write_full(descriptor, text, 621U) == 0);
    assert(close(descriptor) == 0);

    int64_t value = 77;
    assert(infiltratr_read_i64_file_ex(path, &value) == INFILTRATR_IO_OK);
    assert(value == INT64_MIN);
    value = 77;
    assert(infiltratr_read_i64_file(path, &value));
    assert(value == INT64_MIN);
    assert(unlink(path) == 0);

    value = 77;
    assert(infiltratr_read_i64_file_ex(path, &value) == INFILTRATR_IO_NOT_FOUND);
    assert(value == 77);
    assert(infiltratr_read_i64_file_ex(NULL, &value) == INFILTRATR_IO_INVALID_ARGUMENT);
    puts("Libraries signed POSIX numeric reader tests passed.");
    return 0;
}
