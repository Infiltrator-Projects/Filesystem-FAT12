// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 1993-2026 Shannon Smith
 *
 * Portable formatting for the complete system-wide clock catalogue.
 *
 * Canonical instants remain Unix microseconds and internal elapsed-time
 * accounting remains SI/monotonic. This module is presentation only: it turns
 * one canonical instant plus explicit civil offset/location policy into the
 * clock representation selected by System Settings.
 */
#include "infiltratr/temporal.h"

#include "infiltratr/arithmetic.h"
#include "infiltratr/compiler.h"
#include "infiltratr/core.h"
#include "infiltratr/i18n.h"
#include "infiltratr/timing.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MICROSECONDS_PER_SECOND INT64_C(1000000)
#define SECONDS_PER_MINUTE INT64_C(60)
#define MINUTES_PER_HOUR INT64_C(60)
#define SECONDS_PER_HOUR INT64_C(3600)
#define SECONDS_PER_DAY INT64_C(86400)
#define MICROSECONDS_PER_DAY (SECONDS_PER_DAY * MICROSECONDS_PER_SECOND)
#define INTERNET_TICKS_PER_DAY UINT64_C(1000)
#define HEX_TICKS_PER_DAY UINT64_C(65536)
#define DECIMAL_SECONDS_PER_DAY UINT64_C(100000)
#define CHINESE_DOUBLE_HOURS_PER_DAY UINT64_C(12)
#define CHINESE_KE_PER_DAY UINT64_C(100)
#define VIGHATI_PER_DAY UINT64_C(3600)
#define JULIAN_DATE_UNIX_EPOCH 2440587.5L
#define SIDEREAL_RATE_PER_SI_SECOND (24.06570982441908L / 24.0L)
#define JULIAN_DATE_J2000 2451545.0L
#define INFILTRATR_PI 3.141592653589793238462643383279502884L

static bool write_text(char *buffer, size_t capacity, size_t *length,
                       const char *text)
{
    size_t needed;

    if (buffer == NULL || capacity == 0U || text == NULL) {
        return false;
    }
    needed = strlen(text);
    if (needed >= capacity) {
        return false;
    }
    memcpy(buffer, text, needed + 1U);
    if (length != NULL) {
        *length = needed;
    }
    return true;
}

static bool write_printf(char *buffer, size_t capacity, size_t *length,
                         const char *format, ...)
    INFILTRATR_PRINTF_FORMAT(4, 5);
static bool write_printf(char *buffer, size_t capacity, size_t *length,
                         const char *format, ...)
{
    va_list arguments;
    int written;

    if (buffer == NULL || capacity == 0U || format == NULL) {
        return false;
    }
    va_start(arguments, format);
    written = vsnprintf(buffer, capacity, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= capacity) {
        buffer[0] = '\0';
        return false;
    }
    if (length != NULL) {
        *length = (size_t)written;
    }
    return true;
}

static int64_t floor_divide(int64_t value, int64_t divisor)
{
    int64_t quotient = 0;

    if (!infiltratr_i64_floor_divmod(value, divisor, &quotient, NULL)) {
        return 0;
    }
    return quotient;
}

static int64_t positive_modulo(int64_t value, int64_t modulus)
{
    int64_t remainder = 0;

    if (!infiltratr_i64_floor_divmod(value, modulus, NULL, &remainder)) {
        return 0;
    }
    return remainder;
}

static uint64_t day_tick(int64_t microseconds, uint64_t ticks_per_day)
{
    uint64_t tick = 0U;

    if (microseconds < 0 || ticks_per_day == 0U ||
        !infiltratr_cycle_partition_u64((uint64_t)microseconds,
                                        (uint64_t)MICROSECONDS_PER_DAY,
                                        ticks_per_day, &tick, NULL)) {
        return 0U;
    }
    return tick;
}

static void split_clock_seconds(int64_t whole_seconds,
                                int *hour, int *minute, int *second)
{
    const int64_t normalised =
        positive_modulo(whole_seconds, SECONDS_PER_DAY);

    *hour = (int)(normalised / SECONDS_PER_HOUR);
    *minute = (int)((normalised / SECONDS_PER_MINUTE) % MINUTES_PER_HOUR);
    *second = (int)(normalised % SECONDS_PER_MINUTE);
}

static bool write_clock_fields(char *buffer, size_t capacity, size_t *length,
                               int hour, int minute, int second,
                               bool show_seconds, bool vertical,
                               const char *suffix)
{
    const char *separator = vertical ? "\n" : ":";
    const char *suffix_separator = vertical ? "\n" : " ";

    if (show_seconds) {
        return write_printf(buffer, capacity, length,
                            "%02d%s%02d%s%02d%s%s",
                            hour, separator, minute, separator, second,
                            suffix_separator, suffix);
    }
    return write_printf(buffer, capacity, length,
                        "%02d%s%02d%s%s",
                        hour, separator, minute, suffix_separator, suffix);
}

/*
 * Convert a Unix day number to Gregorian day-of-year with exact 400-year
 * cycle arithmetic. Keeping this independent of time_t makes astronomical
 * formatting deterministic on 32-bit and 64-bit hosts alike.
 */
static bool gregorian_day_of_year(int64_t unix_microseconds,
                                  int *day_of_year,
                                  bool *is_leap_year)
{
    static const int cumulative_days[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    int64_t unix_seconds;
    int64_t days;
    int64_t z;
    int64_t era;
    int64_t doe;
    int64_t yoe;
    int64_t doy_march;
    int64_t mp;
    int64_t year;
    int month;
    int day;
    bool leap;

    if (day_of_year == NULL) {
        return false;
    }
    unix_seconds = floor_divide(unix_microseconds, MICROSECONDS_PER_SECOND);
    days = floor_divide(unix_seconds, SECONDS_PER_DAY);
    if (!infiltratr_i64_add_checked(days, INT64_C(719468), &z)) {
        return false;
    }

    era = floor_divide(z, INT64_C(146097));
    doe = z - era * INT64_C(146097);
    yoe = (doe - doe / INT64_C(1460) + doe / INT64_C(36524) -
           doe / INT64_C(146096)) / INT64_C(365);
    year = yoe + era * INT64_C(400);
    doy_march = doe - (INT64_C(365) * yoe + yoe / INT64_C(4) -
                       yoe / INT64_C(100));
    mp = (INT64_C(5) * doy_march + INT64_C(2)) / INT64_C(153);
    day = (int)(doy_march - (INT64_C(153) * mp + INT64_C(2)) /
                INT64_C(5) + INT64_C(1));
    month = (int)(mp + (mp < 10 ? 3 : -9));
    if (month <= 2) {
        year++;
    }

    leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (month < 1 || month > 12 || day < 1 || day > 31) {
        return false;
    }
    *day_of_year = cumulative_days[month - 1] + day +
                   (leap && month > 2 ? 1 : 0);
    if (is_leap_year != NULL) {
        *is_leap_year = leap;
    }
    return true;
}

/*
 * Compact equation-of-time and solar-declination approximation shared by the
 * apparent-solar and seasonal clock families. Both values come from the same
 * fractional-year phase so all location-aware modes remain internally
 * consistent.
 */
static bool solar_terms(int64_t unix_microseconds,
                        double *equation_minutes,
                        double *declination_radians)
{
    const int64_t utc_phase =
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY);
    const long double fractional_hour =
        (long double)utc_phase /
        ((long double)SECONDS_PER_HOUR * MICROSECONDS_PER_SECOND);
    int day_of_year;
    long double gamma;

    if (!gregorian_day_of_year(unix_microseconds, &day_of_year, NULL)) {
        return false;
    }
    gamma = 2.0L * INFILTRATR_PI / 365.0L *
            ((long double)day_of_year - 1.0L +
             (fractional_hour - 12.0L) / 24.0L);

    if (equation_minutes != NULL) {
        *equation_minutes = 229.18 *
            (0.000075 + 0.001868 * cos((double)gamma) -
             0.032077 * sin((double)gamma) -
             0.014615 * cos((double)(2.0L * gamma)) -
             0.040849 * sin((double)(2.0L * gamma)));
    }
    if (declination_radians != NULL) {
        *declination_radians =
            0.006918 - 0.399912 * cos((double)gamma) +
            0.070257 * sin((double)gamma) -
            0.006758 * cos((double)(2.0L * gamma)) +
            0.000907 * sin((double)(2.0L * gamma)) -
            0.002697 * cos((double)(3.0L * gamma)) +
            0.001480 * sin((double)(3.0L * gamma));
    }
    return true;
}

static long double julian_date(int64_t unix_microseconds)
{
    return JULIAN_DATE_UNIX_EPOCH +
           (long double)unix_microseconds / (long double)MICROSECONDS_PER_DAY;
}

static double clamp_longitude(double longitude)
{
    return isfinite(longitude)
        ? infiltratr_clamp_double(longitude, -180.0, 180.0) : 0.0;
}

static double clamp_latitude(double latitude)
{
    return isfinite(latitude)
        ? infiltratr_clamp_double(latitude, -90.0, 90.0) : 0.0;
}

static long double local_sidereal_seconds(int64_t unix_microseconds,
                                          double longitude)
{
    const long double days_since_j2000 =
        julian_date(unix_microseconds) - JULIAN_DATE_J2000;
    const long double gmst_hours =
        18.697374558L + 24.06570982441908L * days_since_j2000;
    long double local_hours =
        fmodl(gmst_hours + (long double)clamp_longitude(longitude) / 15.0L,
              24.0L);

    if (local_hours < 0.0L) {
        local_hours += 24.0L;
    }
    return local_hours * (long double)SECONDS_PER_HOUR;
}

static long double mean_solar_seconds(int64_t unix_microseconds,
                                      double longitude)
{
    const int64_t utc_microseconds =
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY);
    long double seconds =
        (long double)utc_microseconds / MICROSECONDS_PER_SECOND +
        (long double)clamp_longitude(longitude) * 240.0L;

    seconds = fmodl(seconds, (long double)SECONDS_PER_DAY);
    if (seconds < 0.0L) {
        seconds += (long double)SECONDS_PER_DAY;
    }
    return seconds;
}

static long double apparent_solar_seconds(int64_t unix_microseconds,
                                          double longitude)
{
    double equation_minutes = 0.0;
    long double seconds;

    if (!solar_terms(unix_microseconds, &equation_minutes, NULL)) {
        return mean_solar_seconds(unix_microseconds, longitude);
    }
    seconds = mean_solar_seconds(unix_microseconds, longitude) +
              (long double)equation_minutes * 60.0L;
    seconds = fmodl(seconds, (long double)SECONDS_PER_DAY);
    if (seconds < 0.0L) {
        seconds += (long double)SECONDS_PER_DAY;
    }
    return seconds;
}

static bool solar_day_boundaries(int64_t unix_microseconds,
                                 double latitude,
                                 double solar_depression_degrees,
                                 long double *dawn_seconds,
                                 long double *dusk_seconds)
{
    const double safe_latitude = clamp_latitude(latitude);
    double declination = 0.0;
    double latitude_radians;
    double zenith_radians;
    double cosine_hour_angle;
    long double hour_angle;
    long double half_day_seconds;

    if (dawn_seconds == NULL || dusk_seconds == NULL ||
        !isfinite(solar_depression_degrees) ||
        solar_depression_degrees < 0.0 ||
        solar_depression_degrees >= 90.0 ||
        !solar_terms(unix_microseconds, NULL, &declination)) {
        return false;
    }

    latitude_radians = safe_latitude * (double)INFILTRATR_PI / 180.0;
    zenith_radians =
        (90.0 + solar_depression_degrees) * (double)INFILTRATR_PI / 180.0;
    if (fabs(cos(latitude_radians) * cos(declination)) < 1.0e-12) {
        return false;
    }

    cosine_hour_angle =
        cos(zenith_radians) /
            (cos(latitude_radians) * cos(declination)) -
        tan(latitude_radians) * tan(declination);
    if (cosine_hour_angle < -1.0 || cosine_hour_angle > 1.0) {
        return false;
    }

    hour_angle = acosl((long double)infiltratr_clamp_double(
        cosine_hour_angle, -1.0, 1.0));
    half_day_seconds =
        hour_angle * (long double)SECONDS_PER_DAY / (2.0L * INFILTRATR_PI);
    *dawn_seconds = (long double)SECONDS_PER_DAY / 2.0L - half_day_seconds;
    *dusk_seconds = (long double)SECONDS_PER_DAY / 2.0L + half_day_seconds;
    return true;
}

static bool solar_boundary_instants(int64_t unix_microseconds,
                                    double latitude,
                                    double longitude,
                                    double solar_depression_degrees,
                                    int64_t *dawn_microseconds,
                                    int64_t *dusk_microseconds)
{
    const int64_t safety_margin = INT64_C(2) * MICROSECONDS_PER_DAY;
    const double safe_latitude = clamp_latitude(latitude);
    const double safe_longitude = clamp_longitude(longitude);
    int64_t day_index;
    int64_t day_start;
    int64_t noon;
    double equation_minutes = 0.0;
    double declination = 0.0;
    double latitude_radians;
    double zenith_radians;
    double cosine_hour_angle;
    long double hour_angle;
    long double solar_noon_minutes;
    long double hour_angle_minutes;
    long double dawn_value;
    long double dusk_value;

    if (dawn_microseconds == NULL || dusk_microseconds == NULL ||
        unix_microseconds < INT64_MIN + safety_margin ||
        unix_microseconds > INT64_MAX - safety_margin ||
        !isfinite(solar_depression_degrees) ||
        solar_depression_degrees < 0.0 ||
        solar_depression_degrees >= 90.0) {
        return false;
    }

    day_index = floor_divide(unix_microseconds, MICROSECONDS_PER_DAY);
    if (!infiltratr_i64_multiply_checked(
            day_index, MICROSECONDS_PER_DAY, &day_start) ||
        !infiltratr_i64_add_checked(
            day_start, MICROSECONDS_PER_DAY / 2, &noon) ||
        !solar_terms(noon, &equation_minutes, &declination)) {
        return false;
    }

    latitude_radians = safe_latitude * (double)INFILTRATR_PI / 180.0;
    zenith_radians =
        (90.0 + solar_depression_degrees) * (double)INFILTRATR_PI / 180.0;
    if (fabs(cos(latitude_radians) * cos(declination)) < 1.0e-12) {
        return false;
    }

    cosine_hour_angle =
        cos(zenith_radians) /
            (cos(latitude_radians) * cos(declination)) -
        tan(latitude_radians) * tan(declination);
    if (cosine_hour_angle < -1.0 || cosine_hour_angle > 1.0) {
        return false;
    }

    hour_angle = acosl((long double)infiltratr_clamp_double(
        cosine_hour_angle, -1.0, 1.0));
    solar_noon_minutes =
        720.0L - 4.0L * safe_longitude - equation_minutes;
    hour_angle_minutes =
        hour_angle * 180.0L / INFILTRATR_PI * 4.0L;
    dawn_value =
        (long double)day_start +
        (solar_noon_minutes - hour_angle_minutes) *
            60.0L * MICROSECONDS_PER_SECOND;
    dusk_value =
        (long double)day_start +
        (solar_noon_minutes + hour_angle_minutes) *
            60.0L * MICROSECONDS_PER_SECOND;

    if (dawn_value < (long double)INT64_MIN ||
        dawn_value > (long double)INT64_MAX ||
        dusk_value < (long double)INT64_MIN ||
        dusk_value > (long double)INT64_MAX) {
        return false;
    }
    *dawn_microseconds = (int64_t)llroundl(dawn_value);
    *dusk_microseconds = (int64_t)llroundl(dusk_value);
    return true;
}

typedef struct SeasonalPeriod {
    bool daylight;
    unsigned index;
    long double fraction;
} SeasonalPeriod;

static bool seasonal_period_at(int64_t unix_microseconds,
                               double latitude,
                               double longitude,
                               double solar_depression_degrees,
                               unsigned daylight_parts,
                               unsigned night_parts,
                               SeasonalPeriod *period)
{
    long double dawn;
    long double dusk;
    const long double solar =
        apparent_solar_seconds(unix_microseconds, longitude);
    long double span;
    long double position;
    long double unit;
    unsigned parts;

    if (period == NULL || daylight_parts == 0U || night_parts == 0U ||
        !solar_day_boundaries(unix_microseconds, latitude,
                              solar_depression_degrees, &dawn, &dusk)) {
        return false;
    }

    if (solar >= dawn && solar < dusk) {
        period->daylight = true;
        span = dusk - dawn;
        position = solar - dawn;
        parts = daylight_parts;
    } else {
        period->daylight = false;
        span = (long double)SECONDS_PER_DAY - (dusk - dawn);
        position = solar >= dusk
            ? solar - dusk
            : (long double)SECONDS_PER_DAY - dusk + solar;
        parts = night_parts;
    }

    unit = span / parts;
    {
        long double scaled = position / unit;

        if (scaled < 0.0L) {
            scaled = 0.0L;
        }
        if (scaled >= (long double)parts) {
            scaled = nextafterl((long double)parts, 0.0L);
        }
        period->index = (unsigned)floorl(scaled);
        period->fraction = scaled - (long double)period->index;
    }
    return true;
}

typedef struct SeasonalProgress {
    long double daylight_units;
    long double night_units;
} SeasonalProgress;

static bool solar_boundaries_for_day_index(
    int64_t day_index,
    double latitude,
    double longitude,
    double solar_depression_degrees,
    int64_t *dawn_microseconds,
    int64_t *dusk_microseconds)
{
    int64_t day_start;
    int64_t noon;

    if (!infiltratr_i64_multiply_checked(
            day_index, MICROSECONDS_PER_DAY, &day_start) ||
        !infiltratr_i64_add_checked(
            day_start, MICROSECONDS_PER_DAY / 2, &noon)) {
        return false;
    }
    return solar_boundary_instants(
        noon, latitude, longitude, solar_depression_degrees,
        dawn_microseconds, dusk_microseconds);
}

/*
 * Map a civil instant onto two monotonic seasonal-unit coordinates. Every
 * complete dawn-to-dawn cycle contributes exactly daylight_parts daylight
 * units and night_parts night units, while the current partial unit is scaled
 * by that day's real astronomical dawn/dusk span. Taking the difference of two
 * coordinates therefore integrates an elapsed interval without pretending an
 * unequal Roman hora, Roman vigilia or Edo toki has a fixed SI duration.
 */
static bool seasonal_progress_at(int64_t unix_microseconds,
                                 double latitude,
                                 double longitude,
                                 double solar_depression_degrees,
                                 unsigned daylight_parts,
                                 unsigned night_parts,
                                 SeasonalProgress *progress)
{
    const int64_t approximate_day =
        floor_divide(unix_microseconds, MICROSECONDS_PER_DAY);
    int64_t best_dawn = INT64_MIN;
    int64_t best_dusk = INT64_MIN;
    int64_t best_day = 0;
    int offset;

    if (progress == NULL || daylight_parts == 0U || night_parts == 0U) {
        return false;
    }

    for (offset = -2; offset <= 2; ++offset) {
        int64_t candidate_day;
        int64_t dawn;
        int64_t dusk;

        if (!infiltratr_i64_add_checked(
                approximate_day, (int64_t)offset, &candidate_day) ||
            !solar_boundaries_for_day_index(
                candidate_day, latitude, longitude,
                solar_depression_degrees, &dawn, &dusk)) {
            continue;
        }
        if (dawn <= unix_microseconds && dawn > best_dawn) {
            best_dawn = dawn;
            best_dusk = dusk;
            best_day = candidate_day;
        }
    }

    if (best_dawn == INT64_MIN || best_dusk <= best_dawn) {
        return false;
    }

    if (unix_microseconds < best_dusk) {
        const long double fraction =
            (long double)(unix_microseconds - best_dawn) /
            (long double)(best_dusk - best_dawn);
        progress->daylight_units =
            (long double)best_day * daylight_parts +
            fraction * daylight_parts;
        progress->night_units =
            (long double)best_day * night_parts;
        return true;
    }

    {
        int64_t next_day;
        int64_t next_dawn;
        int64_t next_dusk;
        long double fraction;

        if (!infiltratr_i64_add_checked(best_day, INT64_C(1), &next_day) ||
            !solar_boundaries_for_day_index(
                next_day, latitude, longitude,
                solar_depression_degrees, &next_dawn, &next_dusk) ||
            next_dawn <= best_dusk) {
            return false;
        }
        (void)next_dusk;
        fraction =
            (long double)(unix_microseconds - best_dusk) /
            (long double)(next_dawn - best_dusk);
        progress->daylight_units =
            (long double)best_day * daylight_parts + daylight_parts;
        progress->night_units =
            (long double)best_day * night_parts +
            fraction * night_parts;
        return true;
    }
}

static bool seasonal_interval_progress(
    uint64_t elapsed_microseconds,
    int64_t end_unix_microseconds,
    double latitude,
    double longitude,
    double solar_depression_degrees,
    unsigned daylight_parts,
    unsigned night_parts,
    long double *daylight_units,
    long double *night_units)
{
    SeasonalProgress start;
    SeasonalProgress end;
    int64_t start_unix_microseconds;

    if (daylight_units == NULL || night_units == NULL ||
        end_unix_microseconds == INT64_MIN ||
        elapsed_microseconds > (uint64_t)INT64_MAX ||
        !infiltratr_i64_add_checked(
            end_unix_microseconds, -(int64_t)elapsed_microseconds,
            &start_unix_microseconds) ||
        !seasonal_progress_at(
            start_unix_microseconds, latitude, longitude,
            solar_depression_degrees, daylight_parts, night_parts, &start) ||
        !seasonal_progress_at(
            end_unix_microseconds, latitude, longitude,
            solar_depression_degrees, daylight_parts, night_parts, &end)) {
        return false;
    }

    *daylight_units = end.daylight_units - start.daylight_units;
    *night_units = end.night_units - start.night_units;
    if (*daylight_units < -1.0e-9L || *night_units < -1.0e-9L) {
        return false;
    }
    if (*daylight_units < 0.0L) *daylight_units = 0.0L;
    if (*night_units < 0.0L) *night_units = 0.0L;
    return true;
}

static bool seasonal_quantise(long double units,
                              uint64_t subdivisions,
                              bool include_fraction,
                              uint64_t *ticks)
{
    long double scaled;

    if (ticks == NULL || subdivisions == 0U ||
        !isfinite((double)units) || units < 0.0L) {
        return false;
    }
    scaled = include_fraction
        ? units * (long double)subdivisions
        : floorl(units) * (long double)subdivisions;
    if (scaled < 0.0L || scaled > (long double)UINT64_MAX) {
        return false;
    }
    *ticks = include_fraction
        ? (uint64_t)llroundl(scaled)
        : (uint64_t)scaled;
    return true;
}

static bool duration_partition(uint64_t phase_microseconds,
                               uint64_t units_per_day,
                               uint64_t *tick);

static bool format_duration_chinese_partition(
    uint64_t elapsed_microseconds,
    uint64_t units_per_day,
    const char *unit,
    bool vertical,
    char *buffer,
    size_t capacity,
    size_t *length)
{
    const uint64_t day_us = (uint64_t)MICROSECONDS_PER_DAY;
    const uint64_t days = elapsed_microseconds / day_us;
    const uint64_t phase = elapsed_microseconds % day_us;
    uint64_t units = 0U;
    const char *separator = vertical ? "\n" : " · ";

    if (unit == NULL ||
        !duration_partition(phase, units_per_day, &units)) {
        return false;
    }
    if (days != 0U && units != 0U) {
        return write_printf(buffer, capacity, length,
                            "%llu日%s%llu%s",
                            (unsigned long long)days, separator,
                            (unsigned long long)units, unit);
    }
    if (days != 0U) {
        return write_printf(buffer, capacity, length,
                            "%llu日", (unsigned long long)days);
    }
    return write_printf(buffer, capacity, length,
                        "%llu%s", (unsigned long long)units, unit);
}

static bool format_duration_roman_temporal(
    uint64_t elapsed_microseconds,
    int64_t end_unix_microseconds,
    bool show_seconds,
    bool vertical,
    double latitude,
    double longitude,
    char *buffer,
    size_t capacity,
    size_t *length)
{
    long double horae;
    long double vigiliae;
    uint64_t hora_twelfths;
    uint64_t vigilia_twelfths;
    uint64_t whole_days;
    uint64_t whole_horae;
    uint64_t whole_vigiliae;
    unsigned hora_unciae;
    unsigned vigilia_unciae;
    char day_part[48] = "";
    char hora_part[64] = "";
    char vigilia_part[64] = "";
    const char *separator = vertical ? "\n" : " · ";
    int written;

    if (!seasonal_interval_progress(
            elapsed_microseconds, end_unix_microseconds,
            latitude, longitude, 0.833, 12U, 4U,
            &horae, &vigiliae) ||
        !seasonal_quantise(
            horae, UINT64_C(12), show_seconds, &hora_twelfths) ||
        !seasonal_quantise(
            vigiliae, UINT64_C(12), show_seconds, &vigilia_twelfths)) {
        return false;
    }

    /*
     * Twelve daylight horae plus four night vigiliae make one complete Roman
     * civil cycle. Collapse complete paired cycles into dies before showing
     * the residual seasonal units, so long intervals remain immediately
     * understandable instead of becoming unbounded hora/vigilia totals.
     */
    whole_days = hora_twelfths / UINT64_C(144);
    if (vigilia_twelfths / UINT64_C(48) < whole_days) {
        whole_days = vigilia_twelfths / UINT64_C(48);
    }
    hora_twelfths -= whole_days * UINT64_C(144);
    vigilia_twelfths -= whole_days * UINT64_C(48);

    whole_horae = hora_twelfths / UINT64_C(12);
    whole_vigiliae = vigilia_twelfths / UINT64_C(12);
    hora_unciae = (unsigned)(hora_twelfths % UINT64_C(12));
    vigilia_unciae = (unsigned)(vigilia_twelfths % UINT64_C(12));

    if (whole_days != 0U) {
        written = snprintf(day_part, sizeof(day_part), "%llu dies",
                           (unsigned long long)whole_days);
        if (written < 0 || (size_t)written >= sizeof(day_part)) return false;
    }
    if (hora_twelfths != 0U) {
        written = show_seconds && hora_unciae != 0U
            ? snprintf(hora_part, sizeof(hora_part), "%llu %s %u unciae",
                       (unsigned long long)whole_horae,
                       whole_horae == 1U ? "hora" : "horae", hora_unciae)
            : snprintf(hora_part, sizeof(hora_part), "%llu %s",
                       (unsigned long long)whole_horae,
                       whole_horae == 1U ? "hora" : "horae");
        if (written < 0 || (size_t)written >= sizeof(hora_part)) return false;
    }
    if (vigilia_twelfths != 0U) {
        written = show_seconds && vigilia_unciae != 0U
            ? snprintf(vigilia_part, sizeof(vigilia_part),
                       "%llu %s %u unciae",
                       (unsigned long long)whole_vigiliae,
                       whole_vigiliae == 1U ? "vigilia" : "vigiliae",
                       vigilia_unciae)
            : snprintf(vigilia_part, sizeof(vigilia_part), "%llu %s",
                       (unsigned long long)whole_vigiliae,
                       whole_vigiliae == 1U ? "vigilia" : "vigiliae");
        if (written < 0 || (size_t)written >= sizeof(vigilia_part)) return false;
    }

    if (day_part[0] == '\0' && hora_part[0] == '\0' &&
        vigilia_part[0] == '\0') {
        return write_text(buffer, capacity, length, "0 horae");
    }
    if (day_part[0] != '\0' && hora_part[0] != '\0' &&
        vigilia_part[0] != '\0') {
        return write_printf(buffer, capacity, length, "%s%s%s%s%s",
                            day_part, separator, hora_part,
                            separator, vigilia_part);
    }
    if (day_part[0] != '\0' && hora_part[0] != '\0') {
        return write_printf(buffer, capacity, length, "%s%s%s",
                            day_part, separator, hora_part);
    }
    if (day_part[0] != '\0' && vigilia_part[0] != '\0') {
        return write_printf(buffer, capacity, length, "%s%s%s",
                            day_part, separator, vigilia_part);
    }
    if (hora_part[0] != '\0' && vigilia_part[0] != '\0') {
        return write_printf(buffer, capacity, length, "%s%s%s",
                            hora_part, separator, vigilia_part);
    }
    if (day_part[0] != '\0') return write_text(buffer, capacity, length, day_part);
    if (hora_part[0] != '\0') return write_text(buffer, capacity, length, hora_part);
    return write_text(buffer, capacity, length, vigilia_part);
}

static bool format_duration_japanese_temporal(
    uint64_t elapsed_microseconds,
    int64_t end_unix_microseconds,
    bool show_seconds,
    bool vertical,
    double solar_depression_degrees,
    double latitude,
    double longitude,
    char *buffer,
    size_t capacity,
    size_t *length)
{
    long double daylight_toki;
    long double night_toki;
    uint64_t half_toki;
    uint64_t days;
    uint64_t remainder;
    uint64_t whole_toki;
    const char *separator = " · ";

    if (!seasonal_interval_progress(
            elapsed_microseconds, end_unix_microseconds,
            latitude, longitude, solar_depression_degrees, 6U, 6U,
            &daylight_toki, &night_toki) ||
        !seasonal_quantise(
            daylight_toki + night_toki, UINT64_C(2),
            show_seconds, &half_toki)) {
        return false;
    }

    /* Twelve unequal toki form one complete day/night cycle. */
    days = half_toki / UINT64_C(24);
    remainder = half_toki % UINT64_C(24);
    whole_toki = remainder / UINT64_C(2);
    if (vertical) {
        separator = "\n";
    }

    if (remainder == 0U) {
        return days == 0U
            ? write_text(buffer, capacity, length, "0刻")
            : write_printf(buffer, capacity, length,
                           "%llu日", (unsigned long long)days);
    }
    if (show_seconds && (remainder % UINT64_C(2)) != 0U) {
        if (days != 0U) {
            return whole_toki == 0U
                ? write_printf(buffer, capacity, length,
                               "%llu日%s半刻",
                               (unsigned long long)days, separator)
                : write_printf(buffer, capacity, length,
                               "%llu日%s%llu刻半",
                               (unsigned long long)days, separator,
                               (unsigned long long)whole_toki);
        }
        return whole_toki == 0U
            ? write_text(buffer, capacity, length, "半刻")
            : write_printf(buffer, capacity, length,
                           "%llu刻半", (unsigned long long)whole_toki);
    }
    return days != 0U
        ? write_printf(buffer, capacity, length,
                       "%llu日%s%llu刻",
                       (unsigned long long)days, separator,
                       (unsigned long long)whole_toki)
        : write_printf(buffer, capacity, length,
                       "%llu刻", (unsigned long long)whole_toki);
}

static bool format_duration_babylonian_ancient(
    uint64_t elapsed_microseconds,
    bool vertical,
    char *buffer,
    size_t capacity,
    size_t *length)
{
    const uint64_t ush_us = UINT64_C(240) * UINT64_C(1000000);
    const uint64_t beru_us = UINT64_C(30) * ush_us;
    const uint64_t umu_us = UINT64_C(12) * beru_us;
    const uint64_t umu = elapsed_microseconds / umu_us;
    const uint64_t phase = elapsed_microseconds % umu_us;
    const uint64_t beru = phase / beru_us;
    const uint64_t ush = (phase % beru_us) / ush_us;
    const char *separator = vertical ? "\n" : " · ";

    if (umu != 0U) {
        return write_printf(buffer, capacity, length,
                            "%llu ūmu%s%llu bēru%s%02llu UŠ",
                            (unsigned long long)umu, separator,
                            (unsigned long long)beru, separator,
                            (unsigned long long)ush);
    }
    return write_printf(buffer, capacity, length,
                        "%llu bēru%s%02llu UŠ",
                        (unsigned long long)beru, separator,
                        (unsigned long long)ush);
}

typedef enum SolarOrigin {
    SOLAR_ORIGIN_SUNRISE,
    SOLAR_ORIGIN_SUNSET
} SolarOrigin;

static bool solar_origin_window(int64_t unix_microseconds,
                                double latitude,
                                double longitude,
                                SolarOrigin origin,
                                int64_t *previous)
{
    int64_t best_previous = INT64_MIN;
    int offset;

    if (previous == NULL) {
        return false;
    }

    for (offset = -2; offset <= 2; ++offset) {
        int64_t delta;
        int64_t sample;
        int64_t dawn;
        int64_t dusk;
        int64_t candidate;

        if (!infiltratr_i64_multiply_checked(
                (int64_t)offset, MICROSECONDS_PER_DAY, &delta) ||
            !infiltratr_i64_add_checked(
                unix_microseconds, delta, &sample) ||
            !solar_boundary_instants(
                sample, latitude, longitude, 0.833, &dawn, &dusk)) {
            continue;
        }

        candidate = origin == SOLAR_ORIGIN_SUNRISE ? dawn : dusk;
        if (candidate <= unix_microseconds && candidate > best_previous) {
            best_previous = candidate;
        }
    }

    if (best_previous == INT64_MIN) {
        return false;
    }
    *previous = best_previous;
    return true;
}

static bool format_equal_hours(char *buffer, size_t capacity, size_t *length,
                               int64_t unix_microseconds, bool show_seconds,
                               bool vertical, double latitude, double longitude,
                               SolarOrigin origin, const char *suffix)
{
    int64_t start;
    int64_t elapsed_microseconds;
    int64_t whole_seconds;
    int hour;
    int minute;
    int second;
    const char *separator = vertical ? "\n" : ":";

    if (!solar_origin_window(
            unix_microseconds, latitude, longitude, origin, &start)) {
        return write_printf(buffer, capacity, length,
                            vertical ? "N/A\n%s" : "N/A %s", suffix);
    }

    elapsed_microseconds = unix_microseconds - start;
    whole_seconds =
        floor_divide(elapsed_microseconds, MICROSECONDS_PER_SECOND);
    hour = (int)(whole_seconds / SECONDS_PER_HOUR);
    minute =
        (int)((whole_seconds / SECONDS_PER_MINUTE) % MINUTES_PER_HOUR);
    second = (int)(whole_seconds % SECONDS_PER_MINUTE);

    if (show_seconds) {
        return write_printf(buffer, capacity, length,
                            "%02d%s%02d%s%02d%s%s",
                            hour, separator, minute, separator, second,
                            vertical ? "\n" : " ", suffix);
    }
    return write_printf(buffer, capacity, length,
                        "%02d%s%02d%s%s",
                        hour, separator, minute,
                        vertical ? "\n" : " ", suffix);
}

/*
 * Reconstruct the Nürnberger Uhr's Wendetag rule for the configured place.
 * Equal 60-minute hours are assigned as a whole-number block to daylight and
 * the remainder to night. The allocation changes only when astronomical day
 * length crosses the next half-hour threshold. This reproduces the discrete
 * Wendetag behaviour while allowing for the historical fact that nearby towns
 * used slightly different changeover dates.
 */
static bool nuremberg_local_hour_allocation(int64_t unix_microseconds,
                                            double latitude,
                                            unsigned *day_hours,
                                            unsigned *night_hours,
                                            long double *day_start,
                                            long double *day_end)
{
    long double dawn;
    long double dusk;
    long double rounded;
    unsigned day_count;

    if (day_hours == NULL || night_hours == NULL ||
        day_start == NULL || day_end == NULL ||
        !solar_day_boundaries(
            unix_microseconds, latitude, 0.833, &dawn, &dusk)) {
        return false;
    }

    rounded = floorl(
        (dusk - dawn) / (long double)SECONDS_PER_HOUR + 0.5L);
    if (rounded < 8.0L) rounded = 8.0L;
    if (rounded > 16.0L) rounded = 16.0L;
    day_count = (unsigned)rounded;

    *day_hours = day_count;
    *night_hours = 24U - day_count;
    *day_start =
        ((long double)(24U - day_count) / 2.0L) * SECONDS_PER_HOUR;
    *day_end = *day_start +
        (long double)day_count * SECONDS_PER_HOUR;
    return true;
}

static bool nuremberg_historical_hour_allocation(
    int64_t unix_microseconds,
    unsigned *day_hours,
    unsigned *night_hours,
    long double *day_start,
    long double *day_end)
{
    /*
     * Nürnberg's 1488 Wendetage were recorded in the Julian calendar.
     * Converting those dates to the proleptic Gregorian calendar of that
     * epoch adds nine days. The allocation therefore changes on the modern
     * seasonal equivalents below, not at every actual sunrise/sunset.
     */
    static const int thresholds[] = {
        16, 37, 54, 71, 87, 104, 122, 144,
        201, 223, 241, 257, 274, 290, 309, 329
    };
    static const unsigned allocations[] = {
        9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U,
        15U, 14U, 13U, 12U, 11U, 10U, 9U, 8U
    };
    int day_of_year;
    bool leap = false;
    unsigned count = 8U;
    size_t index;

    if (day_hours == NULL || night_hours == NULL ||
        day_start == NULL || day_end == NULL ||
        !gregorian_day_of_year(unix_microseconds, &day_of_year, &leap)) {
        return false;
    }

    for (index = 0U; index < INFILTRATR_ARRAY_LENGTH(thresholds); ++index) {
        int threshold = thresholds[index];
        if (leap && threshold >= 71) {
            threshold += 1;
        }
        if (day_of_year < threshold) {
            break;
        }
        count = allocations[index];
    }

    *day_hours = count;
    *night_hours = 24U - count;
    *day_start =
        ((long double)(24U - count) / 2.0L) * SECONDS_PER_HOUR;
    *day_end = *day_start + (long double)count * SECONDS_PER_HOUR;
    return true;
}

static bool format_nuremberg_hours(char *buffer,
                                   size_t capacity,
                                   size_t *length,
                                   int64_t unix_microseconds,
                                   bool show_seconds,
                                   bool vertical,
                                   bool historical,
                                   double latitude,
                                   double longitude)
{
    static const double nuremberg_longitude = 11.0767;
    unsigned day_hours;
    unsigned night_hours;
    long double day_start;
    long double day_end;
    const double clock_longitude = historical ? nuremberg_longitude : longitude;
    const long double solar =
        apparent_solar_seconds(unix_microseconds, clock_longitude);
    long double elapsed;
    int hour;
    int minute;
    int second;
    const char *suffix;
    const char *separator = vertical ? "\n" : ":";
    bool ok;

    ok = historical
        ? nuremberg_historical_hour_allocation(
            unix_microseconds, &day_hours, &night_hours,
            &day_start, &day_end)
        : nuremberg_local_hour_allocation(
            unix_microseconds, latitude,
            &day_hours, &night_hours, &day_start, &day_end);
    if (!ok) {
        return write_text(buffer, capacity, length,
                          vertical ? "N/A\nNUR" : "N/A NUR");
    }

    if (solar >= day_start && solar < day_end) {
        elapsed = solar - day_start;
        suffix = historical ? "NUR-D" : "NUR-LD";
    } else {
        elapsed = solar >= day_end
            ? solar - day_end
            : (long double)SECONDS_PER_DAY - day_end + solar;
        suffix = historical ? "NUR-N" : "NUR-LN";
    }
    (void)day_hours;
    (void)night_hours;

    hour = (int)floorl(elapsed / SECONDS_PER_HOUR);
    minute = (int)floorl(
        fmodl(elapsed, (long double)SECONDS_PER_HOUR) /
        SECONDS_PER_MINUTE);
    second = (int)floorl(
        fmodl(elapsed, (long double)SECONDS_PER_MINUTE));

    if (show_seconds) {
        return write_printf(buffer, capacity, length,
                            "%02d%s%02d%s%02d%s%s",
                            hour, separator, minute, separator, second,
                            vertical ? "\n" : " ", suffix);
    }
    return write_printf(buffer, capacity, length,
                        "%02d%s%02d%s%s",
                        hour, separator, minute,
                        vertical ? "\n" : " ", suffix);
}

static bool mode_location_valid(const InfiltratrTemporalClockModeInfo *info,
                                bool location_configured,
                                double latitude,
                                double longitude)
{
    if (info == NULL) {
        return false;
    }
    if ((info->requires_latitude || info->requires_longitude) &&
        !location_configured) {
        return false;
    }
    return (!info->requires_latitude ||
            (isfinite(latitude) && latitude >= -90.0 && latitude <= 90.0)) &&
           (!info->requires_longitude ||
            (isfinite(longitude) && longitude >= -180.0 &&
             longitude <= 180.0));
}


static bool format_duration_hms(uint64_t whole_seconds,
                                bool show_seconds,
                                bool vertical,
                                const char *suffix,
                                char *buffer,
                                size_t capacity,
                                size_t *length)
{
    const uint64_t days = whole_seconds / UINT64_C(86400);
    const uint64_t phase = whole_seconds % UINT64_C(86400);
    const unsigned hour = (unsigned)(phase / UINT64_C(3600));
    const unsigned minute =
        (unsigned)((phase / UINT64_C(60)) % UINT64_C(60));
    const unsigned second = (unsigned)(phase % UINT64_C(60));
    const char *separator = vertical ? "\n" : ":";
    const char *field_separator = vertical ? "\n" : " ";
    const bool has_suffix = suffix != NULL && suffix[0] != '\0';

    if (days > 0U) {
        if (show_seconds) {
            return write_printf(
                buffer, capacity, length,
                has_suffix
                    ? (vertical ? "%llud\n%02u%s%02u%s%02u\n%s"
                                : "%llud %02u%s%02u%s%02u %s")
                    : (vertical ? "%llud\n%02u%s%02u%s%02u"
                                : "%llud %02u%s%02u%s%02u"),
                (unsigned long long)days,
                hour, separator, minute, separator, second,
                has_suffix ? suffix : "");
        }
        return write_printf(
            buffer, capacity, length,
            has_suffix
                ? (vertical ? "%llud\n%02u%s%02u\n%s"
                            : "%llud %02u%s%02u %s")
                : (vertical ? "%llud\n%02u%s%02u"
                            : "%llud %02u%s%02u"),
            (unsigned long long)days,
            hour, separator, minute,
            has_suffix ? suffix : "");
    }

    if (show_seconds) {
        return write_printf(
            buffer, capacity, length,
            has_suffix
                ? (vertical ? "%02u%s%02u%s%02u\n%s"
                            : "%02u%s%02u%s%02u %s")
                : "%02u%s%02u%s%02u",
            hour, separator, minute, separator, second,
            has_suffix ? suffix : "");
    }

    if (has_suffix) {
        return write_printf(
            buffer, capacity, length,
            vertical ? "%02u%s%02u\n%s" : "%02u%s%02u %s",
            hour, separator, minute, suffix);
    }
    (void)field_separator;
    return write_printf(
        buffer, capacity, length, "%02u%s%02u",
        hour, separator, minute);
}

static bool duration_partition(uint64_t phase_microseconds,
                               uint64_t ticks_per_day,
                               uint64_t *tick)
{
    return infiltratr_cycle_partition_u64(
        phase_microseconds,
        (uint64_t)MICROSECONDS_PER_DAY,
        ticks_per_day,
        tick,
        NULL);
}

static bool format_duration_decimal(uint64_t elapsed_microseconds,
                                    bool show_seconds,
                                    bool vertical,
                                    char *buffer,
                                    size_t capacity,
                                    size_t *length)
{
    const uint64_t day_us = (uint64_t)MICROSECONDS_PER_DAY;
    const uint64_t days = elapsed_microseconds / day_us;
    const uint64_t phase = elapsed_microseconds % day_us;
    const uint64_t ticks_per_day =
        show_seconds ? DECIMAL_SECONDS_PER_DAY : UINT64_C(1000);
    uint64_t tick = 0U;
    uint64_t hour;
    uint64_t minute;
    uint64_t second = 0U;
    const char *separator = vertical ? "\n" : ":";

    if (!duration_partition(phase, ticks_per_day, &tick)) {
        return false;
    }
    if (show_seconds) {
        hour = tick / UINT64_C(10000);
        minute = (tick / UINT64_C(100)) % UINT64_C(100);
        second = tick % UINT64_C(100);
    } else {
        hour = tick / UINT64_C(100);
        minute = tick % UINT64_C(100);
    }

    if (days > 0U) {
        return show_seconds
            ? write_printf(buffer, capacity, length,
                           vertical ? "%llud\n%llu%s%02llu%s%02llu"
                                    : "%llud %llu%s%02llu%s%02llu",
                           (unsigned long long)days,
                           (unsigned long long)hour, separator,
                           (unsigned long long)minute, separator,
                           (unsigned long long)second)
            : write_printf(buffer, capacity, length,
                           vertical ? "%llud\n%llu%s%02llu"
                                    : "%llud %llu%s%02llu",
                           (unsigned long long)days,
                           (unsigned long long)hour, separator,
                           (unsigned long long)minute);
    }
    return show_seconds
        ? write_printf(buffer, capacity, length, "%llu%s%02llu%s%02llu",
                       (unsigned long long)hour, separator,
                       (unsigned long long)minute, separator,
                       (unsigned long long)second)
        : write_printf(buffer, capacity, length, "%llu%s%02llu",
                       (unsigned long long)hour, separator,
                       (unsigned long long)minute);
}

static bool format_duration_binary(uint64_t elapsed_microseconds,
                                   bool show_seconds,
                                   bool vertical,
                                   char *buffer,
                                   size_t capacity,
                                   size_t *length)
{
    const uint64_t whole_seconds =
        elapsed_microseconds / (uint64_t)MICROSECONDS_PER_SECOND;
    const uint64_t days = whole_seconds / UINT64_C(86400);
    const uint64_t phase = whole_seconds % UINT64_C(86400);
    const unsigned hour = (unsigned)(phase / UINT64_C(3600));
    const unsigned minute =
        (unsigned)((phase / UINT64_C(60)) % UINT64_C(60));
    const unsigned second = (unsigned)(phase % UINT64_C(60));
    char hour_bits[6];
    char minute_bits[7];
    char second_bits[7];
    const char *separator = vertical ? "\n" : ":";
    unsigned bit;

    for (bit = 0U; bit < 5U; ++bit) {
        hour_bits[bit] =
            (hour & (1U << (4U - bit))) != 0U ? '1' : '0';
    }
    hour_bits[5] = '\0';
    for (bit = 0U; bit < 6U; ++bit) {
        minute_bits[bit] =
            (minute & (1U << (5U - bit))) != 0U ? '1' : '0';
        second_bits[bit] =
            (second & (1U << (5U - bit))) != 0U ? '1' : '0';
    }
    minute_bits[6] = '\0';
    second_bits[6] = '\0';

    if (days > 0U) {
        return show_seconds
            ? write_printf(
                buffer, capacity, length,
                vertical ? "%llud\n%s%s%s%s%s"
                         : "%llud %s%s%s%s%s",
                (unsigned long long)days,
                hour_bits, separator, minute_bits, separator, second_bits)
            : write_printf(
                buffer, capacity, length,
                vertical ? "%llud\n%s%s%s" : "%llud %s%s%s",
                (unsigned long long)days,
                hour_bits, separator, minute_bits);
    }
    return show_seconds
        ? write_printf(buffer, capacity, length, "%s%s%s%s%s",
                       hour_bits, separator, minute_bits, separator, second_bits)
        : write_printf(buffer, capacity, length, "%s%s%s",
                       hour_bits, separator, minute_bits);
}

static bool format_duration_day_ticks(uint64_t elapsed_microseconds,
                                      uint64_t ticks_per_day,
                                      const char *prefix,
                                      const char *suffix,
                                      unsigned width,
                                      bool hexadecimal,
                                      bool vertical,
                                      char *buffer,
                                      size_t capacity,
                                      size_t *length)
{
    const uint64_t day_us = (uint64_t)MICROSECONDS_PER_DAY;
    const uint64_t days = elapsed_microseconds / day_us;
    const uint64_t phase = elapsed_microseconds % day_us;
    uint64_t tick = 0U;

    if (!duration_partition(phase, ticks_per_day, &tick)) {
        return false;
    }

    if (hexadecimal) {
        if (days > 0U) {
            return write_printf(
                buffer, capacity, length,
                vertical ? "%llud\n%0*llX" : "%llud %0*llX",
                (unsigned long long)days, (int)width,
                (unsigned long long)tick);
        }
        return write_printf(
            buffer, capacity, length, "%0*llX",
            (int)width, (unsigned long long)tick);
    }

    if (days > 0U) {
        return write_printf(
            buffer, capacity, length,
            vertical ? "%llud\n%s%0*llu%s" : "%llud %s%0*llu%s",
            (unsigned long long)days,
            prefix != NULL ? prefix : "",
            (int)width, (unsigned long long)tick,
            suffix != NULL ? suffix : "");
    }
    return write_printf(
        buffer, capacity, length, "%s%0*llu%s",
        prefix != NULL ? prefix : "",
        (int)width, (unsigned long long)tick,
        suffix != NULL ? suffix : "");
}

static bool format_duration_julian(const char *mode,
                                   uint64_t elapsed_microseconds,
                                   bool show_seconds,
                                   bool vertical,
                                   char *buffer,
                                   size_t capacity,
                                   size_t *length)
{
    const uint64_t day_us = (uint64_t)MICROSECONDS_PER_DAY;
    const uint64_t days = elapsed_microseconds / day_us;
    const uint64_t phase = elapsed_microseconds % day_us;
    const uint64_t scale =
        show_seconds ? UINT64_C(100000) : UINT64_C(1000);
    uint64_t fraction = 0U;
    const int digits = show_seconds ? 5 : 3;
    const char *label =
        strcmp(mode, "modified-julian") == 0 ? "MJD" : "JD";

    if (!duration_partition(phase, scale, &fraction)) {
        return false;
    }
    return write_printf(
        buffer, capacity, length,
        vertical ? "%s\n+%llu.%0*llu d" : "%s +%llu.%0*llu d",
        label, (unsigned long long)days, digits,
        (unsigned long long)fraction);
}

static bool format_duration_internet(uint64_t elapsed_microseconds,
                                     bool show_seconds,
                                     bool vertical,
                                     char *buffer,
                                     size_t capacity,
                                     size_t *length)
{
    const uint64_t day_us = (uint64_t)MICROSECONDS_PER_DAY;
    const uint64_t days = elapsed_microseconds / day_us;
    const uint64_t phase = elapsed_microseconds % day_us;
    const uint64_t ticks_per_day =
        show_seconds
            ? INTERNET_TICKS_PER_DAY * UINT64_C(100)
            : INTERNET_TICKS_PER_DAY;
    uint64_t tick = 0U;

    if (!duration_partition(phase, ticks_per_day, &tick)) {
        return false;
    }
    if (days > 0U) {
        return show_seconds
            ? write_printf(
                buffer, capacity, length,
                vertical ? "%llud\n@%03llu.%02llu"
                         : "%llud @%03llu.%02llu",
                (unsigned long long)days,
                (unsigned long long)(tick / UINT64_C(100)),
                (unsigned long long)(tick % UINT64_C(100)))
            : write_printf(
                buffer, capacity, length,
                vertical ? "%llud\n@%03llu" : "%llud @%03llu",
                (unsigned long long)days,
                (unsigned long long)tick);
    }
    return show_seconds
        ? write_printf(buffer, capacity, length, "@%03llu.%02llu",
                       (unsigned long long)(tick / UINT64_C(100)),
                       (unsigned long long)(tick % UINT64_C(100)))
        : write_printf(buffer, capacity, length, "@%03llu",
                       (unsigned long long)tick);
}

static bool format_duration_sidereal(uint64_t elapsed_microseconds,
                                     bool show_seconds,
                                     bool vertical,
                                     char *buffer,
                                     size_t capacity,
                                     size_t *length)
{
    const long double sidereal_seconds =
        (long double)elapsed_microseconds /
        (long double)MICROSECONDS_PER_SECOND *
        SIDEREAL_RATE_PER_SI_SECOND;

    if (!isfinite((double)sidereal_seconds) ||
        sidereal_seconds < 0.0L ||
        sidereal_seconds > (long double)UINT64_MAX) {
        return false;
    }
    return format_duration_hms(
        (uint64_t)floorl(sidereal_seconds),
        show_seconds, vertical, "LST",
        buffer, capacity, length);
}

static bool format_duration_apparent_solar(uint64_t elapsed_microseconds,
                                           int64_t end_unix_microseconds,
                                           bool show_seconds,
                                           bool vertical,
                                           char *buffer,
                                           size_t capacity,
                                           size_t *length)
{
    double start_equation_minutes = 0.0;
    double end_equation_minutes = 0.0;
    int64_t start_unix_microseconds;
    long double apparent_microseconds;
    long double apparent_seconds;

    if (elapsed_microseconds > (uint64_t)INT64_MAX ||
        end_unix_microseconds <
            INT64_MIN + (int64_t)elapsed_microseconds) {
        return format_duration_hms(
            elapsed_microseconds / (uint64_t)MICROSECONDS_PER_SECOND,
            show_seconds, vertical, "SOL",
            buffer, capacity, length);
    }

    start_unix_microseconds =
        end_unix_microseconds - (int64_t)elapsed_microseconds;
    if (!solar_terms(start_unix_microseconds,
                     &start_equation_minutes, NULL) ||
        !solar_terms(end_unix_microseconds,
                     &end_equation_minutes, NULL)) {
        return false;
    }

    apparent_microseconds =
        (long double)elapsed_microseconds +
        ((long double)end_equation_minutes -
         (long double)start_equation_minutes) *
            60.0L * (long double)MICROSECONDS_PER_SECOND;
    if (apparent_microseconds < 0.0L) {
        apparent_microseconds = 0.0L;
    }
    apparent_seconds =
        floorl(apparent_microseconds /
               (long double)MICROSECONDS_PER_SECOND);
    if (apparent_seconds > (long double)UINT64_MAX) {
        return false;
    }
    return format_duration_hms(
        (uint64_t)apparent_seconds,
        show_seconds, vertical, "SOL",
        buffer, capacity, length);
}

static bool format_duration_ghati(uint64_t elapsed_microseconds,
                                  bool vertical,
                                  char *buffer,
                                  size_t capacity,
                                  size_t *length)
{
    const uint64_t day_us = (uint64_t)MICROSECONDS_PER_DAY;
    const uint64_t days = elapsed_microseconds / day_us;
    const uint64_t phase = elapsed_microseconds % day_us;
    uint64_t pala = 0U;
    uint64_t ghati;
    const char *separator = vertical ? "\n" : " · ";

    if (!duration_partition(phase, VIGHATI_PER_DAY, &pala)) {
        return false;
    }
    ghati = pala / UINT64_C(60);
    pala %= UINT64_C(60);

    if (days > 0U) {
        return write_printf(
            buffer, capacity, length,
            "%llu dina%s%llu ghaṭī%s%llu pala",
            (unsigned long long)days, separator,
            (unsigned long long)ghati, separator,
            (unsigned long long)pala);
    }
    return write_printf(
        buffer, capacity, length,
        "%llu ghaṭī%s%llu pala",
        (unsigned long long)ghati, separator,
        (unsigned long long)pala);
}

bool infiltratr_temporal_format_duration_mode(
    const char *mode,
    uint64_t elapsed_microseconds,
    int64_t end_unix_microseconds,
    bool show_seconds,
    bool vertical,
    bool location_configured,
    double latitude,
    double longitude,
    char *buffer,
    size_t capacity,
    size_t *length)
{
    const InfiltratrTemporalClockModeInfo *info;
    const uint64_t whole_seconds =
        elapsed_microseconds / (uint64_t)MICROSECONDS_PER_SECOND;

    if (mode == NULL || buffer == NULL || capacity == 0U) {
        return false;
    }
    buffer[0] = '\0';
    info = infiltratr_temporal_clock_mode_find(mode);
    if (info == NULL ||
        !mode_location_valid(
            info, location_configured, latitude, longitude)) {
        return false;
    }

    if (strcmp(mode, "standard") == 0 ||
        strcmp(mode, "standard-24") == 0 ||
        strcmp(mode, "standard-12") == 0) {
        return format_duration_hms(
            whole_seconds, show_seconds, vertical, NULL,
            buffer, capacity, length);
    }

    if (strcmp(mode, "roman-temporal") == 0 ||
        strcmp(mode, "japanese-temporal") == 0 ||
        strcmp(mode, "japanese-temporal-early") == 0) {
        if (!location_configured ||
            !isfinite(latitude) || !isfinite(longitude)) {
            return false;
        }
        /*
         * Seasonal civil durations are meaningful only when anchored to a real
         * interval. For abstract accumulated quantities there is no historical
         * seasonal unit to apply honestly, so retain SI duration and label it.
         */
        if (end_unix_microseconds == INT64_MIN) {
            return format_duration_hms(
                whole_seconds, show_seconds, vertical, "SI",
                buffer, capacity, length);
        }
        if (strcmp(mode, "roman-temporal") == 0) {
            return format_duration_roman_temporal(
                elapsed_microseconds, end_unix_microseconds,
                show_seconds, vertical, latitude, longitude,
                buffer, capacity, length);
        }
        return format_duration_japanese_temporal(
            elapsed_microseconds, end_unix_microseconds,
            show_seconds, vertical,
            strcmp(mode, "japanese-temporal") == 0
                ? 7.0 + 21.0 / 60.0 + 40.0 / 3600.0
                : 0.833,
            latitude, longitude,
            buffer, capacity, length);
    }

    if (strcmp(mode, "babylonian-ancient") == 0) {
        return format_duration_babylonian_ancient(
            elapsed_microseconds, vertical,
            buffer, capacity, length);
    }

    if (strcmp(mode, "decimal") == 0) {
        return format_duration_decimal(
            elapsed_microseconds, show_seconds, vertical,
            buffer, capacity, length);
    }

    if (strcmp(mode, "internet") == 0) {
        return format_duration_internet(
            elapsed_microseconds, show_seconds, vertical,
            buffer, capacity, length);
    }

    if (strcmp(mode, "unix") == 0) {
        return write_printf(
            buffer, capacity, length, "%llu s",
            (unsigned long long)whole_seconds);
    }

    if (strcmp(mode, "binary") == 0) {
        return format_duration_binary(
            elapsed_microseconds, show_seconds, vertical,
            buffer, capacity, length);
    }

    if (strcmp(mode, "hexadecimal") == 0) {
        return format_duration_day_ticks(
            elapsed_microseconds, HEX_TICKS_PER_DAY,
            NULL, NULL, 4U, true, vertical,
            buffer, capacity, length);
    }

    if (strcmp(mode, "julian") == 0 ||
        strcmp(mode, "modified-julian") == 0) {
        return format_duration_julian(
            mode, elapsed_microseconds, show_seconds, vertical,
            buffer, capacity, length);
    }

    if (strcmp(mode, "sidereal") == 0) {
        return format_duration_sidereal(
            elapsed_microseconds, show_seconds, vertical,
            buffer, capacity, length);
    }

    if (strcmp(mode, "solar") == 0) {
        if (!location_configured || !isfinite(longitude)) {
            return false;
        }
        return format_duration_apparent_solar(
            elapsed_microseconds, end_unix_microseconds,
            show_seconds, vertical, buffer, capacity, length);
    }

    if (strcmp(mode, "mean-solar") == 0) {
        if (!location_configured || !isfinite(longitude)) {
            return false;
        }
        return format_duration_hms(
            whole_seconds, show_seconds, vertical, "LMT",
            buffer, capacity, length);
    }

    if (strcmp(mode, "chinese-time") == 0) {
        return format_duration_chinese_partition(
            elapsed_microseconds, CHINESE_DOUBLE_HOURS_PER_DAY,
            "時辰", vertical, buffer, capacity, length);
    }

    if (strcmp(mode, "chinese-ke") == 0) {
        return format_duration_chinese_partition(
            elapsed_microseconds, CHINESE_KE_PER_DAY,
            "刻", vertical, buffer, capacity, length);
    }

    if (strcmp(mode, "italian-hours") == 0) {
        if (!location_configured ||
            !isfinite(latitude) || !isfinite(longitude)) {
            return false;
        }
        return format_duration_hms(
            whole_seconds, show_seconds, vertical, "IT",
            buffer, capacity, length);
    }

    if (strcmp(mode, "babylonian-hours") == 0) {
        if (!location_configured ||
            !isfinite(latitude) || !isfinite(longitude)) {
            return false;
        }
        return format_duration_hms(
            whole_seconds, show_seconds, vertical, "BAB",
            buffer, capacity, length);
    }

    if (strcmp(mode, "indian-ghati") == 0) {
        if (!location_configured ||
            !isfinite(latitude) || !isfinite(longitude)) {
            return false;
        }
        return format_duration_ghati(
            elapsed_microseconds, vertical,
            buffer, capacity, length);
    }

    if (strcmp(mode, "nuremberg-hours") == 0) {
        return format_duration_hms(
            whole_seconds, show_seconds, vertical, "NUR",
            buffer, capacity, length);
    }

    if (strcmp(mode, "nuremberg-solar") == 0) {
        if (!location_configured ||
            !isfinite(latitude) || !isfinite(longitude)) {
            return false;
        }
        return format_duration_hms(
            whole_seconds, show_seconds, vertical, "NUR-L",
            buffer, capacity, length);
    }

    return false;
}

/* Names are transliterated proper names, not animal/help labels. Wording
 * belongs to the selected language; the shared phase calculation stays below. */
#define CLOCK_WORDS(coarse, fine, hour, first, second, watch, ke, twelfth, twelfths) \
    { "chinese-coarse", coarse }, { "chinese-fine", fine }, \
    { "hour", hour }, { "first", first }, { "second", second }, \
    { "watch", watch }, { "ke", ke }, \
    { "twelfth", twelfth }, { "twelfths", twelfths }
#define PINYIN_BRANCHES \
    { "zi", "Zǐ" }, { "chou", "Chǒu" }, { "yin", "Yín" }, \
    { "mao", "Mǎo" }, { "chen", "Chén" }, { "si", "Sì" }, \
    { "wu", "Wǔ" }, { "wei", "Wèi" }, { "shen", "Shēn" }, \
    { "you", "Yǒu" }, { "xu", "Xū" }, { "hai", "Hài" }
#define NATIVE_BRANCHES \
    { "zi", "子" }, { "chou", "丑" }, { "yin", "寅" }, \
    { "mao", "卯" }, { "chen", "辰" }, { "si", "巳" }, \
    { "wu", "午" }, { "wei", "未" }, { "shen", "申" }, \
    { "you", "酉" }, { "xu", "戌" }, { "hai", "亥" }
#define JAPANESE_BRANCHES \
    { "jp-ne", "Ne" }, { "jp-ushi", "Ushi" }, { "jp-tora", "Tora" }, \
    { "jp-u", "U" }, { "jp-tatsu", "Tatsu" }, { "jp-mi", "Mi" }, \
    { "jp-uma", "Uma" }, { "jp-hitsuji", "Hitsuji" }, { "jp-saru", "Saru" }, \
    { "jp-tori", "Tori" }, { "jp-inu", "Inu" }, { "jp-i", "I" }
#define JAPANESE_NATIVE_BRANCHES \
    { "jp-ne", "子" }, { "jp-ushi", "丑" }, { "jp-tora", "寅" }, \
    { "jp-u", "卯" }, { "jp-tatsu", "辰" }, { "jp-mi", "巳" }, \
    { "jp-uma", "午" }, { "jp-hitsuji", "未" }, { "jp-saru", "申" }, \
    { "jp-tori", "酉" }, { "jp-inu", "戌" }, { "jp-i", "亥" }
#define EDO_WORDS(horizontal, vertical) \
    { "edo", horizontal }, { "edo-vertical", vertical }
/* Fixed AM/PM markers, not flexible morning/evening periods. CLDR uses
 * AM/PM in several European languages too; East Asian markers precede time. */
#define TWELVE_WORDS(am, pm, horizontal, vertical) \
    { "am", am }, { "pm", pm }, \
    { "twelve", horizontal }, { "twelve-vertical", vertical }

static const InfiltratrI18nEntry clock_en[] = {
    CLOCK_WORDS("{branch} hour", "{branch}, {half}", "Hour",
                "first half", "second half", "Watch", "kè", "twelfth", "twelfths"),
    PINYIN_BRANCHES, JAPANESE_BRANCHES,
    EDO_WORDS("{branch} · {bell} bells", "{branch}\n{bell} bells"),
    TWELVE_WORDS("AM", "PM", "{time} {period}", "{time}\n{period}")
};
static const InfiltratrI18nEntry clock_de[] = {
    CLOCK_WORDS("{branch}-Stunde", "{branch}, {half}", "Stunde",
                "erste Hälfte", "zweite Hälfte", "Wache", "kè", "Zwölftel", "Zwölftel"),
    EDO_WORDS("{branch} · {bell} Schläge", "{branch}\n{bell} Schläge"),
    TWELVE_WORDS("AM", "PM", "{time} {period}", "{time}\n{period}")
};
static const InfiltratrI18nEntry clock_es[] = {
    CLOCK_WORDS("Hora {branch}", "{branch}, {half}", "Hora",
                "primera mitad", "segunda mitad", "Vigilia", "kè", "doceavo", "doceavos"),
    EDO_WORDS("{branch} · {bell} campanadas", "{branch}\n{bell} campanadas"),
    TWELVE_WORDS("a. m.", "p. m.", "{time} {period}", "{time}\n{period}")
};
static const InfiltratrI18nEntry clock_fr[] = {
    CLOCK_WORDS("Heure {branch}", "{branch}, {half}", "Heure",
                "première moitié", "seconde moitié", "Veille", "kè", "douzième", "douzièmes"),
    EDO_WORDS("{branch} · {bell} coups", "{branch}\n{bell} coups"),
    TWELVE_WORDS("AM", "PM", "{time} {period}", "{time}\n{period}")
};
static const InfiltratrI18nEntry clock_it[] = {
    CLOCK_WORDS("Ora {branch}", "{branch}, {half}", "Ora",
                "prima metà", "seconda metà", "Vigilia", "kè", "dodicesimo", "dodicesimi"),
    EDO_WORDS("{branch} · {bell} rintocchi", "{branch}\n{bell} rintocchi"),
    TWELVE_WORDS("AM", "PM", "{time} {period}", "{time}\n{period}")
};
static const InfiltratrI18nEntry clock_pt[] = {
    CLOCK_WORDS("Hora {branch}", "{branch}, {half}", "Hora",
                "primeira metade", "segunda metade", "Vigília", "kè", "duodécimo", "duodécimos"),
    EDO_WORDS("{branch} · {bell} badaladas", "{branch}\n{bell} badaladas"),
    TWELVE_WORDS("AM", "PM", "{time} {period}", "{time}\n{period}")
};
static const InfiltratrI18nEntry clock_ru[] = {
    CLOCK_WORDS("Час {branch}", "{branch}, {half}", "Час",
                "первая половина", "вторая половина", "Стража", "кэ", "двенадцатая", "двенадцатых"),
    EDO_WORDS("{branch} · {bell} ударов", "{branch}\n{bell} ударов"),
    TWELVE_WORDS("AM", "PM", "{time} {period}", "{time}\n{period}")
};
static const InfiltratrI18nEntry clock_ja[] = {
    CLOCK_WORDS("{branch}時", "{branch}{half}", "時",
                "初", "正", "夜番", "刻", "十二分の一", "十二分の一"),
    NATIVE_BRANCHES, JAPANESE_NATIVE_BRANCHES,
    EDO_WORDS("{branch}の刻 · {bell}つ", "{branch}の刻\n{bell}つ"),
    TWELVE_WORDS("午前", "午後", "{period}{time}", "{period}\n{time}")
};
static const InfiltratrI18nEntry clock_zh[] = {
    CLOCK_WORDS("{branch}时", "{branch}{half}", "时",
                "初", "正", "更", "刻", "十二分之一时", "十二分之一时"),
    NATIVE_BRANCHES, JAPANESE_NATIVE_BRANCHES,
    EDO_WORDS("{branch}刻 · {bell}声", "{branch}刻\n{bell}声"),
    TWELVE_WORDS("上午", "下午", "{period}{time}", "{period}\n{time}")
};
static const InfiltratrI18nEntry clock_zh_hant[] = {
    CLOCK_WORDS("{branch}時", "{branch}{half}", "時",
                "初", "正", "更", "刻", "十二分之一時", "十二分之一時"),
    NATIVE_BRANCHES, JAPANESE_NATIVE_BRANCHES,
    EDO_WORDS("{branch}刻 · {bell}聲", "{branch}刻\n{bell}聲"),
    TWELVE_WORDS("上午", "下午", "{period}{time}", "{period}\n{time}")
};
static const InfiltratrI18nEntry clock_ko[] = {
    /* Other specialized-clock wording keeps the documented English fallback. */
    EDO_WORDS("{branch} · {bell}타", "{branch}\n{bell}타"),
    TWELVE_WORDS("오전", "오후", "{period} {time}", "{period}\n{time}")
};
#undef CLOCK_WORDS
#undef PINYIN_BRANCHES
#undef NATIVE_BRANCHES
#undef JAPANESE_BRANCHES
#undef JAPANESE_NATIVE_BRANCHES
#undef EDO_WORDS
#undef TWELVE_WORDS

#define CLOCK_CATALOG(locale, entries) \
    { locale, entries, sizeof(entries) / sizeof((entries)[0]) }
static const InfiltratrI18nCatalog clock_catalogs[] = {
    CLOCK_CATALOG("en", clock_en), CLOCK_CATALOG("de", clock_de),
    CLOCK_CATALOG("es", clock_es), CLOCK_CATALOG("fr", clock_fr),
    CLOCK_CATALOG("it", clock_it), CLOCK_CATALOG("pt", clock_pt),
    CLOCK_CATALOG("ru", clock_ru), CLOCK_CATALOG("ja", clock_ja),
    CLOCK_CATALOG("ko", clock_ko),
    CLOCK_CATALOG("zh", clock_zh), CLOCK_CATALOG("zh-TW", clock_zh_hant),
    CLOCK_CATALOG("zh-HK", clock_zh_hant), CLOCK_CATALOG("zh-MO", clock_zh_hant),
    CLOCK_CATALOG("zh-Hant", clock_zh_hant),
    CLOCK_CATALOG("zh-Hant-TW", clock_zh_hant),
    CLOCK_CATALOG("zh-Hant-HK", clock_zh_hant)
};
#undef CLOCK_CATALOG

static bool write_localized(char *buffer, size_t capacity, size_t *length,
                            const char *pattern,
                            const InfiltratrI18nArgument *arguments,
                            size_t argument_count)
{
    const size_t needed = infiltratr_i18n_format(
        buffer, capacity, pattern, arguments, argument_count);
    if (needed >= capacity) {
        buffer[0] = '\0';
        return false;
    }
    if (length != NULL) *length = needed;
    return true;
}

static bool format_clock_mode(const InfiltratrI18n *language,
                                           const char *mode,
                                           int64_t unix_microseconds,
                                           int32_t utc_offset_seconds,
                                           bool show_seconds,
                                           bool vertical,
                                           bool location_configured,
                                           double latitude,
                                           double longitude,
                                           char *buffer,
                                           size_t capacity,
                                           size_t *length)
{
    const InfiltratrTemporalClockModeInfo *info;
    const int64_t local =
        infiltratr_temporal_local_microseconds_of_day(
            unix_microseconds, utc_offset_seconds);
    const char *separator = vertical ? "\n" : ":";
    char temporary[192];

    if (buffer == NULL || capacity == 0U || mode == NULL) {
        return false;
    }
    buffer[0] = '\0';

    info = infiltratr_temporal_clock_mode_find(mode);
    if (info == NULL ||
        !mode_location_valid(
            info, location_configured, latitude, longitude)) {
        return false;
    }

    if (strcmp(mode, "standard") == 0) {
        return false;
    }

    if (language != NULL && strcmp(mode, "standard-12") == 0) {
        int hour, minute, second;
        split_clock_seconds(local / MICROSECONDS_PER_SECOND,
                            &hour, &minute, &second);
        const int hour12 = hour % 12 == 0 ? 12 : hour % 12;
        if (!(show_seconds
            ? write_printf(temporary, sizeof(temporary), NULL,
                           "%d%s%02d%s%02d", hour12, separator, minute,
                           separator, second)
            : write_printf(temporary, sizeof(temporary), NULL,
                           "%d%s%02d", hour12, separator, minute))) {
            return false;
        }
        const InfiltratrI18nArgument arguments[] = {
            { "time", temporary },
            { "period", infiltratr_i18n_get(language, hour < 12 ? "am" : "pm") }
        };
        return write_localized(buffer, capacity, length,
            infiltratr_i18n_get(language, vertical ? "twelve-vertical" : "twelve"),
            arguments, sizeof(arguments) / sizeof(arguments[0]));
    }

    if (strcmp(mode, "standard-24") == 0 ||
        strcmp(mode, "standard-12") == 0 ||
        strcmp(mode, "decimal") == 0) {
        const InfiltratrClockProfile profile =
            strcmp(mode, "standard-24") == 0
                ? INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24
                : strcmp(mode, "standard-12") == 0
                    ? INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12
                    : INFILTRATR_CLOCK_PROFILE_DECIMAL_10;
        char *cursor;

        if (!infiltratr_temporal_format_clock(
                profile, unix_microseconds, utc_offset_seconds,
                show_seconds, temporary, sizeof(temporary), NULL)) {
            return false;
        }
        if (vertical) {
            for (cursor = temporary; *cursor != '\0'; ++cursor) {
                if (*cursor == ':') {
                    *cursor = '\n';
                }
            }
        }
        return write_text(buffer, capacity, length, temporary);
    }

    if (strcmp(mode, "internet") == 0) {
        const int64_t phase = positive_modulo(
            positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY) +
                SECONDS_PER_HOUR * MICROSECONDS_PER_SECOND,
            MICROSECONDS_PER_DAY);
        const uint64_t ticks = day_tick(
            phase, show_seconds
                ? INTERNET_TICKS_PER_DAY * UINT64_C(100)
                : INTERNET_TICKS_PER_DAY);

        return show_seconds
            ? write_printf(buffer, capacity, length, "@%03llu.%02llu",
                           (unsigned long long)(ticks / UINT64_C(100)),
                           (unsigned long long)(ticks % UINT64_C(100)))
            : write_printf(buffer, capacity, length, "@%03llu",
                           (unsigned long long)ticks);
    }

    if (strcmp(mode, "unix") == 0) {
        return write_printf(
            buffer, capacity, length, "%lld",
            (long long)floor_divide(
                unix_microseconds, MICROSECONDS_PER_SECOND));
    }

    if (strcmp(mode, "hexadecimal") == 0) {
        return write_printf(
            buffer, capacity, length, "%04llX",
            (unsigned long long)day_tick(local, HEX_TICKS_PER_DAY));
    }

    if (strcmp(mode, "binary") == 0) {
        int hour;
        int minute;
        int second;
        char hour_bits[6];
        char minute_bits[7];
        char second_bits[7];
        unsigned bit;

        split_clock_seconds(
            local / MICROSECONDS_PER_SECOND, &hour, &minute, &second);
        for (bit = 0U; bit < 5U; ++bit) {
            hour_bits[bit] =
                ((unsigned)hour & (1U << (4U - bit))) ? '1' : '0';
        }
        hour_bits[5] = '\0';
        for (bit = 0U; bit < 6U; ++bit) {
            minute_bits[bit] =
                ((unsigned)minute & (1U << (5U - bit))) ? '1' : '0';
            second_bits[bit] =
                ((unsigned)second & (1U << (5U - bit))) ? '1' : '0';
        }
        minute_bits[6] = '\0';
        second_bits[6] = '\0';

        return show_seconds
            ? write_printf(buffer, capacity, length, "%s%s%s%s%s",
                           hour_bits, separator, minute_bits, separator,
                           second_bits)
            : write_printf(buffer, capacity, length, "%s%s%s",
                           hour_bits, separator, minute_bits);
    }

    if (strcmp(mode, "sidereal") == 0 ||
        strcmp(mode, "solar") == 0 ||
        strcmp(mode, "mean-solar") == 0) {
        long double clock_seconds;
        const char *suffix;
        int hour;
        int minute;
        int second;

        if (strcmp(mode, "sidereal") == 0) {
            clock_seconds =
                local_sidereal_seconds(unix_microseconds, longitude);
            suffix = "LST";
        } else if (strcmp(mode, "solar") == 0) {
            clock_seconds =
                apparent_solar_seconds(unix_microseconds, longitude);
            suffix = "SOL";
        } else {
            clock_seconds =
                mean_solar_seconds(unix_microseconds, longitude);
            suffix = "LMT";
        }

        split_clock_seconds(
            (int64_t)floorl(clock_seconds), &hour, &minute, &second);
        return write_clock_fields(
            buffer, capacity, length, hour, minute, second,
            show_seconds, vertical, suffix);
    }

    if (strcmp(mode, "julian") == 0 ||
        strcmp(mode, "modified-julian") == 0) {
        const int digits = show_seconds ? 5 : 3;
        const int64_t scale =
            show_seconds ? INT64_C(100000) : INT64_C(1000);
        const long double date =
            julian_date(unix_microseconds) -
            (strcmp(mode, "modified-julian") == 0 ? 2400000.5L : 0.0L);
        const int64_t whole_days = (int64_t)floorl(date);
        int64_t fraction = (int64_t)floorl(
            (date - (long double)whole_days) * scale + 1.0e-10L);

        if (fraction >= scale) {
            fraction = scale - 1;
        }
        return write_printf(
            buffer, capacity, length,
            strcmp(mode, "modified-julian") == 0
                ? "MJD%s%lld.%0*lld"
                : "JD%s%lld.%0*lld",
            vertical ? "\n" : " ",
            (long long)whole_days, digits, (long long)fraction);
    }

    if (strcmp(mode, "chinese-time") == 0) {
        static const char *const branches[] = {
            "子", "丑", "寅", "卯", "辰", "巳",
            "午", "未", "申", "酉", "戌", "亥"
        };
        const int64_t double_hour =
            INT64_C(2) * SECONDS_PER_HOUR * MICROSECONDS_PER_SECOND;
        const int64_t shifted = positive_modulo(
            local + SECONDS_PER_HOUR * MICROSECONDS_PER_SECOND,
            MICROSECONDS_PER_DAY);
        const unsigned branch = (unsigned)(shifted / double_hour);
        const int64_t within = shifted % double_hour;
        const char *suffix = show_seconds
            ? (within < SECONDS_PER_HOUR * MICROSECONDS_PER_SECOND
                   ? "初" : "正")
            : "時";

        if (language != NULL) {
            static const char *const keys[] = {
                "zi", "chou", "yin", "mao", "chen", "si",
                "wu", "wei", "shen", "you", "xu", "hai"
            };
            const InfiltratrI18nArgument arguments[] = {
                { "branch", infiltratr_i18n_get(language, keys[branch]) },
                { "half", infiltratr_i18n_get(language,
                    within < SECONDS_PER_HOUR * MICROSECONDS_PER_SECOND
                        ? "first" : "second") },
                { "unit", infiltratr_i18n_get(language, "hour") }
            };
            const char *pattern = vertical
                ? (show_seconds ? "{branch}\n{half}" : "{branch}\n{unit}")
                : infiltratr_i18n_get(language,
                    show_seconds ? "chinese-fine" : "chinese-coarse");
            return write_localized(buffer, capacity, length, pattern,
                arguments, sizeof(arguments) / sizeof(arguments[0]));
        }

        return write_printf(
            buffer, capacity, length,
            vertical ? "%s\n%s" : "%s%s",
            branches[branch], suffix);
    }

    if (strcmp(mode, "chinese-ke") == 0) {
        const uint64_t ke = day_tick(local, CHINESE_KE_PER_DAY);

        return write_printf(
            buffer, capacity, length,
            language != NULL
                ? (vertical ? "%02llu\n%s" : "%02llu %s")
                : (vertical ? "%02llu\n%s" : "%02llu%s"),
            (unsigned long long)ke,
            language != NULL ? infiltratr_i18n_get(language, "ke") : "刻");
    }

    if (strcmp(mode, "roman-temporal") == 0) {
        static const char *const roman[] = {
            "I", "II", "III", "IV", "V", "VI",
            "VII", "VIII", "IX", "X", "XI", "XII"
        };
        SeasonalPeriod period;

        if (!seasonal_period_at(
                unix_microseconds, latitude, longitude,
                0.833, 12U, 4U, &period)) {
            return write_text(
                buffer, capacity, length,
                vertical ? "N/A\nROM" : "N/A ROM");
        }

        /*
         * Roman daylight horae were seasonal, so finer presentation must
         * subdivide the current unequal hora rather than append modern
         * minutes. An uncia is one twelfth of an hour. Night remains in the
         * selected four-watch civil convention because a fractional military
         * watch would manufacture a precision convention this profile does
         * not claim historically.
         */
        if (show_seconds && period.daylight) {
            unsigned uncia =
                (unsigned)floorl(period.fraction * 12.0L);

            if (uncia > 11U) {
                uncia = 11U;
            }
            if (uncia != 0U) {
                return write_printf(
                    buffer, capacity, length,
                    vertical ? "%s\n%s\n%s %s"
                             : "%s %s · %s %s",
                    language != NULL ? infiltratr_i18n_get(language, "hour") : "Hora",
                    roman[period.index], roman[uncia - 1U],
                    language != NULL ? infiltratr_i18n_get(language,
                        uncia == 1U ? "twelfth" : "twelfths")
                        : (uncia == 1U ? "uncia" : "unciae"));
            }
        }

        return write_printf(
            buffer, capacity, length,
            vertical ? "%s\n%s" : "%s %s",
            language != NULL ? infiltratr_i18n_get(language,
                period.daylight ? "hour" : "watch")
                : (period.daylight ? "Hora" : "Vigilia"),
            roman[period.index]);
    }

    if (strcmp(mode, "japanese-temporal") == 0 ||
        strcmp(mode, "japanese-temporal-early") == 0) {
        typedef struct JapaneseToki {
            const char *character;
            const char *bell;
            const char *key;
        } JapaneseToki;
        static const JapaneseToki day_toki[] = {
            { "卯", "六", "jp-u" }, { "辰", "五", "jp-tatsu" },
            { "巳", "四", "jp-mi" }, { "午", "九", "jp-uma" },
            { "未", "八", "jp-hitsuji" }, { "申", "七", "jp-saru" }
        };
        static const JapaneseToki night_toki[] = {
            { "酉", "六", "jp-tori" }, { "戌", "五", "jp-inu" },
            { "亥", "四", "jp-i" }, { "子", "九", "jp-ne" },
            { "丑", "八", "jp-ushi" }, { "寅", "七", "jp-tora" }
        };
        const double depression =
            strcmp(mode, "japanese-temporal") == 0
                ? 7.0 + 21.0 / 60.0 + 40.0 / 3600.0
                : 0.833;
        SeasonalPeriod period;
        const JapaneseToki *toki;

        if (!seasonal_period_at(
                unix_microseconds, latitude, longitude,
                depression, 6U, 6U, &period)) {
            return write_text(
                buffer, capacity, length,
                vertical ? "N/A\n和時" : "N/A 和時");
        }
        toki = period.daylight
            ? &day_toki[period.index]
            : &night_toki[period.index];
        if (language != NULL) {
            const InfiltratrI18nArgument arguments[] = {
                { "branch", infiltratr_i18n_get(language, toki->key) },
                { "bell", toki->bell }
            };
            return write_localized(buffer, capacity, length,
                infiltratr_i18n_get(language, vertical ? "edo-vertical" : "edo"),
                arguments, sizeof(arguments) / sizeof(arguments[0]));
        }
        return write_printf(
            buffer, capacity, length,
            vertical ? "%sの刻\n%sつ" : "%sの刻 · %sつ",
            toki->character, toki->bell);
    }

    if (strcmp(mode, "italian-hours") == 0) {
        return format_equal_hours(
            buffer, capacity, length, unix_microseconds,
            show_seconds, vertical, latitude, longitude,
            SOLAR_ORIGIN_SUNSET, "IT");
    }

    if (strcmp(mode, "babylonian-hours") == 0) {
        return format_equal_hours(
            buffer, capacity, length, unix_microseconds,
            show_seconds, vertical, latitude, longitude,
            SOLAR_ORIGIN_SUNRISE, "BAB-R");
    }

    if (strcmp(mode, "babylonian-ancient") == 0) {
        int64_t start;
        int64_t elapsed_seconds;
        int beru;
        int ush;
        const char *field_separator = vertical ? "\n" : " · ";

        if (!solar_origin_window(
                unix_microseconds, latitude, longitude,
                SOLAR_ORIGIN_SUNSET, &start)) {
            return write_text(
                buffer, capacity, length,
                vertical ? "N/A\nBAB-A" : "N/A BAB-A");
        }
        elapsed_seconds = floor_divide(
            unix_microseconds - start, MICROSECONDS_PER_SECOND);
        beru = (int)(elapsed_seconds / INT64_C(7200)) + 1;
        ush = (int)((elapsed_seconds % INT64_C(7200)) / INT64_C(240));
        return write_printf(
            buffer, capacity, length,
            "bēru %02d%sUŠ %02d", beru, field_separator, ush);
    }

    if (strcmp(mode, "indian-ghati") == 0) {
        int64_t start;
        int64_t elapsed_seconds;
        int ghati;
        int vighati;

        if (!solar_origin_window(
                unix_microseconds, latitude, longitude,
                SOLAR_ORIGIN_SUNRISE, &start)) {
            return write_text(
                buffer, capacity, length,
                vertical ? "N/A\nGH" : "N/A GH");
        }
        elapsed_seconds = floor_divide(
            unix_microseconds - start, MICROSECONDS_PER_SECOND);
        ghati = (int)(elapsed_seconds /
                      (INT64_C(24) * SECONDS_PER_MINUTE));
        vighati =
            (int)((elapsed_seconds / INT64_C(24)) % INT64_C(60));
        return write_printf(
            buffer, capacity, length,
            vertical ? "%d ghaṭī\n%d pala" : "%d ghaṭī · %d pala",
            ghati, vighati);
    }

    if (strcmp(mode, "nuremberg-hours") == 0) {
        return format_nuremberg_hours(
            buffer, capacity, length, unix_microseconds,
            show_seconds, vertical, true, 0.0, 0.0);
    }

    if (strcmp(mode, "nuremberg-solar") == 0) {
        return format_nuremberg_hours(
            buffer, capacity, length, unix_microseconds,
            show_seconds, vertical, false, latitude, longitude);
    }

    return false;
}

bool infiltratr_temporal_format_clock_mode(const char *mode,
    int64_t unix_microseconds, int32_t utc_offset_seconds,
    bool show_seconds, bool vertical, bool location_configured,
    double latitude, double longitude,
    char *buffer, size_t capacity, size_t *length)
{
    return format_clock_mode(NULL, mode, unix_microseconds, utc_offset_seconds,
        show_seconds, vertical, location_configured, latitude, longitude,
        buffer, capacity, length);
}

bool infiltratr_temporal_format_clock_mode_localized(
    const char *locale, const char *mode,
    int64_t unix_microseconds, int32_t utc_offset_seconds,
    bool show_seconds, bool vertical, bool location_configured,
    double latitude, double longitude,
    char *buffer, size_t capacity, size_t *length)
{
    InfiltratrI18n language;
    if (!infiltratr_i18n_init(&language, clock_catalogs,
            sizeof(clock_catalogs) / sizeof(clock_catalogs[0]), "en")) {
        return false;
    }
    if (locale != NULL) (void)infiltratr_i18n_set_locale(&language, locale);
    return format_clock_mode(&language, mode, unix_microseconds, utc_offset_seconds,
        show_seconds, vertical, location_configured, latitude, longitude,
        buffer, capacity, length);
}
