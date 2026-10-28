// SPDX-License-Identifier: GPL-3.0-or-later
#include "infiltratr/design.h"

#include <stddef.h>
#include <string.h>

int main(void)
{
    const InfiltratrThemePalette *day =
        infiltratr_theme_resolve(INFILTRATR_THEME_DAY, true);
    const InfiltratrThemePalette *night =
        infiltratr_theme_resolve(INFILTRATR_THEME_NIGHT, false);
    const InfiltratrThemePalette *system_light =
        infiltratr_theme_resolve(INFILTRATR_THEME_SYSTEM, false);
    const InfiltratrThemePalette *system_dark =
        infiltratr_theme_resolve(INFILTRATR_THEME_SYSTEM, true);

    if (day == NULL || night == NULL) return 1;
    if (day->abi_version != INFILTRATR_THEME_PALETTE_ABI) return 2;
    if (night->abi_version != INFILTRATR_THEME_PALETTE_ABI) return 3;
    if (day->background_rgb != 0xFFFFFFU) return 4;
    if (night->background_rgb != 0x050608U) return 5;
    if (day->button_background_rgb != 0x20252BU) return 6;
    if (night->button_background_rgb != 0xD7DDE2U) return 7;
    if (day->neutral_accent_rgb != 0x00ADEFU) return 11;
    if (night->neutral_accent_rgb != 0x00ADEFU) return 12;
    if (system_light != day || system_dark != night) return 8;
    if (strcmp(infiltratr_theme_mode_name(INFILTRATR_THEME_SYSTEM), "System") != 0) return 9;
    if (infiltratr_theme_mode_next(INFILTRATR_THEME_SYSTEM) != INFILTRATR_THEME_DAY) return 10;
    if (infiltratr_theme_mode_next(INFILTRATR_THEME_DAY) != INFILTRATR_THEME_NIGHT) return 11;
    if (infiltratr_theme_mode_next(INFILTRATR_THEME_NIGHT) != INFILTRATR_THEME_SYSTEM) return 12;
    if (strcmp(infiltratr_theme_mode_key(INFILTRATR_THEME_SYSTEM), "system") != 0) return 36;
    if (strcmp(infiltratr_theme_mode_key(INFILTRATR_THEME_DAY), "day") != 0) return 37;
    if (strcmp(infiltratr_theme_mode_key(INFILTRATR_THEME_NIGHT), "night") != 0) return 38;
    InfiltratrThemeMode parsed = INFILTRATR_THEME_SYSTEM;
    if (!infiltratr_theme_mode_parse("DAY", &parsed) ||
        parsed != INFILTRATR_THEME_DAY) return 39;
    if (!infiltratr_theme_mode_parse("night", &parsed) ||
        parsed != INFILTRATR_THEME_NIGHT) return 40;
    parsed = INFILTRATR_THEME_DAY;
    if (infiltratr_theme_mode_parse("unknown", &parsed) ||
        parsed != INFILTRATR_THEME_DAY) return 41;
    if (infiltratr_theme_mode_parse(NULL, &parsed) ||
        infiltratr_theme_mode_parse("day", NULL)) return 42;

    if (!infiltratr_theme_palette_is_valid(day) ||
        !infiltratr_theme_palette_is_valid(night) ||
        infiltratr_theme_palette_is_valid(NULL)) return 43;
    InfiltratrThemePalette palette_copy = *day;
    palette_copy.struct_size =
        offsetof(InfiltratrThemePalette, success_border_rgb);
    if (infiltratr_theme_palette_is_valid(&palette_copy)) return 44;
    palette_copy = *day;
    palette_copy.abi_version++;
    if (infiltratr_theme_palette_is_valid(&palette_copy)) return 45;
    palette_copy = *day;
    palette_copy.background_rgb = 0x01000000U;
    if (infiltratr_theme_palette_is_valid(&palette_copy)) return 46;
    palette_copy = *day;
    palette_copy.struct_size = sizeof(palette_copy) + 64U;
    if (!infiltratr_theme_palette_is_valid(&palette_copy)) return 47;

    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    if (metrics == NULL || metrics->abi_version != INFILTRATR_DESIGN_METRICS_ABI) return 13;
    if (metrics->control_radius != 10U || metrics->card_radius != 12U) return 14;
    if (metrics->compact_spacing != 6U || metrics->content_padding != 16U) return 15;
    if (!infiltratr_design_metrics_is_valid(metrics) ||
        infiltratr_design_metrics_is_valid(NULL)) return 48;
    InfiltratrDesignMetrics metrics_copy = *metrics;
    metrics_copy.struct_size = offsetof(InfiltratrDesignMetrics, screen_padding);
    if (infiltratr_design_metrics_is_valid(&metrics_copy)) return 49;
    metrics_copy = *metrics;
    metrics_copy.abi_version++;
    if (infiltratr_design_metrics_is_valid(&metrics_copy)) return 50;

    const InfiltratrTypography *type = infiltratr_typography();
    if (type == NULL || type->abi_version != INFILTRATR_TYPOGRAPHY_ABI) return 16;
    if (strcmp(type->ui_family, "MB Corpo S Title WEB") != 0) return 17;
    if (strcmp(type->brand_family, "MB Corpo A Title Cond WEB") != 0) return 18;
    if (type->ui_regular_weight != 400U || type->ui_bold_weight != 700U ||
        type->brand_weight != 400U) return 19;
    if (strcmp(type->brand_regular_filename, "mb_corpo_a_cond_regular.ttf") != 0 ||
        strcmp(type->ui_bold_filename, "mb_corpo_s_bold.ttf") != 0 ||
        strcmp(type->ui_regular_filename, "mb_corpo_s_regular.ttf") != 0) return 20;
    if (!infiltratr_typography_is_valid(type) ||
        infiltratr_typography_is_valid(NULL)) return 51;
    InfiltratrTypography type_copy = *type;
    type_copy.struct_size = offsetof(InfiltratrTypography, windows_fallback);
    if (infiltratr_typography_is_valid(&type_copy)) return 52;
    type_copy = *type;
    type_copy.abi_version++;
    if (infiltratr_typography_is_valid(&type_copy)) return 53;
    type_copy = *type;
    type_copy.ui_family = "";
    if (infiltratr_typography_is_valid(&type_copy)) return 54;
    type_copy = *type;
    type_copy.windows_fallback = NULL;
    if (infiltratr_typography_is_valid(&type_copy)) return 55;

    if (night->titlebar_rgb != 0x202125U) return 21;
    if (night->connection_rgb != 0x0E1115U) return 22;
    if (night->connection_border_rgb != 0x31363BU) return 23;
    if (night->heading_rgb != 0xE7EBEEU) return 24;
    if (night->summary_rgb != 0x98A1A9U) return 25;
    if (night->kicker_rgb != 0x8C949BU) return 26;
    if (night->detail_label_rgb != 0x7E858CU) return 27;
    if (night->note_rgb != 0x9CA4ABU) return 28;
    if (night->status_border_rgb != 0x3B4147U) return 29;
    if (night->accent_foreground_rgb != 0x031018U) return 30;
    if (night->accent_hover_rgb != 0x25B8F0U) return 31;
    if (night->selected_summary_rgb != 0x79CAE8U) return 32;
    if (night->warning_muted_rgb != 0xC69A4CU) return 33;
    if (night->warning_border_rgb != 0x72572FU) return 34;
    if (night->success_border_rgb != 0x365F45U) return 35;
    return 0;
}
