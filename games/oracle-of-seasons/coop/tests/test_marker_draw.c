#include "marker_draw.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { W = 160, H = 144, STRIDE = 164, BG = 0x6b5a, CREAM = 23487 };
static uint16_t pixels[STRIDE * H + 8], other[STRIDE * H + 8];
static int checks, failures;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #c); } } while (0)

static void reset(void)
{
    for (size_t i = 0; i < sizeof pixels / sizeof pixels[0]; ++i) pixels[i] = BG;
}
static CoopMarkerView view(void)
{
    return (CoopMarkerView){0, 0, W, H, 80, 80, 0};
}
static CoopMarker marker(void)
{
    return (CoopMarker){COOP_MARKER_EDGE, 71, 70, 18, 20, "P2", 1, 0, "Horon Village"};
}
static CoopMarkerSurface surface(void)
{
    return (CoopMarkerSurface){pixels, W, H, STRIDE};
}

/* Catches draws for hidden data, invalid surfaces and invalid marker metadata. */
static void hidden_and_invalid_preserve_buffer(void)
{
    CoopMarkerView v = view(); CoopMarker m = marker(); CoopMarkerSurface s = surface();
    reset(); memcpy(other, pixels, sizeof pixels);
    m.mode = COOP_MARKER_HIDDEN;
    CHECK(!coop_marker_draw(&s, &v, &m));
    CHECK(memcmp(pixels, other, sizeof pixels) == 0);
    m = marker(); s.stride = 159;
    CHECK(!coop_marker_draw(&s, &v, &m));
    s = surface(); m.width = 19;
    CHECK(!coop_marker_draw(&s, &v, &m));
    m = marker(); m.player_label[1] = '3';
    CHECK(!coop_marker_draw(&s, &v, &m));
    m = marker(); m.direction_x = NAN;
    CHECK(!coop_marker_draw(&s, &v, &m));
    CHECK(!coop_marker_draw(NULL, &v, &m));
    CHECK(!coop_marker_draw(&s, NULL, &m));
    CHECK(!coop_marker_draw(&s, &v, NULL));
    CHECK(memcmp(pixels, other, sizeof pixels) == 0);
}

/* Catches stride mistakes, out-of-badge writes, HUD writes and wrong RGB555. */
static void badge_bounds_and_player_colors(void)
{
    CoopMarkerView v = view(); CoopMarker m = marker(); CoopMarkerSurface s = surface();
    m.x = 140;
    reset();
    CHECK(coop_marker_draw(&s, &v, &m));
    int changed = 0, outside = 0;
    for (int y = 0; y < H; ++y) for (int x = 0; x < STRIDE; ++x) {
        if (pixels[y * STRIDE + x] != BG) {
            ++changed;
            if (x < 140 || x >= 158 || y < 70 || y >= 90) ++outside;
        }
    }
    CHECK(changed > 100);
    CHECK(outside == 0);
    CHECK(pixels[73 * STRIDE + 147] == 32292); /* Blue cap: (4,17,31). */
    CHECK(pixels[82 * STRIDE + 149] == CREAM); /* P2's 2 top-left. */
    m.player_label[1] = '1'; reset();
    CHECK(coop_marker_draw(&s, &v, &m));
    CHECK(pixels[73 * STRIDE + 147] == 8005); /* Green cap: (5,26,7). */
    CHECK(pixels[82 * STRIDE + 149] != CREAM); /* P1's 1 is narrow. */
    for (int i = STRIDE * H; i < STRIDE * H + 8; ++i) CHECK(pixels[i] == BG);
}

/* Literal arrow tips catch reversed/missing/merged compass directions. */
static void eight_outward_chevrons(void)
{
    static const struct { double dx, dy; int tx, ty, opposite_x, opposite_y; } cases[] = {
        {0,-1,8,0,8,19}, {1,-1,17,0,0,19}, {1,0,17,9,0,9},
        {1,1,17,19,0,0}, {0,1,8,19,8,0}, {-1,1,0,19,17,0},
        {-1,0,0,9,17,9}, {-1,-1,0,0,17,19},
        {10,1,17,9,0,9}, {1,10,8,19,8,0},
        {1,0.4,17,9,0,9}, {1,0.42,17,19,0,0},
        {-10,1,0,9,17,9}, {-1,-0.4,0,9,17,9}, {-1,-0.42,0,0,17,19}
    };
    CoopMarkerView v = view(); CoopMarker m = marker(); CoopMarkerSurface s = surface();
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        reset(); m.direction_x = cases[i].dx; m.direction_y = cases[i].dy;
        CHECK(coop_marker_draw(&s, &v, &m));
        CHECK(pixels[(m.y + cases[i].ty) * STRIDE + m.x + cases[i].tx] == CREAM);
        CHECK(pixels[(m.y + cases[i].opposite_y) * STRIDE + m.x + cases[i].opposite_x] == BG);
    }
}

/* Catches clipping only against framebuffer instead of gameplay/HUD. */
static void surface_and_gameplay_clipping(void)
{
    CoopMarkerView v = view(); CoopMarker m = marker(); CoopMarkerSurface s = surface();
    m.x = -5; m.y = 10;
    reset(); CHECK(coop_marker_draw(&s, &v, &m));
    int changed = 0, outside = 0;
    for (int y = 0; y < H; ++y) for (int x = 0; x < STRIDE; ++x) {
        if (pixels[y * STRIDE + x] != BG) {
            ++changed;
            if (x < 2 || x >= 158 || y < 18 || y >= 142) ++outside;
        }
    }
    CHECK(changed > 0); CHECK(outside == 0);
    m.x = INT_MAX; m.y = INT_MAX;
    reset(); memcpy(other, pixels, sizeof pixels);
    CHECK(!coop_marker_draw(&s, &v, &m));
    CHECK(memcmp(pixels, other, sizeof pixels) == 0);
    m = marker(); s.width = 80; s.height = 80;
    reset(); CHECK(coop_marker_draw(&s, &v, &m));
    outside = 0;
    for (int y = 0; y < H; ++y) for (int x = 0; x < STRIDE; ++x)
        if (pixels[y * STRIDE + x] != BG && (x >= 80 || y >= 80)) ++outside;
    CHECK(outside == 0);
}

/* Catches fake AREA arrows, dropped captions and unsafe lower/unsupported text. */
static void area_caption_and_sanitizing(void)
{
    CoopMarkerView v = view(); CoopMarker m = marker(); CoopMarkerSurface s = surface();
    m.mode = COOP_MARKER_AREA_LABEL; m.x = 71; m.y = 18;
    m.direction_x = 200; m.direction_y = -100;
    reset(); CHECK(coop_marker_draw(&s, &v, &m));
    CHECK(pixels[18 * STRIDE + 79] == BG); /* No north tip. */
    CHECK(pixels[27 * STRIDE + 88] == BG); /* No east tip. */
    CHECK(pixels[18 * STRIDE + 88] == BG); /* No actual northeast tip for (200,-100). */
    CHECK(pixels[41 * STRIDE + 55] == CREAM); /* H in actual area caption. */
    memcpy(other, pixels, sizeof pixels);
    static const double ignored_directions[][2] = {
        {0,0}, {1,0}, {0,1}, {-1,-1}, {200,-100}
    };
    for (unsigned i = 0; i < sizeof ignored_directions / sizeof ignored_directions[0]; ++i) {
        m.direction_x = ignored_directions[i][0];
        m.direction_y = ignored_directions[i][1];
        reset(); CHECK(coop_marker_draw(&s, &v, &m));
        CHECK(memcmp(pixels, other, sizeof pixels) == 0);
    }
    m.area_name = "HORON VILLAGE";
    reset(); CHECK(coop_marker_draw(&s, &v, &m));
    CHECK(memcmp(pixels, other, sizeof pixels) == 0);
    m.area_name = "?"; reset(); CHECK(coop_marker_draw(&s, &v, &m));
    memcpy(other, pixels, sizeof pixels);
    m.area_name = "@"; reset(); CHECK(coop_marker_draw(&s, &v, &m));
    CHECK(memcmp(pixels, other, sizeof pixels) == 0);
    m.area_name = NULL; reset(); CHECK(coop_marker_draw(&s, &v, &m));
    memcpy(other, pixels, sizeof pixels);
    m.area_name = "UNKNOWN AREA"; reset(); CHECK(coop_marker_draw(&s, &v, &m));
    CHECK(memcmp(pixels, other, sizeof pixels) == 0);
    m.area_name = "A VERY LONG AREA NAME / BASEMENT FLOOR 123456789012345678901234567890";
    reset(); CHECK(coop_marker_draw(&s, &v, &m));
    int outside = 0;
    for (int y = 0; y < H; ++y) for (int x = 0; x < STRIDE; ++x)
        if (pixels[y * STRIDE + x] != BG && (x < 2 || x >= 158 || y < 18 || y >= 142)) ++outside;
    CHECK(outside == 0);
    v.width = 22; v.height = 40; m.x = 2; m.y = 18;
    reset(); CHECK(!coop_marker_draw(&s, &v, &m));
    CHECK(pixels[20 * STRIDE + 5] == BG);
}

int main(void)
{
    hidden_and_invalid_preserve_buffer();
    badge_bounds_and_player_colors();
    eight_outward_chevrons();
    surface_and_gameplay_clipping();
    area_caption_and_sanitizing();
    printf("marker draw: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
