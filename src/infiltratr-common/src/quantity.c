// SPDX-License-Identifier: GPL-3.0-or-later
#include "infiltratr/quantity.h"
#include "infiltratr/arithmetic.h"
#include "ascii_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *digits; /* little-endian decimal digits */
    size_t length;
    size_t capacity;
} DecimalInteger;

static bool suffix_power(const char *suffix, size_t length,
                         unsigned int *power)
{
    static const char *const short_names[] = {"", "k", "m", "g", "t", "p", "e"};
    static const char *const byte_names[] = {"b", "kb", "mb", "gb", "tb", "pb", "eb"};
    static const char *const iec_names[] = {"b", "kib", "mib", "gib", "tib", "pib", "eib"};

    for (unsigned int i = 0U; i < 7U; ++i) {
        if (infiltratr_ascii_equal_ci_span(suffix, length, short_names[i]) ||
            infiltratr_ascii_equal_ci_span(suffix, length, byte_names[i]) ||
            infiltratr_ascii_equal_ci_span(suffix, length, iec_names[i])) {
            *power = i;
            return true;
        }
    }
    return false;
}

static void decimal_destroy(DecimalInteger *integer)
{
    if (!integer) return;
    free(integer->digits);
    integer->digits = NULL;
    integer->length = 0U;
    integer->capacity = 0U;
}

static bool decimal_reserve(DecimalInteger *integer, size_t required)
{
    return infiltratr_array_reserve((void **)&integer->digits,
                                    &integer->capacity,
                                    sizeof(integer->digits[0]),
                                    required, 32U);
}

static bool decimal_append_forward(DecimalInteger *integer, unsigned int digit)
{
    if (integer->length == SIZE_MAX ||
        !decimal_reserve(integer, integer->length + 1U))
        return false;
    integer->digits[integer->length++] = (uint8_t)digit;
    return true;
}

static void decimal_normalise_to_little_endian(DecimalInteger *integer)
{
    size_t first = 0U;
    while (first < integer->length && integer->digits[first] == 0U)
        first++;

    if (first != 0U) {
        integer->length -= first;
        if (integer->length != 0U)
            memmove(integer->digits, integer->digits + first, integer->length);
    }

    for (size_t left = 0U, right = integer->length;
         left < right && left < --right; ++left) {
        const uint8_t temporary = integer->digits[left];
        integer->digits[left] = integer->digits[right];
        integer->digits[right] = temporary;
    }
}

static bool decimal_multiply_small(DecimalInteger *integer,
                                   unsigned int factor)
{
    unsigned int carry = 0U;
    for (size_t i = 0U; i < integer->length; ++i) {
        const unsigned int product =
            (unsigned int)integer->digits[i] * factor + carry;
        integer->digits[i] = (uint8_t)(product % 10U);
        carry = product / 10U;
    }

    while (carry != 0U) {
        if (!decimal_append_forward(integer, carry % 10U))
            return false;
        carry /= 10U;
    }
    return true;
}

static bool decimal_to_u64(const DecimalInteger *integer,
                           size_t low_digits_to_remove,
                           uint64_t *value)
{
    if (low_digits_to_remove >= integer->length)
        return false;

    for (size_t i = 0U; i < low_digits_to_remove; ++i)
        if (integer->digits[i] != 0U)
            return false;

    uint64_t result = 0U;
    for (size_t i = integer->length; i-- > low_digits_to_remove;) {
        const uint64_t digit = (uint64_t)integer->digits[i];
        if (result > (UINT64_MAX - digit) / 10U)
            return false;
        result = result * 10U + digit;
    }
    *value = result;
    return true;
}

static bool parse_exponent(const unsigned char **cursor,
                           const unsigned char *end,
                           bool *negative, size_t *magnitude,
                           bool *overflowed)
{
    *negative = false;
    *magnitude = 0U;
    *overflowed = false;

    if (*cursor >= end || (**cursor != 'e' && **cursor != 'E'))
        return true;

    (*cursor)++;
    if (*cursor < end && (**cursor == '+' || **cursor == '-')) {
        *negative = **cursor == '-';
        (*cursor)++;
    }
    if (*cursor >= end || !infiltratr_ascii_digit(**cursor))
        return false;

    while (*cursor < end && infiltratr_ascii_digit(**cursor)) {
        const size_t digit = (size_t)(**cursor - '0');
        if (!*overflowed) {
            if (*magnitude > (SIZE_MAX - digit) / 10U)
                *overflowed = true;
            else
                *magnitude = *magnitude * 10U + digit;
        }
        (*cursor)++;
    }
    return true;
}

static bool parse_decimal_integer(const char *begin, const char *end,
                                  DecimalInteger *integer,
                                  size_t *fractional_digits,
                                  bool *exponent_negative,
                                  size_t *exponent_magnitude,
                                  bool *exponent_overflowed)
{
    const unsigned char *cursor = (const unsigned char *)begin;
    const unsigned char *limit = (const unsigned char *)end;

    if (cursor < limit && *cursor == '+')
        cursor++;
    else if (cursor < limit && *cursor == '-')
        return false;

    bool saw_digit = false;
    *fractional_digits = 0U;

    while (cursor < limit && infiltratr_ascii_digit(*cursor)) {
        if (!decimal_append_forward(integer, (unsigned int)(*cursor - '0')))
            return false;
        cursor++;
        saw_digit = true;
    }

    if (cursor < limit && *cursor == '.') {
        cursor++;
        while (cursor < limit && infiltratr_ascii_digit(*cursor)) {
            if (!decimal_append_forward(integer, (unsigned int)(*cursor - '0')))
                return false;
            if (*fractional_digits == SIZE_MAX)
                return false;
            (*fractional_digits)++;
            cursor++;
            saw_digit = true;
        }
    }

    if (!saw_digit)
        return false;

    if (!parse_exponent(&cursor, limit, exponent_negative,
                        exponent_magnitude, exponent_overflowed))
        return false;
    if (cursor != limit)
        return false;

    decimal_normalise_to_little_endian(integer);
    return true;
}

static bool decimal_shift(bool exponent_negative, size_t exponent_magnitude,
                          size_t fractional_digits,
                          bool *negative, size_t *magnitude)
{
    if (exponent_negative) {
        if (exponent_magnitude > SIZE_MAX - fractional_digits)
            return false;
        *negative = true;
        *magnitude = fractional_digits + exponent_magnitude;
        return true;
    }

    if (exponent_magnitude >= fractional_digits) {
        *negative = false;
        *magnitude = exponent_magnitude - fractional_digits;
    } else {
        *negative = true;
        *magnitude = fractional_digits - exponent_magnitude;
    }
    return true;
}

bool infiltratr_parse_binary_quantity_u64(const char *text, uint64_t *bytes)
{
    if (!text || !bytes) return false;

    while (*text && infiltratr_ascii_space((unsigned char)*text))
        text++;

    const char *end = text + strlen(text);
    while (end > text && infiltratr_ascii_space((unsigned char)end[-1]))
        end--;
    if (end == text)
        return false;

    const char *suffix_start = end;
    while (suffix_start > text &&
           infiltratr_ascii_alpha((unsigned char)suffix_start[-1]))
        suffix_start--;

    unsigned int power = 0U;
    if (!suffix_power(suffix_start, (size_t)(end - suffix_start), &power))
        return false;

    const char *number_end = suffix_start;
    while (number_end > text && infiltratr_ascii_space((unsigned char)number_end[-1]))
        number_end--;
    if (number_end == text)
        return false;

    DecimalInteger integer = {0};
    size_t fractional_digits = 0U;
    size_t exponent_magnitude = 0U;
    bool exponent_negative = false;
    bool exponent_overflowed = false;

    const bool parsed = parse_decimal_integer(text, number_end, &integer,
                                              &fractional_digits,
                                              &exponent_negative,
                                              &exponent_magnitude,
                                              &exponent_overflowed);
    if (!parsed) {
        decimal_destroy(&integer);
        return false;
    }

    if (integer.length == 0U) {
        decimal_destroy(&integer);
        *bytes = 0U;
        return true;
    }

    /*
     * Any exponent magnitude larger than size_t cannot be cancelled by the
     * finite input's fractional digit count. For a non-zero value a positive
     * exponent therefore exceeds uint64_t, while a negative exponent remains
     * below one even after the largest supported binary suffix (2^60).
     */
    if (exponent_overflowed) {
        decimal_destroy(&integer);
        return false;
    }

    for (unsigned int i = 0U; i < power; ++i) {
        if (!decimal_multiply_small(&integer, 1024U)) {
            decimal_destroy(&integer);
            return false;
        }
    }

    bool shift_negative = false;
    size_t shift_magnitude = 0U;
    if (!decimal_shift(exponent_negative, exponent_magnitude,
                       fractional_digits, &shift_negative,
                       &shift_magnitude)) {
        decimal_destroy(&integer);
        return false;
    }

    uint64_t result = 0U;
    if (shift_negative) {
        if (!decimal_to_u64(&integer, shift_magnitude, &result)) {
            decimal_destroy(&integer);
            return false;
        }
    } else {
        if (!decimal_to_u64(&integer, 0U, &result)) {
            decimal_destroy(&integer);
            return false;
        }

        /*
         * A non-zero uint64_t cannot survive multiplication by 10^20.
         * This is a result-range consequence, not an input-length limit.
         */
        if (shift_magnitude > 19U) {
            decimal_destroy(&integer);
            return false;
        }
        for (size_t i = 0U; i < shift_magnitude; ++i) {
            if (!infiltratr_u64_multiply_checked(result, 10U, &result)) {
                decimal_destroy(&integer);
                return false;
            }
        }
    }

    decimal_destroy(&integer);
    *bytes = result;
    return true;
}
