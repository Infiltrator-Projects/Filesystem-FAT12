// SPDX-License-Identifier: GPL-3.0-or-later
#include "infiltratr/temporal_posix.h"
#include <stdio.h>
#include <string.h>

#ifndef TEMPORAL_DATE_FIXTURE_PATH
#error TEMPORAL_DATE_FIXTURE_PATH must name the explicit test runtime
#endif

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "date provider check failed at line %d\n", __LINE__); \
    return 1; } } while (0)

int main(void)
{
    char text[64] = "keep";
    size_t length = 99U;
    InfiltratrTemporalDateProvider *provider;

    CHECK(infiltratr_temporal_posix_date_provider_new_from(NULL) == NULL);
    CHECK(infiltratr_temporal_posix_date_provider_new_from("relative.so") == NULL);
    CHECK(infiltratr_temporal_posix_date_provider_new_from(
        "/nonexistent/calendar-date-provider.so") == NULL);
    provider = infiltratr_temporal_posix_date_provider_new_from(
        TEMPORAL_DATE_FIXTURE_PATH);
    CHECK(provider != NULL);
    CHECK(infiltratr_temporal_posix_format_date(provider, "chinese",
        2026, 10, 10, "short", text, sizeof(text), &length));
    CHECK(strcmp(text, "中文日期") == 0 && length == strlen(text));
    CHECK(infiltratr_temporal_posix_format_date(provider, "chinese",
        2026, 10, 10, "full", text, sizeof(text), NULL));
    CHECK(strcmp(text, "中文日期 full") == 0);

    const char *const rejected[] = { "chinese", "roman", "julian", "unknown" };
    for (size_t index = 0U; index < sizeof(rejected) / sizeof(rejected[0]); ++index) {
        strcpy(text, "keep");
        length = 99U;
        const size_t capacity = index == 0U ? 5U : sizeof(text);
        CHECK(!infiltratr_temporal_posix_format_date(provider, rejected[index],
            2026, 10, 10, "short", text, capacity, &length));
        CHECK(strcmp(text, "keep") == 0 && length == 99U);
    }
    CHECK(!infiltratr_temporal_posix_format_date(provider, "chinese",
        2026, 10, 10, "invalid", text, sizeof(text), &length));
    CHECK(!infiltratr_temporal_posix_format_date(provider, "chinese",
        2026, 10, 11, "short", text, sizeof(text), &length));
    CHECK(strcmp(text, "keep") == 0 && length == 99U);
    infiltratr_temporal_posix_date_provider_free(provider);
    infiltratr_temporal_posix_date_provider_free(NULL);
    return 0;
}
