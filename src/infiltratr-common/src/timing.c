// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file timing.c
 * @brief Portable elapsed, periodic and fixed-step timing policy implementation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/timing.h"

#include "infiltratr/arithmetic.h"
#include "infiltratr/core.h"

#include <math.h>
#include <stdint.h>

/*
 * Compute multiplicand * multiplier = quotient * divisor + remainder without
 * requiring a non-standard 128-bit integer type. The running addend and the
 * accumulator are each kept as quotient/remainder pairs, so neither the
 * product nor doubled remainder is ever formed in uint64_t storage.
 */
static bool u64_product_divmod(uint64_t multiplicand,
                               uint64_t multiplier,
                               uint64_t divisor,
                               uint64_t *quotient,
                               uint64_t *remainder)
{
    uint64_t add_quotient;
    uint64_t add_remainder;
    uint64_t result_quotient = 0U;
    uint64_t result_remainder = 0U;
    uint64_t factor = multiplier;

    if (divisor == 0U || (!quotient && !remainder)) {
        return false;
    }

    {
        uint64_t product = 0U;

        /*
         * Most consumer partitions fit in uint64_t (Calendar's civil-day
         * clocks are typical). Take the direct exact path first and retain the
         * quotient/remainder doubling algorithm only for true wide products.
         */
        if (infiltratr_u64_multiply_checked(
                multiplicand, multiplier, &product)) {
            if (quotient) {
                *quotient = product / divisor;
            }
            if (remainder) {
                *remainder = product % divisor;
            }
            return true;
        }
    }

    add_quotient = multiplicand / divisor;
    add_remainder = multiplicand % divisor;

    while (factor != 0U) {
        if ((factor & 1U) != 0U) {
            if (result_quotient > UINT64_MAX - add_quotient) {
                return false;
            }
            result_quotient += add_quotient;

            if (add_remainder != 0U &&
                result_remainder >= divisor - add_remainder) {
                result_remainder -= divisor - add_remainder;
                if (result_quotient == UINT64_MAX) {
                    return false;
                }
                result_quotient++;
            } else {
                result_remainder += add_remainder;
            }
        }

        factor >>= 1U;
        if (factor == 0U) {
            break;
        }

        {
            uint64_t carry = 0U;

            if (add_remainder != 0U &&
                add_remainder >= divisor - add_remainder) {
                add_remainder -= divisor - add_remainder;
                carry = 1U;
            } else {
                add_remainder += add_remainder;
            }

            if (add_quotient > (UINT64_MAX - carry) / 2U) {
                return false;
            }
            add_quotient = add_quotient * 2U + carry;
        }
    }

    if (quotient) {
        *quotient = result_quotient;
    }
    if (remainder) {
        *remainder = result_remainder;
    }
    return true;
}

bool infiltratr_interval_due(double now, double last, double interval)
{
    return !isfinite(now) || !isfinite(last) || !isfinite(interval) ||
           last <= 0.0 || interval <= 0.0 || now < last ||
           now - last >= interval;
}

bool infiltratr_fixed_step_configure(InfiltratrFixedStepScheduler *scheduler,
                                     uint64_t ticks_per_second,
                                     uint64_t steps_per_second,
                                     uint64_t max_elapsed_ticks,
                                     uint64_t max_steps_per_update)
{
    InfiltratrFixedStepScheduler configured;

    if (scheduler == NULL || ticks_per_second == 0U || steps_per_second == 0U ||
        max_elapsed_ticks == 0U || max_steps_per_update == 0U ||
        steps_per_second > ticks_per_second) {
        return false;
    }

    configured.ticks_per_second = ticks_per_second;
    configured.steps_per_second = steps_per_second;
    configured.max_elapsed_ticks = max_elapsed_ticks;
    configured.max_steps_per_update = max_steps_per_update;
    configured.previous_tick = 0U;
    configured.phase_numerator = 0U;
    configured.initialized = false;
    *scheduler = configured;
    return true;
}

bool infiltratr_fixed_step_reset(InfiltratrFixedStepScheduler *scheduler,
                                 uint64_t now_tick)
{
    if (scheduler == NULL || scheduler->ticks_per_second == 0U ||
        scheduler->steps_per_second == 0U || scheduler->max_elapsed_ticks == 0U ||
        scheduler->max_steps_per_update == 0U ||
        scheduler->steps_per_second > scheduler->ticks_per_second) {
        return false;
    }

    scheduler->previous_tick = now_tick;
    scheduler->phase_numerator = 0U;
    scheduler->initialized = true;
    return true;
}

bool infiltratr_fixed_step_advance(InfiltratrFixedStepScheduler *scheduler,
                                   uint64_t now_tick,
                                   InfiltratrFixedStepResult *result)
{
    InfiltratrFixedStepResult next = {0U, 0U, 0U, 0U, false};
    uint64_t elapsed;
    uint64_t used_elapsed;
    uint64_t whole_steps = 0U;
    uint64_t elapsed_phase = 0U;
    uint64_t carry = 0U;
    uint64_t due_steps;
    uint64_t new_phase;

    if (scheduler == NULL || result == NULL || scheduler->ticks_per_second == 0U ||
        scheduler->steps_per_second == 0U || scheduler->max_elapsed_ticks == 0U ||
        scheduler->max_steps_per_update == 0U ||
        scheduler->steps_per_second > scheduler->ticks_per_second ||
        scheduler->phase_numerator >= scheduler->ticks_per_second) {
        return false;
    }

    if (!scheduler->initialized) {
        scheduler->previous_tick = now_tick;
        scheduler->phase_numerator = 0U;
        scheduler->initialized = true;
        *result = next;
        return true;
    }

    if (now_tick < scheduler->previous_tick) {
        scheduler->previous_tick = now_tick;
        scheduler->phase_numerator = 0U;
        next.clock_reset = true;
        *result = next;
        return true;
    }

    elapsed = now_tick - scheduler->previous_tick;
    scheduler->previous_tick = now_tick;
    used_elapsed = elapsed;
    if (used_elapsed > scheduler->max_elapsed_ticks) {
        next.clamped_ticks = used_elapsed - scheduler->max_elapsed_ticks;
        used_elapsed = scheduler->max_elapsed_ticks;
    }

    if (!u64_product_divmod(used_elapsed,
                            scheduler->steps_per_second,
                            scheduler->ticks_per_second,
                            &whole_steps,
                            &elapsed_phase)) {
        return false;
    }

    if (elapsed_phase != 0U &&
        scheduler->phase_numerator >= scheduler->ticks_per_second - elapsed_phase) {
        new_phase = scheduler->phase_numerator -
                    (scheduler->ticks_per_second - elapsed_phase);
        carry = 1U;
    } else {
        new_phase = scheduler->phase_numerator + elapsed_phase;
    }

    if (whole_steps == UINT64_MAX && carry != 0U) {
        return false;
    }
    due_steps = whole_steps + carry;

    if (due_steps > scheduler->max_steps_per_update) {
        next.steps_to_run = scheduler->max_steps_per_update;
        next.dropped_steps = due_steps - scheduler->max_steps_per_update;
    } else {
        next.steps_to_run = due_steps;
    }

    scheduler->phase_numerator = new_phase;
    next.phase_numerator = new_phase;
    *result = next;
    return true;
}

bool infiltratr_fixed_step_alpha(const InfiltratrFixedStepScheduler *scheduler,
                                 long double *alpha)
{
    if (scheduler == NULL || alpha == NULL || scheduler->ticks_per_second == 0U ||
        scheduler->phase_numerator >= scheduler->ticks_per_second) {
        return false;
    }

    *alpha = (long double)scheduler->phase_numerator /
             (long double)scheduler->ticks_per_second;
    return true;
}

bool infiltratr_period_remaining(long double position, long double period,
                                 long double *remaining)
{
    long double phase;
    long double result;

    if (remaining == NULL || !isfinite(position) || !isfinite(period) ||
        period <= 0.0L) {
        return false;
    }

    phase = fmodl(position, period);
    if (phase < 0.0L) {
        phase += period;
    }

    result = period - phase;
    if (result <= 0.0L || result > period) {
        result = period;
    }
    *remaining = result;
    return true;
}

bool infiltratr_i64_period_remaining(int64_t position, int64_t period,
                                     uint64_t *remaining)
{
    int64_t phase = 0;

    if (remaining == NULL || period <= 0) {
        return false;
    }
    if (!infiltratr_i64_floor_divmod(position, period, NULL, &phase)) {
        return false;
    }

    *remaining = (uint64_t)(period - phase);
    return true;
}

bool infiltratr_cycle_partition_u64(uint64_t position,
                                    uint64_t cycle_length,
                                    uint64_t partition_count,
                                    uint64_t *partition_index,
                                    uint64_t *units_until_next)
{
    uint64_t index = 0U;
    uint64_t scaled_remainder = 0U;
    uint64_t distance_numerator;
    uint64_t whole_units;
    const uint64_t phase = cycle_length != 0U ? position % cycle_length : 0U;

    if (cycle_length == 0U || partition_count == 0U ||
        (!partition_index && !units_until_next)) {
        return false;
    }

    if (!u64_product_divmod(phase, partition_count, cycle_length,
                            &index, &scaled_remainder)) {
        return false;
    }

    distance_numerator = cycle_length - scaled_remainder;
    whole_units = distance_numerator / partition_count;
    if (distance_numerator % partition_count != 0U) {
        whole_units++;
    }

    if (partition_index) {
        *partition_index = index;
    }
    if (units_until_next) {
        *units_until_next = whole_units;
    }
    return true;
}

bool infiltratr_microseconds_to_milliseconds_ceil(uint64_t microseconds,
                                                   uint64_t *milliseconds)
{
    uint64_t result;

    if (milliseconds == NULL || microseconds == 0U) {
        return false;
    }

    result = microseconds / 1000U;
    if (microseconds % 1000U != 0U) {
        result++;
    }
    *milliseconds = result;
    return true;
}

bool infiltratr_seconds_to_milliseconds_ceil(long double seconds,
                                              uint64_t *milliseconds)
{
    long double value;

    if (milliseconds == NULL || !isfinite(seconds) || seconds <= 0.0L) {
        return false;
    }

    value = ceill(seconds * 1000.0L);
    if (value < 1.0L) {
        value = 1.0L;
    }
    if (value >= (long double)UINT64_MAX) {
        *milliseconds = UINT64_MAX;
        return true;
    }

    *milliseconds = (uint64_t)value;
    return true;
}

bool infiltratr_periodic_deadline_advance(uint64_t deadline,
                                          uint64_t now,
                                          uint64_t interval,
                                          uint64_t *next_deadline)
{
    uint64_t result;

    if (next_deadline == NULL || interval == 0U) {
        return false;
    }

    if (deadline > now) {
        result = infiltratr_u64_add_saturating(deadline, interval);
    } else {
        const uint64_t late_by = now - deadline;
        const uint64_t steps = infiltratr_u64_add_saturating(
            late_by / interval, 1U);
        const uint64_t advance =
            infiltratr_u64_multiply_saturating(interval, steps);

        result = infiltratr_u64_add_saturating(deadline, advance);
    }

    *next_deadline = result;
    return true;
}
