// SPDX-License-Identifier: GPL-3.0-or-later
#include <infiltratr/escape.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    size_t required = 0U;
    assert(infiltratr_escape_html("&<>\"'", NULL, 0U, &required));
    assert(required == strlen("&amp;&lt;&gt;&quot;&#39;") + 1U);

    char html[64];
    assert(infiltratr_escape_html("&<>\"'", html, sizeof(html), &required));
    assert(strcmp(html, "&amp;&lt;&gt;&quot;&#39;") == 0);

    char tiny[4] = "xxx";
    assert(!infiltratr_escape_html("&", tiny, sizeof(tiny), &required));
    assert(strcmp(tiny, "") == 0);
    assert(required == 6U);

    const char json_input[] = {'"', '\\', '\b', '\f', '\n', '\r', '\t', 1, 0};
    char json[64];
    assert(infiltratr_escape_json(json_input, json, sizeof(json), &required));
    assert(strcmp(json, "\\\"\\\\\\b\\f\\n\\r\\t\\u0001") == 0);

    char uri[64];
    assert(infiltratr_escape_uri_component("azAZ09-._~ /?", uri, sizeof(uri), &required));
    assert(strcmp(uri, "azAZ09-._~%20%2F%3F") == 0);

    assert(!infiltratr_escape_html(NULL, html, sizeof(html), &required));
    assert(html[0] == '\0');
    assert(required == 0U);
    assert(!infiltratr_escape_json("x", NULL, 1U, &required));

    char csv[128];
    assert(infiltratr_escape_csv_field("normal \"text\"", true,
                                       csv, sizeof(csv), &required));
    assert(strcmp(csv, "\"normal \"\"text\"\"\"") == 0);
    assert(required == strlen(csv) + 1U);
    assert(infiltratr_escape_csv_field("  =SUM(A1:A2)", true,
                                       csv, sizeof(csv), &required));
    assert(strcmp(csv, "\"'  =SUM(A1:A2)\"") == 0);
    assert(infiltratr_escape_csv_field("a\tb\nc\r\x01" "d", false,
                                       csv, sizeof(csv), &required));
    assert(strcmp(csv, "\"a bcd\"") == 0);
    char csv_tiny[4] = "xxx";
    assert(!infiltratr_escape_csv_field("hello", true, csv_tiny,
                                        sizeof(csv_tiny), &required));
    assert(csv_tiny[0] == '\0');
    assert(required == 8U);
    assert(!infiltratr_escape_csv_field(NULL, true, csv, sizeof(csv),
                                        &required));

    puts("Libraries escape contract tests passed.");
    return 0;
}
