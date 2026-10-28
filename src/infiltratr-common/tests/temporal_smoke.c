// SPDX-License-Identifier: GPL-3.0-or-later
#include "infiltratr/temporal.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "temporal test failed: %s (%s:%d)\n", \
                    #expression, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

static void test_catalogue(void)
{
    InfiltratrClockProfile profile = INFILTRATR_CLOCK_PROFILE_SYSTEM;
    CHECK(infiltratr_clock_profile_count() == 4U);
    CHECK(infiltratr_clock_profile_from_id("decimal-10", &profile));
    CHECK(profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10);
    CHECK(strcmp(infiltratr_clock_profile_id(profile), "decimal-10") == 0);
    CHECK(strcmp(infiltratr_clock_profile_name(profile),
                  "Decimal time (10-hour day)") == 0);
    CHECK(!infiltratr_clock_profile_from_id("missing", &profile));
    CHECK(infiltratr_temporal_clock_mode_find("italian-modern") == NULL);
    CHECK(strcmp(
        infiltratr_temporal_clock_mode_find("italian-hours")->name,
        "Historical Italian hours (24 equal hours counted from sunset)") == 0);
    CHECK(strcmp(
        infiltratr_temporal_clock_mode_find("babylonian-hours")->name,
        "Renaissance European ‘Babylonian’ hours (24 equal hours counted from sunrise)") == 0);
    CHECK(strcmp(
        infiltratr_temporal_clock_mode_find("babylonian-ancient")->name,
        "Ancient Babylonian time (sunset-start day; 12 fixed bēru/day; 30 UŠ per bēru)") == 0);
    CHECK(strcmp(
        infiltratr_temporal_clock_mode_find("nuremberg-hours")->name,
        "Historical Nuremberg Great Clock (equal hours; day/night counts change on fixed dates)") == 0);
    CHECK(strcmp(
        infiltratr_temporal_clock_mode_find("roman-temporal")->name,
        "Roman seasonal time (12 unequal daylight hours; night in 4 military watches)") == 0);
    CHECK(infiltratr_temporal_clock_mode_find("roman-temporal")->supports_seconds);
    CHECK(infiltratr_temporal_clock_mode_find("chinese-time")->supports_seconds);
    CHECK(strcmp(
        infiltratr_temporal_clock_mode_find("japanese-temporal")->name,
        "Late Edo Japanese unequal hours (1797 twilight; 6 day / 6 night)") == 0);
    CHECK(strcmp(
        infiltratr_temporal_clock_mode_find("japanese-temporal-early")->name,
        "Early Edo Japanese unequal hours (sunrise/sunset; 6 day / 6 night)") == 0);
    CHECK(strcmp(
        infiltratr_temporal_clock_mode_find("solar")->name,
        "Apparent solar time (compact Sun-position approximation)") == 0);
    CHECK(strcmp(
        infiltratr_temporal_calendar_find("japanese")->name,
        "Japanese imperial era (modern Meiji–Reiwa)") == 0);
    CHECK(strcmp(
        infiltratr_temporal_calendar_find("bahai")->name,
        "Bahá’í (Badíʿ; modern Naw-Rúz from Tehran equinox)") == 0);
    CHECK(strcmp(
        infiltratr_temporal_clock_mode_find("indian-ghati")->name,
        "Indian ghaṭī time (60 per sunrise day; 1 ghaṭī = 24 min)") == 0);
    CHECK(strcmp(
        infiltratr_temporal_calendar_find("mayan")->name,
        "Maya Long Count (Goodman–Martínez–Thompson 584283 correlation)") == 0);
    CHECK(strcmp(
        infiltratr_temporal_calendar_find("positivist")->name,
        "Positivist calendar (Comte; 13 × 28-day months + festival days; Year 1 = 1789)") == 0);
    CHECK(infiltratr_temporal_clock_mode_find("nuremberg-solar") != NULL);

    for (size_t index = 0U;
         index < infiltratr_temporal_clock_mode_count(); ++index) {
        const InfiltratrTemporalClockModeInfo *info =
            infiltratr_temporal_clock_mode_at(index);

        CHECK(info != NULL);
        CHECK(info->id != NULL && info->id[0] != '\0');
        CHECK(info->name != NULL && info->name[0] != '\0');
        CHECK(infiltratr_temporal_clock_mode_find(info->id) == info);
        for (size_t other = index + 1U;
             other < infiltratr_temporal_clock_mode_count(); ++other) {
            const InfiltratrTemporalClockModeInfo *candidate =
                infiltratr_temporal_clock_mode_at(other);
            CHECK(candidate != NULL);
            CHECK(strcmp(info->id, candidate->id) != 0);
        }
    }

    for (size_t index = 0U;
         index < infiltratr_temporal_calendar_count(); ++index) {
        const InfiltratrTemporalCalendarInfo *info =
            infiltratr_temporal_calendar_at(index);

        CHECK(info != NULL);
        CHECK(info->id != NULL && info->id[0] != '\0');
        CHECK(info->name != NULL && info->name[0] != '\0');
        CHECK(infiltratr_temporal_calendar_find(info->id) == info);
        for (size_t other = index + 1U;
             other < infiltratr_temporal_calendar_count(); ++other) {
            const InfiltratrTemporalCalendarInfo *candidate =
                infiltratr_temporal_calendar_at(other);
            CHECK(candidate != NULL);
            CHECK(strcmp(info->id, candidate->id) != 0);
        }
    }

    CHECK(infiltratr_temporal_clock_mode_at(
              infiltratr_temporal_clock_mode_count()) == NULL);
    CHECK(infiltratr_temporal_calendar_at(
              infiltratr_temporal_calendar_count()) == NULL);
}

static void test_policy_round_trip(void)
{
    InfiltratrTemporalPolicy policy;
    InfiltratrTemporalPolicy parsed;
    char text[256];
    size_t length = 0U;

    CHECK(infiltratr_temporal_policy_default(&policy));
    policy.clock_profile = INFILTRATR_CLOCK_PROFILE_DECIMAL_10;
    policy.show_seconds = true;
    CHECK(infiltratr_temporal_policy_serialize(&policy, text, sizeof(text),
                                                &length));
    CHECK(length == strlen(text));
    CHECK(infiltratr_temporal_policy_parse(text, &parsed));
    CHECK(parsed.version == INFILTRATR_TEMPORAL_POLICY_VERSION);
    CHECK(parsed.clock_profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10);
    CHECK(parsed.show_seconds);
    CHECK(!infiltratr_temporal_policy_parse(
        "version=2\nclock-profile=decimal-10\n", &parsed));
}

static void test_policy_v3(void)
{
    InfiltratrTemporalPolicyV3 policy;
    InfiltratrTemporalPolicyV3 parsed;
    char text[1024];
    size_t length = 0U;

    CHECK(infiltratr_temporal_policy_v3_default(&policy));
    CHECK(policy.version == INFILTRATR_TEMPORAL_POLICY_V3_VERSION);
    CHECK(strcmp(policy.clock_mode, "standard") == 0);
    CHECK(strcmp(policy.calendar, "gregorian") == 0);

    strcpy(policy.clock_mode, "decimal");
    strcpy(policy.calendar, "egyptian-nabonassar");
    policy.show_seconds = true;
    policy.location_configured = true;
    policy.latitude = -36.39;
    policy.longitude = 145.36;

    CHECK(infiltratr_temporal_policy_v3_serialize(
        &policy, text, sizeof(text), &length));
    CHECK(length == strlen(text));
    CHECK(strstr(text, "version=3\n") != NULL);
    CHECK(strstr(text, "calendar=egyptian-nabonassar\n") != NULL);
    CHECK(strstr(text, "secondary-calendar=") == NULL);

    CHECK(infiltratr_temporal_policy_v3_parse(text, &parsed));
    CHECK(strcmp(parsed.clock_mode, "decimal") == 0);
    CHECK(strcmp(parsed.calendar, "egyptian-nabonassar") == 0);
    CHECK(parsed.show_seconds);
    CHECK(parsed.location_configured);
    CHECK(parsed.latitude == -36.39);
    CHECK(parsed.longitude == 145.36);

    policy.latitude = 90.000001;
    CHECK(!infiltratr_temporal_policy_v3_serialize(
        &policy, text, sizeof(text), &length));
    policy.latitude = -36.39;
    policy.longitude = 180.000001;
    CHECK(!infiltratr_temporal_policy_v3_serialize(
        &policy, text, sizeof(text), &length));
    policy.longitude = NAN;
    CHECK(!infiltratr_temporal_policy_v3_serialize(
        &policy, text, sizeof(text), &length));
    policy.longitude = 145.36;

    CHECK(infiltratr_temporal_policy_v3_parse(
        "version=2\n"
        "clock-mode=roman-temporal\n"
        "primary-calendar=hebrew\n"
        "secondary-calendar=gregorian\n"
        "show-seconds=true\n"
        "location-configured=false\n"
        "latitude=0.000000\n"
        "longitude=0.000000\n",
        &parsed));
    CHECK(strcmp(parsed.clock_mode, "roman-temporal") == 0);
    CHECK(strcmp(parsed.calendar, "hebrew") == 0);
    CHECK(parsed.show_seconds);

    CHECK(!infiltratr_temporal_policy_v3_parse(
        "version=3\ncalendar=none\n", &parsed));
    CHECK(!infiltratr_temporal_policy_v3_parse(
        "version=3\n"
        "clock-mode=standard\n"
        "calendar=gregorian\n"
        "show-seconds=false\n"
        "location-configured=false\n"
        "latitude=0.000000\n",
        &parsed));
    CHECK(!infiltratr_temporal_policy_v3_parse(
        "version=3\n"
        "clock-mode=standard\n"
        "clock-mode=standard-24\n"
        "calendar=gregorian\n"
        "show-seconds=false\n"
        "location-configured=false\n"
        "latitude=0.000000\n"
        "longitude=0.000000\n",
        &parsed));

    CHECK(infiltratr_temporal_policy_v3_default(&policy));
    memset(policy.clock_mode, 'x', sizeof(policy.clock_mode));
    CHECK(!infiltratr_temporal_policy_v3_serialize(
        &policy, text, sizeof(text), &length));
    CHECK(infiltratr_temporal_policy_v3_default(&policy));
    memset(policy.calendar, 'x', sizeof(policy.calendar));
    CHECK(!infiltratr_temporal_policy_v3_serialize(
        &policy, text, sizeof(text), &length));

    CHECK(infiltratr_temporal_calendar_count() == 30U);
    CHECK(infiltratr_temporal_calendar_find("none") == NULL);
}

static void test_local_microseconds_of_day(void)
{
    CHECK(infiltratr_temporal_local_microseconds_of_day(0, 0) == INT64_C(0));
    CHECK(infiltratr_temporal_local_microseconds_of_day(-1, 0) ==
          INT64_C(86399999999));
    CHECK(infiltratr_temporal_local_microseconds_of_day(0, 3600) ==
          INT64_C(3600000000));
    CHECK(infiltratr_temporal_local_microseconds_of_day(0, -3600) ==
          INT64_C(82800000000));
    CHECK(infiltratr_temporal_local_microseconds_of_day(
              INT64_MIN, INT32_MIN) == INT64_C(60297224192));
    CHECK(infiltratr_temporal_local_microseconds_of_day(
              INT64_MAX, INT32_MAX) == INT64_C(26101775807));
}

static void test_clock_formats(void)
{
    char text[64];

    CHECK(infiltratr_temporal_format_clock(
        INFILTRATR_CLOCK_PROFILE_DECIMAL_10,
        INT64_C(0), 0, true, text, sizeof(text), NULL));
    CHECK(strcmp(text, "0:00:00") == 0);

    CHECK(infiltratr_temporal_format_clock(
        INFILTRATR_CLOCK_PROFILE_DECIMAL_10,
        INT64_C(864000), 0, true, text, sizeof(text), NULL));
    CHECK(strcmp(text, "0:00:01") == 0);

    CHECK(infiltratr_temporal_format_clock(
        INFILTRATR_CLOCK_PROFILE_DECIMAL_10,
        INT64_C(43200) * INT64_C(1000000), 0, true,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "5:00:00") == 0);

    CHECK(infiltratr_temporal_format_clock(
        INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24,
        INT64_C(19) * INT64_C(3600) * INT64_C(1000000) +
            INT64_C(42) * INT64_C(60) * INT64_C(1000000),
        0, false, text, sizeof(text), NULL));
    CHECK(strcmp(text, "19:42") == 0);

    CHECK(infiltratr_temporal_format_clock(
        INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12,
        INT64_C(19) * INT64_C(3600) * INT64_C(1000000) +
            INT64_C(42) * INT64_C(60) * INT64_C(1000000),
        0, false, text, sizeof(text), NULL));
    CHECK(strcmp(text, "7:42 PM") == 0);

    CHECK(infiltratr_temporal_format_clock(
        INFILTRATR_CLOCK_PROFILE_DECIMAL_10,
        INT64_MIN, INT32_MIN, true, text, sizeof(text), NULL));
    CHECK(text[0] != '\0');
    CHECK(infiltratr_temporal_format_clock(
        INFILTRATR_CLOCK_PROFILE_DECIMAL_10,
        INT64_MAX, INT32_MAX, true, text, sizeof(text), NULL));
    CHECK(text[0] != '\0');

    CHECK(!infiltratr_temporal_format_clock(
        INFILTRATR_CLOCK_PROFILE_SYSTEM,
        0, 0, false, text, sizeof(text), NULL));
}


static void test_duration_mode_formats(void)
{
    char text[192];

    CHECK(infiltratr_temporal_format_duration_mode(
        "standard", UINT64_C(3661) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "01:01:01") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "decimal", UINT64_C(86) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "0:00:99") == 0);
    CHECK(infiltratr_temporal_format_duration_mode(
        "decimal", UINT64_C(87) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "0:01:00") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "internet", UINT64_C(87) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "@001.00") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "unix", UINT64_C(61) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "61 s") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "binary", UINT64_C(61) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "00000:000001:000001") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "hexadecimal", UINT64_C(43200) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "8000") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "julian", UINT64_C(43200) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "JD +0.50000 d") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "modified-julian", UINT64_C(43200) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        false, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "MJD +0.500 d") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "sidereal", UINT64_C(43200) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, true, -36.39, 145.36,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "12:01:58 LST") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "chinese-time", UINT64_C(7200) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        false, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "1時辰") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "chinese-ke", UINT64_C(864) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        false, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "1刻") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "indian-ghati", UINT64_C(1440) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        false, false, true, -36.39, 145.36,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "1 ghaṭī · 0 pala") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "roman-temporal", UINT64_C(3661) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, true, -36.39, 145.36,
        text, sizeof(text), NULL));
    CHECK(strstr(text, "hora") != NULL || strstr(text, "vigilia") != NULL);
    CHECK(strchr(text, ':') == NULL);
    CHECK(infiltratr_temporal_format_duration_mode(
        "roman-temporal", UINT64_C(3661) * UINT64_C(1000000),
        INT64_MIN,
        true, false, true, -36.39, 145.36,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "01:01:01 SI") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "japanese-temporal", UINT64_C(3661) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, true, -36.39, 145.36,
        text, sizeof(text), NULL));
    CHECK(strstr(text, "刻") != NULL);
    CHECK(strchr(text, ':') == NULL);
    CHECK(infiltratr_temporal_format_duration_mode(
        "japanese-temporal", UINT64_C(3661) * UINT64_C(1000000),
        INT64_MIN,
        true, false, true, -36.39, 145.36,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "01:01:01 SI") == 0);

    CHECK(infiltratr_temporal_format_duration_mode(
        "roman-temporal", UINT64_C(86400) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, true, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "1 dies") == 0);
    CHECK(infiltratr_temporal_format_duration_mode(
        "babylonian-ancient", UINT64_C(86400) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, true, 49.45, 11.08,
        text, sizeof(text), NULL));
    CHECK(strstr(text, "ūmu") != NULL);
    CHECK(infiltratr_temporal_format_duration_mode(
        "japanese-temporal", UINT64_C(86400) * UINT64_C(1000000),
        INT64_C(1789990000) * INT64_C(1000000),
        true, false, true, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strstr(text, "日") != NULL);

    for (size_t index = 0U;
         index < infiltratr_temporal_clock_mode_count(); ++index) {
        const InfiltratrTemporalClockModeInfo *info =
            infiltratr_temporal_clock_mode_at(index);
        CHECK(info != NULL);
        CHECK(infiltratr_temporal_format_duration_mode(
            info->id,
            UINT64_C(98765) * UINT64_C(1000000),
            INT64_C(1789990000) * INT64_C(1000000),
            true, false, true, -36.39, 145.36,
            text, sizeof(text), NULL));
        CHECK(text[0] != '\0');
        if (info->requires_latitude || info->requires_longitude) {
            CHECK(!infiltratr_temporal_format_duration_mode(
                info->id,
                UINT64_C(98765) * UINT64_C(1000000),
                INT64_C(1789990000) * INT64_C(1000000),
                true, false, false, -36.39, 145.36,
                text, sizeof(text), NULL));
        }
    }

    CHECK(!infiltratr_temporal_format_duration_mode(
        "roman-temporal", UINT64_C(1), INT64_C(0),
        true, false, true, 90.0001, 145.36,
        text, sizeof(text), NULL));
    CHECK(!infiltratr_temporal_format_duration_mode(
        "sidereal", UINT64_C(1), INT64_C(0),
        true, false, true, 0.0, 180.0001,
        text, sizeof(text), NULL));

    CHECK(!infiltratr_temporal_format_duration_mode(
        "missing-mode", UINT64_C(1), INT64_C(0),
        true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
}

static void test_clock_mode_formats(void)
{
    char text[192];
    size_t length = 0U;

    CHECK(!infiltratr_temporal_format_clock_mode(
        "standard", INT64_C(0), 0, true, false, false,
        0.0, 0.0, text, sizeof(text), &length));

    CHECK(infiltratr_temporal_format_clock_mode(
        "standard-24",
        INT64_C(19) * INT64_C(3600) * INT64_C(1000000) +
            INT64_C(42) * INT64_C(60) * INT64_C(1000000),
        0, false, false, false, 0.0, 0.0,
        text, sizeof(text), &length));
    CHECK(strcmp(text, "19:42") == 0);
    CHECK(length == strlen(text));

    CHECK(infiltratr_temporal_format_clock_mode(
        "decimal",
        INT64_C(43200) * INT64_C(1000000),
        0, true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "5:00:00") == 0);

    CHECK(infiltratr_temporal_format_clock_mode(
        "babylonian-ancient",
        INT64_C(1789990000) * INT64_C(1000000),
        0, true, false, true, 49.45, 11.08,
        text, sizeof(text), NULL));
    CHECK(strstr(text, "bēru") != NULL);
    CHECK(strstr(text, "UŠ") != NULL);

    CHECK(infiltratr_temporal_format_clock_mode(
        "nuremberg-hours",
        INT64_C(1789990000) * INT64_C(1000000),
        0, true, false, true, 49.45, 11.08,
        text, sizeof(text), NULL));
    CHECK(strstr(text, "NUR-D") != NULL || strstr(text, "NUR-N") != NULL);

    for (size_t index = 0U;
         index < infiltratr_temporal_clock_mode_count(); ++index) {
        const InfiltratrTemporalClockModeInfo *info =
            infiltratr_temporal_clock_mode_at(index);
        const bool standard =
            info != NULL && strcmp(info->id, "standard") == 0;

        CHECK(info != NULL);
        CHECK(infiltratr_temporal_format_clock_mode(
                  info->id,
                  INT64_C(1789990000) * INT64_C(1000000),
                  36000, true, false, true, -36.39, 145.36,
                  text, sizeof(text), NULL) != standard);
        if (!standard) {
            CHECK(text[0] != '\0');
        }
        if (info->requires_latitude || info->requires_longitude) {
            CHECK(!infiltratr_temporal_format_clock_mode(
                info->id,
                INT64_C(1789990000) * INT64_C(1000000),
                36000, true, false, false, -36.39, 145.36,
                text, sizeof(text), NULL));
        }
    }

    CHECK(infiltratr_temporal_format_clock_mode(
        "chinese-time",
        INT64_C(30) * INT64_C(60) * INT64_C(1000000),
        0, false, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "子時") == 0);
    CHECK(infiltratr_temporal_format_clock_mode(
        "chinese-time",
        INT64_C(30) * INT64_C(60) * INT64_C(1000000),
        0, true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "子正") == 0);
    CHECK(infiltratr_temporal_format_clock_mode(
        "chinese-time",
        (INT64_C(23) * INT64_C(3600) + INT64_C(30) * INT64_C(60)) *
            INT64_C(1000000),
        0, true, false, false, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strcmp(text, "子初") == 0);

    CHECK(infiltratr_temporal_format_clock_mode(
        "roman-temporal",
        INT64_C(1789990000) * INT64_C(1000000),
        36000, false, false, true, -36.39, 145.36,
        text, sizeof(text), NULL));
    CHECK(strncmp(text, "Hora ", 5U) == 0 ||
          strncmp(text, "Vigilia ", 8U) == 0);

    CHECK(infiltratr_temporal_format_clock_mode(
        "roman-temporal",
        (INT64_C(12) * INT64_C(3600) + INT64_C(30) * INT64_C(60)) *
            INT64_C(1000000),
        0, true, false, true, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strncmp(text, "Hora ", 5U) == 0);
    CHECK(strstr(text, "uncia") != NULL);
    CHECK(strchr(text, ':') == NULL);

    CHECK(!infiltratr_temporal_format_clock_mode(
        "roman-temporal",
        INT64_C(1789990000) * INT64_C(1000000),
        36000, false, false, true, -90.0001, 145.36,
        text, sizeof(text), NULL));
    CHECK(!infiltratr_temporal_format_clock_mode(
        "solar",
        INT64_C(1789990000) * INT64_C(1000000),
        36000, false, false, true, 0.0, -180.0001,
        text, sizeof(text), NULL));

    CHECK(!infiltratr_temporal_format_clock_mode(
        "missing-mode", INT64_C(0), 0, false, false, false,
        0.0, 0.0, text, sizeof(text), NULL));
}

static void test_localized_clock_modes(void)
{
    static const struct {
        const char *locale;
        bool fine;
        bool vertical;
        const char *text;
    } vectors[] = {
        { "en_AU.UTF-8", false, false, "Zǐ hour" },
        { "en-GB", true, false, "Zǐ, second half" },
        { "en", true, true, "Zǐ\nsecond half" },
        { "zh_CN.UTF-8", false, false, "子时" },
        { "zh_TW.UTF-8", false, false, "子時" },
        { "zh-Hant-TW", true, false, "子正" },
        { "fr_FR.UTF-8", true, false, "Zǐ, seconde moitié" },
        { "de_DE", true, false, "Zǐ, zweite Hälfte" },
        { "C", true, false, "Zǐ, second half" },
        { "POSIX", true, false, "Zǐ, second half" },
        { "xx_XX", true, false, "Zǐ, second half" },
        { NULL, true, false, "Zǐ, second half" }
    };
    char text[192];
    size_t length = 0;
    for (size_t index = 0; index < sizeof(vectors) / sizeof(vectors[0]); ++index) {
        CHECK(infiltratr_temporal_format_clock_mode_localized(
            vectors[index].locale, "chinese-time", INT64_C(1800000000), 0,
            vectors[index].fine, vectors[index].vertical, false, 0.0, 0.0,
            text, sizeof(text), &length));
        CHECK(strcmp(text, vectors[index].text) == 0);
        CHECK(length == strlen(text));
    }
    /* Fine presentation changes at the same native hourly boundary. */
    CHECK(infiltratr_temporal_format_clock_mode_localized("en", "chinese-time",
        -1, 0, true, false, false, 0.0, 0.0, text, sizeof(text), NULL));
    CHECK(strcmp(text, "Zǐ, first half") == 0);
    CHECK(infiltratr_temporal_format_clock_mode_localized("en", "chinese-time",
        0, 0, true, false, false, 0.0, 0.0, text, sizeof(text), NULL));
    CHECK(strcmp(text, "Zǐ, second half") == 0);
    CHECK(!infiltratr_temporal_format_clock_mode_localized("en", "chinese-time",
        0, 0, true, false, false, 0.0, 0.0, text, 4U, NULL));
    CHECK(text[0] == '\0');
    CHECK(!infiltratr_temporal_format_clock_mode_localized("en", "missing-mode",
        0, 0, true, false, false, 0.0, 0.0, text, sizeof(text), NULL));
    CHECK(infiltratr_temporal_format_clock_mode_localized("en", "roman-temporal",
        INT64_C(45000000000), 0, true, false, true, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strncmp(text, "Hour ", 5U) == 0);
    CHECK(strstr(text, "twelfth") != NULL);
    CHECK(strchr(text, ':') == NULL);
    CHECK(infiltratr_temporal_format_clock_mode_localized("fr", "roman-temporal",
        INT64_C(45000000000), 0, true, false, true, 0.0, 0.0,
        text, sizeof(text), NULL));
    CHECK(strncmp(text, "Heure ", 6U) == 0);
    CHECK(infiltratr_temporal_format_clock_mode_localized("en", "chinese-ke",
        0, 0, false, false, false, 0.0, 0.0, text, sizeof(text), NULL));
    CHECK(strcmp(text, "00 kè") == 0);
}

static void test_localized_twelve_hour_clock(void)
{
    static const struct {
        const char *locale;
        int64_t instant;
        int32_t offset;
        bool fine;
        bool vertical;
        const char *expected;
    } vectors[] = {
        { "en_AU", 0, 0, false, false, "12:00 AM" },
        { "en_AU", INT64_C(43200000000), 0, true, false, "12:00:00 PM" },
        { "ja_JP.UTF-8", 0, 0, false, false, "午前12:00" },
        { "ja", INT64_C(43200000000), 0, false, false, "午後12:00" },
        { "zh_TW.UTF-8", -1, 0, true, false, "下午11:59:59" },
        { "zh-Hant-TW", INT64_C(43200000000), -3600, false, false,
          "上午11:00" },
        { "zh_CN", INT64_C(43200000000), 0, true, true, "下午\n12\n00\n00" },
        { "ko_KR", INT64_C(46800000000), 0, false, false, "오후 1:00" },
        { "es_ES", INT64_C(46800000000), 0, false, false, "1:00 p. m." },
        { "en", 0, 0, false, true, "12\n00\nAM" },
        { "xx_XX", 0, 0, false, false, "12:00 AM" }
    };
    char text[192];
    size_t length = 0U;
    for (size_t i = 0U; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
        CHECK(infiltratr_temporal_format_clock_mode_localized(
            vectors[i].locale, "standard-12", vectors[i].instant,
            vectors[i].offset, vectors[i].fine, vectors[i].vertical,
            false, 0.0, 0.0, text, sizeof(text), &length));
        CHECK(strcmp(text, vectors[i].expected) == 0);
        CHECK(length == strlen(text));
    }
    CHECK(!infiltratr_temporal_format_clock_mode_localized("zh-TW", "standard-12",
        0, 0, false, false, false, 0.0, 0.0, text, 8U, &length));
    CHECK(text[0] == '\0');
    /* The old locale-free API retains its conventional compatibility form. */
    CHECK(infiltratr_temporal_format_clock_mode("standard-12", 0, 0,
        false, false, false, 0.0, 0.0, text, sizeof(text), NULL));
    CHECK(strcmp(text, "12:00 AM") == 0);
}

static void test_localized_edo_clock(void)
{
    static const struct {
        const char *locale;
        bool vertical;
        const char *expected;
    } vectors[] = {
        { "en_AU", false, "Mi · 四 bells" },
        { "ja_JP", false, "巳の刻 · 四つ" },
        { "ja", true, "巳の刻\n四つ" },
        { "zh_CN", false, "巳刻 · 四声" },
        { "zh_TW.UTF-8", true, "巳刻\n四聲" },
        { "fr_FR", false, "Mi · 四 coups" },
        { "de_DE", false, "Mi · 四 Schläge" },
        { "ko_KR", false, "Mi · 四타" },
        { "xx_XX", false, "Mi · 四 bells" }
    };
    static const char *const modes[] = {
        "japanese-temporal", "japanese-temporal-early"
    };
    char text[192];
    for (size_t mode = 0U; mode < sizeof(modes) / sizeof(modes[0]); ++mode) {
        for (size_t i = 0U; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
            size_t length = 0U;
            CHECK(infiltratr_temporal_format_clock_mode_localized(
                vectors[i].locale, modes[mode], INT64_C(946728000000000),
                0, false, vectors[i].vertical, true, 0.0, 0.0,
                text, sizeof(text), &length));
            CHECK(strcmp(text, vectors[i].expected) == 0);
            CHECK(length == strlen(text));
        }
        /* Branches distinguish repeated daytime/nighttime bell counts. */
        char day[192];
        CHECK(infiltratr_temporal_format_clock_mode_localized("en", modes[mode],
            INT64_C(946731600000000), 0, false, false, true, 0.0, 0.0,
            day, sizeof(day), NULL));
        CHECK(infiltratr_temporal_format_clock_mode_localized("en", modes[mode],
            INT64_C(946688400000000), 0, false, false, true, 0.0, 0.0,
            text, sizeof(text), NULL));
        CHECK(strstr(day, "九 bells") != NULL);
        CHECK(strstr(text, "九 bells") != NULL);
        CHECK(strcmp(day, text) != 0);
        CHECK(!infiltratr_temporal_format_clock_mode_localized("ja", modes[mode],
            INT64_C(946728000000000), 0, false, false, true, 0.0, 0.0,
            text, 8U, NULL));
        CHECK(text[0] == '\0');
        CHECK(infiltratr_temporal_format_clock_mode(modes[mode],
            INT64_C(946728000000000), 0, false, false, true, 0.0, 0.0,
            text, sizeof(text), NULL));
        CHECK(strcmp(text, "巳の刻 · 四つ") == 0);
    }
}

int main(void)
{
    test_catalogue();
    test_policy_round_trip();
    test_policy_v3();
    test_local_microseconds_of_day();
    test_clock_formats();
    test_duration_mode_formats();
    test_clock_mode_formats();
    test_localized_clock_modes();
    test_localized_twelve_hour_clock();
    test_localized_edo_clock();
    return 0;
}
