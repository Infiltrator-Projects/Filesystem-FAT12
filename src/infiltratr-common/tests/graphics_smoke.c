// SPDX-License-Identifier: GPL-3.0-or-later
#include <infiltratr/graphics.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <math.h>

int main(void) {
    InfiltratrSurface a = {0};
    InfiltratrSurface b = {0};
    InfiltratrSurface region = {0};
    InfiltratrSurface tinted = {0};
    InfiltratrSurface smooth_source = {0};
    InfiltratrSurface smooth_target = {0};
    InfiltratrSurface edge = {0};
    InfiltratrColor red = {255, 0, 0, 255};
    InfiltratrColor blue_half = {0, 0, 255, 128};
    InfiltratrColor c;

    assert(infiltratr_surface_init(&a, 4, 4));
    assert(infiltratr_surface_init(&b, 8, 8));
    infiltratr_surface_clear(&a, (InfiltratrColor){0, 0, 0, 0});
    infiltratr_surface_clear(&b, (InfiltratrColor){10, 20, 30, 255});
    infiltratr_surface_fill_rect(&a, 1, 1, 2, 2, red);
    infiltratr_surface_blit_scaled_nearest(&b, &a, 0, 0, 8, 8);

    c = infiltratr_surface_get_pixel(&b, 3, 3);
    assert(c.r == 255 && c.g == 0 && c.b == 0 && c.a == 255);

    infiltratr_surface_blend_rect(&b, 3, 3, 1, 1, blue_half);
    c = infiltratr_surface_get_pixel(&b, 3, 3);
    assert(c.r >= 126 && c.r <= 128);
    assert(c.b >= 127 && c.b <= 129);

    assert(infiltratr_surface_copy_region(&region, &b, 2, 2, 3, 3));
    assert(region.width == 3 && region.height == 3);
    c = infiltratr_surface_get_pixel(&region, 1, 1);
    assert(c.r >= 126 && c.r <= 128);
    assert(c.b >= 127 && c.b <= 129);

    infiltratr_surface_clear(&a, (InfiltratrColor){0, 0, 0, 0});
    infiltratr_surface_fill_rect(&a, 1, 1, 2, 2, (InfiltratrColor){180, 180, 180, 255});
    assert(infiltratr_surface_copy_luma_tinted(&tinted, &a, (InfiltratrColor){255, 64, 32, 255}));
    c = infiltratr_surface_get_pixel(&tinted, 1, 1);
    assert(c.r >= 179 && c.r <= 181);
    assert(c.g >= 44 && c.g <= 46);
    assert(c.b >= 22 && c.b <= 23);

    infiltratr_surface_clear(&b, (InfiltratrColor){0, 0, 0, 255});
    infiltratr_surface_blit_region_scaled_nearest(&b, &tinted, 1, 1, 2, 2, 0, 0, 4, 4);
    c = infiltratr_surface_get_pixel(&b, 3, 3);
    assert(c.r >= 179 && c.r <= 181);

    assert(infiltratr_surface_copy(&a, &b));
    assert(a.width == 8 && a.height == 8 && a.pixel_count == 64);

    /*
     * Bilinear filtering is evaluated in premultiplied-alpha space. The
     * transparent blue neighbours must not introduce a blue fringe around the
     * opaque red corner, and blitting onto transparent storage must preserve
     * the interpolated alpha rather than forcing the pixel opaque.
     */
    assert(infiltratr_surface_init(&smooth_source, 2, 2));
    assert(infiltratr_surface_init(&smooth_target, 3, 3));
    infiltratr_surface_clear(&smooth_source, (InfiltratrColor){0, 0, 255, 0});
    infiltratr_surface_clear(&smooth_target, (InfiltratrColor){0, 0, 0, 0});
    infiltratr_surface_set_pixel(&smooth_source, 0, 0, red);
    infiltratr_surface_blit_scaled_bilinear(&smooth_target, &smooth_source,
                                            0, 0, 3, 3);
    c = infiltratr_surface_get_pixel(&smooth_target, 1, 1);
    assert(c.r == 255 && c.g == 0 && c.b == 0);
    assert(c.a >= 63 && c.a <= 64);

    infiltratr_surface_clear(&smooth_target, (InfiltratrColor){0, 0, 0, 0});
    infiltratr_surface_blit_region_scaled_bilinear(
        &smooth_target, &smooth_source, 0, 0, 2, 2, 0, 0, 3, 3);
    c = infiltratr_surface_get_pixel(&smooth_target, 1, 1);
    assert(c.r == 255 && c.b == 0 && c.a >= 63 && c.a <= 64);


    /*
     * Source/destination aliasing must behave as a snapshot, not as an
     * overlapping forward copy. This exercises the exact in-place cases that
     * previously allowed source pixels to be destroyed before they were read.
     */
    assert(infiltratr_surface_init(&edge, 4, 1));
    infiltratr_surface_set_pixel(&edge, 0, 0, (InfiltratrColor){10, 0, 0, 255});
    infiltratr_surface_set_pixel(&edge, 1, 0, (InfiltratrColor){20, 0, 0, 255});
    infiltratr_surface_set_pixel(&edge, 2, 0, (InfiltratrColor){30, 0, 0, 255});
    infiltratr_surface_set_pixel(&edge, 3, 0, (InfiltratrColor){40, 0, 0, 255});
    assert(infiltratr_surface_copy(&edge, &edge));
    assert(infiltratr_surface_get_pixel(&edge, 3, 0).r == 40);

    infiltratr_surface_blit_region(&edge, &edge, 0, 0, 3, 1, 1, 0);
    assert(infiltratr_surface_get_pixel(&edge, 0, 0).r == 10);
    assert(infiltratr_surface_get_pixel(&edge, 1, 0).r == 10);
    assert(infiltratr_surface_get_pixel(&edge, 2, 0).r == 20);
    assert(infiltratr_surface_get_pixel(&edge, 3, 0).r == 30);

    infiltratr_surface_set_pixel(&edge, 0, 0, (InfiltratrColor){10, 0, 0, 255});
    infiltratr_surface_set_pixel(&edge, 1, 0, (InfiltratrColor){20, 0, 0, 255});
    infiltratr_surface_set_pixel(&edge, 2, 0, (InfiltratrColor){30, 0, 0, 255});
    infiltratr_surface_set_pixel(&edge, 3, 0, (InfiltratrColor){40, 0, 0, 255});
    assert(infiltratr_surface_copy_region(&edge, &edge, 1, 0, 2, 1));
    assert(edge.width == 2U && edge.height == 1U);
    assert(infiltratr_surface_get_pixel(&edge, 0, 0).r == 20);
    assert(infiltratr_surface_get_pixel(&edge, 1, 0).r == 30);

    /*
     * EXT-style range validation clips before constructing signed endpoints.
     * Extreme coordinates therefore remain defined and either intersect the
     * surface correctly or become clean no-ops.
     */
    assert(infiltratr_surface_resize(&edge, 2, 2));
    infiltratr_surface_clear(&edge, (InfiltratrColor){1, 2, 3, 255});
    infiltratr_surface_fill_rect(&edge, INT_MAX, 0, INT_MAX, 1, red);
    c = infiltratr_surface_get_pixel(&edge, 0, 0);
    assert(c.r == 1 && c.g == 2 && c.b == 3);
    infiltratr_surface_fill_rect(&edge, INT_MIN, 0, INT_MAX, 1, red);
    c = infiltratr_surface_get_pixel(&edge, 1, 0);
    assert(c.r == 1 && c.g == 2 && c.b == 3);
    infiltratr_surface_fill_rect(&edge, -1, 0, INT_MAX, 1, red);
    assert(infiltratr_surface_get_pixel(&edge, 0, 0).r == 255);
    assert(infiltratr_surface_get_pixel(&edge, 1, 0).r == 255);

    infiltratr_surface_clear(&edge, (InfiltratrColor){1, 2, 3, 255});
    infiltratr_surface_blend_rect(&edge, INT_MAX, INT_MAX, INT_MAX, INT_MAX,
                                  blue_half);
    c = infiltratr_surface_get_pixel(&edge, 0, 0);
    assert(c.r == 1 && c.g == 2 && c.b == 3);

    infiltratr_surface_blit_region(&edge, &smooth_source, 0, 0, 1, 1,
                                   INT_MAX, 0);
    c = infiltratr_surface_get_pixel(&edge, 0, 0);
    assert(c.r == 1 && c.g == 2 && c.b == 3);
    infiltratr_surface_blit_region(&edge, &smooth_source, INT_MAX, 0,
                                   INT_MAX, 1, 0, 0);
    c = infiltratr_surface_get_pixel(&edge, 0, 0);
    assert(c.r == 1 && c.g == 2 && c.b == 3);

    infiltratr_surface_blit_region_scaled_nearest(
        &edge, &smooth_source, 0, 0, 2, 2, INT_MAX, 0, INT_MAX, 1);
    c = infiltratr_surface_get_pixel(&edge, 0, 0);
    assert(c.r == 1 && c.g == 2 && c.b == 3);
    infiltratr_surface_blit_region_scaled_bilinear(
        &edge, &smooth_source, 0, 0, 2, 2, INT_MAX, 0, INT_MAX, 1);
    c = infiltratr_surface_get_pixel(&edge, 0, 0);
    assert(c.r == 1 && c.g == 2 && c.b == 3);

    infiltratr_surface_blit_rotated(&edge, &smooth_source,
                                    INT_MAX, INT_MAX, 0.0);
    c = infiltratr_surface_get_pixel(&edge, 0, 0);
    assert(c.r == 1 && c.g == 2 && c.b == 3);

    {
        InfiltratrSurface rotation_source = {0};
        InfiltratrSurface rotation_target = {0};
        const double right_angle = acos(-1.0) * 0.5;

        assert(infiltratr_surface_init(&rotation_source, 5, 1));
        assert(infiltratr_surface_init(&rotation_target, 9, 9));
        infiltratr_surface_clear(&rotation_source,
                                 (InfiltratrColor){255, 0, 0, 255});
        infiltratr_surface_clear(&rotation_target,
                                 (InfiltratrColor){0, 0, 0, 255});

        infiltratr_surface_blit_rotated(&rotation_target, &rotation_source,
                                        4, 4, right_angle);
        for (int y = 2; y <= 6; ++y) {
            c = infiltratr_surface_get_pixel(&rotation_target, 4, y);
            assert(c.r == 255 && c.g == 0 && c.b == 0);
        }

        infiltratr_surface_clear(&rotation_target,
                                 (InfiltratrColor){1, 2, 3, 255});
        infiltratr_surface_blit_rotated(&rotation_target, &rotation_source,
                                        4, 4, NAN);
        c = infiltratr_surface_get_pixel(&rotation_target, 4, 4);
        assert(c.r == 1 && c.g == 2 && c.b == 3);
        infiltratr_surface_blit_rotated(&rotation_target, &rotation_source,
                                        4, 4, INFINITY);
        c = infiltratr_surface_get_pixel(&rotation_target, 4, 4);
        assert(c.r == 1 && c.g == 2 && c.b == 3);

        infiltratr_surface_release(&rotation_source);
        infiltratr_surface_release(&rotation_target);
    }

    infiltratr_surface_release(&a);
    infiltratr_surface_release(&b);
    infiltratr_surface_release(&region);
    infiltratr_surface_release(&tinted);
    infiltratr_surface_release(&smooth_source);
    infiltratr_surface_release(&smooth_target);
    infiltratr_surface_release(&edge);
    return 0;
}
