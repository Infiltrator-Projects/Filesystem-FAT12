// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file design.h
 * @brief Product-neutral Infiltrator theme contract for native consumers.
 *
 * The platform adapter owns detection of the operating-system appearance.
 * Common owns only the stable System/Day/Night policy and semantic Day/Night
 * palette values. Product accents and domain-specific colours remain local.
 */
#ifndef INFILTRATR_COMMON_DESIGN_H
#define INFILTRATR_COMMON_DESIGN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INFILTRATR_THEME_PALETTE_ABI 1U
#define INFILTRATR_DESIGN_METRICS_ABI 1U
#define INFILTRATR_TYPOGRAPHY_ABI 1U

typedef enum {
    INFILTRATR_THEME_SYSTEM = 0,
    INFILTRATR_THEME_DAY = 1,
    INFILTRATR_THEME_NIGHT = 2
} InfiltratrThemeMode;

typedef struct {
    size_t struct_size;
    uint32_t abi_version;
    uint32_t background_rgb;
    uint32_t panel_rgb;
    uint32_t card_rgb;
    uint32_t surface_rgb;
    uint32_t input_rgb;
    uint32_t border_rgb;
    uint32_t text_rgb;
    uint32_t title_rgb;
    uint32_t muted_rgb;
    uint32_t subtle_rgb;
    uint32_t button_background_rgb;
    uint32_t button_foreground_rgb;
    uint32_t selection_background_rgb;
    uint32_t selection_foreground_rgb;
    uint32_t neutral_accent_rgb;
    uint32_t success_rgb;
    uint32_t warning_rgb;
    uint32_t fault_rgb;
    uint32_t info_rgb;
    uint32_t operation_rgb;
    uint32_t card_hover_rgb;
    uint32_t surface_hover_rgb;
    uint32_t operation_hover_rgb;
    uint32_t equals_hover_rgb;
    /* Exact product-neutral colour roles promoted from the Linux MBLINK reference face. */
    uint32_t titlebar_rgb;
    uint32_t connection_rgb;
    uint32_t connection_border_rgb;
    uint32_t heading_rgb;
    uint32_t summary_rgb;
    uint32_t kicker_rgb;
    uint32_t detail_label_rgb;
    uint32_t note_rgb;
    uint32_t status_border_rgb;
    uint32_t accent_foreground_rgb;
    uint32_t accent_hover_rgb;
    uint32_t selected_summary_rgb;
    uint32_t warning_muted_rgb;
    uint32_t warning_border_rgb;
    uint32_t success_border_rgb;
} InfiltratrThemePalette;

/** Product-neutral native rendering metrics from the canonical design contract. */
typedef struct {
    size_t struct_size;
    uint32_t abi_version;
    uint32_t small_radius;
    uint32_t control_radius;
    uint32_t card_radius;
    uint32_t panel_radius;
    uint32_t compact_spacing;
    uint32_t control_spacing;
    uint32_t section_spacing;
    uint32_t content_padding;
    uint32_t screen_padding;
} InfiltratrDesignMetrics;

/**
 * Canonical typography identity. Common owns family names, role weights,
 * filenames and normal platform fallbacks; individual products may impose a
 * stricter no-fallback policy when they bundle the required faces themselves.
 */
typedef struct {
    size_t struct_size;
    uint32_t abi_version;
    const char *ui_family;
    const char *brand_family;
    uint32_t ui_regular_weight;
    uint32_t ui_bold_weight;
    uint32_t brand_weight;
    const char *brand_regular_filename;
    const char *ui_bold_filename;
    const char *ui_regular_filename;
    const char *gtk_fallback;
    const char *apple_fallback;
    const char *windows_fallback;
} InfiltratrTypography;

const char *infiltratr_theme_mode_name(InfiltratrThemeMode mode);
/** Return the canonical lowercase persistence key: system, day or night. */
const char *infiltratr_theme_mode_key(InfiltratrThemeMode mode);
/**
 * Parse a complete ASCII theme key case-insensitively.
 *
 * Accepted keys are system, day and night. Caller output is unchanged for
 * NULL, empty or unknown input.
 */
bool infiltratr_theme_mode_parse(const char *text, InfiltratrThemeMode *mode);
InfiltratrThemeMode infiltratr_theme_mode_next(InfiltratrThemeMode mode);
const InfiltratrThemePalette *infiltratr_theme_resolve(
    InfiltratrThemeMode mode, bool system_is_dark);
const InfiltratrDesignMetrics *infiltratr_design_metrics(void);
const InfiltratrTypography *infiltratr_typography(void);

#ifdef __cplusplus
}
#endif

#endif
