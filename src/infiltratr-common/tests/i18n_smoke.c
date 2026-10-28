// SPDX-License-Identifier: GPL-3.0-or-later
#include "infiltratr/i18n.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

static const InfiltratrI18nEntry english_entries[] = {
    {"nav.vehicle", "Vehicle"},
    {"scan.modules_found", "Found {count} modules"},
    {"fallback.only", "English fallback"}
};
static const InfiltratrI18nEntry german_entries[] = {
    {"nav.vehicle", "Fahrzeug"},
    {"scan.modules_found", "{count} Steuergeraete gefunden"}
};
static const InfiltratrI18nCatalog catalogs[] = {
    {"en-AU", english_entries, sizeof(english_entries) / sizeof(english_entries[0])},
    {"de-DE", german_entries, sizeof(german_entries) / sizeof(german_entries[0])}
};

int main(void)
{
    InfiltratrI18n context;
    InfiltratrI18nArgument arguments[] = {{"count", "7"}};
    char output[64];
    size_t required;

    CHECK(infiltratr_i18n_init(&context, catalogs,
                               sizeof(catalogs) / sizeof(catalogs[0]), "en_AU"));
    CHECK(strcmp(infiltratr_i18n_locale(&context), "en-AU") == 0);
    CHECK(strcmp(infiltratr_i18n_get(&context, "nav.vehicle"), "Vehicle") == 0);

    CHECK(infiltratr_i18n_set_locale(&context, "de_AT.UTF-8"));
    CHECK(strcmp(infiltratr_i18n_locale(&context), "de-AT") == 0);
    CHECK(strcmp(infiltratr_i18n_get(&context, "nav.vehicle"), "Fahrzeug") == 0);
    CHECK(strcmp(infiltratr_i18n_get(&context, "fallback.only"), "English fallback") == 0);

    CHECK(infiltratr_i18n_set_locale(&context, "de_Latn_DE.UTF-8"));
    CHECK(strcmp(infiltratr_i18n_locale(&context), "de-Latn-DE") == 0);
    CHECK(strcmp(infiltratr_i18n_get(&context, "nav.vehicle"), "Fahrzeug") == 0);

    required = infiltratr_i18n_format(output, sizeof(output),
        infiltratr_i18n_get(&context, "scan.modules_found"),
        arguments, sizeof(arguments) / sizeof(arguments[0]));
    CHECK(required == strlen("7 Steuergeraete gefunden"));
    CHECK(strcmp(output, "7 Steuergeraete gefunden") == 0);

    required = infiltratr_i18n_format(output, 8U, "Hello {name}", NULL, 0U);
    CHECK(required == strlen("Hello {name}"));
    CHECK(strcmp(output, "Hello {") == 0);

    const char before[INFILTRATR_I18N_LOCALE_CAPACITY] = "de-Latn-DE";
    CHECK(!infiltratr_i18n_set_locale(&context, "en--AU"));
    CHECK(strcmp(infiltratr_i18n_locale(&context), before) == 0);
    CHECK(!infiltratr_i18n_set_locale(&context,
        "en-abcdefghijklmnopqrstuvwxyz0123456789-TOO-LONG"));
    CHECK(strcmp(infiltratr_i18n_locale(&context), before) == 0);

    CHECK(!infiltratr_i18n_set_locale(&context, "zz-ZZ"));
    CHECK(strcmp(infiltratr_i18n_get(&context, "nav.vehicle"), "Vehicle") == 0);

    {
        const InfiltratrI18nEntry null_key_entries[] = {
            {NULL, "value"}
        };
        const InfiltratrI18nEntry null_value_entries[] = {
            {"key", NULL}
        };
        const InfiltratrI18nCatalog missing_entries[] = {
            {"en-AU", NULL, 1U}
        };
        const InfiltratrI18nCatalog missing_locale[] = {
            {NULL, english_entries, 1U}
        };
        const InfiltratrI18nCatalog malformed_locale[] = {
            {"en--AU", english_entries, 1U}
        };
        const InfiltratrI18nCatalog null_key[] = {
            {"en-AU", null_key_entries, 1U}
        };
        const InfiltratrI18nCatalog null_value[] = {
            {"en-AU", null_value_entries, 1U}
        };

        CHECK(!infiltratr_i18n_init(&context, missing_entries, 1U, "en-AU"));
        CHECK(context.catalogs == NULL && context.catalog_count == 0U);
        CHECK(!infiltratr_i18n_init(&context, missing_locale, 1U, "en-AU"));
        CHECK(context.catalogs == NULL && context.catalog_count == 0U);
        CHECK(!infiltratr_i18n_init(&context, malformed_locale, 1U, "en-AU"));
        CHECK(context.catalogs == NULL && context.catalog_count == 0U);
        CHECK(!infiltratr_i18n_init(&context, null_key, 1U, "en-AU"));
        CHECK(context.catalogs == NULL && context.catalog_count == 0U);
        CHECK(!infiltratr_i18n_init(&context, null_value, 1U, "en-AU"));
        CHECK(context.catalogs == NULL && context.catalog_count == 0U);
        CHECK(!infiltratr_i18n_init(&context, catalogs,
                                    sizeof(catalogs) / sizeof(catalogs[0]),
                                    "en--AU"));
        CHECK(context.catalogs == NULL && context.catalog_count == 0U);

        CHECK(infiltratr_i18n_init(&context, catalogs,
                                   sizeof(catalogs) / sizeof(catalogs[0]),
                                   "en-AU"));
    }

    if (failures != 0) {
        fprintf(stderr, "%d localisation test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
