// SPDX-License-Identifier: GPL-3.0-or-later
#include <infiltratr/escape.h>
#include <infiltratr/arithmetic.h>
#include "ascii_internal.h"

#include <stdint.h>
#include <string.h>

typedef enum {
    INFILTRATR_ESCAPE_MODE_HTML,
    INFILTRATR_ESCAPE_MODE_JSON,
    INFILTRATR_ESCAPE_MODE_URI
} InfiltratrEscapeMode;

static bool uri_unreserved(unsigned char value)
{
    return infiltratr_ascii_alpha(value) || infiltratr_ascii_digit(value) ||
           value == (unsigned char)'-' || value == (unsigned char)'.' ||
           value == (unsigned char)'_' || value == (unsigned char)'~';
}

static size_t encoded_length(InfiltratrEscapeMode mode, unsigned char value)
{
    if (mode == INFILTRATR_ESCAPE_MODE_URI)
        return uri_unreserved(value) ? 1U : 3U;

    if (mode == INFILTRATR_ESCAPE_MODE_HTML) {
        switch (value) {
        case '&': return 5U;
        case '<': return 4U;
        case '>': return 4U;
        case '"': return 6U;
        case '\'': return 5U;
        default: return 1U;
        }
    }

    switch (value) {
    case '"':
    case '\\':
    case '\b':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
        return 2U;
    default:
        return value < 0x20U ? 6U : 1U;
    }
}

static bool measure(InfiltratrEscapeMode mode, const char *input, size_t *required)
{
    if (!input || !required) return false;
    size_t total = 1U;
    for (const unsigned char *p = (const unsigned char *)input; *p; ++p) {
        if (!infiltratr_size_add_checked(total, encoded_length(mode, *p), &total)) return false;
    }
    *required = total;
    return true;
}

static char hex_digit(unsigned value)
{
    static const char digits[] = "0123456789ABCDEF";
    return digits[value & 0x0fU];
}

static char *emit_html(char *out, unsigned char value)
{
    const char *replacement = NULL;
    switch (value) {
    case '&': replacement = "&amp;"; break;
    case '<': replacement = "&lt;"; break;
    case '>': replacement = "&gt;"; break;
    case '"': replacement = "&quot;"; break;
    case '\'': replacement = "&#39;"; break;
    default:
        *out++ = (char)value;
        return out;
    }
    const size_t length = strlen(replacement);
    memcpy(out, replacement, length);
    return out + length;
}

static char *emit_json(char *out, unsigned char value)
{
    switch (value) {
    case '"': *out++ = '\\'; *out++ = '"'; return out;
    case '\\': *out++ = '\\'; *out++ = '\\'; return out;
    case '\b': *out++ = '\\'; *out++ = 'b'; return out;
    case '\f': *out++ = '\\'; *out++ = 'f'; return out;
    case '\n': *out++ = '\\'; *out++ = 'n'; return out;
    case '\r': *out++ = '\\'; *out++ = 'r'; return out;
    case '\t': *out++ = '\\'; *out++ = 't'; return out;
    default:
        if (value < 0x20U) {
            *out++ = '\\'; *out++ = 'u'; *out++ = '0'; *out++ = '0';
            *out++ = hex_digit(value >> 4U);
            *out++ = hex_digit(value);
            return out;
        }
        *out++ = (char)value;
        return out;
    }
}

static char *emit_uri(char *out, unsigned char value)
{
    if (uri_unreserved(value)) {
        *out++ = (char)value;
        return out;
    }
    *out++ = '%';
    *out++ = hex_digit(value >> 4U);
    *out++ = hex_digit(value);
    return out;
}

static bool transform(InfiltratrEscapeMode mode, const char *input,
                      char *output, size_t size, size_t *required_size)
{
    size_t required = 0U;
    if (!measure(mode, input, &required)) {
        if (required_size) *required_size = 0U;
        if (output && size) output[0] = '\0';
        return false;
    }
    if (required_size) *required_size = required;

    if (!output) return size == 0U;
    if (size < required) {
        if (size) output[0] = '\0';
        return false;
    }

    char *cursor = output;
    for (const unsigned char *p = (const unsigned char *)input; *p; ++p) {
        if (mode == INFILTRATR_ESCAPE_MODE_HTML) cursor = emit_html(cursor, *p);
        else if (mode == INFILTRATR_ESCAPE_MODE_JSON) cursor = emit_json(cursor, *p);
        else cursor = emit_uri(cursor, *p);
    }
    *cursor = '\0';
    return true;
}

bool infiltratr_escape_html(const char *input, char *output, size_t size,
                            size_t *required_size)
{
    return transform(INFILTRATR_ESCAPE_MODE_HTML, input, output, size, required_size);
}

bool infiltratr_escape_json(const char *input, char *output, size_t size,
                            size_t *required_size)
{
    return transform(INFILTRATR_ESCAPE_MODE_JSON, input, output, size, required_size);
}

bool infiltratr_escape_uri_component(const char *input, char *output, size_t size,
                                     size_t *required_size)
{
    return transform(INFILTRATR_ESCAPE_MODE_URI, input, output, size, required_size);
}

static bool csv_formula_candidate(const char *input)
{
    const unsigned char *cursor = (const unsigned char *)input;
    while (*cursor) {
        if (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' ||
            *cursor == '\n' || *cursor < 32U || *cursor == 127U) {
            cursor++;
            continue;
        }
        return *cursor == '=' || *cursor == '+' || *cursor == '-' ||
               *cursor == '@';
    }
    return false;
}

bool infiltratr_escape_csv_field(const char *input, bool spreadsheet_safe,
                                 char *output, size_t size,
                                 size_t *required_size)
{
    if (required_size) *required_size = 0U;
    if (output && size > 0U) output[0] = '\0';
    if (!input) return false;

    size_t required = 3U; /* opening quote, closing quote, NUL */
    const bool protect = spreadsheet_safe && csv_formula_candidate(input);
    if (protect && !infiltratr_size_add_checked(required, 1U, &required)) return false;

    for (const unsigned char *cursor = (const unsigned char *)input;
         *cursor; cursor++) {
        size_t amount = 0U;
        if (*cursor == '"')
            amount = 2U;
        else if (*cursor == '\t' || (*cursor >= 32U && *cursor != 127U))
            amount = 1U;
        if (!infiltratr_size_add_checked(required, amount, &required)) return false;
    }
    if (required_size) *required_size = required;
    if (!output) return size == 0U;
    if (size < required) return false;

    char *destination = output;
    *destination++ = '"';
    if (protect) *destination++ = '\'';
    for (const unsigned char *cursor = (const unsigned char *)input;
         *cursor; cursor++) {
        if (*cursor == '"') {
            *destination++ = '"';
            *destination++ = '"';
        } else if (*cursor == '\t') {
            *destination++ = ' ';
        } else if (*cursor >= 32U && *cursor != 127U) {
            *destination++ = (char)*cursor;
        }
    }
    *destination++ = '"';
    *destination = '\0';
    return true;
}
