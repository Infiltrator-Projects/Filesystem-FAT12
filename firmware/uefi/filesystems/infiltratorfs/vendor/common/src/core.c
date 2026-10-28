// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file core.c
 * @brief Dependency-free implementation of shared C project primitives.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/core.h"
#include "infiltratr/compiler.h"
#include "infiltratr/token.h"
#include "ascii_internal.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <string.h>

#define INFILTRATR_PROJECT_INFO_ABI1_SIZE \
    (offsetof(InfiltratrProjectInfo, copyright_text) + \
     sizeof(((InfiltratrProjectInfo *)0)->copyright_text))
#define INFILTRATR_SCALE_OPTIONS_ABI1_SIZE \
    (offsetof(InfiltratrScaleOptions, zero_below_minimum_unit) + \
     sizeof(((InfiltratrScaleOptions *)0)->zero_below_minimum_unit))

static bool populated(const char *text)
{
    return text && text[0] != '\0';
}

bool infiltratr_project_info_is_valid(const InfiltratrProjectInfo *info)
{
    return info && info->struct_size >= INFILTRATR_PROJECT_INFO_ABI1_SIZE &&
           info->abi_version == INFILTRATR_PROJECT_INFO_ABI &&
           populated(info->program_name) && populated(info->executable_name) &&
           populated(info->application_id) && populated(info->version) &&
           populated(info->source_id) && populated(info->build_profile) &&
           populated(info->author) && populated(info->website) &&
           populated(info->license_id) && populated(info->comments) &&
           populated(info->icon_name) && populated(info->copyright_text);
}

static int print_field(FILE *stream, const char *name, const char *value)
{
    if (fprintf(stream, "%s=", name) < 0) return -1;
    for (const unsigned char *cursor = (const unsigned char *)value;
         *cursor; cursor++) {
        const int character = *cursor < 32U || *cursor == 127U ? ' ' : *cursor;
        if (fputc(character, stream) == EOF) return -1;
    }
    return fputc('\n', stream) == EOF ? -1 : 0;
}

int infiltratr_project_info_print(FILE *stream,
                                  const InfiltratrProjectInfo *info)
{
    if (!stream || !infiltratr_project_info_is_valid(info)) return -1;
    if (print_field(stream, "name", info->program_name) != 0 ||
        print_field(stream, "version", info->version) != 0 ||
        print_field(stream, "common-library",
                    "infiltratr-common-" INFILTRATR_COMMON_VERSION) != 0 ||
        print_field(stream, "source-id", info->source_id) != 0 ||
        print_field(stream, "build-profile", info->build_profile) != 0 ||
        print_field(stream, "application-id", info->application_id) != 0 ||
        print_field(stream, "author", info->author) != 0 ||
        print_field(stream, "website", info->website) != 0 ||
        print_field(stream, "license", info->license_id) != 0)
        return -1;
    return ferror(stream) == 0 ? 0 : -1;
}

const char *infiltratr_build_profile_label(const char *profile)
{
    if (infiltratr_string_equal(profile, "native"))
        return "Native / local machine compile";
    if (infiltratr_string_equal(profile, "generic"))
        return "Generic / APT package";
    if (infiltratr_string_equal(profile, "cmake"))
        return "Source / CMake build";
    return "Source / development build";
}

void infiltratr_copy_string(char *destination, size_t size,
                            const char *source)
{
    if (INFILTRATR_UNLIKELY(!destination || size == 0U)) return;
    if (!source) source = "";

    size_t length = 0U;
    while (length + 1U < size && source[length] != '\0') length++;
    memmove(destination, source, length);
    destination[length] = '\0';
}

void infiltratr_trim(char *text)
{
    if (!text) return;

    char *start = text;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;

    const size_t length = (size_t)(end - start);
    if (start != text && length > 0U) memmove(text, start, length);
    text[length] = '\0';
}

void infiltratr_trim_line_end(char *text)
{
    if (!text) return;
    size_t length = strlen(text);
    while (length > 0U &&
           (text[length - 1U] == '\n' || text[length - 1U] == '\r'))
        text[--length] = '\0';
}

bool infiltratr_string_equal(const char *left, const char *right)
{
    if (left == right) return true;
    return left && right && strcmp(left, right) == 0;
}

bool infiltratr_string_starts_with(const char *text, const char *prefix)
{
    if (!text || !prefix) return false;
    const size_t prefix_length = strlen(prefix);
    return strncmp(text, prefix, prefix_length) == 0;
}

bool infiltratr_string_ends_with(const char *text, const char *suffix)
{
    if (!text || !suffix) return false;
    const size_t text_length = strlen(text);
    const size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length &&
           strcmp(text + text_length - suffix_length, suffix) == 0;
}

bool infiltratr_ascii_is_space(unsigned char value)
{
    return infiltratr_ascii_space(value);
}

bool infiltratr_ascii_is_alpha(unsigned char value)
{
    return infiltratr_ascii_alpha(value);
}

bool infiltratr_ascii_is_digit(unsigned char value)
{
    return infiltratr_ascii_digit(value);
}

bool infiltratr_ascii_is_alnum(unsigned char value)
{
    return infiltratr_ascii_alnum(value);
}

bool infiltratr_ascii_is_xdigit(unsigned char value)
{
    return infiltratr_ascii_xdigit(value);
}

unsigned char infiltratr_ascii_to_lower(unsigned char value)
{
    return infiltratr_ascii_lower(value);
}

unsigned char infiltratr_ascii_to_upper(unsigned char value)
{
    return infiltratr_ascii_upper(value);
}

bool infiltratr_ascii_equal_ci(const char *left, const char *right)
{
    if (left == right) return true;
    if (!left || !right) return false;
    while (*left && *right) {
        if (infiltratr_ascii_lower((unsigned char)*left) !=
            infiltratr_ascii_lower((unsigned char)*right))
            return false;
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

int infiltratr_ascii_compare_ci(const char *left, const char *right)
{
    if (left == right) return 0;
    if (!left) return -1;
    if (!right) return 1;
    while (*left && *right) {
        const unsigned char folded_left =
            infiltratr_ascii_lower((unsigned char)*left);
        const unsigned char folded_right =
            infiltratr_ascii_lower((unsigned char)*right);
        if (folded_left != folded_right)
            return folded_left < folded_right ? -1 : 1;
        left++;
        right++;
    }
    if (*left == *right) return 0;
    return *left == '\0' ? -1 : 1;
}

bool infiltratr_ascii_starts_with_ci(const char *text, const char *prefix)
{
    if (!text || !prefix) return false;
    while (*prefix) {
        if (!*text ||
            infiltratr_ascii_lower((unsigned char)*text) !=
            infiltratr_ascii_lower((unsigned char)*prefix))
            return false;
        text++;
        prefix++;
    }
    return true;
}

bool infiltratr_ascii_contains_ci(const char *text, const char *needle)
{
    if (!text || !needle) return false;
    if (!*needle) return true;
    for (const char *start = text; *start; start++)
        if (infiltratr_ascii_starts_with_ci(start, needle))
            return true;
    return false;
}

uint64_t infiltratr_fnv1a64_mix_byte(uint64_t hash, unsigned char value)
{
    hash ^= (uint64_t)value;
    return hash * UINT64_C(1099511628211);
}

uint64_t infiltratr_fnv1a64_mix_text(uint64_t hash, const char *text)
{
    const unsigned char *cursor =
        (const unsigned char *)(text ? text : "");
    while (*cursor)
        hash = infiltratr_fnv1a64_mix_byte(hash, *cursor++);
    return hash;
}

uint64_t infiltratr_fnv1a64_mix_u64_le(uint64_t hash, uint64_t value)
{
    for (unsigned int shift = 0U; shift < 64U; shift += 8U)
        hash = infiltratr_fnv1a64_mix_byte(
            hash, (unsigned char)((value >> shift) & UINT64_C(0xff)));
    return hash;
}

bool infiltratr_parse_u64(const char *text, unsigned int base,
                          uint64_t *value)
{
    if (!text || !value) return false;
    const char *cursor = text;
    uint64_t parsed = 0U;
    if (!infiltratr_parse_u64_token(&cursor, base, &parsed)) return false;
    while (*cursor && isspace((unsigned char)*cursor)) cursor++;
    if (*cursor != '\0') return false;
    *value = parsed;
    return true;
}

bool infiltratr_parse_i64(const char *text, unsigned int base, int64_t *value)
{
    if (!text || !value) return false;
    const char *cursor = text;
    int64_t parsed = 0;
    if (!infiltratr_parse_i64_token(&cursor, base, &parsed)) return false;
    while (*cursor && isspace((unsigned char)*cursor)) cursor++;
    if (*cursor != '\0') return false;
    *value = parsed;
    return true;
}

bool infiltratr_parse_u64_range(const char *text, unsigned int base,
                                uint64_t minimum, uint64_t maximum,
                                uint64_t *value)
{
    if (!value || minimum > maximum) return false;
    uint64_t parsed = 0U;
    if (!infiltratr_parse_u64(text, base, &parsed) ||
        parsed < minimum || parsed > maximum)
        return false;
    *value = parsed;
    return true;
}

bool infiltratr_parse_i64_range(const char *text, unsigned int base,
                                int64_t minimum, int64_t maximum,
                                int64_t *value)
{
    if (!value || minimum > maximum) return false;
    int64_t parsed = 0;
    if (!infiltratr_parse_i64(text, base, &parsed) ||
        parsed < minimum || parsed > maximum)
        return false;
    *value = parsed;
    return true;
}

/*
 * Correctly rounded binary64 conversion
 * -------------------------------------
 *
 * A binary64 rounding midpoint is an odd integer times 2^-k with k <= 1075.
 * Written exactly in decimal, such a midpoint has fewer than 770 significant
 * digits: the odd integer contributes at most 17 decimal digits and 5^1075
 * contributes fewer than 752. Keeping 800 significant input digits plus a
 * sticky "discarded non-zero" bit is therefore sufficient to decide every
 * binary64 rounding boundary exactly, while still accepting input strings of
 * arbitrary length.
 *
 * The fixed big-integer workspace is also representation-derived rather than
 * an input limit. 800 decimal digits need fewer than 3200 bits; the largest
 * relevant power of five and binary scaling keep all intermediates below 4443
 * bits. 160 32-bit limbs provide 5120 bits of checked workspace.
 */
#define INFILTRATR_DOUBLE_DECIMAL_DIGITS 800U
#define INFILTRATR_DOUBLE_BIG_LIMBS 160U

_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 &&
               DBL_MAX_EXP == 1024 && DBL_MIN_EXP == -1021 &&
               sizeof(double) == sizeof(uint64_t),
               "infiltratr_parse_double requires IEEE-754 binary64 double");

typedef struct {
    uint32_t limbs[INFILTRATR_DOUBLE_BIG_LIMBS];
    size_t length;
} InfiltratrBigUInt;

typedef struct {
    bool negative;
    size_t magnitude;
} InfiltratrDecimalExponent;

static void big_normalise(InfiltratrBigUInt *value)
{
    while (value->length > 0U &&
           value->limbs[value->length - 1U] == 0U)
        value->length--;
}

static bool big_mul_add_small(InfiltratrBigUInt *value,
                              uint32_t multiplier, uint32_t addend)
{
    uint64_t carry = addend;
    for (size_t i = 0U; i < value->length; ++i) {
        const uint64_t product =
            (uint64_t)value->limbs[i] * multiplier + carry;
        value->limbs[i] = (uint32_t)product;
        carry = product >> 32;
    }

    while (carry != 0U) {
        if (value->length >= INFILTRATR_DOUBLE_BIG_LIMBS)
            return false;
        value->limbs[value->length++] = (uint32_t)carry;
        carry >>= 32;
    }
    return true;
}

static bool big_from_decimal_digits(InfiltratrBigUInt *value,
                                    const uint8_t *digits, size_t count)
{
    memset(value, 0, sizeof(*value));
    for (size_t i = 0U; i < count; ++i)
        if (!big_mul_add_small(value, 10U, digits[i]))
            return false;
    return value->length != 0U;
}

static bool big_set_one(InfiltratrBigUInt *value)
{
    memset(value, 0, sizeof(*value));
    value->limbs[0] = 1U;
    value->length = 1U;
    return true;
}

static bool big_multiply_small(InfiltratrBigUInt *value, uint32_t multiplier)
{
    return big_mul_add_small(value, multiplier, 0U);
}

static bool big_power_five(InfiltratrBigUInt *value, size_t exponent)
{
    (void)big_set_one(value);
    for (size_t i = 0U; i < exponent; ++i)
        if (!big_multiply_small(value, 5U))
            return false;
    return true;
}

static size_t big_bit_length(const InfiltratrBigUInt *value)
{
    if (!value || value->length == 0U) return 0U;

    uint32_t top = value->limbs[value->length - 1U];
    size_t bits = (value->length - 1U) * 32U;
    while (top != 0U) {
        bits++;
        top >>= 1U;
    }
    return bits;
}

static int big_compare(const InfiltratrBigUInt *left,
                       const InfiltratrBigUInt *right)
{
    if (left->length != right->length)
        return left->length < right->length ? -1 : 1;

    for (size_t i = left->length; i-- > 0U;) {
        if (left->limbs[i] != right->limbs[i])
            return left->limbs[i] < right->limbs[i] ? -1 : 1;
    }
    return 0;
}

static bool big_shift_left(InfiltratrBigUInt *value, size_t bits)
{
    if (value->length == 0U || bits == 0U) return true;

    const size_t words = bits / 32U;
    const unsigned int remainder = (unsigned int)(bits % 32U);
    const size_t old_length = value->length;
    const size_t extra = remainder != 0U ? 1U : 0U;

    if (words > INFILTRATR_DOUBLE_BIG_LIMBS - old_length ||
        extra > INFILTRATR_DOUBLE_BIG_LIMBS - old_length - words)
        return false;

    memmove(value->limbs + words, value->limbs,
            old_length * sizeof(value->limbs[0]));
    memset(value->limbs, 0, words * sizeof(value->limbs[0]));
    value->length = old_length + words;

    if (remainder != 0U) {
        uint32_t carry = 0U;
        for (size_t i = words; i < words + old_length; ++i) {
            const uint64_t shifted =
                ((uint64_t)value->limbs[i] << remainder) | carry;
            value->limbs[i] = (uint32_t)shifted;
            carry = (uint32_t)(shifted >> 32);
        }
        if (carry != 0U)
            value->limbs[value->length++] = carry;
    }

    return true;
}

static void big_shift_right_one(InfiltratrBigUInt *value)
{
    uint32_t carry = 0U;
    for (size_t i = value->length; i-- > 0U;) {
        const uint32_t next = value->limbs[i] & 1U;
        value->limbs[i] = (value->limbs[i] >> 1U) | (carry << 31U);
        carry = next;
    }
    big_normalise(value);
}

static void big_subtract(InfiltratrBigUInt *left,
                         const InfiltratrBigUInt *right)
{
    uint64_t borrow = 0U;
    for (size_t i = 0U; i < left->length; ++i) {
        const uint64_t subtrahend =
            (i < right->length ? (uint64_t)right->limbs[i] : 0U) + borrow;
        const uint64_t minuend = left->limbs[i];
        left->limbs[i] = (uint32_t)(minuend - subtrahend);
        borrow = minuend < subtrahend ? 1U : 0U;
    }
    big_normalise(left);
}

static bool big_ratio_at_least_power_two(const InfiltratrBigUInt *numerator,
                                         const InfiltratrBigUInt *denominator,
                                         int64_t power)
{
    InfiltratrBigUInt shifted;

    if (power >= 0) {
        shifted = *denominator;
        if (!big_shift_left(&shifted, (size_t)power)) return false;
        return big_compare(numerator, &shifted) >= 0;
    }

    shifted = *numerator;
    if (!big_shift_left(&shifted, (size_t)(-power))) return false;
    return big_compare(&shifted, denominator) >= 0;
}

static bool big_divide_u64(const InfiltratrBigUInt *numerator,
                           const InfiltratrBigUInt *denominator,
                           uint64_t *quotient,
                           InfiltratrBigUInt *remainder)
{
    if (!numerator || !denominator || denominator->length == 0U ||
        !quotient || !remainder)
        return false;

    *remainder = *numerator;
    *quotient = 0U;
    if (big_compare(remainder, denominator) < 0)
        return true;

    const size_t numerator_bits = big_bit_length(remainder);
    const size_t denominator_bits = big_bit_length(denominator);
    const size_t shift = numerator_bits - denominator_bits;
    if (shift >= 64U) return false;

    InfiltratrBigUInt divisor = *denominator;
    if (!big_shift_left(&divisor, shift)) return false;

    for (size_t bit = shift;; --bit) {
        if (big_compare(remainder, &divisor) >= 0) {
            big_subtract(remainder, &divisor);
            *quotient |= UINT64_C(1) << bit;
        }
        if (bit == 0U) break;
        big_shift_right_one(&divisor);
    }
    return true;
}

static bool decimal_exponent_combine(bool explicit_negative,
                                     size_t explicit_magnitude,
                                     size_t fractional_digits,
                                     size_t positive_adjustment,
                                     InfiltratrDecimalExponent *result)
{
    if (!result) return false;

    if (!explicit_negative) {
        if (explicit_magnitude >= fractional_digits) {
            const size_t base = explicit_magnitude - fractional_digits;
            if (positive_adjustment > SIZE_MAX - base) return false;
            result->negative = false;
            result->magnitude = base + positive_adjustment;
            return true;
        }

        const size_t negative = fractional_digits - explicit_magnitude;
        if (positive_adjustment >= negative) {
            result->negative = false;
            result->magnitude = positive_adjustment - negative;
        } else {
            result->negative = true;
            result->magnitude = negative - positive_adjustment;
        }
        return true;
    }

    if (positive_adjustment >= explicit_magnitude) {
        const size_t positive = positive_adjustment - explicit_magnitude;
        if (positive >= fractional_digits) {
            result->negative = false;
            result->magnitude = positive - fractional_digits;
        } else {
            result->negative = true;
            result->magnitude = fractional_digits - positive;
        }
        return true;
    }

    const size_t negative = explicit_magnitude - positive_adjustment;
    if (fractional_digits > SIZE_MAX - negative) return false;
    result->negative = true;
    result->magnitude = negative + fractional_digits;
    return true;
}

static bool decimal_exponent_add_positive(
    const InfiltratrDecimalExponent *value, size_t addend,
    InfiltratrDecimalExponent *result)
{
    if (!value || !result) return false;

    if (!value->negative) {
        if (addend > SIZE_MAX - value->magnitude) return false;
        result->negative = false;
        result->magnitude = value->magnitude + addend;
        return true;
    }

    if (addend >= value->magnitude) {
        result->negative = false;
        result->magnitude = addend - value->magnitude;
    } else {
        result->negative = true;
        result->magnitude = value->magnitude - addend;
    }
    return true;
}

static bool parse_decimal_exponent(const char **cursor,
                                   bool *negative,
                                   size_t *magnitude,
                                   bool *overflowed)
{
    if (!cursor || !*cursor || !negative || !magnitude || !overflowed)
        return false;

    const char *position = *cursor;
    *negative = false;
    *magnitude = 0U;
    *overflowed = false;

    if (*position != 'e' && *position != 'E')
        return true;

    position++;
    if (*position == '+' || *position == '-') {
        *negative = *position == '-';
        position++;
    }
    if (!infiltratr_ascii_digit(*position)) return false;

    while (infiltratr_ascii_digit(*position)) {
        const size_t digit = (size_t)(*position - '0');
        if (!*overflowed) {
            if (*magnitude > (SIZE_MAX - digit) / 10U)
                *overflowed = true;
            else
                *magnitude = *magnitude * 10U + digit;
        }
        position++;
    }

    *cursor = position;
    return true;
}

static bool binary64_from_decimal(const uint8_t *digits, size_t digit_count,
                                  bool sticky,
                                  const InfiltratrDecimalExponent *decimal_exp,
                                  bool negative, double *value)
{
    if (!digits || digit_count == 0U || !decimal_exp || !value)
        return false;

    InfiltratrDecimalExponent order;
    if (!decimal_exponent_add_positive(decimal_exp, digit_count - 1U, &order))
        return false;
    if ((!order.negative && order.magnitude > 308U) ||
        (order.negative && order.magnitude > 324U))
        return false;

    /*
     * The decimal-order gate above proves the effective base-10 exponent is
     * small enough for binary64 conversion: +308 at most, or no less than
     * -(799 + 324) = -1123 with the retained 800-digit coefficient.
     */
    if (decimal_exp->magnitude > 1123U) return false;
    const int64_t exponent10 = decimal_exp->negative
        ? -(int64_t)decimal_exp->magnitude
        : (int64_t)decimal_exp->magnitude;

    InfiltratrBigUInt numerator;
    InfiltratrBigUInt denominator;
    if (!big_from_decimal_digits(&numerator, digits, digit_count))
        return false;
    (void)big_set_one(&denominator);

    int64_t binary_shift = 0;
    if (exponent10 >= 0) {
        for (int64_t i = 0; i < exponent10; ++i)
            if (!big_multiply_small(&numerator, 5U))
                return false;
        binary_shift = exponent10;
    } else {
        if (!big_power_five(&denominator, (size_t)(-exponent10)))
            return false;
        binary_shift = exponent10;
    }

    const size_t numerator_bits = big_bit_length(&numerator);
    const size_t denominator_bits = big_bit_length(&denominator);
    if (numerator_bits == 0U || denominator_bits == 0U ||
        numerator_bits > (size_t)INT64_MAX ||
        denominator_bits > (size_t)INT64_MAX)
        return false;

    const int64_t bit_difference =
        (int64_t)numerator_bits - (int64_t)denominator_bits;
    const bool at_candidate =
        big_ratio_at_least_power_two(&numerator, &denominator,
                                     bit_difference);
    int64_t exponent2 =
        (at_candidate ? bit_difference : bit_difference - 1) + binary_shift;

    const bool normal = exponent2 >= -1022;
    const int64_t scale_power = normal
        ? binary_shift + 52 - exponent2
        : binary_shift + 1074;

    InfiltratrBigUInt scaled_numerator = numerator;
    InfiltratrBigUInt scaled_denominator = denominator;
    if (scale_power >= 0) {
        if (!big_shift_left(&scaled_numerator, (size_t)scale_power))
            return false;
    } else if (!big_shift_left(&scaled_denominator,
                               (size_t)(-scale_power))) {
        return false;
    }

    uint64_t significand = 0U;
    InfiltratrBigUInt remainder;
    if (!big_divide_u64(&scaled_numerator, &scaled_denominator,
                        &significand, &remainder))
        return false;

    InfiltratrBigUInt twice_remainder = remainder;
    if (!big_multiply_small(&twice_remainder, 2U))
        return false;
    const int midpoint =
        big_compare(&twice_remainder, &scaled_denominator);
    if (midpoint > 0 ||
        (midpoint == 0 && ((significand & 1U) != 0U || sticky))) {
        if (significand == UINT64_MAX) return false;
        significand++;
    }

    uint64_t bits = 0U;
    if (normal) {
        if (significand == (UINT64_C(1) << 53)) {
            significand >>= 1U;
            exponent2++;
        }
        if (exponent2 > 1023 ||
            significand < (UINT64_C(1) << 52) ||
            significand >= (UINT64_C(1) << 53))
            return false;

        bits = ((uint64_t)(exponent2 + 1023) << 52) |
               (significand - (UINT64_C(1) << 52));
    } else {
        if (significand == 0U)
            return false; /* Contract rejects underflow rounded to zero. */
        if (significand == (UINT64_C(1) << 52)) {
            bits = UINT64_C(1) << 52; /* Smallest normal value. */
        } else {
            if (significand >= (UINT64_C(1) << 52))
                return false;
            bits = significand;
        }
    }

    if (negative) bits |= UINT64_C(1) << 63;
    double converted;
    memcpy(&converted, &bits, sizeof(converted));
    if (!isfinite(converted) || converted == 0.0)
        return false;
    *value = converted;
    return true;
}

static bool parse_double_prefix(const char *text, bool allow_sign,
                                const char **end, double *value)
{
    if (!text || !end || !value) return false;

    const char *cursor = text;
    while (infiltratr_ascii_space(*cursor)) cursor++;

    bool negative = false;
    if (*cursor == '+' || *cursor == '-') {
        if (!allow_sign) return false;
        negative = *cursor == '-';
        cursor++;
    }

    uint8_t digits[INFILTRATR_DOUBLE_DECIMAL_DIGITS];
    size_t kept_digits = 0U;
    size_t significant_digits = 0U;
    size_t fractional_digits = 0U;
    bool discarded_nonzero = false;
    bool saw_digit = false;
    bool saw_nonzero = false;

    while (infiltratr_ascii_digit(*cursor)) {
        const uint8_t digit = (uint8_t)(*cursor - '0');
        saw_digit = true;
        if (!saw_nonzero && digit != 0U) saw_nonzero = true;
        if (saw_nonzero) {
            significant_digits++;
            if (kept_digits < INFILTRATR_DOUBLE_DECIMAL_DIGITS)
                digits[kept_digits++] = digit;
            else if (digit != 0U)
                discarded_nonzero = true;
        }
        cursor++;
    }

    if (*cursor == '.') {
        cursor++;
        while (infiltratr_ascii_digit(*cursor)) {
            const uint8_t digit = (uint8_t)(*cursor - '0');
            saw_digit = true;
            if (fractional_digits == SIZE_MAX) return false;
            fractional_digits++;
            if (!saw_nonzero && digit != 0U) saw_nonzero = true;
            if (saw_nonzero) {
                significant_digits++;
                if (kept_digits < INFILTRATR_DOUBLE_DECIMAL_DIGITS)
                    digits[kept_digits++] = digit;
                else if (digit != 0U)
                    discarded_nonzero = true;
            }
            cursor++;
        }
    }

    if (!saw_digit) return false;

    bool exponent_negative = false;
    size_t exponent_magnitude = 0U;
    bool exponent_overflowed = false;
    if (!parse_decimal_exponent(&cursor, &exponent_negative,
                                &exponent_magnitude,
                                &exponent_overflowed))
        return false;

    double parsed = 0.0;
    if (!saw_nonzero) {
        parsed = negative ? -0.0 : 0.0;
    } else {
        if (exponent_overflowed) return false;

        const size_t discarded_digits = significant_digits - kept_digits;
        size_t positive_adjustment = discarded_digits;

        if (!discarded_nonzero) {
            size_t trailing_zeroes = 0U;
            while (kept_digits > 0U && digits[kept_digits - 1U] == 0U) {
                kept_digits--;
                trailing_zeroes++;
            }
            if (trailing_zeroes > SIZE_MAX - positive_adjustment)
                return false;
            positive_adjustment += trailing_zeroes;
        }

        InfiltratrDecimalExponent decimal_exp;
        if (!decimal_exponent_combine(exponent_negative, exponent_magnitude,
                                      fractional_digits, positive_adjustment,
                                      &decimal_exp))
            return false;

        if (!binary64_from_decimal(digits, kept_digits, discarded_nonzero,
                                   &decimal_exp, negative, &parsed))
            return false;
    }

    *end = cursor;
    *value = parsed;
    return true;
}

bool infiltratr_parse_double_token(const char **cursor, bool allow_sign,
                                   double *value)
{
    if (!cursor || !*cursor || !value) return false;

    const char *end = NULL;
    double parsed = 0.0;
    if (!parse_double_prefix(*cursor, allow_sign, &end, &parsed))
        return false;

    *cursor = end;
    *value = parsed;
    return true;
}

bool infiltratr_parse_double(const char *text, double *value)
{
    if (!text || !value) return false;

    const char *end = NULL;
    double parsed = 0.0;
    if (!parse_double_prefix(text, true, &end, &parsed))
        return false;

    while (infiltratr_ascii_space(*end)) end++;
    if (*end != '\0') return false;

    *value = parsed;
    return true;
}
bool infiltratr_parse_double_range(const char *text, double minimum,
                                   double maximum, double *value)
{
    if (!value || isnan(minimum) || isnan(maximum) || minimum > maximum) return false;
    double parsed = 0.0;
    if (!infiltratr_parse_double(text, &parsed) || parsed < minimum || parsed > maximum)
        return false;
    *value = parsed;
    return true;
}

double infiltratr_clamp_double(double value, double lower, double upper)
{
    if (isnan(value) || isnan(lower) || isnan(upper) || lower > upper) return value;
    if (value < lower) return lower;
    return value > upper ? upper : value;
}

bool infiltratr_u64_add_checked(uint64_t left, uint64_t right, uint64_t *result)
{
    if (!result || right > UINT64_MAX - left) return false;
    *result = left + right;
    return true;
}

uint64_t infiltratr_u64_add_saturating(uint64_t left, uint64_t right)
{
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
}

uint64_t infiltratr_u64_multiply_saturating(uint64_t left, uint64_t right)
{
    return left != 0U && right > UINT64_MAX / left ? UINT64_MAX : left * right;
}

double infiltratr_percent_u64(uint64_t part, uint64_t whole)
{
    if (whole == 0U) return 0.0;
    const long double percentage = 100.0L * (long double)part / (long double)whole;
    if (percentage <= 0.0L) return 0.0;
    return percentage >= 100.0L ? 100.0 : (double)percentage;
}

bool infiltratr_u64_counter_delta(uint64_t current, uint64_t previous,
                                  uint64_t *delta)
{
    if (!delta || current < previous) return false;
    *delta = current - previous;
    return true;
}

bool infiltratr_u64_counter_rate(uint64_t current, uint64_t previous,
                                 long double units_per_count,
                                 double elapsed_seconds, double *rate)
{
    if (!rate) return false;
    *rate = 0.0;
    uint64_t delta = 0U;
    if (!infiltratr_u64_counter_delta(current, previous, &delta) ||
        units_per_count < 0.0L || !isfinite(units_per_count) ||
        elapsed_seconds <= 0.0 || !isfinite(elapsed_seconds))
        return false;
    const long double calculated =
        (long double)delta * units_per_count / (long double)elapsed_seconds;
    if (!isfinite(calculated) || calculated > (long double)DBL_MAX) return false;
    *rate = (double)calculated;
    return true;
}

static bool scale_options_valid(const InfiltratrScaleOptions *options,
                                size_t unit_count, size_t *maximum_unit)
{
    if (!options || options->struct_size < INFILTRATR_SCALE_OPTIONS_ABI1_SIZE ||
        options->abi_version != INFILTRATR_SCALE_OPTIONS_ABI ||
        !isfinite(options->divisor) || options->divisor <= 1.0L ||
        unit_count == 0U || options->minimum_unit >= unit_count ||
        options->decimal_places > 9U ||
        !isfinite(options->integer_threshold) ||
        options->integer_threshold < 0.0L)
        return false;

    const size_t maximum = options->maximum_unit == SIZE_MAX
        ? unit_count - 1U : options->maximum_unit;
    if (maximum >= unit_count || maximum < options->minimum_unit) return false;
    if (maximum_unit) *maximum_unit = maximum;
    return true;
}

bool infiltratr_scale_quantity(long double value,
                               const InfiltratrScaleOptions *options,
                               size_t unit_count, long double *scaled_value,
                               size_t *unit_index)
{
    size_t maximum_unit = 0U;
    if (!scaled_value || !unit_index || !isfinite(value) ||
        !scale_options_valid(options, unit_count, &maximum_unit))
        return false;
    const long double magnitude = fabsl(value);
    long double minimum_boundary = 1.0L;
    for (size_t index = 0U; index < options->minimum_unit; index++) {
        if (minimum_boundary > LDBL_MAX / options->divisor) {
            minimum_boundary = LDBL_MAX;
            break;
        }
        minimum_boundary *= options->divisor;
    }
    size_t unit = options->minimum_unit;
    if (options->zero_below_minimum_unit && magnitude < minimum_boundary) {
        *scaled_value = 0.0L;
        *unit_index = unit;
        return true;
    }
    long double scaled = value;
    for (size_t index = 0U; index < unit; index++) scaled /= options->divisor;
    while (unit < maximum_unit && fabsl(scaled) >= options->divisor) {
        scaled /= options->divisor;
        unit++;
    }
    *scaled_value = scaled;
    *unit_index = unit;
    return true;
}

bool infiltratr_format_scaled_quantity(long double value,
                                        const char *const *units,
                                        size_t unit_count,
                                        const char *suffix,
                                        const InfiltratrScaleOptions *options,
                                        char *buffer, size_t buffer_size)
{
    if (buffer && buffer_size > 0U) buffer[0] = '\0';
    if (!units || !buffer || buffer_size == 0U) return false;
    long double scaled = 0.0L;
    size_t unit = 0U;
    if (!infiltratr_scale_quantity(value, options, unit_count, &scaled, &unit) ||
        !units[unit])
        return false;
    unsigned int decimal_places = options->decimal_places;
    if ((options->integer_at_minimum_unit && unit == options->minimum_unit) ||
        (options->integer_threshold > 0.0L &&
         fabsl(scaled) >= options->integer_threshold))
        decimal_places = 0U;
    const char *actual_suffix = suffix ? suffix : "";
    const int written = snprintf(buffer, buffer_size, "%.*Lf %s%s",
                                 (int)decimal_places, scaled, units[unit], actual_suffix);
    if (written < 0 || (size_t)written >= buffer_size) {
        buffer[0] = '\0';
        return false;
    }
    return true;
}

char *infiltratr_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size)
{
    static const char *const units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    const InfiltratrScaleOptions options = INFILTRATR_SCALE_OPTIONS_INIT;
    (void)infiltratr_format_scaled_quantity((long double)bytes, units,
                                             INFILTRATR_ARRAY_LENGTH(units),
                                             "", &options, buffer, buffer_size);
    return buffer;
}

char *infiltratr_format_rate(double bytes_per_second, char *buffer,
                             size_t buffer_size)
{
    if (!isfinite(bytes_per_second) || bytes_per_second < 0.0) bytes_per_second = 0.0;
    static const char *const units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    const InfiltratrScaleOptions options = INFILTRATR_SCALE_OPTIONS_INIT;
    (void)infiltratr_format_scaled_quantity((long double)bytes_per_second, units,
                                             INFILTRATR_ARRAY_LENGTH(units),
                                             "/s", &options, buffer, buffer_size);
    return buffer;
}
