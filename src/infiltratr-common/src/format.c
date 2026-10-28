// SPDX-License-Identifier: GPL-3.0-or-later
/** @file format.c @brief Dependency-free shared metric formatting implementation. */
#include "infiltratr/format.h"
#include "infiltratr/core.h"

#include <math.h>
#include <stdio.h>

#define INFILTRATR_SCALAR_FORMAT_OPTIONS_ABI1_SIZE \
    (offsetof(InfiltratrScalarFormatOptions, unavailable_text) + \
     sizeof(((InfiltratrScalarFormatOptions *)0)->unavailable_text))

static bool formatting_complete(char *buffer, size_t size, int written)
{
    if (!buffer || size == 0U) return false;
    if (written < 0 || (size_t)written >= size) {
        buffer[0] = '\0';
        return false;
    }
    return true;
}

static char *unavailable(char *buffer, size_t size)
{
    if (!buffer || size == 0U) return buffer;
    (void)formatting_complete(buffer, size, snprintf(buffer, size, "N/A"));
    return buffer;
}

bool infiltratr_format_scalar(bool available, long double value,
                              const InfiltratrScalarFormatOptions *options,
                              char *buffer, size_t size)
{
    if (buffer && size > 0U) buffer[0] = '\0';
    if (!options || options->struct_size < INFILTRATR_SCALAR_FORMAT_OPTIONS_ABI1_SIZE ||
        options->abi_version != INFILTRATR_SCALAR_FORMAT_OPTIONS_ABI ||
        !buffer || size == 0U || options->decimal_places > 9U)
        return false;

    const char *prefix = options->prefix ? options->prefix : "";
    const char *suffix = options->suffix ? options->suffix : "";
    const char *missing = options->unavailable_text ? options->unavailable_text : "N/A";
    if (!available || !isfinite(value)) {
        const int written = snprintf(buffer, size, "%s", missing);
        return formatting_complete(buffer, size, written);
    }
    if (options->clamp) {
        if (isnan(options->minimum) || isnan(options->maximum) ||
            options->minimum > options->maximum)
            return false;
        if (value < options->minimum) value = options->minimum;
        if (value > options->maximum) value = options->maximum;
    }
    const int written = snprintf(buffer, size, "%s%.*Lf%s", prefix,
                                 (int)options->decimal_places, value, suffix);
    return formatting_complete(buffer, size, written);
}

bool infiltratr_format_fixed_ascii(double value, unsigned int decimal_places,
                                   char *buffer, size_t size)
{
    if (buffer && size > 0U) buffer[0] = '\0';
    if (!buffer || size == 0U || !isfinite(value) || decimal_places > 9U)
        return false;

    uint64_t scale = 1U;
    for (unsigned int index = 0U; index < decimal_places; index++)
        scale *= 10U;

    const bool negative = signbit(value) && value != 0.0;
    const long double magnitude = fabsl((long double)value);
    const long double whole_value = floorl(magnitude);
    if (whole_value > (long double)UINT64_MAX) return false;

    uint64_t whole = (uint64_t)whole_value;
    const uint64_t fraction =
        (uint64_t)llroundl((magnitude - whole_value) * (long double)scale);
    uint64_t rendered_fraction = fraction;
    if (fraction >= scale) {
        if (whole == UINT64_MAX) return false;
        whole++;
        rendered_fraction = 0U;
    }

    const int written = decimal_places > 0U
        ? snprintf(buffer, size, "%s%llu.%0*llu",
                   negative ? "-" : "",
                   (unsigned long long)whole, (int)decimal_places,
                   (unsigned long long)rendered_fraction)
        : snprintf(buffer, size, "%s%llu",
                   negative ? "-" : "", (unsigned long long)whole);
    return formatting_complete(buffer, size, written);
}

static InfiltratrScaleOptions fixed_scale(size_t unit, unsigned int decimals)
{
    InfiltratrScaleOptions options = INFILTRATR_SCALE_OPTIONS_INIT;
    options.minimum_unit = unit;
    options.maximum_unit = unit;
    options.decimal_places = decimals;
    options.integer_threshold = 0.0L;
    options.integer_at_minimum_unit = false;
    return options;
}

char *infiltratr_format_memory_gb(uint64_t bytes, char *buffer, size_t size)
{
    static const char *const units[] = {"B", "KB", "MB", "GB", "TB"};
    const InfiltratrScaleOptions options = fixed_scale(3U, 1U);
    (void)infiltratr_format_scaled_quantity((long double)bytes, units,
                                             INFILTRATR_ARRAY_LENGTH(units),
                                             "", &options, buffer, size);
    return buffer;
}

char *infiltratr_format_disk_capacity(uint64_t bytes, char *buffer, size_t size)
{
    return infiltratr_format_bytes(bytes, buffer, size);
}

char *infiltratr_format_network(long double bytes, bool use_bits,
                                bool per_second, char *buffer, size_t size)
{
    static const char *const byte_units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    static const char *const bit_units[] = {"b", "Kb", "Mb", "Gb", "Tb", "Pb", "Eb"};
    if (!buffer || size == 0U) return buffer;
    if (!isfinite(bytes) || bytes < 0.0L) bytes = 0.0L;
    InfiltratrScaleOptions options = INFILTRATR_SCALE_OPTIONS_INIT;
    options.divisor = 1000.0L;
    options.minimum_unit = 1U;
    options.decimal_places = 1U;
    options.integer_threshold = 0.0L;
    options.integer_at_minimum_unit = false;
    options.zero_below_minimum_unit = true;
    const long double value = use_bits ? bytes * 8.0L : bytes;
    const char *const *units = use_bits ? bit_units : byte_units;
    (void)infiltratr_format_scaled_quantity(value, units,
                                             INFILTRATR_ARRAY_LENGTH(byte_units),
                                             per_second ? "/s" : "", &options,
                                             buffer, size);
    return buffer;
}

char *infiltratr_format_network_pair(long double send_bytes,
                                     long double receive_bytes, bool use_bits,
                                     char *buffer, size_t size)
{
    static const char *const byte_units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    static const char *const bit_units[] = {"b", "Kb", "Mb", "Gb", "Tb", "Pb", "Eb"};
    if (!buffer || size == 0U) return buffer;
    if (!isfinite(send_bytes) || send_bytes < 0.0L) send_bytes = 0.0L;
    if (!isfinite(receive_bytes) || receive_bytes < 0.0L) receive_bytes = 0.0L;
    long double send = use_bits ? send_bytes * 8.0L : send_bytes;
    long double receive = use_bits ? receive_bytes * 8.0L : receive_bytes;
    const long double maximum = fmaxl(send, receive);
    InfiltratrScaleOptions options = INFILTRATR_SCALE_OPTIONS_INIT;
    options.divisor = 1000.0L;
    options.minimum_unit = 1U;
    options.decimal_places = 1U;
    options.integer_threshold = 0.0L;
    options.integer_at_minimum_unit = false;
    options.zero_below_minimum_unit = true;
    long double ignored = 0.0L;
    size_t unit = 1U;
    if (!infiltratr_scale_quantity(maximum, &options,
                                   INFILTRATR_ARRAY_LENGTH(byte_units),
                                   &ignored, &unit)) {
        buffer[0] = '\0';
        return buffer;
    }
    options.minimum_unit = unit;
    options.maximum_unit = unit;
    options.zero_below_minimum_unit = false;
    long double scaled_send = 0.0L;
    long double scaled_receive = 0.0L;
    size_t selected_unit = unit;
    (void)infiltratr_scale_quantity(send, &options,
                                    INFILTRATR_ARRAY_LENGTH(byte_units),
                                    &scaled_send, &selected_unit);
    (void)infiltratr_scale_quantity(receive, &options,
                                    INFILTRATR_ARRAY_LENGTH(byte_units),
                                    &scaled_receive, &selected_unit);
    const char *const *units = use_bits ? bit_units : byte_units;
    const int written = snprintf(buffer, size, "S:%.1Lf R:%.1Lf %s/s",
                                 scaled_send, scaled_receive, units[unit]);
    (void)formatting_complete(buffer, size, written);
    return buffer;
}

char *infiltratr_format_link_speed_mbps(double megabits_per_second,
                                        char *buffer, size_t size)
{
    static const char *const units[] = {"b/s", "Kb/s", "Mb/s", "Gb/s", "Tb/s", "Pb/s", "Eb/s"};
    if (!buffer || size == 0U) return buffer;
    if (!isfinite(megabits_per_second) || megabits_per_second <= 0.0)
        return unavailable(buffer, size);
    InfiltratrScaleOptions options = INFILTRATR_SCALE_OPTIONS_INIT;
    options.divisor = 1000.0L;
    options.minimum_unit = 1U;
    options.decimal_places = 2U;
    options.integer_threshold = 0.0L;
    options.integer_at_minimum_unit = false;
    const long double bits_per_second =
        (long double)megabits_per_second * 1000.0L * 1000.0L;
    (void)infiltratr_format_scaled_quantity(bits_per_second, units,
                                             INFILTRATR_ARRAY_LENGTH(units),
                                             "", &options, buffer, size);
    return buffer;
}

char *infiltratr_format_percent(bool available, double value,
                                char *buffer, size_t size)
{
    InfiltratrScalarFormatOptions options = INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    options.decimal_places = 0U;
    options.clamp = true;
    options.minimum = 0.0L;
    options.maximum = 100.0L;
    options.suffix = "%";
    (void)infiltratr_format_scalar(available, (long double)value, &options, buffer, size);
    return buffer;
}

char *infiltratr_format_mhz(bool available, double value, char *buffer, size_t size)
{
    InfiltratrScalarFormatOptions options = INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    options.decimal_places = 0U;
    options.suffix = " MHz";
    (void)infiltratr_format_scalar(available, (long double)value, &options, buffer, size);
    return buffer;
}

char *infiltratr_format_ghz(bool available, double value, char *buffer, size_t size)
{
    InfiltratrScalarFormatOptions options = INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    options.decimal_places = 2U;
    options.suffix = " GHz";
    (void)infiltratr_format_scalar(available, (long double)value, &options, buffer, size);
    return buffer;
}

char *infiltratr_format_celsius(bool available, double value, char *buffer, size_t size)
{
    InfiltratrScalarFormatOptions options = INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    options.decimal_places = 0U;
    options.suffix = " °C";
    (void)infiltratr_format_scalar(available, (long double)value, &options, buffer, size);
    return buffer;
}

char *infiltratr_format_watts(bool available, double value, char *buffer, size_t size)
{
    InfiltratrScalarFormatOptions options = INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    options.suffix = " W";
    (void)infiltratr_format_scalar(available, (long double)value, &options, buffer, size);
    return buffer;
}

char *infiltratr_format_duration_clock(uint64_t seconds, char *buffer, size_t size)
{
    if (!buffer || size == 0U) return buffer;
    const uint64_t days = seconds / 86400U;
    seconds %= 86400U;
    const unsigned int hours = (unsigned int)(seconds / 3600U);
    const unsigned int minutes = (unsigned int)((seconds % 3600U) / 60U);
    const unsigned int remainder = (unsigned int)(seconds % 60U);
    if (days > 0U)
        (void)formatting_complete(
            buffer, size,
            snprintf(buffer, size, "%llud %02u:%02u:%02u",
                     (unsigned long long)days, hours, minutes, remainder));
    else
        (void)formatting_complete(
            buffer, size,
            snprintf(buffer, size, "%02u:%02u:%02u",
                     hours, minutes, remainder));
    return buffer;
}

char *infiltratr_format_duration_compact(bool available, uint64_t seconds,
                                         char *buffer, size_t size)
{
    if (!buffer || size == 0U) return buffer;
    if (!available) return unavailable(buffer, size);
    const uint64_t days = seconds / 86400U;
    seconds %= 86400U;
    const unsigned int hours = (unsigned int)(seconds / 3600U);
    const unsigned int minutes = (unsigned int)((seconds % 3600U) / 60U);
    if (days > 0U)
        (void)formatting_complete(
            buffer, size,
            snprintf(buffer, size, "%llud %02uh %02um",
                     (unsigned long long)days, hours, minutes));
    else if (hours > 0U)
        (void)formatting_complete(
            buffer, size,
            snprintf(buffer, size, "%uh %02um", hours, minutes));
    else
        (void)formatting_complete(
            buffer, size,
            snprintf(buffer, size, "%um", minutes));
    return buffer;
}
