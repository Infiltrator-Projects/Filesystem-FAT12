// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "infiltratr/posix_io.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

_Static_assert((off_t)-1 < (off_t)0, "POSIX off_t must be signed");

static int valid_descriptor_buffer(int descriptor, const void *buffer,
                                   size_t count)
{
    if (descriptor < 0 || (!buffer && count != 0U)) {
        errno = EINVAL;
        return 0;
    }
    return 1;
}

static size_t transfer_chunk(size_t count)
{
    return count > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : count;
}

static uint64_t offset_maximum(void)
{
    const size_t bits = sizeof(off_t) * CHAR_BIT;
    if (bits > 64U) return UINT64_MAX;
    if (bits == 64U) return (uint64_t)INT64_MAX;
    return (UINT64_C(1) << (bits - 1U)) - 1U;
}

static int valid_positioned_range(size_t count, uint64_t offset)
{
    if (count == 0U) return 1;

    const uint64_t maximum = offset_maximum();
    if (offset > maximum ||
        (uintmax_t)(count - 1U) > (uintmax_t)(maximum - offset)) {
        errno = EOVERFLOW;
        return 0;
    }
    return 1;
}

int infiltratr_read_full(int descriptor, void *buffer, size_t count)
{
    if (!valid_descriptor_buffer(descriptor, buffer, count)) return -1;
    uint8_t *cursor = (uint8_t *)buffer;
    while (count != 0U) {
        const size_t request = transfer_chunk(count);
        ssize_t amount;
        do {
            amount = read(descriptor, cursor, request);
        } while (amount < 0 && errno == EINTR);
        if (amount < 0) return -1;
        if (amount == 0) {
            errno = EIO;
            return -1;
        }
        cursor += (size_t)amount;
        count -= (size_t)amount;
    }
    return 0;
}

int infiltratr_write_full(int descriptor, const void *buffer, size_t count)
{
    if (!valid_descriptor_buffer(descriptor, buffer, count)) return -1;
    const uint8_t *cursor = (const uint8_t *)buffer;
    while (count != 0U) {
        const size_t request = transfer_chunk(count);
        ssize_t amount;
        do {
            amount = write(descriptor, cursor, request);
        } while (amount < 0 && errno == EINTR);
        if (amount < 0) return -1;
        if (amount == 0) {
            errno = EIO;
            return -1;
        }
        cursor += (size_t)amount;
        count -= (size_t)amount;
    }
    return 0;
}

int infiltratr_pread_full(int descriptor, void *buffer, size_t count,
                          uint64_t offset)
{
    if (!valid_descriptor_buffer(descriptor, buffer, count) ||
        !valid_positioned_range(count, offset)) return -1;
    uint8_t *cursor = (uint8_t *)buffer;
    while (count != 0U) {
        const size_t request = transfer_chunk(count);

        ssize_t amount;
        do {
            amount = pread(descriptor, cursor, request, (off_t)offset);
        } while (amount < 0 && errno == EINTR);
        if (amount < 0) return -1;
        if (amount == 0) {
            errno = EIO;
            return -1;
        }

        const size_t completed = (size_t)amount;
        cursor += completed;
        count -= completed;
        if (count != 0U) {
            if ((uint64_t)completed > UINT64_MAX - offset) {
                errno = EOVERFLOW;
                return -1;
            }
            offset += (uint64_t)completed;
        }
    }
    return 0;
}

int infiltratr_pwrite_full(int descriptor, const void *buffer, size_t count,
                           uint64_t offset)
{
    if (!valid_descriptor_buffer(descriptor, buffer, count) ||
        !valid_positioned_range(count, offset)) return -1;
    const uint8_t *cursor = (const uint8_t *)buffer;
    while (count != 0U) {
        const size_t request = transfer_chunk(count);

        ssize_t amount;
        do {
            amount = pwrite(descriptor, cursor, request, (off_t)offset);
        } while (amount < 0 && errno == EINTR);
        if (amount < 0) return -1;
        if (amount == 0) {
            errno = EIO;
            return -1;
        }

        const size_t completed = (size_t)amount;
        cursor += completed;
        count -= completed;
        if (count != 0U) {
            if ((uint64_t)completed > UINT64_MAX - offset) {
                errno = EOVERFLOW;
                return -1;
            }
            offset += (uint64_t)completed;
        }
    }
    return 0;
}
