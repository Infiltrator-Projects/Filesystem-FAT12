// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "infiltratr/temporal_posix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                    __FILE__, __LINE__, #expression); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    char template_path[] = "/tmp/infiltratr-temporal-XXXXXX";
    char *root = mkdtemp(template_path);
    char expected[4096];
    char actual[4096];
    char marker[4096];
    char provider_path[4096];
    const char *old_provider_value =
        getenv("INFILTRATR_TEMPORAL_PROVIDER_MARKER_PATH");
    char *old_provider =
        old_provider_value != NULL ? strdup(old_provider_value) : NULL;
    InfiltratrTemporalPolicyV3 policy;
    InfiltratrTemporalPolicyV3 loaded;
    bool found = true;

    CHECK(root != NULL);
    CHECK(setenv("XDG_CONFIG_HOME", root, 1) == 0);

    CHECK(infiltratr_temporal_posix_policy_directory(
        actual, sizeof(actual)));
    CHECK(snprintf(expected, sizeof(expected), "%s/infiltrator", root) > 0);
    CHECK(strcmp(actual, expected) == 0);

    CHECK(infiltratr_temporal_posix_policy_path(actual, sizeof(actual)));
    CHECK(snprintf(expected, sizeof(expected), "%s/infiltrator/presentation.conf",
                    root) > 0);
    CHECK(strcmp(actual, expected) == 0);

    CHECK(snprintf(provider_path, sizeof(provider_path),
                   "%s/temporal-v3", root) > 0);
    CHECK(setenv("INFILTRATR_TEMPORAL_PROVIDER_MARKER_PATH",
                 provider_path, 1) == 0);
    CHECK(infiltratr_temporal_posix_provider_marker_path(
        marker, sizeof(marker)));
    CHECK(strcmp(marker, provider_path) == 0);
    CHECK(!infiltratr_temporal_posix_provider_available());

    {
        FILE *provider = fopen(provider_path, "wb");
        CHECK(provider != NULL);
        CHECK(fputs("provider=wrong\npolicy-version=3\n"
                    "contract=infiltratr-temporal-v3\n", provider) >= 0);
        CHECK(fclose(provider) == 0);
    }
    CHECK(!infiltratr_temporal_posix_provider_available());

    {
        FILE *provider = fopen(provider_path, "wb");
        CHECK(provider != NULL);
        CHECK(fputs("provider=system-settings\n"
                    "policy-version=3\n"
                    "contract=infiltratr-temporal-v3\n", provider) >= 0);
        CHECK(fclose(provider) == 0);
    }
    CHECK(infiltratr_temporal_posix_provider_available());

    {
        FILE *provider = fopen(provider_path, "wb");
        CHECK(provider != NULL);
        for (size_t index = 0U; index < 2048U; ++index)
            CHECK(fputc('x', provider) != EOF);
        CHECK(fclose(provider) == 0);
    }
    CHECK(!infiltratr_temporal_posix_provider_available());

    {
        FILE *provider = fopen(provider_path, "wb");
        CHECK(provider != NULL);
        CHECK(fputs("provider=system-settings\n"
                    "policy-version=3\n"
                    "contract=infiltratr-temporal-v3\n", provider) >= 0);
        CHECK(fclose(provider) == 0);
    }
    CHECK(infiltratr_temporal_posix_provider_available());

    CHECK(infiltratr_temporal_posix_policy_load(&loaded, &found) ==
           INFILTRATR_IO_OK);
    CHECK(!found);
    CHECK(strcmp(loaded.clock_mode, "standard") == 0);
    CHECK(strcmp(loaded.calendar, "gregorian") == 0);

    CHECK(infiltratr_temporal_policy_v3_default(&policy));
    strcpy(policy.clock_mode, "standard-24");
    strcpy(policy.calendar, "hebrew");
    policy.show_seconds = true;
    policy.location_configured = true;
    policy.latitude = -36.39;
    policy.longitude = 145.36;

    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    found = false;
    CHECK(infiltratr_temporal_posix_policy_load(&loaded, &found) ==
           INFILTRATR_IO_OK);
    CHECK(found);
    CHECK(strcmp(loaded.clock_mode, "standard-24") == 0);
    CHECK(strcmp(loaded.calendar, "hebrew") == 0);
    CHECK(loaded.show_seconds);
    CHECK(loaded.location_configured);
    CHECK(loaded.latitude == -36.39);
    CHECK(loaded.longitude == 145.36);

    {
        FILE *policy_file = fopen(expected, "wb");
        CHECK(policy_file != NULL);
        for (size_t index = 0U; index < 2048U; ++index)
            CHECK(fputc('x', policy_file) != EOF);
        CHECK(fclose(policy_file) == 0);
    }
    found = true;
    CHECK(infiltratr_temporal_posix_policy_load(&loaded, &found) ==
           INFILTRATR_IO_TRUNCATED);
    CHECK(!found);
    CHECK(strcmp(loaded.clock_mode, "standard") == 0);
    CHECK(strcmp(loaded.calendar, "gregorian") == 0);

    {
        static const char prefix[] =
            "version=3\n"
            "clock-mode=standard\n";
        static const char suffix[] =
            "calendar=gregorian\n"
            "show-seconds=false\n"
            "location-configured=false\n"
            "latitude=0.000000\n"
            "longitude=0.000000\n";
        FILE *policy_file = fopen(expected, "wb");

        CHECK(policy_file != NULL);
        CHECK(fwrite(prefix, 1U, sizeof(prefix) - 1U, policy_file) ==
              sizeof(prefix) - 1U);
        CHECK(fputc('\0', policy_file) != EOF);
        CHECK(fwrite(suffix, 1U, sizeof(suffix) - 1U, policy_file) ==
              sizeof(suffix) - 1U);
        CHECK(fclose(policy_file) == 0);
    }
    found = true;
    CHECK(infiltratr_temporal_posix_policy_load(&loaded, &found) ==
           INFILTRATR_IO_INVALID_VALUE);
    CHECK(!found);
    CHECK(strcmp(loaded.clock_mode, "standard") == 0);
    CHECK(strcmp(loaded.calendar, "gregorian") == 0);

    CHECK(unlink(expected) == 0);
    CHECK(unlink(provider_path) == 0);
    if (old_provider != NULL) {
        CHECK(setenv("INFILTRATR_TEMPORAL_PROVIDER_MARKER_PATH",
                     old_provider, 1) == 0);
        free(old_provider);
    } else {
        CHECK(unsetenv("INFILTRATR_TEMPORAL_PROVIDER_MARKER_PATH") == 0);
    }
    {
        char directory[4096];
        CHECK(snprintf(directory, sizeof(directory), "%s/infiltrator", root) > 0);
        CHECK(rmdir(directory) == 0);
    }
    CHECK(rmdir(root) == 0);

    puts("Libraries POSIX temporal policy contract passed.");
    return 0;
}
