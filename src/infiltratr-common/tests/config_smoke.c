// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file config_smoke.c
 * @brief Contract tests for allocation-free configuration parsing.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/config.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>

int main(void)
{
    char *key = NULL;
    char *value = NULL;
    char entry[] = "  update_interval_ms = 1000 \r\n";
    assert(infiltratr_config_parse_line(entry, &key, &value) ==
           INFILTRATR_CONFIG_LINE_ENTRY);
    assert(strcmp(key, "update_interval_ms") == 0);
    assert(strcmp(value, "1000") == 0);

    char empty_value[] = "name =   ";
    assert(infiltratr_config_parse_line(empty_value, &key, &value) ==
           INFILTRATR_CONFIG_LINE_ENTRY);
    assert(strcmp(key, "name") == 0);
    assert(strcmp(value, "") == 0);

    char comment[] = "  # comment";
    assert(infiltratr_config_parse_line(comment, &key, &value) ==
           INFILTRATR_CONFIG_LINE_IGNORED);
    assert(key == NULL && value == NULL);

    char invalid[] = "missing separator";
    assert(infiltratr_config_parse_line(invalid, &key, &value) ==
           INFILTRATR_CONFIG_LINE_INVALID);

    bool boolean = false;
    assert(infiltratr_config_parse_bool(" YES ", &boolean) && boolean);
    assert(infiltratr_config_parse_bool("false", &boolean) && !boolean);
    boolean = true;
    assert(!infiltratr_config_parse_bool("on", &boolean));
    assert(boolean);
    return 0;
}
