// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file core_smoke.c
 * @brief Standalone regression coverage for Libraries.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "infiltratr/core.h"
#include "infiltratr/posix.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char text[8];
    infiltratr_copy_string(text, sizeof(text), "123456789");
    assert(strcmp(text, "1234567") == 0);

    char whitespace[] = "  value \n";
    infiltratr_trim(whitespace);
    assert(strcmp(whitespace, "value") == 0);

    assert(infiltratr_string_equal(NULL, NULL));
    assert(infiltratr_string_equal("calendar", "calendar"));
    assert(!infiltratr_string_equal("calendar", NULL));
    assert(infiltratr_string_starts_with("alpha-omega", "alpha"));
    assert(infiltratr_string_ends_with("alpha-omega", "omega"));
    assert(!infiltratr_string_ends_with("alpha-omega", "sigma"));
    assert(infiltratr_ascii_is_space(' '));
    assert(infiltratr_ascii_is_space('\n'));
    assert(!infiltratr_ascii_is_space((unsigned char)0xa0));
    assert(infiltratr_ascii_is_alpha('Z'));
    assert(infiltratr_ascii_is_alpha('q'));
    assert(!infiltratr_ascii_is_alpha('7'));
    assert(infiltratr_ascii_is_digit('7'));
    assert(!infiltratr_ascii_is_digit('x'));
    assert(infiltratr_ascii_is_alnum('7'));
    assert(infiltratr_ascii_is_alnum('x'));
    assert(!infiltratr_ascii_is_alnum('-'));
    assert(infiltratr_ascii_is_xdigit('F'));
    assert(infiltratr_ascii_is_xdigit('a'));
    assert(!infiltratr_ascii_is_xdigit('G'));
    assert(infiltratr_ascii_to_lower('Q') == 'q');
    assert(infiltratr_ascii_to_lower((unsigned char)0xc0) == (unsigned char)0xc0);
    assert(infiltratr_ascii_to_upper('q') == 'Q');
    assert(infiltratr_ascii_equal_ci("NVIDIA", "nvidia"));
    assert(!infiltratr_ascii_equal_ci("NVIDIA", "nvidia0"));
    assert(infiltratr_ascii_equal_ci(NULL, NULL));
    assert(!infiltratr_ascii_equal_ci(NULL, "x"));
    assert(infiltratr_ascii_compare_ci("Alpha", "alpha") == 0);
    assert(infiltratr_ascii_compare_ci("alpha", "Bravo") < 0);
    assert(infiltratr_ascii_compare_ci("Zulu", "beta") > 0);
    assert(infiltratr_ascii_compare_ci(NULL, NULL) == 0);
    assert(infiltratr_ascii_compare_ci(NULL, "x") < 0);
    assert(infiltratr_ascii_compare_ci("x", NULL) > 0);
    assert(infiltratr_ascii_starts_with_ci("FAT32", "fat"));
    assert(!infiltratr_ascii_starts_with_ci("ext4", "fat"));
    assert(infiltratr_ascii_contains_ci("Mint-Y-Dark", "dark"));
    assert(infiltratr_ascii_contains_ci("DARKER", "ark"));
    assert(infiltratr_ascii_contains_ci("anything", ""));
    assert(!infiltratr_ascii_contains_ci("Mint-Y", "dark"));
    assert(!infiltratr_ascii_contains_ci(NULL, "dark"));

    uint64_t stable_hash = INFILTRATR_FNV1A64_OFFSET_BASIS;
    stable_hash = infiltratr_fnv1a64_mix_text(stable_hash, "hello");
    assert(stable_hash == UINT64_C(0xa430d84680aabd0b));
    assert(infiltratr_fnv1a64_mix_text(stable_hash, NULL) == stable_hash);
    uint64_t word_hash = INFILTRATR_FNV1A64_OFFSET_BASIS;
    word_hash = infiltratr_fnv1a64_mix_u64_le(
        word_hash, UINT64_C(0x0807060504030201));
    uint64_t byte_hash = INFILTRATR_FNV1A64_OFFSET_BASIS;
    for (unsigned int byte = 1U; byte <= 8U; byte++)
        byte_hash = infiltratr_fnv1a64_mix_byte(
            byte_hash, (unsigned char)byte);
    assert(word_hash == byte_hash);

    assert(strcmp(infiltratr_build_profile_label("native"),
                  "Native / local machine compile") == 0);
    assert(strcmp(infiltratr_build_profile_label("generic"),
                  "Generic / APT package") == 0);
    assert(strcmp(infiltratr_build_profile_label("cmake"),
                  "Source / CMake build") == 0);
    assert(strcmp(infiltratr_build_profile_label("source"),
                  "Source / development build") == 0);
    assert(strcmp(infiltratr_build_profile_label("development"),
                  "Source / development build") == 0);
    assert(strcmp(infiltratr_build_profile_label("unknown"),
                  "Source / development build") == 0);
    assert(strcmp(infiltratr_build_profile_label(NULL),
                  "Source / development build") == 0);

    uint64_t parsed = 0U;
    assert(infiltratr_parse_u64("0x2a", 0U, &parsed));
    assert(parsed == 42U);
    assert(!infiltratr_parse_u64("-1", 10U, &parsed));
    assert(!infiltratr_parse_u64("18446744073709551616", 10U, &parsed));
    double parsed_double = 0.0;
    assert(infiltratr_parse_double(" -12.5 ", &parsed_double));
    assert(parsed_double == -12.5);
    assert(infiltratr_parse_double(".125e2", &parsed_double));
    assert(parsed_double == 12.5);
    assert(infiltratr_parse_double("1.25e-2", &parsed_double));
    assert(fabs(parsed_double - 0.0125) < 1e-15);
    assert(infiltratr_parse_double("-0", &parsed_double));
    assert(signbit(parsed_double));
    assert(!infiltratr_parse_double("12,5", &parsed_double));
    assert(!infiltratr_parse_double("0x1.8p+1", &parsed_double));
    assert(!infiltratr_parse_double("1e", &parsed_double));
    assert(!infiltratr_parse_double("1e999", &parsed_double));
    assert(!infiltratr_parse_double("infinity", &parsed_double));
    assert(infiltratr_clamp_double(-5.0, 0.0, 100.0) == 0.0);
    assert(infiltratr_clamp_double(105.0, 0.0, 100.0) == 100.0);
    assert(isnan(infiltratr_clamp_double(NAN, 0.0, 100.0)));

    char path[32];
    assert(infiltratr_path_concat(path, sizeof(path), "/sys/", "device"));
    assert(strcmp(path, "/sys/device") == 0);
    assert(infiltratr_path_join(path, sizeof(path), "/sys", "device"));
    assert(strcmp(path, "/sys/device") == 0);

    char temporary[] = "infiltratr-common-XXXXXX";
    const int descriptor = mkstemp(temporary);
    assert(descriptor >= 0);
    static const char number[] = "  18446744073709551615\n";
    assert(write(descriptor, number, sizeof(number) - 1U) ==
           (ssize_t)(sizeof(number) - 1U));
    assert(close(descriptor) == 0);
    assert(infiltratr_read_u64_file(temporary, &parsed));
    assert(parsed == UINT64_MAX);
    assert(infiltratr_read_u64_file_ex(temporary, &parsed) == INFILTRATR_IO_OK);
    assert(parsed == UINT64_MAX);
    assert(unlink(temporary) == 0);
    assert(infiltratr_read_u64_file_ex(temporary, &parsed) ==
           INFILTRATR_IO_NOT_FOUND);
    assert(strcmp(infiltratr_io_result_name(INFILTRATR_IO_NOT_FOUND),
                  "not-found") == 0);

    char long_temporary[] = "infiltratr-common-long-XXXXXX";
    const int long_descriptor = mkstemp(long_temporary);
    assert(long_descriptor >= 0);
    static const char long_text[] = "abcdefghijklmnopqrstuvwxyz\n";
    assert(write(long_descriptor, long_text, sizeof(long_text) - 1U) ==
           (ssize_t)(sizeof(long_text) - 1U));
    assert(close(long_descriptor) == 0);
    char short_buffer[8];
    size_t short_length = 0U;
    assert(infiltratr_read_text_file_ex(long_temporary, short_buffer,
                                        sizeof(short_buffer), &short_length) ==
           INFILTRATR_IO_TRUNCATED);
    assert(short_length == sizeof(short_buffer) - 1U);
    assert(strcmp(short_buffer, "abcdefg") == 0);
    assert(unlink(long_temporary) == 0);

    uint64_t checked_sum = 99U;
    assert(infiltratr_u64_add_checked(40U, 2U, &checked_sum));
    assert(checked_sum == 42U);
    checked_sum = 99U;
    assert(!infiltratr_u64_add_checked(UINT64_MAX, 1U, &checked_sum));
    assert(checked_sum == 99U);
    assert(!infiltratr_u64_add_checked(1U, 2U, NULL));
    assert(infiltratr_u64_add_saturating(UINT64_MAX, 1U) == UINT64_MAX);
    assert(infiltratr_u64_multiply_saturating(UINT64_MAX, 2U) == UINT64_MAX);
    assert(infiltratr_percent_u64(1U, 8U) == 12.5);

    uint64_t counter_delta = 0U;
    assert(infiltratr_u64_counter_delta(12U, 10U, &counter_delta));
    assert(counter_delta == 2U);
    counter_delta = 99U;
    assert(!infiltratr_u64_counter_delta(9U, 10U, &counter_delta));
    assert(counter_delta == 99U);

    double rate = -1.0;
    assert(infiltratr_u64_counter_rate(12U, 10U, 512.0L, 2.0, &rate));
    assert(rate == 512.0);
    assert(!infiltratr_u64_counter_rate(9U, 10U, 1.0L, 1.0, &rate));
    assert(rate == 0.0);

    char quantity[32];
    assert(strcmp(infiltratr_format_bytes(1536U, quantity, sizeof(quantity)),
                  "1.5 KB") == 0);
    assert(strcmp(infiltratr_format_rate(NAN, quantity, sizeof(quantity)),
                  "0 B/s") == 0);

    static const char *const decimal_units[] = {"B", "kB", "MB", "GB"};
    InfiltratrScaleOptions scale = INFILTRATR_SCALE_OPTIONS_INIT;
    scale.divisor = 1000.0L;
    scale.decimal_places = 2U;
    scale.integer_threshold = 0.0L;
    scale.integer_at_minimum_unit = false;
    assert(infiltratr_format_scaled_quantity(1500.0L, decimal_units,
                                             INFILTRATR_ARRAY_LENGTH(decimal_units),
                                             "/s", &scale, quantity,
                                             sizeof(quantity)));
    assert(strcmp(quantity, "1.50 kB/s") == 0);
    scale.minimum_unit = 2U;
    scale.maximum_unit = 2U;
    long double scaled = 0.0L;
    size_t unit = 0U;
    assert(infiltratr_scale_quantity(2500000.0L, &scale,
                                     INFILTRATR_ARRAY_LENGTH(decimal_units),
                                     &scaled, &unit));
    assert(unit == 2U);
    assert(fabsl(scaled - 2.5L) < 1e-15L);

    uint64_t nanoseconds = 0U;
    assert(infiltratr_monotonic_nanoseconds(&nanoseconds));
    assert(nanoseconds > 0U);
    assert(infiltratr_monotonic_seconds() > 0.0);

    const InfiltratrProjectInfo info = {
        .struct_size = sizeof(InfiltratrProjectInfo),
        .abi_version = INFILTRATR_PROJECT_INFO_ABI,
        .program_name = "Test Program",
        .executable_name = "test-program",
        .application_id = "example.test.Program",
        .version = "1.2.3",
        .source_id = "test-program-1.2.3",
        .build_profile = "test",
        .author = "Shannon Smith",
        .website = "https://github.com/The-Infiltratr",
        .license_id = "GPL-3.0-or-later",
        .comments = "Shared library test",
        .icon_name = "test-program",
        .copyright_text = "Copyright (c) 1993-2026 Shannon Smith"
    };
    assert(infiltratr_project_info_is_valid(&info));
    FILE *metadata = tmpfile();
    assert(metadata != NULL);
    assert(infiltratr_project_info_print(metadata, &info) == 0);
    rewind(metadata);
    char metadata_text[1024];
    const size_t metadata_size =
        fread(metadata_text, 1U, sizeof(metadata_text) - 1U, metadata);
    metadata_text[metadata_size] = '\0';
    assert(strstr(metadata_text,
                  "common-library=infiltratr-common-" INFILTRATR_COMMON_VERSION "\n") != NULL);
    assert(fclose(metadata) == 0);

    puts("Libraries core smoke test passed.");
    return 0;
}
