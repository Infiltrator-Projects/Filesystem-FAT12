// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file token_smoke.c
 * @brief Regression coverage for allocation-free numeric token parsing.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/token.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_decimal_token(void)
{
    const char *cursor = "  12345 rest";
    uint64_t value = 0U;
    assert(infiltratr_parse_u64_token(&cursor, 10U, &value));
    assert(value == 12345U);
    assert(strcmp(cursor, " rest") == 0);
}

static void test_base_detection(void)
{
    const char *cursor = "0xff,tail";
    uint64_t value = 0U;
    assert(infiltratr_parse_u64_token(&cursor, 0U, &value));
    assert(value == 255U);
    assert(strcmp(cursor, ",tail") == 0);
}

static void test_double_token(void)
{
    const char *text = "  -12.5e2+tail";
    const char *cursor = text;
    double value = 0.0;
    assert(infiltratr_parse_double_token(&cursor, true, &value));
    assert(value == -1250.0);
    assert(strcmp(cursor, "+tail") == 0);

    text = "3.25*2";
    cursor = text;
    value = 0.0;
    assert(infiltratr_parse_double_token(&cursor, false, &value));
    assert(value == 3.25);
    assert(strcmp(cursor, "*2") == 0);

    text = "-1";
    cursor = text;
    value = 77.0;
    assert(!infiltratr_parse_double_token(&cursor, false, &value));
    assert(cursor == text);
    assert(value == 77.0);

    text = "1e+";
    cursor = text;
    assert(!infiltratr_parse_double_token(&cursor, true, &value));
    assert(cursor == text);
    assert(value == 77.0);

    text = "1e999999";
    cursor = text;
    assert(!infiltratr_parse_double_token(&cursor, true, &value));
    assert(cursor == text);
    assert(value == 77.0);
}

static void test_failure_preserves_outputs(void)
{
    const char *text = "-1";
    const char *cursor = text;
    uint64_t value = 77U;
    assert(!infiltratr_parse_u64_token(&cursor, 10U, &value));
    assert(cursor == text);
    assert(value == 77U);

    text = "184467440737095516160";
    cursor = text;
    assert(!infiltratr_parse_u64_token(&cursor, 10U, &value));
    assert(cursor == text);
    assert(value == 77U);

    text = "  ";
    cursor = text;
    assert(!infiltratr_parse_u64_token(&cursor, 10U, &value));
    assert(cursor == text);
    assert(value == 77U);

    assert(!infiltratr_parse_u64_token(NULL, 10U, &value));
    assert(!infiltratr_parse_u64_token(&cursor, 1U, &value));
    assert(!infiltratr_parse_u64_token(&cursor, 37U, &value));
}

int main(void)
{
    test_decimal_token();
    test_base_detection();
    test_double_token();
    test_failure_preserves_outputs();
    puts("Libraries token tests passed.");
    return 0;
}
