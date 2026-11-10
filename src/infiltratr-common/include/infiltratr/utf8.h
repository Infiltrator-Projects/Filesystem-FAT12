// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file utf8.h
 * @brief Allocation-free strict UTF-8 validation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATR_COMMON_UTF8_H
#define INFILTRATR_COMMON_UTF8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Validate one bounded UTF-8 byte sequence.
 *
 * The validator rejects truncated sequences, stray continuation bytes,
 * overlong encodings, UTF-16 surrogate code points and values above U+10FFFF.
 * Embedded NUL bytes are ordinary U+0000 code points; callers that forbid NUL
 * in names or text fields apply that policy separately.
 */
static inline bool infiltratr_utf8_validate(const void *bytes, size_t length)
{
    const uint8_t *p = (const uint8_t *)bytes;
    size_t i = 0U;

    if (!p && length != 0U)
        return false;

    while (i < length) {
        const uint8_t first = p[i++];
        if (first <= 0x7fU)
            continue;

        uint32_t value;
        unsigned int continuation;
        uint32_t minimum;
        if (first >= 0xc2U && first <= 0xdfU) {
            value = first & 0x1fU;
            continuation = 1U;
            minimum = 0x80U;
        } else if (first >= 0xe0U && first <= 0xefU) {
            value = first & 0x0fU;
            continuation = 2U;
            minimum = 0x800U;
        } else if (first >= 0xf0U && first <= 0xf4U) {
            value = first & 0x07U;
            continuation = 3U;
            minimum = 0x10000U;
        } else {
            return false;
        }

        if ((size_t)continuation > length - i)
            return false;
        for (unsigned int n = 0U; n < continuation; ++n) {
            const uint8_t next = p[i++];
            if ((next & 0xc0U) != 0x80U)
                return false;
            value = (value << 6) | (next & 0x3fU);
        }

        if (value < minimum || value > 0x10ffffU ||
            (value >= 0xd800U && value <= 0xdfffU))
            return false;
    }

    return true;
}

/**
 * Encode one Unicode scalar value as its canonical UTF-8 byte sequence.
 *
 * Valid input is U+0000 through U+10FFFF excluding the UTF-16 surrogate
 * range U+D800 through U+DFFF. The encoded sequence is one through four bytes
 * and is not NUL-terminated; U+0000 therefore legitimately emits one zero
 * byte. Output storage and @length remain unchanged when the scalar is invalid
 * or @capacity is insufficient. @length may be NULL.
 */
static inline bool infiltratr_utf8_encode_codepoint(uint32_t codepoint,
                                                     char *output,
                                                     size_t capacity,
                                                     size_t *length)
{
    uint8_t encoded[4];
    size_t used = 0U;

    if (codepoint <= 0x7fU) {
        encoded[0] = (uint8_t)codepoint;
        used = 1U;
    } else if (codepoint <= 0x7ffU) {
        encoded[0] = (uint8_t)(0xc0U | (codepoint >> 6U));
        encoded[1] = (uint8_t)(0x80U | (codepoint & 0x3fU));
        used = 2U;
    } else if (codepoint >= 0xd800U && codepoint <= 0xdfffU) {
        return false;
    } else if (codepoint <= 0xffffU) {
        encoded[0] = (uint8_t)(0xe0U | (codepoint >> 12U));
        encoded[1] = (uint8_t)(0x80U | ((codepoint >> 6U) & 0x3fU));
        encoded[2] = (uint8_t)(0x80U | (codepoint & 0x3fU));
        used = 3U;
    } else if (codepoint <= 0x10ffffU) {
        encoded[0] = (uint8_t)(0xf0U | (codepoint >> 18U));
        encoded[1] = (uint8_t)(0x80U | ((codepoint >> 12U) & 0x3fU));
        encoded[2] = (uint8_t)(0x80U | ((codepoint >> 6U) & 0x3fU));
        encoded[3] = (uint8_t)(0x80U | (codepoint & 0x3fU));
        used = 4U;
    } else {
        return false;
    }

    if (!output || capacity < used) return false;
    for (size_t index = 0U; index < used; ++index)
        output[index] = (char)encoded[index];
    if (length) *length = used;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif
