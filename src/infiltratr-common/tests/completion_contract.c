// SPDX-License-Identifier: GPL-3.0-or-later
#include "infiltratr/arithmetic.h"
#include "infiltratr/core.h"
#include "infiltratr/endian.h"
#include "infiltratr/format.h"
#include "infiltratr/quantity.h"
#include "infiltratr/token.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_endian_load_store(void)
{
    uint8_t bytes[8] = {0};
    infiltratr_store_be64(bytes, UINT64_C(0x0123456789abcdef));
    assert(bytes[0] == 0x01U && bytes[7] == 0xefU);
    assert(infiltratr_load_be64(bytes) == UINT64_C(0x0123456789abcdef));
    infiltratr_store_le64(bytes, UINT64_C(0x0123456789abcdef));
    assert(bytes[0] == 0xefU && bytes[7] == 0x01U);
    assert(infiltratr_load_le64(bytes) == UINT64_C(0x0123456789abcdef));
    const uint32_t be32 = infiltratr_cpu_to_be32(UINT32_C(0x12345678));
    assert(infiltratr_be32_to_cpu(be32) == UINT32_C(0x12345678));
}

static void test_checked_arithmetic(void)
{
    int64_t signed_result = 77;
    uint64_t result = 77U;

    assert(infiltratr_i64_add_checked(10, -3, &signed_result) &&
           signed_result == 7);
    signed_result = 77;
    assert(!infiltratr_i64_add_checked(INT64_MAX, 1, &signed_result) &&
           signed_result == 77);
    assert(infiltratr_i64_multiply_checked(-12, 12, &signed_result) &&
           signed_result == -144);
    signed_result = 77;
    assert(!infiltratr_i64_multiply_checked(INT64_MIN, -1, &signed_result) &&
           signed_result == 77);
    assert(infiltratr_i64_add_saturating(INT64_MAX, 1) == INT64_MAX);
    assert(infiltratr_i64_multiply_saturating(INT64_MAX, 2) == INT64_MAX);
    assert(infiltratr_i64_multiply_saturating(INT64_MIN, -1) == INT64_MAX);
    assert(infiltratr_i64_multiply_saturating(INT64_MIN, 2) == INT64_MIN);

    assert(infiltratr_u64_subtract_checked(10U, 3U, &result) && result == 7U);
    result = 77U;
    assert(!infiltratr_u64_subtract_checked(3U, 10U, &result) && result == 77U);
    assert(infiltratr_u64_multiply_checked(12U, 12U, &result) && result == 144U);
    result = 77U;
    assert(!infiltratr_u64_multiply_checked(UINT64_MAX, 2U, &result));
    assert(result == 77U);
}

static void test_signed_token(void)
{
    const char *text = "  -42,tail";
    const char *cursor = text;
    int64_t value = 17;
    assert(infiltratr_parse_i64_token(&cursor, 10U, &value));
    assert(value == -42 && strcmp(cursor, ",tail") == 0);
    text = "9223372036854775808";
    cursor = text;
    value = 17;
    assert(!infiltratr_parse_i64_token(&cursor, 10U, &value));
    assert(cursor == text && value == 17);
}

static void test_binary_quantity(void)
{
    uint64_t bytes = 0U;
    assert(infiltratr_parse_binary_quantity_u64("32K", &bytes));
    assert(bytes == UINT64_C(32768));
    assert(infiltratr_parse_binary_quantity_u64("1.5 MiB", &bytes));
    assert(bytes == UINT64_C(1572864));
    assert(infiltratr_parse_binary_quantity_u64("8GB", &bytes));
    assert(bytes == UINT64_C(8589934592));
    assert(infiltratr_parse_binary_quantity_u64("9007199254740993B", &bytes));
    assert(bytes == UINT64_C(9007199254740993));
    assert(infiltratr_parse_binary_quantity_u64("18446744073709551615B", &bytes));
    assert(bytes == UINT64_MAX);
    assert(infiltratr_parse_binary_quantity_u64("0.125 KiB", &bytes));
    assert(bytes == UINT64_C(128));
    assert(infiltratr_parse_binary_quantity_u64("1.953125 KiB", &bytes));
    assert(bytes == UINT64_C(2000));
    assert(infiltratr_parse_binary_quantity_u64("1e3 B", &bytes));
    assert(bytes == UINT64_C(1000));

    char long_exact[260];
    long_exact[0] = '1';
    memset(long_exact + 1, '0', 200U);
    memcpy(long_exact + 201, "e-200B", sizeof("e-200B"));
    assert(infiltratr_parse_binary_quantity_u64(long_exact, &bytes));
    assert(bytes == UINT64_C(1));

    assert(infiltratr_parse_binary_quantity_u64(
        "0e999999999999999999999999999999999999999999B", &bytes));
    assert(bytes == UINT64_C(0));

    bytes = 99U;
    assert(!infiltratr_parse_binary_quantity_u64("18446744073709551616B", &bytes));
    assert(bytes == 99U);
    assert(!infiltratr_parse_binary_quantity_u64("1.1B", &bytes));
    assert(bytes == 99U);
    assert(!infiltratr_parse_binary_quantity_u64("1e-3 KiB", &bytes));
    assert(bytes == 99U);
    assert(!infiltratr_parse_binary_quantity_u64("-0B", &bytes));
    assert(bytes == 99U);
    assert(!infiltratr_parse_binary_quantity_u64(
        "1e999999999999999999999999999999999999999999B", &bytes));
    assert(bytes == 99U);
}

static void test_abi_prefix_sizes(void)
{
    static const char *const units[] = {"B", "KB"};
    InfiltratrScaleOptions scale = INFILTRATR_SCALE_OPTIONS_INIT;
    scale.struct_size = offsetof(InfiltratrScaleOptions, zero_below_minimum_unit) +
                        sizeof(scale.zero_below_minimum_unit);
    long double scaled = 0.0L;
    size_t unit = 0U;
    assert(infiltratr_scale_quantity(2048.0L, &scale, 2U, &scaled, &unit));
    assert(unit == 1U && scaled == 2.0L);

    InfiltratrScalarFormatOptions scalar = INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    scalar.struct_size = offsetof(InfiltratrScalarFormatOptions, unavailable_text) +
                         sizeof(scalar.unavailable_text);
    char text[32];
    assert(infiltratr_format_scalar(true, 12.5L, &scalar, text, sizeof(text)));
    assert(strcmp(text, "12.5") == 0);

    (void)units;
}

int main(void)
{
    test_endian_load_store();
    test_checked_arithmetic();
    test_signed_token();
    test_binary_quantity();
    test_abi_prefix_sizes();
    puts("Libraries completion contract tests passed.");
    return 0;
}
