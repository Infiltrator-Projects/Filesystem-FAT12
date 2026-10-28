// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal.c
 * @brief Portable temporal presentation policy implementation.
 */
#include "infiltratr/temporal.h"

#include "infiltratr/arithmetic.h"
#include "infiltratr/config.h"
#include "infiltratr/core.h"
#include "infiltratr/format.h"
#include "infiltratr/timing.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MICROSECONDS_PER_SECOND INT64_C(1000000)
#define SECONDS_PER_DAY INT64_C(86400)
#define MICROSECONDS_PER_DAY (SECONDS_PER_DAY * MICROSECONDS_PER_SECOND)
#define DECIMAL_SECONDS_PER_DAY UINT64_C(100000)

typedef struct ClockProfileEntry {
    InfiltratrClockProfile profile;
    const char *id;
    const char *name;
} ClockProfileEntry;

static const ClockProfileEntry clock_profiles[] = {
    { INFILTRATR_CLOCK_PROFILE_SYSTEM, "system", "Follow system" },
    { INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12, "conventional-12", "12-hour time" },
    { INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24, "conventional-24", "24-hour time" },
    { INFILTRATR_CLOCK_PROFILE_DECIMAL_10, "decimal-10", "Decimal time (10-hour day)" }
};

static const InfiltratrTemporalClockModeInfo temporal_clock_modes[] = {
    { "standard", "Standard time (OS locale)", true, false, false },
    { "standard-24", "Standard time (24-hour)", true, false, false },
    { "standard-12", "Standard time (12-hour)", true, false, false },
    { "internet", "Swatch Internet Time (beats @000–@999)", true, false, false },
    { "unix", "Unix time (seconds since 1970-01-01 UTC)", false, false, false },
    { "binary", "Binary clock (binary HH:MM:SS)", true, false, false },
    { "hexadecimal", "Hexadecimal day fraction (0000–FFFF)", false, false, false },
    { "julian", "Julian Date (astronomical continuous day count)", true, false, false },
    { "modified-julian", "Modified Julian Date (MJD = JD − 2400000.5)", true, false, false },
    { "sidereal", "Local sidereal time (LST; compact J2000 approximation)", true, false, true },
    { "solar", "Apparent solar time (compact Sun-position approximation)", true, false, true },
    { "mean-solar", "Local mean solar time (LMT)", true, false, true },
    { "decimal", "French Republican decimal time (10h × 100m × 100s)", true, false, false },
    { "chinese-time", "Traditional Chinese double-hours (Zǐ begins 23:00; 初/正 half-shí precision)", true, false, false },
    { "chinese-ke", "Historical Chinese hundred-kè variant (100 equal kè per day; finer subdivisions varied by era)", false, false, false },
    { "roman-temporal", "Roman seasonal time (12 unequal daylight hours; night in 4 military watches)", true, true, true },
    { "japanese-temporal", "Late Edo Japanese unequal hours (1797 twilight; 6 day / 6 night)", false, true, true },
    { "japanese-temporal-early", "Early Edo Japanese unequal hours (sunrise/sunset; 6 day / 6 night)", false, true, true },
    { "italian-hours", "Historical Italian hours (24 equal hours counted from sunset)", true, true, true },
    { "babylonian-hours", "Renaissance European ‘Babylonian’ hours (24 equal hours counted from sunrise)", true, true, true },
    { "babylonian-ancient", "Ancient Babylonian time (sunset-start day; 12 fixed bēru/day; 30 UŠ per bēru)", false, true, true },
    { "indian-ghati", "Indian ghaṭī time (60 per sunrise day; 1 ghaṭī = 24 min)", false, true, true },
    { "nuremberg-hours", "Historical Nuremberg Great Clock (equal hours; day/night counts change on fixed dates)", true, false, false },
    { "nuremberg-solar", "Nuremberg-style seasonal reconstruction (location-based, not the fixed civic schedule)", true, true, true }
};

static const InfiltratrTemporalCalendarInfo temporal_calendars[] = {
    { "gregorian", "Gregorian" },
    { "iso-week", "ISO week calendar" },
    { "julian", "Julian" },
    { "revised-julian", "Revised Julian" },
    { "hebrew", "Hebrew (fixed arithmetic calendar)" },
    { "islamic-umalqura", "Islamic (Umm al-Qura table; AH 1300–1600)" },
    { "islamic-civil", "Islamic (civil/tabular)" },
    { "islamic-tbla", "Islamic (tabular, astronomical epoch)" },
    { "islamic", "Islamic (computational/astronomical; not local sighting)" },
    { "persian", "Persian (Solar Hijri arithmetic)" },
    { "bahai", "Bahá’í (Badíʿ; modern Naw-Rúz from Tehran equinox)" },
    { "buddhist", "Buddhist Era (Gregorian-derived; +543 years)" },
    { "coptic", "Coptic (Alexandrian 13-month calendar)" },
    { "ethiopian", "Ethiopian Amete Mihret (13-month calendar)" },
    { "ethiopic-amete-alem", "Ethiopic Amete Alem (Amete Mihret + 5500 years)" },
    { "chinese", "Chinese traditional (computational lunisolar)" },
    { "dangi", "Dangi (traditional Korean computational lunisolar)" },
    { "indian", "Indian National (Saka)" },
    { "japanese", "Japanese imperial era (modern Meiji–Reiwa)" },
    { "minguo", "Minguo / Republic of China (Year 1 = 1912)" },
    { "roman", "Roman civil date notation (Julian; inclusive Kalends/Nones/Ides; Varronian A.U.C.)" },
    { "byzantine", "Byzantine Anno Mundi — Constantinople (Julian; year begins 1 September)" },
    { "egyptian-nabonassar", "Ptolemaic Nabonassar-era Egyptian civil calendar (365-day wandering year; epoch 747 BC)" },
    { "armenian-traditional", "Traditional Armenian wandering calendar (365-day year; era begins 11 July 552 Julian)" },
    { "mayan", "Maya Long Count (Goodman–Martínez–Thompson 584283 correlation)" },
    { "french-republican", "French Republican (epoch 22 September 1792; Romme arithmetic continuation after historical use)" },
    { "swedish-historical", "Swedish historical calendar (1700–1753, incl. 30 Feb 1712)" },
    { "international-fixed", "International Fixed (13 × 28-day months + Year Day)" },
    { "world", "World Calendar (quarter-based; intercalary World/Leap days)" },
    { "positivist", "Positivist calendar (Comte; 13 × 28-day months + festival days; Year 1 = 1789)" }
};

static bool profile_valid(InfiltratrClockProfile profile)
{
    return profile >= INFILTRATR_CLOCK_PROFILE_SYSTEM &&
           profile <= INFILTRATR_CLOCK_PROFILE_DECIMAL_10;
}

static int64_t positive_phase(int64_t value, int64_t period)
{
    int64_t remainder = 0;
    if (!infiltratr_i64_floor_divmod(value, period, NULL, &remainder)) {
        return 0;
    }
    return remainder;
}

int64_t infiltratr_temporal_local_microseconds_of_day(
    int64_t unix_microseconds,
    int32_t utc_offset_seconds)
{
    const int64_t instant_phase =
        positive_phase(unix_microseconds, MICROSECONDS_PER_DAY);
    const int64_t offset_microseconds =
        (int64_t)utc_offset_seconds * MICROSECONDS_PER_SECOND;
    const int64_t offset_phase =
        positive_phase(offset_microseconds, MICROSECONDS_PER_DAY);
    return positive_phase(instant_phase + offset_phase, MICROSECONDS_PER_DAY);
}

bool infiltratr_temporal_policy_default(InfiltratrTemporalPolicy *policy)
{
    if (policy == NULL) {
        return false;
    }
    policy->struct_size = (uint32_t)sizeof(*policy);
    policy->version = INFILTRATR_TEMPORAL_POLICY_VERSION;
    policy->clock_profile = INFILTRATR_CLOCK_PROFILE_SYSTEM;
    policy->show_seconds = false;
    return true;
}

bool infiltratr_clock_profile_from_id(const char *id,
                                      InfiltratrClockProfile *profile)
{
    size_t index;
    if (id == NULL || profile == NULL) {
        return false;
    }
    for (index = 0U; index < sizeof(clock_profiles) / sizeof(clock_profiles[0]);
         ++index) {
        if (strcmp(id, clock_profiles[index].id) == 0) {
            *profile = clock_profiles[index].profile;
            return true;
        }
    }
    return false;
}

const char *infiltratr_clock_profile_id(InfiltratrClockProfile profile)
{
    size_t index;
    for (index = 0U; index < sizeof(clock_profiles) / sizeof(clock_profiles[0]);
         ++index) {
        if (clock_profiles[index].profile == profile) {
            return clock_profiles[index].id;
        }
    }
    return NULL;
}

const char *infiltratr_clock_profile_name(InfiltratrClockProfile profile)
{
    size_t index;
    for (index = 0U; index < sizeof(clock_profiles) / sizeof(clock_profiles[0]);
         ++index) {
        if (clock_profiles[index].profile == profile) {
            return clock_profiles[index].name;
        }
    }
    return NULL;
}

size_t infiltratr_clock_profile_count(void)
{
    return sizeof(clock_profiles) / sizeof(clock_profiles[0]);
}

bool infiltratr_clock_profile_at(size_t index, InfiltratrClockProfile *profile)
{
    if (profile == NULL || index >= infiltratr_clock_profile_count()) {
        return false;
    }
    *profile = clock_profiles[index].profile;
    return true;
}

bool infiltratr_temporal_policy_parse(const char *text,
                                      InfiltratrTemporalPolicy *policy)
{
    InfiltratrTemporalPolicy parsed;
    const char *cursor;
    bool saw_version = false;

    if (text == NULL || policy == NULL ||
        !infiltratr_temporal_policy_default(&parsed)) {
        return false;
    }

    cursor = text;
    while (*cursor != '\0') {
        const char *newline = strchr(cursor, '\n');
        const size_t line_length =
            newline != NULL ? (size_t)(newline - cursor) : strlen(cursor);
        char line[256];
        char *key = NULL;
        char *value = NULL;
        InfiltratrConfigLineStatus status;

        if (line_length >= sizeof(line)) {
            return false;
        }
        memcpy(line, cursor, line_length);
        line[line_length] = '\0';

        status = infiltratr_config_parse_line(line, &key, &value);
        if (status == INFILTRATR_CONFIG_LINE_INVALID) {
            return false;
        }
        if (status == INFILTRATR_CONFIG_LINE_ENTRY) {
            if (strcmp(key, "version") == 0) {
                if (strcmp(value, "1") != 0) {
                    return false;
                }
                saw_version = true;
            } else if (strcmp(key, "clock-profile") == 0) {
                InfiltratrClockProfile profile;
                if (!infiltratr_clock_profile_from_id(value, &profile)) {
                    return false;
                }
                parsed.clock_profile = profile;
            } else if (strcmp(key, "show-seconds") == 0) {
                bool show_seconds;
                if (!infiltratr_config_parse_bool(value, &show_seconds)) {
                    return false;
                }
                parsed.show_seconds = show_seconds;
            }
        }

        if (newline == NULL) {
            break;
        }
        cursor = newline + 1;
    }

    if (!saw_version) {
        return false;
    }
    *policy = parsed;
    return true;
}

bool infiltratr_temporal_policy_serialize(const InfiltratrTemporalPolicy *policy,
                                          char *buffer,
                                          size_t capacity,
                                          size_t *length)
{
    char temporary[256];
    const char *profile_id;
    int written;

    if (policy == NULL || buffer == NULL ||
        policy->struct_size < sizeof(*policy) ||
        policy->version != INFILTRATR_TEMPORAL_POLICY_VERSION ||
        !profile_valid(policy->clock_profile)) {
        return false;
    }

    profile_id = infiltratr_clock_profile_id(policy->clock_profile);
    if (profile_id == NULL) {
        return false;
    }

    written = snprintf(temporary, sizeof(temporary),
                       "version=1\nclock-profile=%s\nshow-seconds=%s\n",
                       profile_id,
                       policy->show_seconds ? "true" : "false");
    if (written < 0 || (size_t)written >= sizeof(temporary) ||
        capacity <= (size_t)written) {
        return false;
    }

    memcpy(buffer, temporary, (size_t)written + 1U);
    if (length != NULL) {
        *length = (size_t)written;
    }
    return true;
}

bool infiltratr_temporal_format_clock(InfiltratrClockProfile profile,
                                      int64_t unix_microseconds,
                                      int32_t utc_offset_seconds,
                                      bool show_seconds,
                                      char *buffer,
                                      size_t capacity,
                                      size_t *length)
{
    char temporary[64];
    const int64_t local =
        infiltratr_temporal_local_microseconds_of_day(
            unix_microseconds, utc_offset_seconds);
    int written = -1;

    if (buffer == NULL || capacity == 0U || !profile_valid(profile) ||
        profile == INFILTRATR_CLOCK_PROFILE_SYSTEM) {
        return false;
    }

    if (profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10) {
        uint64_t ticks = 0U;
        const uint64_t ticks_per_day =
            show_seconds ? DECIMAL_SECONDS_PER_DAY : UINT64_C(1000);

        if (!infiltratr_cycle_partition_u64((uint64_t)local,
                                            (uint64_t)MICROSECONDS_PER_DAY,
                                            ticks_per_day,
                                            &ticks,
                                            NULL)) {
            return false;
        }
        if (show_seconds) {
            written = snprintf(temporary, sizeof(temporary),
                               "%llu:%02llu:%02llu",
                               (unsigned long long)(ticks / UINT64_C(10000)),
                               (unsigned long long)((ticks / UINT64_C(100)) % UINT64_C(100)),
                               (unsigned long long)(ticks % UINT64_C(100)));
        } else {
            written = snprintf(temporary, sizeof(temporary),
                               "%llu:%02llu",
                               (unsigned long long)(ticks / UINT64_C(100)),
                               (unsigned long long)(ticks % UINT64_C(100)));
        }
    } else {
        const int64_t whole_seconds = local / MICROSECONDS_PER_SECOND;
        const int hour24 = (int)(whole_seconds / INT64_C(3600));
        const int minute = (int)((whole_seconds / INT64_C(60)) % INT64_C(60));
        const int second = (int)(whole_seconds % INT64_C(60));

        if (profile == INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24) {
            written = show_seconds
                ? snprintf(temporary, sizeof(temporary), "%02d:%02d:%02d",
                           hour24, minute, second)
                : snprintf(temporary, sizeof(temporary), "%02d:%02d",
                           hour24, minute);
        } else {
            const int hour12 = hour24 % 12 == 0 ? 12 : hour24 % 12;
            const char *suffix = hour24 < 12 ? "AM" : "PM";
            written = show_seconds
                ? snprintf(temporary, sizeof(temporary), "%d:%02d:%02d %s",
                           hour12, minute, second, suffix)
                : snprintf(temporary, sizeof(temporary), "%d:%02d %s",
                           hour12, minute, suffix);
        }
    }

    if (written < 0 || (size_t)written >= sizeof(temporary) ||
        capacity <= (size_t)written) {
        return false;
    }
    memcpy(buffer, temporary, (size_t)written + 1U);
    if (length != NULL) {
        *length = (size_t)written;
    }
    return true;
}


static bool copy_temporal_id(char destination[INFILTRATR_TEMPORAL_ID_CAPACITY],
                             const char *source)
{
    size_t length;

    if (destination == NULL || source == NULL) {
        return false;
    }
    length = strlen(source);
    if (length == 0U || length >= INFILTRATR_TEMPORAL_ID_CAPACITY) {
        return false;
    }
    memcpy(destination, source, length + 1U);
    return true;
}

size_t infiltratr_temporal_clock_mode_count(void)
{
    return sizeof(temporal_clock_modes) / sizeof(temporal_clock_modes[0]);
}

const InfiltratrTemporalClockModeInfo *
infiltratr_temporal_clock_mode_at(size_t index)
{
    return index < infiltratr_temporal_clock_mode_count()
        ? &temporal_clock_modes[index] : NULL;
}

const InfiltratrTemporalClockModeInfo *
infiltratr_temporal_clock_mode_find(const char *id)
{
    size_t index;

    if (id == NULL) {
        return NULL;
    }
    for (index = 0U; index < infiltratr_temporal_clock_mode_count(); ++index) {
        if (strcmp(id, temporal_clock_modes[index].id) == 0) {
            return &temporal_clock_modes[index];
        }
    }
    return NULL;
}

size_t infiltratr_temporal_calendar_count(void)
{
    return sizeof(temporal_calendars) / sizeof(temporal_calendars[0]);
}

const InfiltratrTemporalCalendarInfo *
infiltratr_temporal_calendar_at(size_t index)
{
    return index < infiltratr_temporal_calendar_count()
        ? &temporal_calendars[index] : NULL;
}

const InfiltratrTemporalCalendarInfo *
infiltratr_temporal_calendar_find(const char *id)
{
    size_t index;

    if (id == NULL) {
        return NULL;
    }
    for (index = 0U; index < infiltratr_temporal_calendar_count(); ++index) {
        if (strcmp(id, temporal_calendars[index].id) == 0) {
            return &temporal_calendars[index];
        }
    }
    return NULL;
}

static bool temporal_document_version(const char *text, unsigned int *version)
{
    const char *cursor;

    if (text == NULL || version == NULL) {
        return false;
    }

    cursor = text;
    while (*cursor != '\0') {
        const char *newline = strchr(cursor, '\n');
        const size_t line_length =
            newline != NULL ? (size_t)(newline - cursor) : strlen(cursor);
        char line[256];
        char *key = NULL;
        char *value = NULL;
        InfiltratrConfigLineStatus status;
        uint64_t parsed = 0U;

        if (line_length >= sizeof(line)) {
            return false;
        }

        memcpy(line, cursor, line_length);
        line[line_length] = '\0';
        status = infiltratr_config_parse_line(line, &key, &value);
        if (status == INFILTRATR_CONFIG_LINE_INVALID) {
            return false;
        }

        if (status == INFILTRATR_CONFIG_LINE_ENTRY &&
            strcmp(key, "version") == 0) {
            if (!infiltratr_parse_u64_range(
                    value, 10U, 1U, UINT32_MAX, &parsed)) {
                return false;
            }
            *version = (unsigned int)parsed;
            return true;
        }

        if (newline == NULL) {
            break;
        }
        cursor = newline + 1;
    }

    return false;
}

bool infiltratr_temporal_policy_v3_default(InfiltratrTemporalPolicyV3 *policy)
{
    if (policy == NULL) {
        return false;
    }

    memset(policy, 0, sizeof(*policy));
    policy->struct_size = (uint32_t)sizeof(*policy);
    policy->version = INFILTRATR_TEMPORAL_POLICY_V3_VERSION;

    return copy_temporal_id(policy->clock_mode, "standard") &&
           copy_temporal_id(policy->calendar, "gregorian");
}

static bool migrate_v1_document(const char *text,
                                InfiltratrTemporalPolicyV3 *policy)
{
    InfiltratrTemporalPolicy legacy;
    const char *mode = "standard";

    if (!infiltratr_temporal_policy_parse(text, &legacy) ||
        !infiltratr_temporal_policy_v3_default(policy)) {
        return false;
    }

    switch (legacy.clock_profile) {
    case INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12:
        mode = "standard-12";
        break;
    case INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24:
        mode = "standard-24";
        break;
    case INFILTRATR_CLOCK_PROFILE_DECIMAL_10:
        mode = "decimal";
        break;
    case INFILTRATR_CLOCK_PROFILE_SYSTEM:
    default:
        mode = "standard";
        break;
    }

    policy->show_seconds = legacy.show_seconds;
    return copy_temporal_id(policy->clock_mode, mode);
}

/*
 * Private migration only. This is deliberately not an exported v2 ABI.
 * It exists solely so a presentation.conf already written by System Settings
 * 0.2.0 can be upgraded without losing the user's selected clock/calendar.
 * secondary-calendar is intentionally ignored.
 */
static bool migrate_v2_document(const char *text,
                                InfiltratrTemporalPolicyV3 *policy)
{
    InfiltratrTemporalPolicyV3 parsed;
    const char *cursor;

    if (text == NULL || policy == NULL ||
        !infiltratr_temporal_policy_v3_default(&parsed)) {
        return false;
    }

    cursor = text;
    while (*cursor != '\0') {
        const char *newline = strchr(cursor, '\n');
        const size_t line_length =
            newline != NULL ? (size_t)(newline - cursor) : strlen(cursor);
        char line[256];
        char *key = NULL;
        char *value = NULL;
        InfiltratrConfigLineStatus status;

        if (line_length >= sizeof(line)) {
            return false;
        }

        memcpy(line, cursor, line_length);
        line[line_length] = '\0';
        status = infiltratr_config_parse_line(line, &key, &value);
        if (status == INFILTRATR_CONFIG_LINE_INVALID) {
            return false;
        }

        if (status == INFILTRATR_CONFIG_LINE_ENTRY) {
            if (strcmp(key, "version") == 0) {
                if (strcmp(value, "2") != 0) {
                    return false;
                }
            } else if (strcmp(key, "clock-mode") == 0) {
                if (infiltratr_temporal_clock_mode_find(value) == NULL ||
                    !copy_temporal_id(parsed.clock_mode, value)) {
                    return false;
                }
            } else if (strcmp(key, "primary-calendar") == 0) {
                if (infiltratr_temporal_calendar_find(value) == NULL ||
                    !copy_temporal_id(parsed.calendar, value)) {
                    return false;
                }
            } else if (strcmp(key, "show-seconds") == 0) {
                if (!infiltratr_config_parse_bool(
                        value, &parsed.show_seconds)) {
                    return false;
                }
            } else if (strcmp(key, "location-configured") == 0) {
                if (!infiltratr_config_parse_bool(
                        value, &parsed.location_configured)) {
                    return false;
                }
            } else if (strcmp(key, "latitude") == 0) {
                if (!infiltratr_parse_double_range(
                        value, -90.0, 90.0, &parsed.latitude)) {
                    return false;
                }
            } else if (strcmp(key, "longitude") == 0) {
                if (!infiltratr_parse_double_range(
                        value, -180.0, 180.0, &parsed.longitude)) {
                    return false;
                }
            }
        }

        if (newline == NULL) {
            break;
        }
        cursor = newline + 1;
    }

    if (infiltratr_temporal_clock_mode_find(parsed.clock_mode) == NULL ||
        infiltratr_temporal_calendar_find(parsed.calendar) == NULL) {
        return false;
    }

    *policy = parsed;
    return true;
}

bool infiltratr_temporal_policy_v3_parse(const char *text,
                                         InfiltratrTemporalPolicyV3 *policy)
{
    enum {
        V3_SEEN_VERSION = 1U << 0,
        V3_SEEN_CLOCK_MODE = 1U << 1,
        V3_SEEN_CALENDAR = 1U << 2,
        V3_SEEN_SHOW_SECONDS = 1U << 3,
        V3_SEEN_LOCATION_CONFIGURED = 1U << 4,
        V3_SEEN_LATITUDE = 1U << 5,
        V3_SEEN_LONGITUDE = 1U << 6,
        V3_REQUIRED_FIELDS = (1U << 7) - 1U
    };
    InfiltratrTemporalPolicyV3 parsed;
    const char *cursor;
    unsigned int version = 0U;
    unsigned int seen = 0U;

    if (text == NULL || policy == NULL ||
        !temporal_document_version(text, &version)) {
        return false;
    }

    if (version == INFILTRATR_TEMPORAL_POLICY_VERSION) {
        return migrate_v1_document(text, policy);
    }
    if (version == 2U) {
        return migrate_v2_document(text, policy);
    }
    if (version != INFILTRATR_TEMPORAL_POLICY_V3_VERSION ||
        !infiltratr_temporal_policy_v3_default(&parsed)) {
        return false;
    }

    cursor = text;
    while (*cursor != '\0') {
        const char *newline = strchr(cursor, '\n');
        const size_t line_length =
            newline != NULL ? (size_t)(newline - cursor) : strlen(cursor);
        char line[256];
        char *key = NULL;
        char *value = NULL;
        InfiltratrConfigLineStatus status;

        if (line_length >= sizeof(line)) {
            return false;
        }

        memcpy(line, cursor, line_length);
        line[line_length] = '\0';
        status = infiltratr_config_parse_line(line, &key, &value);
        if (status == INFILTRATR_CONFIG_LINE_INVALID) {
            return false;
        }

        if (status == INFILTRATR_CONFIG_LINE_ENTRY) {
            if (strcmp(key, "version") == 0) {
                if ((seen & V3_SEEN_VERSION) != 0U ||
                    strcmp(value, "3") != 0) {
                    return false;
                }
                seen |= V3_SEEN_VERSION;
            } else if (strcmp(key, "clock-mode") == 0) {
                if ((seen & V3_SEEN_CLOCK_MODE) != 0U ||
                    infiltratr_temporal_clock_mode_find(value) == NULL ||
                    !copy_temporal_id(parsed.clock_mode, value)) {
                    return false;
                }
                seen |= V3_SEEN_CLOCK_MODE;
            } else if (strcmp(key, "calendar") == 0) {
                if ((seen & V3_SEEN_CALENDAR) != 0U ||
                    infiltratr_temporal_calendar_find(value) == NULL ||
                    !copy_temporal_id(parsed.calendar, value)) {
                    return false;
                }
                seen |= V3_SEEN_CALENDAR;
            } else if (strcmp(key, "show-seconds") == 0) {
                if ((seen & V3_SEEN_SHOW_SECONDS) != 0U ||
                    !infiltratr_config_parse_bool(
                        value, &parsed.show_seconds)) {
                    return false;
                }
                seen |= V3_SEEN_SHOW_SECONDS;
            } else if (strcmp(key, "location-configured") == 0) {
                if ((seen & V3_SEEN_LOCATION_CONFIGURED) != 0U ||
                    !infiltratr_config_parse_bool(
                        value, &parsed.location_configured)) {
                    return false;
                }
                seen |= V3_SEEN_LOCATION_CONFIGURED;
            } else if (strcmp(key, "latitude") == 0) {
                if ((seen & V3_SEEN_LATITUDE) != 0U ||
                    !infiltratr_parse_double_range(
                        value, -90.0, 90.0, &parsed.latitude)) {
                    return false;
                }
                seen |= V3_SEEN_LATITUDE;
            } else if (strcmp(key, "longitude") == 0) {
                if ((seen & V3_SEEN_LONGITUDE) != 0U ||
                    !infiltratr_parse_double_range(
                        value, -180.0, 180.0, &parsed.longitude)) {
                    return false;
                }
                seen |= V3_SEEN_LONGITUDE;
            }
        }

        if (newline == NULL) {
            break;
        }
        cursor = newline + 1;
    }

    if (seen != V3_REQUIRED_FIELDS ||
        infiltratr_temporal_clock_mode_find(parsed.clock_mode) == NULL ||
        infiltratr_temporal_calendar_find(parsed.calendar) == NULL) {
        return false;
    }

    *policy = parsed;
    return true;
}

bool infiltratr_temporal_policy_v3_serialize(
    const InfiltratrTemporalPolicyV3 *policy,
    char *buffer,
    size_t capacity,
    size_t *length)
{
    char latitude[64];
    char longitude[64];
    char temporary[512];
    int written;

    if (policy == NULL || buffer == NULL ||
        policy->struct_size < sizeof(*policy) ||
        policy->version != INFILTRATR_TEMPORAL_POLICY_V3_VERSION ||
        memchr(policy->clock_mode, '\0', sizeof(policy->clock_mode)) == NULL ||
        memchr(policy->calendar, '\0', sizeof(policy->calendar)) == NULL ||
        infiltratr_temporal_clock_mode_find(policy->clock_mode) == NULL ||
        infiltratr_temporal_calendar_find(policy->calendar) == NULL ||
        !isfinite(policy->latitude) ||
        policy->latitude < -90.0 || policy->latitude > 90.0 ||
        !isfinite(policy->longitude) ||
        policy->longitude < -180.0 || policy->longitude > 180.0 ||
        !infiltratr_format_fixed_ascii(policy->latitude, 6U,
                                       latitude, sizeof(latitude)) ||
        !infiltratr_format_fixed_ascii(policy->longitude, 6U,
                                       longitude, sizeof(longitude))) {
        return false;
    }

    written = snprintf(temporary, sizeof(temporary),
                       "version=3\n"
                       "clock-mode=%s\n"
                       "calendar=%s\n"
                       "show-seconds=%s\n"
                       "location-configured=%s\n"
                       "latitude=%s\n"
                       "longitude=%s\n",
                       policy->clock_mode,
                       policy->calendar,
                       policy->show_seconds ? "true" : "false",
                       policy->location_configured ? "true" : "false",
                       latitude,
                       longitude);

    if (written < 0 || (size_t)written >= sizeof(temporary) ||
        capacity <= (size_t)written) {
        return false;
    }

    memcpy(buffer, temporary, (size_t)written + 1U);
    if (length != NULL) {
        *length = (size_t)written;
    }
    return true;
}
