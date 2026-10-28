// SPDX-License-Identifier: GPL-3.0-or-later
#include "infiltratr/token.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

bool infiltratr_parse_u64_token(const char **cursor, unsigned int base,
                                uint64_t *value)
{
    if (!cursor || !*cursor || !value || base == 1U || base > 36U)
        return false;
    const char *start = *cursor;
    while (*start && isspace((unsigned char)*start)) start++;
    if (*start == '+' || *start == '-' || *start == '\0') return false;
    errno = 0;
    char *end = NULL;
    const unsigned long long parsed = strtoull(start, &end, (int)base);
    if (errno != 0 || end == start ||
        parsed > (unsigned long long)UINT64_MAX)
        return false;
    *cursor = end;
    *value = (uint64_t)parsed;
    return true;
}

bool infiltratr_parse_i64_token(const char **cursor, unsigned int base,
                                int64_t *value)
{
    if (!cursor || !*cursor || !value || base == 1U || base > 36U)
        return false;
    const char *start = *cursor;
    while (*start && isspace((unsigned char)*start)) start++;
    if (*start == '\0') return false;
    errno = 0;
    char *end = NULL;
    const long long parsed = strtoll(start, &end, (int)base);
    if (errno != 0 || end == start || parsed < (long long)INT64_MIN ||
        parsed > (long long)INT64_MAX)
        return false;
    *cursor = end;
    *value = (int64_t)parsed;
    return true;
}
