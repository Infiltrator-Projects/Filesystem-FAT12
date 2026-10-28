// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file timing_smoke.c
 * @brief Contract tests for portable elapsed, periodic and fixed-step timing policy.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/timing.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

static void test_elapsed_and_continuous_periods(void)
{
    long double remaining = 0.0L;

    assert(!infiltratr_interval_due(10.5, 10.0, 1.0));
    assert(infiltratr_interval_due(11.0, 10.0, 1.0));
    assert(infiltratr_interval_due(10.0, 0.0, 1.0));
    assert(infiltratr_interval_due(9.0, 10.0, 1.0));
    assert(infiltratr_interval_due(NAN, 10.0, 1.0));
    assert(infiltratr_interval_due(10.0, 10.0, 0.0));

    assert(infiltratr_period_remaining(0.0L, 60.0L, &remaining));
    assert(fabsl(remaining - 60.0L) < 1.0e-12L);
    assert(infiltratr_period_remaining(59.5L, 60.0L, &remaining));
    assert(fabsl(remaining - 0.5L) < 1.0e-12L);
    assert(infiltratr_period_remaining(-1.0L, 60.0L, &remaining));
    assert(fabsl(remaining - 1.0L) < 1.0e-12L);
    remaining = 77.0L;
    assert(!infiltratr_period_remaining(1.0L, 0.0L, &remaining));
    assert(remaining == 77.0L);
}

static void test_fixed_step_scheduler(void)
{
    InfiltratrFixedStepScheduler scheduler = {0};
    InfiltratrFixedStepResult result = {0};
    long double alpha = -1.0L;
    uint64_t total_steps = 0U;
    uint64_t i;

    assert(!infiltratr_fixed_step_configure(NULL, 1000000000U, 60U,
                                            250000000U, 8U));
    assert(!infiltratr_fixed_step_configure(&scheduler, 60U, 61U, 15U, 8U));
    assert(infiltratr_fixed_step_configure(&scheduler, 1000000000U, 60U,
                                           250000000U, 8U));

    /* First sample only establishes the monotonic baseline. */
    assert(infiltratr_fixed_step_advance(&scheduler, 1000000000U, &result));
    assert(result.steps_to_run == 0U);
    assert(result.phase_numerator == 0U);
    assert(!result.clock_reset);

    /* 16,666,666 ns is just short of one exact 60 Hz step. */
    assert(infiltratr_fixed_step_advance(&scheduler, 1016666666U, &result));
    assert(result.steps_to_run == 0U);
    assert(result.phase_numerator == 999999960U);

    /* One more nanosecond crosses the rational boundary exactly. */
    assert(infiltratr_fixed_step_advance(&scheduler, 1016666667U, &result));
    assert(result.steps_to_run == 1U);
    assert(result.phase_numerator == 20U);

    /* A further 33,333,333 ns yields exactly two more steps and zero phase. */
    assert(infiltratr_fixed_step_advance(&scheduler, 1050000000U, &result));
    assert(result.steps_to_run == 2U);
    assert(result.phase_numerator == 0U);
    assert(infiltratr_fixed_step_alpha(&scheduler, &alpha));
    assert(alpha == 0.0L);

    /* One second late: clamp to 250 ms (15 steps), then cap catch-up at 8. */
    assert(infiltratr_fixed_step_advance(&scheduler, 2050000000U, &result));
    assert(result.clamped_ticks == 750000000U);
    assert(result.steps_to_run == 8U);
    assert(result.dropped_steps == 7U);
    assert(result.phase_numerator == 0U);

    /* Clock rollback is contained by resetting baseline and fractional phase. */
    assert(infiltratr_fixed_step_advance(&scheduler, 2000000000U, &result));
    assert(result.clock_reset);
    assert(result.steps_to_run == 0U);
    assert(result.phase_numerator == 0U);

    /* Exact drift test: 1000 irregularly-consumed 1 ms samples = 60 steps. */
    assert(infiltratr_fixed_step_configure(&scheduler, 1000000000U, 60U,
                                           250000000U, 8U));
    assert(infiltratr_fixed_step_advance(&scheduler, 0U, &result));
    for (i = 1U; i <= 1000U; ++i) {
        assert(infiltratr_fixed_step_advance(&scheduler, i * 1000000U, &result));
        total_steps += result.steps_to_run;
        assert(result.dropped_steps == 0U);
        assert(result.clamped_ticks == 0U);
    }
    assert(total_steps == 60U);
    assert(scheduler.phase_numerator == 0U);

    assert(infiltratr_fixed_step_reset(&scheduler, 5000000000U));
    assert(scheduler.initialized);
    assert(scheduler.previous_tick == 5000000000U);
    assert(scheduler.phase_numerator == 0U);
}

static void test_exact_integer_periods(void)
{
    uint64_t remaining = 77U;

    assert(infiltratr_i64_period_remaining(0, 1000000, &remaining));
    assert(remaining == 1000000U);
    assert(infiltratr_i64_period_remaining(999999, 1000000, &remaining));
    assert(remaining == 1U);
    assert(infiltratr_i64_period_remaining(-1, 1000000, &remaining));
    assert(remaining == 1U);
    assert(infiltratr_i64_period_remaining(INT64_MIN, 1000000, &remaining));
    assert(remaining == 775808U);

    remaining = 77U;
    assert(!infiltratr_i64_period_remaining(1, 0, &remaining));
    assert(remaining == 77U);
    assert(!infiltratr_i64_period_remaining(1, 1, NULL));
}

static void test_exact_cycle_partitions(void)
{
    const uint64_t day_microseconds = 86400000000ULL;
    uint64_t index = 77U;
    uint64_t until_next = 77U;

    /* Calendar's ordinary civil-day partition takes the checked direct path. */
    assert(infiltratr_cycle_partition_u64(
        0U, day_microseconds, 100000U, &index, &until_next));
    assert(index == 0U);
    assert(until_next == 864000U);

    assert(infiltratr_cycle_partition_u64(
        863999U, day_microseconds, 100000U, &index, &until_next));
    assert(index == 0U);
    assert(until_next == 1U);

    assert(infiltratr_cycle_partition_u64(
        864000U, day_microseconds, 100000U, &index, &until_next));
    assert(index == 1U);
    assert(until_next == 864000U);

    assert(infiltratr_cycle_partition_u64(
        0U, day_microseconds, 65536U, &index, &until_next));
    assert(index == 0U);
    assert(until_next == 1318360U);

    /* This product cannot fit in uint64_t and exercises the wide fallback. */
    assert(infiltratr_cycle_partition_u64(
        UINT64_MAX - 1U, UINT64_MAX, UINT64_MAX, &index, &until_next));
    assert(index == UINT64_MAX - 1U);
    assert(until_next == 1U);

    assert(infiltratr_cycle_partition_u64(
        UINT64_MAX, UINT64_MAX, UINT64_MAX, &index, &until_next));
    assert(index == 0U);
    assert(until_next == 1U);

    index = 77U;
    assert(infiltratr_cycle_partition_u64(
        1U, 2U, UINT64_MAX, &index, NULL));
    assert(index == UINT64_MAX / 2U);
    until_next = 77U;
    assert(infiltratr_cycle_partition_u64(
        1U, 2U, UINT64_MAX, NULL, &until_next));
    assert(until_next == 1U);

    index = 77U;
    until_next = 77U;
    assert(!infiltratr_cycle_partition_u64(
        1U, 0U, 2U, &index, &until_next));
    assert(index == 77U && until_next == 77U);
    assert(!infiltratr_cycle_partition_u64(
        1U, 2U, 0U, &index, &until_next));
    assert(index == 77U && until_next == 77U);
    assert(!infiltratr_cycle_partition_u64(1U, 2U, 2U, NULL, NULL));
}

static void test_delay_conversions(void)
{
    uint64_t milliseconds = 0U;

    assert(infiltratr_microseconds_to_milliseconds_ceil(1U, &milliseconds));
    assert(milliseconds == 1U);
    assert(infiltratr_microseconds_to_milliseconds_ceil(1000U, &milliseconds));
    assert(milliseconds == 1U);
    assert(infiltratr_microseconds_to_milliseconds_ceil(1001U, &milliseconds));
    assert(milliseconds == 2U);
    assert(infiltratr_microseconds_to_milliseconds_ceil(
        UINT64_MAX, &milliseconds));
    assert(milliseconds == UINT64_MAX / 1000U + 1U);
    milliseconds = 77U;
    assert(!infiltratr_microseconds_to_milliseconds_ceil(0U, &milliseconds));
    assert(milliseconds == 77U);

    assert(infiltratr_seconds_to_milliseconds_ceil(
        0.0001L, &milliseconds));
    assert(milliseconds == 1U);
    assert(infiltratr_seconds_to_milliseconds_ceil(
        1.0001L, &milliseconds));
    assert(milliseconds == 1001U);
    milliseconds = 77U;
    assert(!infiltratr_seconds_to_milliseconds_ceil(0.0L, &milliseconds));
    assert(milliseconds == 77U);
}

static void test_deadline_advance(void)
{
    uint64_t deadline = 0U;

    assert(infiltratr_periodic_deadline_advance(100U, 50U, 10U,
                                                &deadline));
    assert(deadline == 110U);
    assert(infiltratr_periodic_deadline_advance(100U, 100U, 10U,
                                                &deadline));
    assert(deadline == 110U);
    assert(infiltratr_periodic_deadline_advance(100U, 135U, 10U,
                                                &deadline));
    assert(deadline == 140U);
    assert(infiltratr_periodic_deadline_advance(UINT64_MAX - 2U,
                                                UINT64_MAX - 3U,
                                                10U,
                                                &deadline));
    assert(deadline == UINT64_MAX);
    deadline = 77U;
    assert(!infiltratr_periodic_deadline_advance(1U, 1U, 0U, &deadline));
    assert(deadline == 77U);
}

int main(void)
{
    test_elapsed_and_continuous_periods();
    test_fixed_step_scheduler();
    test_exact_integer_periods();
    test_exact_cycle_partitions();
    test_delay_conversions();
    test_deadline_advance();
    return 0;
}
