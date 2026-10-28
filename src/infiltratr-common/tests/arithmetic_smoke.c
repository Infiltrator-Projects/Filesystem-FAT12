// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file arithmetic_smoke.c
 * @brief Regression coverage for dependency-free arithmetic helpers.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/arithmetic.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void test_floor_divmod(void)
{
    int64_t quotient = 77;
    int64_t remainder = 77;

    assert(infiltratr_i64_floor_divmod(7, 3, &quotient, &remainder));
    assert(quotient == 2 && remainder == 1);

    assert(infiltratr_i64_floor_divmod(-7, 3, &quotient, &remainder));
    assert(quotient == -3 && remainder == 2);

    assert(infiltratr_i64_floor_divmod(INT64_MIN, INT64_MAX,
                                       &quotient, &remainder));
    assert(quotient == -2 && remainder == INT64_MAX - 1);

    quotient = 77;
    remainder = 77;
    assert(!infiltratr_i64_floor_divmod(1, 0, &quotient, &remainder));
    assert(quotient == 77 && remainder == 77);
    assert(!infiltratr_i64_floor_divmod(1, -1, &quotient, &remainder));
    assert(quotient == 77 && remainder == 77);
    assert(!infiltratr_i64_floor_divmod(1, 1, NULL, NULL));

    assert(infiltratr_i64_floor_divmod(-1, 7, &quotient, NULL));
    assert(quotient == -1);
    assert(infiltratr_i64_floor_divmod(-1, 7, NULL, &remainder));
    assert(remainder == 6);
}

static void test_signed_checked_and_saturating(void)
{
    int64_t result = 77;

    assert(infiltratr_i64_add_checked(40, 2, &result) && result == 42);
    result = 77;
    assert(!infiltratr_i64_add_checked(INT64_MAX, 1, &result));
    assert(result == 77);
    assert(!infiltratr_i64_add_checked(INT64_MIN, -1, &result));
    assert(result == 77);
    assert(!infiltratr_i64_add_checked(1, 1, NULL));

    assert(infiltratr_i64_multiply_checked(7, -6, &result) && result == -42);
    result = 77;
    assert(!infiltratr_i64_multiply_checked(INT64_MAX, 2, &result));
    assert(result == 77);
    assert(!infiltratr_i64_multiply_checked(INT64_MIN, -1, &result));
    assert(result == 77);
    assert(infiltratr_i64_multiply_checked(INT64_MIN, 1, &result));
    assert(result == INT64_MIN);
    assert(!infiltratr_i64_multiply_checked(1, 1, NULL));

    assert(infiltratr_i64_add_saturating(10, 3) == 13);
    assert(infiltratr_i64_add_saturating(INT64_MAX, 1) == INT64_MAX);
    assert(infiltratr_i64_add_saturating(INT64_MIN, -1) == INT64_MIN);

    assert(infiltratr_i64_subtract_saturating(10, 3) == 7);
    assert(infiltratr_i64_subtract_saturating(-10, -3) == -7);
    assert(infiltratr_i64_subtract_saturating(INT64_MIN, 1) == INT64_MIN);
    assert(infiltratr_i64_subtract_saturating(INT64_MAX, -1) == INT64_MAX);
    assert(infiltratr_i64_subtract_saturating(INT64_MIN, -1) ==
           INT64_MIN + 1);
    assert(infiltratr_i64_subtract_saturating(INT64_MAX, 1) ==
           INT64_MAX - 1);

    assert(infiltratr_i64_multiply_saturating(7, -6) == -42);
    assert(infiltratr_i64_multiply_saturating(0, INT64_MIN) == 0);
    assert(infiltratr_i64_multiply_saturating(INT64_MAX, 2) == INT64_MAX);
    assert(infiltratr_i64_multiply_saturating(INT64_MIN, -1) == INT64_MAX);
    assert(infiltratr_i64_multiply_saturating(INT64_MIN, 2) == INT64_MIN);
    assert(infiltratr_i64_multiply_saturating(-2, INT64_MAX) == INT64_MIN);
}

static void test_checked_sizes(void)
{
    size_t result = 99U;
    assert(infiltratr_size_add_checked(7U, 5U, &result));
    assert(result == 12U);

    result = 99U;
    assert(!infiltratr_size_add_checked(SIZE_MAX, 1U, &result));
    assert(result == 99U);
    assert(!infiltratr_size_add_checked(1U, 1U, NULL));

    assert(infiltratr_size_multiply_checked(7U, 5U, &result));
    assert(result == 35U);
    result = 99U;
    assert(!infiltratr_size_multiply_checked(SIZE_MAX, 2U, &result));
    assert(result == 99U);
    assert(!infiltratr_size_multiply_checked(1U, 1U, NULL));
}

static void test_array_reserve(void)
{
    uint32_t *items = NULL;
    size_t capacity = 0U;
    assert(infiltratr_array_reserve((void **)&items, &capacity,
                                    sizeof(*items), 5U, 4U));
    assert(items != NULL);
    assert(capacity >= 5U);
    items[0] = 17U;
    items[4] = 23U;

    const size_t retained_capacity = capacity;
    uint32_t *retained_pointer = items;
    assert(infiltratr_array_reserve((void **)&items, &capacity,
                                    sizeof(*items), 2U, 4U));
    assert(items == retained_pointer);
    assert(capacity == retained_capacity);
    assert(items[0] == 17U && items[4] == 23U);

    assert(!infiltratr_array_reserve((void **)&items, &capacity,
                                     0U, capacity + 1U, 4U));
    assert(items == retained_pointer);
    assert(capacity == retained_capacity);

    void *overflow_items = NULL;
    size_t overflow_capacity = 0U;
    assert(!infiltratr_array_reserve(&overflow_items, &overflow_capacity,
                                     SIZE_MAX, 2U, 1U));
    assert(overflow_items == NULL);
    assert(overflow_capacity == 0U);

    void *inconsistent = NULL;
    size_t inconsistent_capacity = 8U;
    assert(!infiltratr_array_reserve(&inconsistent, &inconsistent_capacity,
                                     1U, 9U, 8U));
    assert(inconsistent == NULL);
    assert(inconsistent_capacity == 8U);

    free(items);
}

int main(void)
{
    test_floor_divmod();
    test_signed_checked_and_saturating();
    test_checked_sizes();
    test_array_reserve();
    puts("Libraries arithmetic tests passed.");
    return 0;
}
