// SPDX-License-Identifier: GPL-3.0-or-later
#include "infiltratr/arithmetic.h"

#include <limits.h>
#include <stdlib.h>

bool infiltratr_i64_floor_divmod(int64_t value, int64_t divisor,
                                 int64_t *quotient, int64_t *remainder)
{
    if (divisor <= 0 || (!quotient && !remainder)) return false;
    int64_t q = value / divisor;
    int64_t r = value % divisor;
    if (r < 0) {
        q--;
        r += divisor;
    }
    if (quotient) *quotient = q;
    if (remainder) *remainder = r;
    return true;
}

bool infiltratr_i64_add_checked(int64_t left, int64_t right, int64_t *result)
{
    if (!result) return false;
    if (right > 0 && left > INT64_MAX - right) return false;
    if (right < 0 && left < INT64_MIN - right) return false;
    *result = left + right;
    return true;
}

bool infiltratr_i64_multiply_checked(int64_t left, int64_t right,
                                     int64_t *result)
{
    if (!result) return false;
    if (left == 0 || right == 0) {
        *result = 0;
        return true;
    }

    if (left > 0) {
        if (right > 0) {
            if (left > INT64_MAX / right) return false;
        } else if (right < INT64_MIN / left) {
            return false;
        }
    } else if (right > 0) {
        if (left < INT64_MIN / right) return false;
    } else if (left < INT64_MAX / right) {
        return false;
    }

    *result = left * right;
    return true;
}

int64_t infiltratr_i64_add_saturating(int64_t left, int64_t right)
{
    int64_t result = 0;
    if (infiltratr_i64_add_checked(left, right, &result)) return result;
    return right >= 0 ? INT64_MAX : INT64_MIN;
}

int64_t infiltratr_i64_subtract_saturating(int64_t left, int64_t right)
{
    if (right > 0 && left < INT64_MIN + right) return INT64_MIN;
    if (right < 0 && left > INT64_MAX + right) return INT64_MAX;
    return left - right;
}

int64_t infiltratr_i64_multiply_saturating(int64_t left, int64_t right)
{
    int64_t result = 0;
    if (infiltratr_i64_multiply_checked(left, right, &result)) return result;
    return ((left < 0) != (right < 0)) ? INT64_MIN : INT64_MAX;
}

bool infiltratr_u64_subtract_checked(uint64_t left, uint64_t right,
                                     uint64_t *result)
{
    if (!result || right > left) return false;
    *result = left - right;
    return true;
}

bool infiltratr_u64_multiply_checked(uint64_t left, uint64_t right,
                                     uint64_t *result)
{
    if (!result || (left != 0U && right > UINT64_MAX / left)) return false;
    *result = left * right;
    return true;
}

bool infiltratr_size_add_checked(size_t left, size_t right, size_t *result)
{
    if (!result || right > SIZE_MAX - left) return false;
    *result = left + right;
    return true;
}

bool infiltratr_size_multiply_checked(size_t left, size_t right,
                                      size_t *result)
{
    if (!result || (left != 0U && right > SIZE_MAX / left)) return false;
    *result = left * right;
    return true;
}

bool infiltratr_array_reserve(void **array, size_t *capacity,
                              size_t element_size, size_t required,
                              size_t initial_capacity)
{
    if (!array || !capacity || element_size == 0U ||
        (*capacity > 0U && !*array))
        return false;
    if (required <= *capacity) return true;

    size_t next = *capacity;
    if (next == 0U) next = initial_capacity > 0U ? initial_capacity : 1U;
    while (next < required) {
        if (next > SIZE_MAX / 2U) {
            next = required;
            break;
        }
        next *= 2U;
    }

    size_t bytes = 0U;
    if (!infiltratr_size_multiply_checked(next, element_size, &bytes))
        return false;
    void *grown = realloc(*array, bytes);
    if (!grown) return false;
    *array = grown;
    *capacity = next;
    return true;
}
