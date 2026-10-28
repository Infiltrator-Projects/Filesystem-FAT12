// SPDX-License-Identifier: GPL-3.0-or-later
/* Optional Calendar date rendering without a compile-time toolkit dependency. */
#define _POSIX_C_SOURCE 200809L

#include "infiltratr/temporal_posix.h"
#include "infiltratr/dynlib.h"
#include "infiltratr/utf8.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef INFILTRATR_COMMON_LIBRARY_ARCHITECTURE
#define INFILTRATR_COMMON_LIBRARY_ARCHITECTURE ""
#endif

struct InfiltratrTemporalDateProvider {
    InfiltratrDynlib library;
    InfiltratrTemporalDateRenderer render;
    bool discover;
    uint64_t retry_after_nanoseconds;
};

static bool bind_runtime(InfiltratrTemporalDateProvider *provider,
                         const char *path)
{
    InfiltratrTemporalDateRenderer render = NULL;

    if (!infiltratr_dynlib_open(&provider->library, path)) {
        return false;
    }
    if (!infiltratr_dynlib_symbol(&provider->library,
            INFILTRATR_TEMPORAL_DATE_RENDERER_SYMBOL,
            &render, sizeof(render)) || render == NULL) {
        infiltratr_dynlib_close(&provider->library);
        return false;
    }
    provider->render = render;
    return true;
}

static bool try_directory(InfiltratrTemporalDateProvider *provider,
                          const char *directory)
{
    char path[4096];
    struct stat status;

    if (!infiltratr_path_join(path, sizeof(path), directory,
                             "libcalendar.so.0") ||
        stat(path, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_uid != 0 ||
        (status.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
        return false;
    }
    return bind_runtime(provider, path);
}

static bool ensure_runtime(InfiltratrTemporalDateProvider *provider)
{
    static const char *const roots[] = {
        "/usr/lib", "/lib", "/usr/local/lib", "/usr/lib64", "/lib64"
    };
    uint64_t now = 0U;

    if (provider == NULL) {
        return false;
    }
    if (provider->render != NULL) {
        return true;
    }
    if (!provider->discover) {
        return false;
    }
    /* A failed clock read must not turn per-row formatting into repeated I/O. */
    if (!infiltratr_monotonic_nanoseconds(&now) ||
        now < provider->retry_after_nanoseconds) {
        return false;
    }
    provider->retry_after_nanoseconds = now <= UINT64_MAX - UINT64_C(60000000000)
        ? now + UINT64_C(60000000000) : UINT64_MAX;

    for (size_t index = 0U; index < sizeof(roots) / sizeof(roots[0]); ++index) {
        char directory[4096];
        if (INFILTRATR_COMMON_LIBRARY_ARCHITECTURE[0] != '\0' &&
            infiltratr_path_join(directory, sizeof(directory), roots[index],
                                INFILTRATR_COMMON_LIBRARY_ARCHITECTURE) &&
            try_directory(provider, directory)) {
            return true;
        }
        if (try_directory(provider, roots[index])) {
            return true;
        }
    }
    return false;
}

InfiltratrTemporalDateProvider *infiltratr_temporal_posix_date_provider_new(void)
{
    InfiltratrTemporalDateProvider *provider = calloc(1U, sizeof(*provider));
    if (provider != NULL) {
        provider->discover = true;
    }
    return provider;
}

InfiltratrTemporalDateProvider *infiltratr_temporal_posix_date_provider_new_from(
    const char *library_path)
{
    InfiltratrTemporalDateProvider *provider;

    if (library_path == NULL || library_path[0] != '/') {
        return NULL;
    }
    provider = calloc(1U, sizeof(*provider));
    if (provider != NULL && !bind_runtime(provider, library_path)) {
        infiltratr_temporal_posix_date_provider_free(provider);
        return NULL;
    }
    return provider;
}

void infiltratr_temporal_posix_date_provider_free(
    InfiltratrTemporalDateProvider *provider)
{
    if (provider != NULL) {
        infiltratr_dynlib_close(&provider->library);
        free(provider);
    }
}

bool infiltratr_temporal_posix_format_date(
    InfiltratrTemporalDateProvider *provider,
    const char *calendar_id,
    int32_t gregorian_year,
    int32_t gregorian_month,
    int32_t gregorian_day,
    const char *part,
    char *buffer,
    size_t capacity,
    size_t *length)
{
    char text[INFILTRATR_TEMPORAL_DATE_CAPACITY] = { 0 };
    size_t size = 0U;

    if (buffer == NULL || capacity == 0U || part == NULL ||
        (strcmp(part, "short") != 0 && strcmp(part, "full") != 0) ||
        infiltratr_temporal_calendar_find(calendar_id) == NULL ||
        !ensure_runtime(provider) ||
        !provider->render(calendar_id, gregorian_year, gregorian_month,
                          gregorian_day, part, text, sizeof(text), &size) ||
        size == 0U || size >= sizeof(text) || size >= capacity ||
        text[size] != '\0' || strlen(text) != size ||
        !infiltratr_utf8_validate(text, size)) {
        return false;
    }
    memcpy(buffer, text, size + 1U);
    if (length != NULL) {
        *length = size;
    }
    return true;
}
