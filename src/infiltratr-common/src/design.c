// SPDX-License-Identifier: GPL-3.0-or-later
#include "infiltratr/design.h"

#include "ascii_internal.h"

#include <stddef.h>
#include <string.h>

static const InfiltratrDesignMetrics design_metrics = {
    .struct_size = sizeof(InfiltratrDesignMetrics),
    .abi_version = INFILTRATR_DESIGN_METRICS_ABI,
    .small_radius = 6U,
    .control_radius = 10U,
    .card_radius = 12U,
    .panel_radius = 18U,
    .compact_spacing = 6U,
    .control_spacing = 10U,
    .section_spacing = 18U,
    .content_padding = 16U,
    .screen_padding = 20U
};

static const InfiltratrTypography typography = {
    .struct_size = sizeof(InfiltratrTypography),
    .abi_version = INFILTRATR_TYPOGRAPHY_ABI,
    .ui_family = "MB Corpo S Title WEB",
    .brand_family = "MB Corpo A Title Cond WEB",
    .ui_regular_weight = 400U,
    .ui_bold_weight = 700U,
    .brand_weight = 400U,
    .brand_regular_filename = "mb_corpo_a_cond_regular.ttf",
    .ui_bold_filename = "mb_corpo_s_bold.ttf",
    .ui_regular_filename = "mb_corpo_s_regular.ttf",
    .gtk_fallback = "Sans",
    .apple_fallback = "system",
    .windows_fallback = "Segoe UI"
};

static const InfiltratrThemePalette day_palette = {
    .struct_size = sizeof(InfiltratrThemePalette),
    .abi_version = INFILTRATR_THEME_PALETTE_ABI,
    .background_rgb = 0xFFFFFFU,
    .panel_rgb = 0xFFFFFFU,
    .card_rgb = 0xF8F9FAU,
    .surface_rgb = 0xECEFF2U,
    .input_rgb = 0xFFFFFFU,
    .border_rgb = 0xC7CDD3U,
    .text_rgb = 0x20252BU,
    .title_rgb = 0x111418U,
    .muted_rgb = 0x59636CU,
    .subtle_rgb = 0x737D86U,
    .button_background_rgb = 0x20252BU,
    .button_foreground_rgb = 0xFFFFFFU,
    .selection_background_rgb = 0xDDE2E7U,
    .selection_foreground_rgb = 0x111418U,
    .neutral_accent_rgb = 0x00ADEFU,
    .success_rgb = 0x3A8A58U,
    .warning_rgb = 0x9A6500U,
    .fault_rgb = 0xB54848U,
    .info_rgb = 0x467AA3U,
    .operation_rgb = 0xE8ECEFU,
    .card_hover_rgb = 0xEEF1F3U,
    .surface_hover_rgb = 0xF1F3F5U,
    .operation_hover_rgb = 0xDDE2E7U,
    .equals_hover_rgb = 0x343B42U,
    .titlebar_rgb = 0xECEFF2U,
    .connection_rgb = 0xF8F9FAU,
    .connection_border_rgb = 0xC7CDD3U,
    .heading_rgb = 0x111418U,
    .summary_rgb = 0x59636CU,
    .kicker_rgb = 0x737D86U,
    .detail_label_rgb = 0x737D86U,
    .note_rgb = 0x59636CU,
    .status_border_rgb = 0xC7CDD3U,
    .accent_foreground_rgb = 0x031018U,
    .accent_hover_rgb = 0x25B8F0U,
    .selected_summary_rgb = 0x467AA3U,
    .warning_muted_rgb = 0x9A6500U,
    .warning_border_rgb = 0x9A6500U,
    .success_border_rgb = 0x3A8A58U
};

static const InfiltratrThemePalette night_palette = {
    .struct_size = sizeof(InfiltratrThemePalette),
    .abi_version = INFILTRATR_THEME_PALETTE_ABI,
    .background_rgb = 0x050608U,
    .panel_rgb = 0x101318U,
    .card_rgb = 0x171B20U,
    .surface_rgb = 0x0D1014U,
    .input_rgb = 0x0E1115U,
    .border_rgb = 0x353A40U,
    .text_rgb = 0xE8ECEFU,
    .title_rgb = 0xEEF1F3U,
    .muted_rgb = 0xAEB6BDU,
    .subtle_rgb = 0x899198U,
    .button_background_rgb = 0xD7DDE2U,
    .button_foreground_rgb = 0x111418U,
    .selection_background_rgb = 0x2B3137U,
    .selection_foreground_rgb = 0xEEF1F3U,
    .neutral_accent_rgb = 0x00ADEFU,
    .success_rgb = 0x63AB7CU,
    .warning_rgb = 0xD19E47U,
    .fault_rgb = 0xC96B6BU,
    .info_rgb = 0x7FA7C9U,
    .operation_rgb = 0x20252BU,
    .card_hover_rgb = 0x22272DU,
    .surface_hover_rgb = 0x171B20U,
    .operation_hover_rgb = 0x2B3137U,
    .equals_hover_rgb = 0xEEF1F3U,
    .titlebar_rgb = 0x202125U,
    .connection_rgb = 0x0E1115U,
    .connection_border_rgb = 0x31363BU,
    .heading_rgb = 0xE7EBEEU,
    .summary_rgb = 0x98A1A9U,
    .kicker_rgb = 0x8C949BU,
    .detail_label_rgb = 0x7E858CU,
    .note_rgb = 0x9CA4ABU,
    .status_border_rgb = 0x3B4147U,
    .accent_foreground_rgb = 0x031018U,
    .accent_hover_rgb = 0x25B8F0U,
    .selected_summary_rgb = 0x79CAE8U,
    .warning_muted_rgb = 0xC69A4CU,
    .warning_border_rgb = 0x72572FU,
    .success_border_rgb = 0x365F45U
};

static bool structure_covers(size_t structure_size, size_t field_offset,
                             size_t field_size)
{
    return structure_size >= field_offset + field_size;
}

static bool rgb24(uint32_t value)
{
    return value <= UINT32_C(0xFFFFFF);
}

static bool nonempty(const char *text)
{
    return text && text[0] != '\0';
}

bool infiltratr_theme_palette_is_valid(const InfiltratrThemePalette *palette)
{
    if (!palette || palette->abi_version != INFILTRATR_THEME_PALETTE_ABI ||
        !structure_covers(
            palette->struct_size,
            offsetof(InfiltratrThemePalette, success_border_rgb),
            sizeof(palette->success_border_rgb)))
        return false;

    const uint32_t colours[] = {
        palette->background_rgb,
        palette->panel_rgb,
        palette->card_rgb,
        palette->surface_rgb,
        palette->input_rgb,
        palette->border_rgb,
        palette->text_rgb,
        palette->title_rgb,
        palette->muted_rgb,
        palette->subtle_rgb,
        palette->button_background_rgb,
        palette->button_foreground_rgb,
        palette->selection_background_rgb,
        palette->selection_foreground_rgb,
        palette->neutral_accent_rgb,
        palette->success_rgb,
        palette->warning_rgb,
        palette->fault_rgb,
        palette->info_rgb,
        palette->operation_rgb,
        palette->card_hover_rgb,
        palette->surface_hover_rgb,
        palette->operation_hover_rgb,
        palette->equals_hover_rgb,
        palette->titlebar_rgb,
        palette->connection_rgb,
        palette->connection_border_rgb,
        palette->heading_rgb,
        palette->summary_rgb,
        palette->kicker_rgb,
        palette->detail_label_rgb,
        palette->note_rgb,
        palette->status_border_rgb,
        palette->accent_foreground_rgb,
        palette->accent_hover_rgb,
        palette->selected_summary_rgb,
        palette->warning_muted_rgb,
        palette->warning_border_rgb,
        palette->success_border_rgb
    };
    for (size_t index = 0U; index < sizeof(colours) / sizeof(colours[0]); index++)
        if (!rgb24(colours[index])) return false;
    return true;
}

bool infiltratr_design_metrics_is_valid(const InfiltratrDesignMetrics *metrics)
{
    return metrics &&
        metrics->abi_version == INFILTRATR_DESIGN_METRICS_ABI &&
        structure_covers(
            metrics->struct_size,
            offsetof(InfiltratrDesignMetrics, screen_padding),
            sizeof(metrics->screen_padding));
}

bool infiltratr_typography_is_valid(const InfiltratrTypography *value)
{
    if (!value || value->abi_version != INFILTRATR_TYPOGRAPHY_ABI ||
        !structure_covers(
            value->struct_size,
            offsetof(InfiltratrTypography, windows_fallback),
            sizeof(value->windows_fallback)))
        return false;
    return nonempty(value->ui_family) &&
        nonempty(value->brand_family) &&
        nonempty(value->brand_regular_filename) &&
        nonempty(value->ui_bold_filename) &&
        nonempty(value->ui_regular_filename) &&
        nonempty(value->gtk_fallback) &&
        nonempty(value->apple_fallback) &&
        nonempty(value->windows_fallback);
}

const char *infiltratr_theme_mode_name(InfiltratrThemeMode mode)
{
    switch (mode) {
    case INFILTRATR_THEME_SYSTEM:
        return "System";
    case INFILTRATR_THEME_DAY:
        return "Day";
    case INFILTRATR_THEME_NIGHT:
        return "Night";
    default:
        return "System";
    }
}

const char *infiltratr_theme_mode_key(InfiltratrThemeMode mode)
{
    switch (mode) {
    case INFILTRATR_THEME_DAY:
        return "day";
    case INFILTRATR_THEME_NIGHT:
        return "night";
    case INFILTRATR_THEME_SYSTEM:
    default:
        return "system";
    }
}

bool infiltratr_theme_mode_parse(const char *text, InfiltratrThemeMode *mode)
{
    if (!text || !mode) return false;
    const size_t length = strlen(text);
    InfiltratrThemeMode parsed;
    if (infiltratr_ascii_equal_ci_span(text, length, "system"))
        parsed = INFILTRATR_THEME_SYSTEM;
    else if (infiltratr_ascii_equal_ci_span(text, length, "day"))
        parsed = INFILTRATR_THEME_DAY;
    else if (infiltratr_ascii_equal_ci_span(text, length, "night"))
        parsed = INFILTRATR_THEME_NIGHT;
    else
        return false;
    *mode = parsed;
    return true;
}

InfiltratrThemeMode infiltratr_theme_mode_next(InfiltratrThemeMode mode)
{
    switch (mode) {
    case INFILTRATR_THEME_SYSTEM:
        return INFILTRATR_THEME_DAY;
    case INFILTRATR_THEME_DAY:
        return INFILTRATR_THEME_NIGHT;
    case INFILTRATR_THEME_NIGHT:
    default:
        return INFILTRATR_THEME_SYSTEM;
    }
}

const InfiltratrThemePalette *infiltratr_theme_resolve(
    InfiltratrThemeMode mode, bool system_is_dark)
{
    if (mode == INFILTRATR_THEME_DAY) {
        return &day_palette;
    }
    if (mode == INFILTRATR_THEME_NIGHT) {
        return &night_palette;
    }
    return system_is_dark ? &night_palette : &day_palette;
}

const InfiltratrDesignMetrics *infiltratr_design_metrics(void)
{
    return &design_metrics;
}

const InfiltratrTypography *infiltratr_typography(void)
{
    return &typography;
}
