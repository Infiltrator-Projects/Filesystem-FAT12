// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file format_smoke.c
 * @brief Regression coverage for shared metric formatting.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/format.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <string.h>

int main(void)
{
    char text[64];

    assert(strcmp(infiltratr_format_memory_gb(1610612736U, text, sizeof(text)),
                  "1.5 GB") == 0);
    assert(strcmp(infiltratr_format_disk_capacity(1536U, text, sizeof(text)),
                  "1.5 KB") == 0);
    assert(strcmp(infiltratr_format_disk_capacity(UINT64_MAX, text, sizeof(text)),
                  "16.0 EB") == 0);
    assert(strcmp(infiltratr_format_network(1000.0L, false, true,
                                            text, sizeof(text)),
                  "1.0 KB/s") == 0);
    assert(strcmp(infiltratr_format_network(125.0L, true, true,
                                            text, sizeof(text)),
                  "1.0 Kb/s") == 0);
    assert(strcmp(infiltratr_format_network_pair(1000.0L, 2000.0L, false,
                                                 text, sizeof(text)),
                  "S:1.0 R:2.0 KB/s") == 0);
    assert(strcmp(infiltratr_format_link_speed_mbps(1000.0, text, sizeof(text)),
                  "1.00 Gb/s") == 0);
    assert(strcmp(infiltratr_format_link_speed_mbps(1024.0, text, sizeof(text)),
                  "1.02 Gb/s") == 0);
    assert(strcmp(infiltratr_format_link_speed_mbps(0.0, text, sizeof(text)),
                  "N/A") == 0);
    assert(strcmp(infiltratr_format_percent(true, 52.4, text, sizeof(text)),
                  "52%") == 0);
    assert(strcmp(infiltratr_format_percent(false, 0.0, text, sizeof(text)),
                  "N/A") == 0);
    assert(strcmp(infiltratr_format_mhz(true, 3200.0, text, sizeof(text)),
                  "3200 MHz") == 0);
    assert(strcmp(infiltratr_format_ghz(true, 4.275, text, sizeof(text)),
                  "4.28 GHz") == 0);
    assert(strcmp(infiltratr_format_ghz(false, 0.0, text, sizeof(text)),
                  "N/A") == 0);
    assert(strcmp(infiltratr_format_celsius(true, 64.0, text, sizeof(text)),
                  "64 °C") == 0);
    assert(strcmp(infiltratr_format_watts(true, 12.25, text, sizeof(text)),
                  "12.2 W") == 0);
    assert(strcmp(infiltratr_format_watts(true, NAN, text, sizeof(text)),
                  "N/A") == 0);

    assert(infiltratr_format_fixed_ascii(12.375, 3U, text, sizeof(text)));
    assert(strcmp(text, "12.375") == 0);
    assert(infiltratr_format_fixed_ascii(-0.0, 3U, text, sizeof(text)));
    assert(strcmp(text, "0.000") == 0);
    assert(infiltratr_format_fixed_ascii(-1.25, 2U, text, sizeof(text)));
    assert(strcmp(text, "-1.25") == 0);
    assert(infiltratr_format_fixed_ascii(9.9996, 3U, text, sizeof(text)));
    assert(strcmp(text, "10.000") == 0);
    assert(!infiltratr_format_fixed_ascii(NAN, 3U, text, sizeof(text)));
    assert(strcmp(text, "") == 0);
    assert(!infiltratr_format_fixed_ascii(1.0, 10U, text, sizeof(text)));
    assert(strcmp(text, "") == 0);

    assert(strcmp(infiltratr_format_duration_clock(0U, text, sizeof(text)),
                  "00:00:00") == 0);
    assert(strcmp(infiltratr_format_duration_clock(3661U, text, sizeof(text)),
                  "01:01:01") == 0);
    assert(strcmp(infiltratr_format_duration_clock(90061U, text, sizeof(text)),
                  "1d 01:01:01") == 0);
    assert(strcmp(infiltratr_format_duration_compact(false, 0U, text,
                                                     sizeof(text)),
                  "N/A") == 0);
    assert(strcmp(infiltratr_format_duration_compact(true, 0U, text,
                                                     sizeof(text)),
                  "0m") == 0);
    assert(strcmp(infiltratr_format_duration_compact(true, 3661U, text,
                                                     sizeof(text)),
                  "1h 01m") == 0);
    assert(strcmp(infiltratr_format_duration_compact(true, 176460U, text,
                                                     sizeof(text)),
                  "2d 01h 01m") == 0);

    char tiny[2] = {'x', 'x'};
    assert(strcmp(infiltratr_format_percent(false, 0.0, tiny, sizeof(tiny)),
                  "") == 0);
    assert(strcmp(infiltratr_format_duration_clock(3661U, tiny, sizeof(tiny)),
                  "") == 0);
    assert(strcmp(infiltratr_format_network_pair(1000.0L, 2000.0L, false,
                                                 tiny, sizeof(tiny)),
                  "") == 0);

    return 0;
}
