// SPDX-License-Identifier: GPL-3.0-or-later
#include <infiltratr/graphics.h>
#include <infiltratr/timing.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int valid_dimensions(size_t width, size_t height, size_t *count) {
    if (!count || width == 0 || height == 0) return 0;
    if (width > SIZE_MAX / height) return 0;
    *count = width * height;
    if (*count > SIZE_MAX / sizeof(uint32_t)) return 0;
    return 1;
}

static uintmax_t negative_int_magnitude(int value) {
    return value < 0 ? (uintmax_t)(-(value + 1)) + 1U : 0U;
}

static int clip_axis_span(int origin, size_t length, size_t limit,
                          size_t *local_start, size_t *surface_start,
                          size_t *count) {
    const uintmax_t span = (uintmax_t)length;
    const uintmax_t skipped = negative_int_magnitude(origin);
    uintmax_t start;
    uintmax_t visible;
    uintmax_t available;

    if (!local_start || !surface_start || !count || length == 0U || limit == 0U)
        return 0;
    if (skipped >= span) return 0;

    start = origin >= 0 ? (uintmax_t)origin : 0U;
    if (start >= (uintmax_t)limit) return 0;

    visible = span - skipped;
    available = (uintmax_t)limit - start;
    if (visible > available) visible = available;
    if (visible == 0U) return 0;

    *local_start = (size_t)skipped;
    *surface_start = (size_t)start;
    *count = (size_t)visible;
    return 1;
}

static int clip_blit_axis(int source_origin, int destination_origin,
                          size_t length, size_t source_limit,
                          size_t destination_limit, size_t *source_start,
                          size_t *destination_start, size_t *count) {
    const uintmax_t span = (uintmax_t)length;
    const uintmax_t source_negative = negative_int_magnitude(source_origin);
    const uintmax_t destination_negative =
        negative_int_magnitude(destination_origin);
    const uintmax_t skipped =
        source_negative > destination_negative
            ? source_negative : destination_negative;
    uintmax_t source_position;
    uintmax_t destination_position;
    uintmax_t visible;
    uintmax_t available;

    if (!source_start || !destination_start || !count ||
        length == 0U || source_limit == 0U || destination_limit == 0U)
        return 0;
    if (skipped >= span) return 0;

    source_position = source_origin >= 0
        ? (uintmax_t)source_origin + skipped
        : skipped - source_negative;
    destination_position = destination_origin >= 0
        ? (uintmax_t)destination_origin + skipped
        : skipped - destination_negative;

    if (source_position >= (uintmax_t)source_limit ||
        destination_position >= (uintmax_t)destination_limit)
        return 0;

    visible = span - skipped;
    available = (uintmax_t)source_limit - source_position;
    if (visible > available) visible = available;
    available = (uintmax_t)destination_limit - destination_position;
    if (visible > available) visible = available;
    if (visible == 0U) return 0;

    *source_start = (size_t)source_position;
    *destination_start = (size_t)destination_position;
    *count = (size_t)visible;
    return 1;
}

static int coordinate_from_local(int origin, size_t local, size_t limit,
                                 size_t *coordinate) {
    const uintmax_t negative = negative_int_magnitude(origin);
    uintmax_t position;

    if (!coordinate) return 0;
    if (origin >= 0)
        position = (uintmax_t)origin + (uintmax_t)local;
    else {
        if ((uintmax_t)local < negative) return 0;
        position = (uintmax_t)local - negative;
    }

    if (position >= (uintmax_t)limit) return 0;
    *coordinate = (size_t)position;
    return 1;
}

static int scaled_offset(size_t local, size_t source_span,
                         size_t destination_span, size_t *offset) {
    uint64_t partition = 0U;

    if (!offset || source_span == 0U || destination_span == 0U)
        return 0;
#if SIZE_MAX > UINT64_MAX
    if (local > UINT64_MAX || source_span > UINT64_MAX ||
        destination_span > UINT64_MAX)
        return 0;
#endif
    if (!infiltratr_cycle_partition_u64(
            (uint64_t)local, (uint64_t)destination_span,
            (uint64_t)source_span, &partition, NULL))
        return 0;

    *offset = (size_t)partition;
    return 1;
}

static int clip_centered_axis(int center, size_t radius, size_t limit,
                              size_t *start, size_t *count) {
    const uintmax_t negative = negative_int_magnitude(center);
    uintmax_t first = 0U;
    uintmax_t last;
    uintmax_t c;

    if (!start || !count || limit == 0U) return 0;

    if (center >= 0) {
        c = (uintmax_t)center;
        first = (uintmax_t)radius > c ? 0U : c - (uintmax_t)radius;
        last = (uintmax_t)radius > UINTMAX_MAX - c
            ? UINTMAX_MAX : c + (uintmax_t)radius;
    } else {
        if ((uintmax_t)radius < negative) return 0;
        last = (uintmax_t)radius - negative;
    }

    if (first >= (uintmax_t)limit) return 0;
    if (last >= (uintmax_t)limit - 1U)
        last = (uintmax_t)limit - 1U;
    if (last < first) return 0;

    *start = (size_t)first;
    *count = (size_t)(last - first + 1U);
    return 1;
}

static int snapshot_source_if_needed(
    InfiltratrSurface *destination, const InfiltratrSurface *source,
    const InfiltratrSurface **effective_source, InfiltratrSurface *snapshot) {
    if (!effective_source || !snapshot) return 0;
    *effective_source = source;
    if (destination != source) return 1;
    if (!infiltratr_surface_copy(snapshot, source)) return 0;
    *effective_source = snapshot;
    return 1;
}

uint32_t infiltratr_color_pack(InfiltratrColor color) {
    return ((uint32_t)color.a << 24U) | ((uint32_t)color.r << 16U) |
           ((uint32_t)color.g << 8U) | (uint32_t)color.b;
}

InfiltratrColor infiltratr_color_unpack(uint32_t pixel) {
    InfiltratrColor color;
    color.r = (uint8_t)((pixel >> 16U) & 0xffU);
    color.g = (uint8_t)((pixel >> 8U) & 0xffU);
    color.b = (uint8_t)(pixel & 0xffU);
    color.a = (uint8_t)((pixel >> 24U) & 0xffU);
    return color;
}

int infiltratr_surface_init(InfiltratrSurface *surface, size_t width, size_t height) {
    if (!surface) return 0;
    surface->width = 0;
    surface->height = 0;
    surface->pixel_count = 0;
    surface->pixels = NULL;
    return infiltratr_surface_resize(surface, width, height);
}

int infiltratr_surface_resize(InfiltratrSurface *surface, size_t width, size_t height) {
    size_t count = 0;
    uint32_t *pixels = NULL;
    if (!surface || !valid_dimensions(width, height, &count)) return 0;
    if (surface->width == width && surface->height == height && surface->pixels) return 1;
    pixels = (uint32_t *)realloc(surface->pixels, count * sizeof(uint32_t));
    if (!pixels) return 0;
    surface->pixels = pixels;
    surface->width = width;
    surface->height = height;
    surface->pixel_count = count;
    return 1;
}

void infiltratr_surface_release(InfiltratrSurface *surface) {
    if (!surface) return;
    free(surface->pixels);
    surface->pixels = NULL;
    surface->width = 0;
    surface->height = 0;
    surface->pixel_count = 0;
}

int infiltratr_surface_copy(InfiltratrSurface *destination, const InfiltratrSurface *source) {
    if (!destination || !source || !source->pixels || source->pixel_count == 0) return 0;
    if (destination == source) return 1;
    if (!infiltratr_surface_resize(destination, source->width, source->height)) return 0;
    memmove(destination->pixels, source->pixels,
            source->pixel_count * sizeof(uint32_t));
    return 1;
}

int infiltratr_surface_copy_region(InfiltratrSurface *destination,
                                   const InfiltratrSurface *source,
                                   int source_x, int source_y,
                                   int width, int height) {
    InfiltratrSurface snapshot = {0};
    const InfiltratrSurface *input = source;
    InfiltratrColor transparent = {0, 0, 0, 0};
    size_t local_x, local_y, source_start_x, source_start_y;
    size_t copy_width, copy_height;
    int result = 0;

    if (!destination || !source || !source->pixels || width <= 0 || height <= 0)
        return 0;
    if (!snapshot_source_if_needed(destination, source, &input, &snapshot))
        return 0;
    if (!infiltratr_surface_resize(destination, (size_t)width, (size_t)height))
        goto out;

    infiltratr_surface_clear(destination, transparent);
    if (clip_axis_span(source_x, (size_t)width, input->width,
                       &local_x, &source_start_x, &copy_width) &&
        clip_axis_span(source_y, (size_t)height, input->height,
                       &local_y, &source_start_y, &copy_height)) {
        for (size_t row = 0U; row < copy_height; ++row) {
            memmove(destination->pixels +
                        (local_y + row) * destination->width + local_x,
                    input->pixels +
                        (source_start_y + row) * input->width + source_start_x,
                    copy_width * sizeof(uint32_t));
        }
    }
    result = 1;

out:
    infiltratr_surface_release(&snapshot);
    return result;
}

int infiltratr_surface_copy_luma_tinted(InfiltratrSurface *destination,
                                        const InfiltratrSurface *source,
                                        InfiltratrColor tint) {
    size_t i;
    if (!destination || !source || !source->pixels || source->pixel_count == 0) return 0;
    if (!infiltratr_surface_resize(destination, source->width, source->height)) return 0;
    for (i = 0; i < source->pixel_count; ++i) {
        InfiltratrColor c = infiltratr_color_unpack(source->pixels[i]);
        const unsigned luma = ((unsigned)c.r * 54U + (unsigned)c.g * 183U + (unsigned)c.b * 19U + 128U) >> 8U;
        c.r = (uint8_t)((luma * (unsigned)tint.r + 127U) / 255U);
        c.g = (uint8_t)((luma * (unsigned)tint.g + 127U) / 255U);
        c.b = (uint8_t)((luma * (unsigned)tint.b + 127U) / 255U);
        c.a = (uint8_t)(((unsigned)c.a * (unsigned)tint.a + 127U) / 255U);
        destination->pixels[i] = infiltratr_color_pack(c);
    }
    return 1;
}

void infiltratr_surface_clear(InfiltratrSurface *surface, InfiltratrColor color) {
    size_t i;
    uint32_t pixel;
    if (!surface || !surface->pixels) return;
    pixel = infiltratr_color_pack(color);
    for (i = 0; i < surface->pixel_count; ++i) surface->pixels[i] = pixel;
}

void infiltratr_surface_set_pixel(InfiltratrSurface *surface, int x, int y, InfiltratrColor color) {
    if (!surface || !surface->pixels || x < 0 || y < 0 ||
        (size_t)x >= surface->width || (size_t)y >= surface->height) return;
    surface->pixels[(size_t)y * surface->width + (size_t)x] = infiltratr_color_pack(color);
}

InfiltratrColor infiltratr_surface_get_pixel(const InfiltratrSurface *surface, int x, int y) {
    InfiltratrColor transparent = {0, 0, 0, 0};
    if (!surface || !surface->pixels || x < 0 || y < 0 ||
        (size_t)x >= surface->width || (size_t)y >= surface->height) return transparent;
    return infiltratr_color_unpack(surface->pixels[(size_t)y * surface->width + (size_t)x]);
}

void infiltratr_surface_fill_rect(InfiltratrSurface *surface, int x, int y, int width, int height,
                                  InfiltratrColor color) {
    size_t local_x, local_y, start_x, start_y, fill_width, fill_height;
    uint32_t pixel;

    if (!surface || !surface->pixels || width <= 0 || height <= 0) return;
    if (!clip_axis_span(x, (size_t)width, surface->width,
                        &local_x, &start_x, &fill_width) ||
        !clip_axis_span(y, (size_t)height, surface->height,
                        &local_y, &start_y, &fill_height))
        return;

    (void)local_x;
    (void)local_y;
    pixel = infiltratr_color_pack(color);
    for (size_t yy = 0U; yy < fill_height; ++yy) {
        uint32_t *row = surface->pixels + (start_y + yy) * surface->width;
        for (size_t xx = 0U; xx < fill_width; ++xx)
            row[start_x + xx] = pixel;
    }
}

static InfiltratrColor composite_source_over(InfiltratrColor source,
                                              InfiltratrColor destination) {
    const unsigned source_alpha = source.a;
    const unsigned destination_alpha = destination.a;
    const unsigned inverse = 255U - source_alpha;
    const unsigned alpha_scaled =
        source_alpha * 255U + destination_alpha * inverse;
    InfiltratrColor output = {0, 0, 0, 0};

    if (alpha_scaled == 0U) return output;

    output.a = (uint8_t)((alpha_scaled + 127U) / 255U);
    output.r = (uint8_t)(
        ((unsigned)source.r * source_alpha * 255U +
         (unsigned)destination.r * destination_alpha * inverse +
         alpha_scaled / 2U) / alpha_scaled);
    output.g = (uint8_t)(
        ((unsigned)source.g * source_alpha * 255U +
         (unsigned)destination.g * destination_alpha * inverse +
         alpha_scaled / 2U) / alpha_scaled);
    output.b = (uint8_t)(
        ((unsigned)source.b * source_alpha * 255U +
         (unsigned)destination.b * destination_alpha * inverse +
         alpha_scaled / 2U) / alpha_scaled);
    return output;
}

void infiltratr_surface_blend_rect(InfiltratrSurface *surface, int x, int y, int width, int height,
                                   InfiltratrColor color) {
    size_t local_x, local_y, start_x, start_y, blend_width, blend_height;

    if (!surface || !surface->pixels || width <= 0 || height <= 0 || color.a == 0) return;
    if (color.a == 255) {
        infiltratr_surface_fill_rect(surface, x, y, width, height, color);
        return;
    }
    if (!clip_axis_span(x, (size_t)width, surface->width,
                        &local_x, &start_x, &blend_width) ||
        !clip_axis_span(y, (size_t)height, surface->height,
                        &local_y, &start_y, &blend_height))
        return;

    (void)local_x;
    (void)local_y;
    for (size_t yy = 0U; yy < blend_height; ++yy) {
        uint32_t *row = surface->pixels + (start_y + yy) * surface->width;
        for (size_t xx = 0U; xx < blend_width; ++xx) {
            const size_t column = start_x + xx;
            const InfiltratrColor destination =
                infiltratr_color_unpack(row[column]);
            row[column] =
                infiltratr_color_pack(composite_source_over(color, destination));
        }
    }
}

static void blend_pixel_at(InfiltratrSurface *destination, size_t x, size_t y,
                           InfiltratrColor source) {
    InfiltratrColor existing;

    if (source.a == 0 || !destination || !destination->pixels ||
        x >= destination->width || y >= destination->height)
        return;
    if (source.a == 255) {
        destination->pixels[y * destination->width + x] =
            infiltratr_color_pack(source);
        return;
    }

    existing = infiltratr_color_unpack(
        destination->pixels[y * destination->width + x]);
    destination->pixels[y * destination->width + x] =
        infiltratr_color_pack(composite_source_over(source, existing));
}

static void surface_blit_region_core(
    InfiltratrSurface *destination, const InfiltratrSurface *source,
    int source_x, int source_y, size_t source_width, size_t source_height,
    int destination_x, int destination_y) {
    InfiltratrSurface snapshot = {0};
    const InfiltratrSurface *input = source;
    size_t source_start_x, source_start_y;
    size_t destination_start_x, destination_start_y;
    size_t copy_width, copy_height;

    if (!destination || !destination->pixels || !source || !source->pixels ||
        source_width == 0U || source_height == 0U)
        return;
    if (!snapshot_source_if_needed(destination, source, &input, &snapshot))
        return;

    if (!clip_blit_axis(source_x, destination_x, source_width,
                        input->width, destination->width,
                        &source_start_x, &destination_start_x, &copy_width) ||
        !clip_blit_axis(source_y, destination_y, source_height,
                        input->height, destination->height,
                        &source_start_y, &destination_start_y, &copy_height))
        goto out;

    for (size_t y = 0U; y < copy_height; ++y) {
        for (size_t x = 0U; x < copy_width; ++x) {
            blend_pixel_at(
                destination, destination_start_x + x, destination_start_y + y,
                infiltratr_color_unpack(
                    input->pixels[(source_start_y + y) * input->width +
                                  source_start_x + x]));
        }
    }

out:
    infiltratr_surface_release(&snapshot);
}

void infiltratr_surface_blit(InfiltratrSurface *destination, const InfiltratrSurface *source,
                             int destination_x, int destination_y) {
    if (!source) return;
    surface_blit_region_core(destination, source, 0, 0,
                             source->width, source->height,
                             destination_x, destination_y);
}

void infiltratr_surface_blit_region(InfiltratrSurface *destination,
                                    const InfiltratrSurface *source,
                                    int source_x, int source_y,
                                    int source_width, int source_height,
                                    int destination_x, int destination_y) {
    if (source_width <= 0 || source_height <= 0) return;
    surface_blit_region_core(destination, source, source_x, source_y,
                             (size_t)source_width, (size_t)source_height,
                             destination_x, destination_y);
}

static void surface_blit_region_scaled_nearest_core(
    InfiltratrSurface *destination, const InfiltratrSurface *source,
    int source_x, int source_y, size_t source_width, size_t source_height,
    int destination_x, int destination_y,
    size_t destination_width, size_t destination_height) {
    InfiltratrSurface snapshot = {0};
    const InfiltratrSurface *input = source;
    size_t local_start_x, local_start_y;
    size_t destination_start_x, destination_start_y;
    size_t visible_width, visible_height;

    if (!destination || !destination->pixels || !source || !source->pixels ||
        source_width == 0U || source_height == 0U ||
        destination_width == 0U || destination_height == 0U)
        return;
    if (!snapshot_source_if_needed(destination, source, &input, &snapshot))
        return;

    if (!clip_axis_span(destination_x, destination_width, destination->width,
                        &local_start_x, &destination_start_x, &visible_width) ||
        !clip_axis_span(destination_y, destination_height, destination->height,
                        &local_start_y, &destination_start_y, &visible_height))
        goto out;

    for (size_t y = 0U; y < visible_height; ++y) {
        size_t source_offset_y;
        size_t source_position_y;
        const size_t local_y = local_start_y + y;

        if (!scaled_offset(local_y, source_height, destination_height,
                           &source_offset_y) ||
            !coordinate_from_local(source_y, source_offset_y, input->height,
                                   &source_position_y))
            continue;

        for (size_t x = 0U; x < visible_width; ++x) {
            size_t source_offset_x;
            size_t source_position_x;
            const size_t local_x = local_start_x + x;

            if (!scaled_offset(local_x, source_width, destination_width,
                               &source_offset_x) ||
                !coordinate_from_local(source_x, source_offset_x, input->width,
                                       &source_position_x))
                continue;

            blend_pixel_at(
                destination, destination_start_x + x, destination_start_y + y,
                infiltratr_color_unpack(
                    input->pixels[source_position_y * input->width +
                                  source_position_x]));
        }
    }

out:
    infiltratr_surface_release(&snapshot);
}

void infiltratr_surface_blit_scaled_nearest(InfiltratrSurface *destination,
                                            const InfiltratrSurface *source,
                                            int destination_x, int destination_y,
                                            int destination_width, int destination_height) {
    if (!source || destination_width <= 0 || destination_height <= 0) return;
    surface_blit_region_scaled_nearest_core(
        destination, source, 0, 0, source->width, source->height,
        destination_x, destination_y,
        (size_t)destination_width, (size_t)destination_height);
}

void infiltratr_surface_blit_region_scaled_nearest(InfiltratrSurface *destination,
                                                   const InfiltratrSurface *source,
                                                   int source_x, int source_y,
                                                   int source_width, int source_height,
                                                   int destination_x, int destination_y,
                                                   int destination_width, int destination_height) {
    if (source_width <= 0 || source_height <= 0 ||
        destination_width <= 0 || destination_height <= 0)
        return;
    surface_blit_region_scaled_nearest_core(
        destination, source, source_x, source_y,
        (size_t)source_width, (size_t)source_height,
        destination_x, destination_y,
        (size_t)destination_width, (size_t)destination_height);
}

static InfiltratrColor sample_surface_i64(const InfiltratrSurface *source,
                                          int64_t x, int64_t y) {
    InfiltratrColor transparent = {0, 0, 0, 0};
    if (!source || !source->pixels || x < 0 || y < 0) return transparent;
    if ((uint64_t)x >= (uint64_t)source->width ||
        (uint64_t)y >= (uint64_t)source->height) return transparent;
    return infiltratr_color_unpack(
        source->pixels[(size_t)y * source->width + (size_t)x]);
}

static uint8_t rounded_u8(double value) {
    if (!(value > 0.0)) return 0U;
    if (value >= 255.0) return 255U;
    return (uint8_t)floor(value + 0.5);
}

static InfiltratrColor bilinear_sample(const InfiltratrSurface *source,
                                       double source_x, double source_y) {
    const int64_t x0 = (int64_t)floor(source_x);
    const int64_t y0 = (int64_t)floor(source_y);
    const int64_t x1 = x0 + 1;
    const int64_t y1 = y0 + 1;
    const double fx = source_x - floor(source_x);
    const double fy = source_y - floor(source_y);
    const double weights[4] = {
        (1.0 - fx) * (1.0 - fy),
        fx * (1.0 - fy),
        (1.0 - fx) * fy,
        fx * fy
    };
    const InfiltratrColor samples[4] = {
        sample_surface_i64(source, x0, y0),
        sample_surface_i64(source, x1, y0),
        sample_surface_i64(source, x0, y1),
        sample_surface_i64(source, x1, y1)
    };
    double alpha = 0.0;
    double red_alpha = 0.0;
    double green_alpha = 0.0;
    double blue_alpha = 0.0;
    int i;
    InfiltratrColor result = {0, 0, 0, 0};

    for (i = 0; i < 4; ++i) {
        const double weighted_alpha = weights[i] * (double)samples[i].a;
        alpha += weighted_alpha;
        red_alpha += weighted_alpha * (double)samples[i].r;
        green_alpha += weighted_alpha * (double)samples[i].g;
        blue_alpha += weighted_alpha * (double)samples[i].b;
    }
    if (!(alpha > 0.0)) return result;

    result.a = rounded_u8(alpha);
    result.r = rounded_u8(red_alpha / alpha);
    result.g = rounded_u8(green_alpha / alpha);
    result.b = rounded_u8(blue_alpha / alpha);
    return result;
}

static void surface_blit_region_scaled_bilinear_core(
    InfiltratrSurface *destination, const InfiltratrSurface *source,
    int source_x, int source_y, size_t source_width, size_t source_height,
    int destination_x, int destination_y,
    size_t destination_width, size_t destination_height) {
    InfiltratrSurface snapshot = {0};
    const InfiltratrSurface *input = source;
    size_t local_start_x, local_start_y;
    size_t destination_start_x, destination_start_y;
    size_t visible_width, visible_height;

    if (!destination || !destination->pixels || !source || !source->pixels ||
        source_width == 0U || source_height == 0U ||
        destination_width == 0U || destination_height == 0U)
        return;
    if (!snapshot_source_if_needed(destination, source, &input, &snapshot))
        return;

    if (!clip_axis_span(destination_x, destination_width, destination->width,
                        &local_start_x, &destination_start_x, &visible_width) ||
        !clip_axis_span(destination_y, destination_height, destination->height,
                        &local_start_y, &destination_start_y, &visible_height))
        goto out;

    for (size_t y = 0U; y < visible_height; ++y) {
        const size_t local_y = local_start_y + y;
        const double fy = destination_height == 1U ? 0.0 :
            (double)local_y * (double)(source_height - 1U) /
            (double)(destination_height - 1U);

        for (size_t x = 0U; x < visible_width; ++x) {
            const size_t local_x = local_start_x + x;
            const double fx = destination_width == 1U ? 0.0 :
                (double)local_x * (double)(source_width - 1U) /
                (double)(destination_width - 1U);
            const InfiltratrColor sample =
                bilinear_sample(input, (double)source_x + fx,
                                (double)source_y + fy);
            blend_pixel_at(destination, destination_start_x + x,
                           destination_start_y + y, sample);
        }
    }

out:
    infiltratr_surface_release(&snapshot);
}

void infiltratr_surface_blit_scaled_bilinear(InfiltratrSurface *destination,
                                             const InfiltratrSurface *source,
                                             int destination_x, int destination_y,
                                             int destination_width, int destination_height) {
    if (!source || destination_width <= 0 || destination_height <= 0) return;
    surface_blit_region_scaled_bilinear_core(
        destination, source, 0, 0, source->width, source->height,
        destination_x, destination_y,
        (size_t)destination_width, (size_t)destination_height);
}

void infiltratr_surface_blit_region_scaled_bilinear(InfiltratrSurface *destination,
                                                    const InfiltratrSurface *source,
                                                    int source_x, int source_y,
                                                    int source_width, int source_height,
                                                    int destination_x, int destination_y,
                                                    int destination_width, int destination_height) {
    if (source_width <= 0 || source_height <= 0 ||
        destination_width <= 0 || destination_height <= 0)
        return;
    surface_blit_region_scaled_bilinear_core(
        destination, source, source_x, source_y,
        (size_t)source_width, (size_t)source_height,
        destination_x, destination_y,
        (size_t)destination_width, (size_t)destination_height);
}

void infiltratr_surface_blit_rotated(InfiltratrSurface *destination,
                                     const InfiltratrSurface *source,
                                     int center_x, int center_y,
                                     double angle_radians) {
    InfiltratrSurface snapshot = {0};
    const InfiltratrSurface *input = source;
    size_t start_x, start_y, count_x, count_y;
    size_t radius_x, radius_y;
    double cosine, sine, half_width, half_height;
    double extent_x, extent_y;

    if (!destination || !destination->pixels || !source || !source->pixels ||
        source->width == 0U || source->height == 0U ||
        !isfinite(angle_radians))
        return;
    if (!snapshot_source_if_needed(destination, source, &input, &snapshot))
        return;

    cosine = cos(angle_radians);
    sine = sin(angle_radians);
    half_width = (double)input->width * 0.5;
    half_height = (double)input->height * 0.5;
    extent_x = fabs(cosine) * half_width + fabs(sine) * half_height + 1.0;
    extent_y = fabs(sine) * half_width + fabs(cosine) * half_height + 1.0;
    radius_x = extent_x >= (double)SIZE_MAX ? SIZE_MAX : (size_t)ceil(extent_x);
    radius_y = extent_y >= (double)SIZE_MAX ? SIZE_MAX : (size_t)ceil(extent_y);

    if (!clip_centered_axis(center_x, radius_x, destination->width,
                            &start_x, &count_x) ||
        !clip_centered_axis(center_y, radius_y, destination->height,
                            &start_y, &count_y))
        goto out;

    const double source_cx = ((double)input->width - 1.0) * 0.5;
    const double source_cy = ((double)input->height - 1.0) * 0.5;

    for (size_t y = 0U; y < count_y; ++y) {
        const size_t destination_y = start_y + y;
        const double dy = (double)destination_y - (double)center_y;

        for (size_t x = 0U; x < count_x; ++x) {
            const size_t destination_x = start_x + x;
            const double dx = (double)destination_x - (double)center_x;
            const double sx = source_cx + cosine * dx + sine * dy;
            const double sy = source_cy - sine * dx + cosine * dy;

            /*
             * Nearest-neighbour sampling owns a half-pixel footprint around
             * each source pixel centre. This also prevents exact right-angle
             * rotations losing edge pixels to tiny sin/cos round-off.
             */
            if (sx < -0.5 || sy < -0.5 ||
                sx >= (double)input->width - 0.5 ||
                sy >= (double)input->height - 0.5)
                continue;

            const int64_t isx = (int64_t)floor(sx + 0.5);
            const int64_t isy = (int64_t)floor(sy + 0.5);
            if (isx < 0 || isy < 0 ||
                (uintmax_t)isx >= (uintmax_t)input->width ||
                (uintmax_t)isy >= (uintmax_t)input->height)
                continue;

            blend_pixel_at(
                destination, destination_x, destination_y,
                infiltratr_color_unpack(
                    input->pixels[(size_t)isy * input->width + (size_t)isx]));
        }
    }

out:
    infiltratr_surface_release(&snapshot);
}
