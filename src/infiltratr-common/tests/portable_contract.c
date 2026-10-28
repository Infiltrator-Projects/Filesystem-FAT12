// SPDX-License-Identifier: GPL-3.0-or-later
/** @file portable_contract.c @brief Boundary and failure-state portable API coverage. */
#include "infiltratr/arithmetic.h"
#include "infiltratr/core.h"
#include "infiltratr/format.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static InfiltratrProjectInfo valid_project_info(void)
{
    const InfiltratrProjectInfo info = {
        .struct_size = sizeof(InfiltratrProjectInfo),
        .abi_version = INFILTRATR_PROJECT_INFO_ABI,
        .program_name = "Contract Test",
        .executable_name = "contract-test",
        .application_id = "example.contract.Test",
        .version = "1.0.0",
        .source_id = "contract-test-1.0.0",
        .build_profile = "test",
        .author = "Shannon Smith",
        .website = "https://github.com/Infiltrator-Projects/Libraries",
        .license_id = "GPL-3.0-or-later",
        .comments = "Portable public API contract regression suite",
        .icon_name = "contract-test",
        .copyright_text = "Copyright (c) 1993-2026 Shannon Smith"
    };
    return info;
}

static void test_project_info(void)
{
    InfiltratrProjectInfo info = valid_project_info();
    assert(infiltratr_project_info_is_valid(&info));
    info.struct_size = offsetof(InfiltratrProjectInfo, copyright_text) +
                       sizeof(info.copyright_text) - 1U;
    assert(!infiltratr_project_info_is_valid(&info));
    info = valid_project_info();
    info.abi_version++;
    assert(!infiltratr_project_info_is_valid(&info));
    info = valid_project_info();
    info.website = "";
    assert(!infiltratr_project_info_is_valid(&info));
    assert(!infiltratr_project_info_is_valid(NULL));

    info = valid_project_info();
    info.author = "Shannon\nSmith";
    FILE *stream = tmpfile();
    assert(stream != NULL);
    assert(infiltratr_project_info_print(stream, &info) == 0);
    rewind(stream);
    char output[1024];
    const size_t amount = fread(output, 1U, sizeof(output) - 1U, stream);
    output[amount] = '\0';
    assert(strstr(output, "author=Shannon Smith\n") != NULL);
    assert(fclose(stream) == 0);
    assert(infiltratr_project_info_print(NULL, &info) == -1);
}

static void test_strings(void)
{
    char text[5] = "xxxx";
    infiltratr_copy_string(text, sizeof(text), "abcd");
    assert(strcmp(text, "abcd") == 0);
    infiltratr_copy_string(text, sizeof(text), "abcde");
    assert(strcmp(text, "abcd") == 0);
    infiltratr_copy_string(text, sizeof(text), NULL);
    assert(strcmp(text, "") == 0);
    char single = 'Q';
    infiltratr_copy_string(&single, 0U, "x");
    assert(single == 'Q');
    infiltratr_copy_string(NULL, 8U, "x");
    char overlap_right[8] = "abcdef";
    infiltratr_copy_string(overlap_right + 1, sizeof(overlap_right) - 1U,
                           overlap_right);
    assert(strcmp(overlap_right + 1, "abcdef") == 0);
    char overlap_left[8] = "abcdef";
    infiltratr_copy_string(overlap_left, sizeof(overlap_left),
                           overlap_left + 1);
    assert(strcmp(overlap_left, "bcdef") == 0);
    char whitespace[] = "\t  value \r\n";
    infiltratr_trim(whitespace);
    assert(strcmp(whitespace, "value") == 0);
    char only_whitespace[] = " \t\n";
    infiltratr_trim(only_whitespace);
    assert(strcmp(only_whitespace, "") == 0);
    infiltratr_trim(NULL);
    char line[] = "value\r\n\n";
    infiltratr_trim_line_end(line);
    assert(strcmp(line, "value") == 0);
    infiltratr_trim_line_end(NULL);
    assert(infiltratr_string_equal(NULL, NULL));
    assert(!infiltratr_string_equal(NULL, ""));
    assert(infiltratr_string_starts_with("abc", ""));
    assert(infiltratr_string_ends_with("abc", ""));
    assert(!infiltratr_string_starts_with(NULL, ""));
    assert(!infiltratr_string_ends_with("abc", NULL));
}

static void test_parsers(void)
{
    uint64_t unsigned_value = 777U;
    assert(infiltratr_parse_u64("  18446744073709551615  ", 10U, &unsigned_value));
    assert(unsigned_value == UINT64_MAX);
    unsigned_value = 777U;
    assert(!infiltratr_parse_u64("+1", 10U, &unsigned_value));
    assert(unsigned_value == 777U);
    assert(!infiltratr_parse_u64("1x", 10U, &unsigned_value));
    assert(unsigned_value == 777U);
    assert(!infiltratr_parse_u64("1", 1U, &unsigned_value));
    assert(!infiltratr_parse_u64("1", 37U, &unsigned_value));
    assert(!infiltratr_parse_u64(NULL, 10U, &unsigned_value));
    assert(!infiltratr_parse_u64("1", 10U, NULL));

    int64_t signed_value = 333;
    assert(infiltratr_parse_i64("-9223372036854775808", 10U, &signed_value));
    assert(signed_value == INT64_MIN);
    signed_value = 333;
    assert(!infiltratr_parse_i64("9223372036854775808", 10U, &signed_value));
    assert(signed_value == 333);

    unsigned_value = 88U;
    assert(!infiltratr_parse_u64_range("5", 10U, 10U, 1U, &unsigned_value));
    assert(unsigned_value == 88U);
    assert(!infiltratr_parse_u64_range("9", 10U, 10U, 20U, &unsigned_value));
    assert(unsigned_value == 88U);
    signed_value = 88;
    assert(!infiltratr_parse_i64_range("0", 10U, 1, -1, &signed_value));
    assert(signed_value == 88);

    double decimal = 123.0;
    assert(infiltratr_parse_double("\t-0.125e+2\n", &decimal));
    assert(decimal == -12.5);
    decimal = 123.0;
    assert(!infiltratr_parse_double("nan", &decimal));
    assert(decimal == 123.0);
    assert(!infiltratr_parse_double("inf", &decimal));
    assert(decimal == 123.0);
    assert(!infiltratr_parse_double("1e-400", &decimal));
    assert(decimal == 123.0);
    assert(!infiltratr_parse_double("1e500", &decimal));
    assert(decimal == 123.0);
    assert(!infiltratr_parse_double("1.0 trailing", &decimal));
    assert(decimal == 123.0);

    assert(infiltratr_parse_double("41346.59693578e194", &decimal));
    assert(decimal == 0x1.ba7fd277e456fp+659);
    assert(infiltratr_parse_double(
        "92684359178468.83560938576290468711402e-316", &decimal));
    assert(decimal == 0x1.96c845caed0a3p-1004);
    assert(infiltratr_parse_double("969e-230", &decimal));
    assert(decimal == 0x1.d61f33d8086e5p-755);
    assert(infiltratr_parse_double("611824598.18898e242", &decimal));
    assert(decimal == 0x1.11739dcad5beap+833);
    assert(infiltratr_parse_double("2.4703282292062328e-324", &decimal));
    assert(decimal == DBL_TRUE_MIN);
    decimal = 123.0;
    assert(!infiltratr_parse_double("2.4703282292062327e-324", &decimal));
    assert(decimal == 123.0);
    assert(infiltratr_parse_double("1.7976931348623157e308", &decimal));
    assert(decimal == DBL_MAX);
    decimal = 123.0;
    assert(!infiltratr_parse_double("1.7976931348623159e308", &decimal));
    assert(decimal == 123.0);

    char long_decimal[1200];
    long_decimal[0] = '1';
    memset(long_decimal + 1, '0', 1000U);
    memcpy(long_decimal + 1001, "e-1000", sizeof("e-1000"));
    assert(infiltratr_parse_double(long_decimal, &decimal));
    assert(decimal == 1.0);

    char smallest_text[64];
    const int smallest_written = snprintf(smallest_text, sizeof(smallest_text),
                                           "%.17e", DBL_TRUE_MIN);
    assert(smallest_written > 0 && (size_t)smallest_written < sizeof(smallest_text));
    decimal = 0.0;
    assert(infiltratr_parse_double(smallest_text, &decimal));
    assert(decimal == DBL_TRUE_MIN);
    decimal = 99.0;
    assert(!infiltratr_parse_double_range("5", NAN, 10.0, &decimal));
    assert(decimal == 99.0);
    assert(!infiltratr_parse_double_range("5", 10.0, 1.0, &decimal));
    assert(decimal == 99.0);
}

static void test_arithmetic(void)
{
    assert(infiltratr_clamp_double(-1.0, 0.0, 10.0) == 0.0);
    assert(infiltratr_clamp_double(11.0, 0.0, 10.0) == 10.0);
    assert(infiltratr_clamp_double(5.0, 10.0, 0.0) == 5.0);
    int64_t signed_result = 42;
    assert(infiltratr_i64_add_checked(20, 22, &signed_result));
    assert(signed_result == 42);
    signed_result = 42;
    assert(!infiltratr_i64_add_checked(INT64_MAX, 1, &signed_result));
    assert(signed_result == 42);
    assert(infiltratr_i64_multiply_checked(-7, 6, &signed_result));
    assert(signed_result == -42);
    signed_result = 42;
    assert(!infiltratr_i64_multiply_checked(INT64_MIN, -1, &signed_result));
    assert(signed_result == 42);
    assert(infiltratr_i64_add_saturating(INT64_MAX, 1) == INT64_MAX);
    assert(infiltratr_i64_add_saturating(INT64_MIN, -1) == INT64_MIN);
    assert(infiltratr_i64_multiply_saturating(INT64_MAX, 2) == INT64_MAX);
    assert(infiltratr_i64_multiply_saturating(INT64_MIN, -1) == INT64_MAX);
    assert(infiltratr_i64_multiply_saturating(INT64_MIN, 2) == INT64_MIN);
    assert(infiltratr_i64_multiply_saturating(-7, 6) == -42);

    uint64_t result = 42U;
    assert(!infiltratr_u64_add_checked(UINT64_MAX, 1U, &result));
    assert(result == 42U);
    assert(!infiltratr_u64_add_checked(1U, 1U, NULL));
    assert(infiltratr_u64_add_saturating(UINT64_MAX - 1U, 1U) == UINT64_MAX);
    assert(infiltratr_u64_add_saturating(UINT64_MAX, 1U) == UINT64_MAX);
    assert(infiltratr_u64_multiply_saturating(0U, UINT64_MAX) == 0U);
    assert(infiltratr_u64_multiply_saturating(UINT64_MAX, 2U) == UINT64_MAX);
    assert(infiltratr_percent_u64(0U, 0U) == 0.0);
    assert(infiltratr_percent_u64(200U, 100U) == 100.0);
    uint64_t delta = 77U;
    assert(infiltratr_u64_counter_delta(12U, 10U, &delta));
    assert(delta == 2U);
    delta = 77U;
    assert(!infiltratr_u64_counter_delta(9U, 10U, &delta));
    assert(delta == 77U);
    assert(!infiltratr_u64_counter_delta(10U, 9U, NULL));
    double rate = 77.0;
    assert(!infiltratr_u64_counter_rate(9U, 10U, 1.0L, 1.0, &rate));
    assert(rate == 0.0);
    rate = 77.0;
    assert(!infiltratr_u64_counter_rate(10U, 9U, -1.0L, 1.0, &rate));
    assert(rate == 0.0);
    assert(!infiltratr_u64_counter_rate(10U, 9U, 1.0L, 0.0, &rate));
    assert(rate == 0.0);
    assert(!infiltratr_u64_counter_rate(10U, 9U, 1.0L, 1.0, NULL));
}

static void test_scaling_and_formatting(void)
{
    static const char *const units[] = {"B", "KB", "MB"};
    InfiltratrScaleOptions scale = INFILTRATR_SCALE_OPTIONS_INIT;
    long double scaled = 77.0L;
    size_t unit = 77U;
    assert(infiltratr_scale_quantity(1024.0L, &scale,
                                     INFILTRATR_ARRAY_LENGTH(units), &scaled, &unit));
    assert(scaled == 1.0L && unit == 1U);

    InfiltratrScaleOptions invalid = scale;
    invalid.abi_version++;
    scaled = 77.0L;
    unit = 77U;
    assert(!infiltratr_scale_quantity(1.0L, &invalid,
                                      INFILTRATR_ARRAY_LENGTH(units), &scaled, &unit));
    assert(scaled == 77.0L && unit == 77U);
    invalid = scale;
    invalid.struct_size = offsetof(InfiltratrScaleOptions, zero_below_minimum_unit) +
                          sizeof(invalid.zero_below_minimum_unit) - 1U;
    assert(!infiltratr_scale_quantity(1.0L, &invalid,
                                      INFILTRATR_ARRAY_LENGTH(units), &scaled, &unit));
    invalid = scale;
    invalid.divisor = 1.0L;
    assert(!infiltratr_scale_quantity(1.0L, &invalid,
                                      INFILTRATR_ARRAY_LENGTH(units), &scaled, &unit));

    char buffer[16] = "unchanged";
    assert(!infiltratr_format_scaled_quantity(NAN, units,
                                               INFILTRATR_ARRAY_LENGTH(units),
                                               "", &scale, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "") == 0);

    InfiltratrScaleOptions fixed = INFILTRATR_SCALE_OPTIONS_INIT;
    fixed.minimum_unit = 0U;
    fixed.maximum_unit = 0U;
    fixed.decimal_places = 0U;
    char tiny[3] = {'x', 'x', 'x'};
    assert(!infiltratr_format_scaled_quantity(1.0L, units,
                                               INFILTRATR_ARRAY_LENGTH(units),
                                               "", &fixed, tiny, sizeof(tiny)));
    assert(strcmp(tiny, "") == 0);

    InfiltratrScalarFormatOptions scalar = INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    scalar.unavailable_text = "missing";
    assert(infiltratr_format_scalar(false, 0.0L, &scalar, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "missing") == 0);
    scalar.struct_size = offsetof(InfiltratrScalarFormatOptions, unavailable_text) +
                         sizeof(scalar.unavailable_text) - 1U;
    infiltratr_copy_string(buffer, sizeof(buffer), "old");
    assert(!infiltratr_format_scalar(true, 1.0L, &scalar, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "") == 0);

    scalar = (InfiltratrScalarFormatOptions)INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    scalar.clamp = true;
    scalar.minimum = 10.0L;
    scalar.maximum = 1.0L;
    assert(!infiltratr_format_scalar(true, 5.0L, &scalar, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "") == 0);
}

int main(void)
{
    test_project_info();
    test_strings();
    test_parsers();
    test_arithmetic();
    test_scaling_and_formatting();
    puts("Libraries portable API contract tests passed.");
    return 0;
}
