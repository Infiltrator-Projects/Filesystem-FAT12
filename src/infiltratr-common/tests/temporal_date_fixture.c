// SPDX-License-Identifier: GPL-3.0-or-later
#include "infiltratr/temporal.h"
#include <string.h>

bool calendar_format_date_v1(
    const char *calendar_id, int32_t year, int32_t month, int32_t day,
    const char *part, char *buffer, size_t capacity, size_t *length);

bool calendar_format_date_v1(
    const char *calendar_id, int32_t year, int32_t month, int32_t day,
    const char *part, char *buffer, size_t capacity, size_t *length)
{
    const char *text = strcmp(part, "full") == 0 ? "中文日期 full" : "中文日期";
    const size_t size = strlen(text);

    if (capacity <= size || year != 2026 || month != 10 || day != 10) {
        return false;
    }
    memcpy(buffer, text, size + 1U);
    *length = size;
    if (strcmp(calendar_id, "roman") == 0) {
        buffer[0] = (char)0xff;
    }
    return strcmp(calendar_id, "julian") != 0;
}
