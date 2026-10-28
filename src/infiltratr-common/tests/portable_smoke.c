// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file portable_smoke.c
 * @brief POSIX-free regression coverage for the reusable Common core.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/core.h"
#include "infiltratr/format.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    int64_t signed_value = 0;
    uint64_t unsigned_value = 0U;
    double decimal_value = 0.0;

    assert(infiltratr_parse_i64("-42", 10U, &signed_value));
    assert(signed_value == -42);
    assert(infiltratr_parse_i64("0x7f", 0U, &signed_value));
    assert(signed_value == 127);
    assert(infiltratr_parse_i64("-9223372036854775808", 10U, &signed_value));
    assert(signed_value == INT64_MIN);
    assert(!infiltratr_parse_i64("9223372036854775808", 10U, &signed_value));

    assert(infiltratr_parse_u64_range("42", 10U, 10U, 50U, &unsigned_value));
    assert(unsigned_value == 42U);
    assert(!infiltratr_parse_u64_range("9", 10U, 10U, 50U, &unsigned_value));
    assert(infiltratr_parse_i64_range("-5", 10U, -10, 0, &signed_value));
    assert(!infiltratr_parse_i64_range("-11", 10U, -10, 0, &signed_value));
    assert(infiltratr_parse_double_range("12.5", 0.0, 20.0, &decimal_value));
    assert(decimal_value == 12.5);
    assert(!infiltratr_parse_double_range("20.5", 0.0, 20.0, &decimal_value));

    uint64_t checked_sum = 17U;
    assert(infiltratr_u64_add_checked(20U, 22U, &checked_sum));
    assert(checked_sum == 42U);
    checked_sum = 17U;
    assert(!infiltratr_u64_add_checked(UINT64_MAX, 1U, &checked_sum));
    assert(checked_sum == 17U);

    char text[64];
    InfiltratrScalarFormatOptions options = INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    options.decimal_places = 2U;
    options.prefix = "$";
    options.suffix = " AUD";
    assert(infiltratr_format_scalar(true, 12.5L, &options, text, sizeof(text)));
    assert(strcmp(text, "$12.50 AUD") == 0);

    options.prefix = "";
    options.suffix = "%";
    options.decimal_places = 0U;
    options.clamp = true;
    options.minimum = 0.0L;
    options.maximum = 100.0L;
    assert(infiltratr_format_scalar(true, 150.0L, &options, text, sizeof(text)));
    assert(strcmp(text, "100%") == 0);
    assert(infiltratr_format_scalar(false, 0.0L, &options, text, sizeof(text)));
    assert(strcmp(text, "N/A") == 0);

    return 0;
}
