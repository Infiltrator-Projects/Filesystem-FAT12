// SPDX-License-Identifier: GPL-3.0-or-later
/** @file posix_io_contract.c @brief Exact POSIX descriptor I/O regression coverage. */
#define _POSIX_C_SOURCE 200809L
#include "infiltratr/posix_io.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static uint64_t native_offset_maximum(void)
{
    const size_t bits = sizeof(off_t) * CHAR_BIT;
    if (bits > 64U) return UINT64_MAX;
    if (bits == 64U) return (uint64_t)INT64_MAX;
    return (UINT64_C(1) << (bits - 1U)) - 1U;
}

int main(void)
{
    char path[] = "infiltratr-posix-io-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    assert(unlink(path) == 0);
    assert(ftruncate(descriptor, 16) == 0);

    static const char payload[] = "hello";
    assert(infiltratr_pwrite_full(descriptor, payload, sizeof(payload) - 1U, 4U) == 0);
    char readback[sizeof(payload)] = {0};
    assert(infiltratr_pread_full(descriptor, readback, sizeof(payload) - 1U, 4U) == 0);
    assert(memcmp(readback, payload, sizeof(payload) - 1U) == 0);

    int pipefd[2];
    assert(pipe(pipefd) == 0);
    assert(infiltratr_write_full(pipefd[1], payload, sizeof(payload) - 1U) == 0);
    memset(readback, 0, sizeof(readback));
    assert(infiltratr_read_full(pipefd[0], readback, sizeof(payload) - 1U) == 0);
    assert(memcmp(readback, payload, sizeof(payload) - 1U) == 0);
    assert(close(pipefd[0]) == 0);
    assert(close(pipefd[1]) == 0);

    assert(infiltratr_read_full(descriptor, NULL, 0U) == 0);
    assert(infiltratr_write_full(descriptor, NULL, 0U) == 0);
    assert(infiltratr_pread_full(descriptor, NULL, 0U, 0U) == 0);
    assert(infiltratr_pwrite_full(descriptor, NULL, 0U, 0U) == 0);

    errno = 0;
    char short_read[4] = {0};
    assert(infiltratr_pread_full(descriptor, short_read, sizeof(short_read), 15U) == -1);
    assert(errno == EIO);
    const uint64_t maximum_offset = native_offset_maximum();
    if (maximum_offset < UINT64_MAX) {
        const uint64_t invalid_offset = maximum_offset + 1U;
        errno = 0;
        assert(infiltratr_pread_full(descriptor, readback, 1U,
                                     invalid_offset) == -1);
        assert(errno == EOVERFLOW);
        errno = 0;
        assert(infiltratr_pwrite_full(descriptor, payload, 1U,
                                      invalid_offset) == -1);
        assert(errno == EOVERFLOW);

        struct stat before;
        struct stat after;
        assert(fstat(descriptor, &before) == 0);
        errno = 0;
        assert(infiltratr_pwrite_full(descriptor, payload, 2U,
                                      maximum_offset) == -1);
        assert(errno == EOVERFLOW);
        assert(fstat(descriptor, &after) == 0);
        assert(after.st_size == before.st_size);
        errno = 0;
        assert(infiltratr_pread_full(descriptor, readback, 2U,
                                     maximum_offset) == -1);
        assert(errno == EOVERFLOW);
    }
    errno = 0;
    assert(infiltratr_pread_full(descriptor, NULL, 1U, 0U) == -1);
    assert(errno == EINVAL);

    assert(close(descriptor) == 0);
    puts("Libraries exact descriptor I/O contract tests passed.");
    return 0;
}
