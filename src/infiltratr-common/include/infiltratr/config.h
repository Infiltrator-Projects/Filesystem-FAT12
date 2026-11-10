// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file config.h
 * @brief Allocation-free parsing primitives for simple key=value configuration.
 *
 * The parser is deliberately toolkit- and platform-neutral. It recognises
 * blank/comment lines, splits one mutable key=value line in place and parses
 * the conservative boolean vocabulary shared by graphical applications.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATR_COMMON_CONFIG_H
#define INFILTRATR_COMMON_CONFIG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Classification returned by infiltratr_config_parse_line(). */
typedef enum {
    INFILTRATR_CONFIG_LINE_IGNORED = 0,
    INFILTRATR_CONFIG_LINE_ENTRY,
    INFILTRATR_CONFIG_LINE_INVALID
} InfiltratrConfigLineStatus;

/**
 * Parse one mutable key=value configuration line without allocation.
 *
 * Leading/trailing ASCII whitespace and CR/LF are removed. Blank lines and
 * lines whose first non-whitespace byte is '#' are ignored. The first '='
 * separates key from value; surrounding whitespace is removed from both.
 * Empty keys and non-comment lines without '=' are invalid. Empty values are
 * valid. On ENTRY, @p key and @p value point inside @p line.
 *
 * @param [in,out] line Mutable NUL-terminated line.
 * @param [out] key Receives the key pointer for an entry, otherwise NULL.
 * @param [out] value Receives the value pointer for an entry, otherwise NULL.
 * @return Line classification.
 */
InfiltratrConfigLineStatus infiltratr_config_parse_line(
    char *line, char **key, char **value);

/**
 * Parse the conservative configuration boolean vocabulary.
 *
 * ASCII case is ignored. Accepted true values are 1, true and yes; accepted
 * false values are 0, false and no. Surrounding ASCII whitespace is allowed.
 * Caller output is unchanged on failure.
 *
 * @param [in] text NUL-terminated input.
 * @param [out] value Receives the parsed value on success.
 * @return true only for a complete recognised boolean token.
 */
bool infiltratr_config_parse_bool(const char *text, bool *value);

#ifdef __cplusplus
}
#endif

#endif
