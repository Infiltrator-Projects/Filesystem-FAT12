// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file format.h
 * @brief Dependency-free formatting helpers shared by Infiltrator programs.
 *
 * Convenience formatters use caller-owned buffers and never allocate. Unless a
 * function explicitly returns a boolean contract, the returned pointer is the
 * same buffer supplied by the caller; NULL/zero-sized buffers are tolerated by
 * the convenience wrappers and simply cannot receive text.
 */
#ifndef INFILTRATR_COMMON_FORMAT_H
#define INFILTRATR_COMMON_FORMAT_H

#include "infiltratr/core.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INFILTRATR_SCALAR_FORMAT_OPTIONS_ABI 1U

/**
 * Policy for rendering one optional scalar value.
 *
 * `decimal_places` is restricted to 0..9. When `clamp` is true, `minimum` and
 * `maximum` must not be NaN and must satisfy `minimum <= maximum`. NULL prefix
 * or suffix pointers are treated as empty strings; a NULL `unavailable_text`
 * selects the default `N/A` representation.
 *
 * ABI version 1 requires `struct_size` to cover every field through
 * `unavailable_text`. Trailing structure padding and future appended fields are
 * not required, permitting compatible extension within the same ABI version.
 */
typedef struct {
    size_t struct_size;
    uint32_t abi_version;
    unsigned int decimal_places;
    bool clamp;
    long double minimum;
    long double maximum;
    const char *prefix;
    const char *suffix;
    const char *unavailable_text;
} InfiltratrScalarFormatOptions;

#define INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT \
    { .struct_size = sizeof(InfiltratrScalarFormatOptions), \
      .abi_version = INFILTRATR_SCALAR_FORMAT_OPTIONS_ABI, \
      .decimal_places = 1U, \
      .clamp = false, \
      .minimum = 0.0L, \
      .maximum = 0.0L, \
      .prefix = "", \
      .suffix = "", \
      .unavailable_text = "N/A" }

/** Render one finite/available scalar according to a versioned option policy. */
bool infiltratr_format_scalar(bool available, long double value,
                              const InfiltratrScalarFormatOptions *options,
                              char *buffer, size_t size);

/**
 * Render one finite binary64 value as locale-independent fixed-point ASCII.
 *
 * Exactly `decimal_places` digits are emitted after a literal '.' when the
 * precision is non-zero. The function never consults the process locale and is
 * therefore suitable for machine-readable preferences, history and CSV data.
 * Negative zero is rendered as zero. Precision is limited to 0..9 and values
 * whose whole magnitude exceeds UINT64_MAX are rejected. On failure valid
 * output storage is cleared.
 */
bool infiltratr_format_fixed_ascii(double value, unsigned int decimal_places,
                                   char *buffer, size_t size);

/** Render bytes as a fixed one-decimal binary-scaled GB value. */
char *infiltratr_format_memory_gb(uint64_t bytes, char *buffer, size_t size);
/** Render storage bytes with Common's normal binary auto-scaling policy. */
char *infiltratr_format_disk_capacity(uint64_t bytes, char *buffer, size_t size);
/** Render byte/bit quantity or rate with decimal 1000-based network scaling. */
char *infiltratr_format_network(long double bytes, bool use_bits,
                                bool per_second, char *buffer, size_t size);
/** Render send/receive rates with one shared decimal network scale selected from the larger value. */
char *infiltratr_format_network_pair(long double send_bytes,
                                     long double receive_bytes, bool use_bits,
                                     char *buffer, size_t size);
/**
 * Render a positive Mb/s link speed using decimal 1000-based Kb/Mb/Gb
 * network scaling; unavailable input becomes N/A.
 */
char *infiltratr_format_link_speed_mbps(double megabits_per_second,
                                        char *buffer, size_t size);
/** Render an optional percentage rounded to whole percent and clamped to 0..100. */
char *infiltratr_format_percent(bool available, double value,
                                char *buffer, size_t size);
/** Render an optional whole-number MHz value. */
char *infiltratr_format_mhz(bool available, double value,
                            char *buffer, size_t size);
/** Render an optional GHz value with two fractional digits. */
char *infiltratr_format_ghz(bool available, double value,
                            char *buffer, size_t size);
/** Render an optional whole-number degrees-Celsius value. */
char *infiltratr_format_celsius(bool available, double value,
                                char *buffer, size_t size);
/** Render an optional watt value with one fractional digit. */
char *infiltratr_format_watts(bool available, double value,
                              char *buffer, size_t size);
/** Render seconds as HH:MM:SS, prefixing whole days when required. */
char *infiltratr_format_duration_clock(uint64_t seconds,
                                       char *buffer, size_t size);
/** Render an optional compact day/hour/minute duration; unavailable becomes N/A. */
char *infiltratr_format_duration_compact(bool available, uint64_t seconds,
                                         char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif
