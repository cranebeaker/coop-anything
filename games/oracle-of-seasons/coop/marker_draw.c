#include "marker_draw.h"

#include <math.h>
#include <stdint.h>

enum {
    INK = 3 | (4 << 5) | (5 << 10),
    CREAM = 31 | (29 << 5) | (22 << 10),
    BLUE = 4 | (17 << 5) | (31 << 10),
    GREEN = 5 | (26 << 5) | (7 << 10),
    HAIR = 27 | (18 << 5) | (4 << 10),
    SKIN = 31 | (23 << 5) | (13 << 10)
};

typedef struct {
    CoopMarkerSurface *surface;
    int64_t left, top, right, bottom;
    bool wrote;
} Canvas;

static void pixel(Canvas *c, int64_t x, int64_t y, uint16_t color)
{
    if (x < c->left || x >= c->right || y < c->top || y >= c->bottom) return;
    c->surface->pixels[(size_t)y * c->surface->stride + (size_t)x] = color;
    c->wrote = true;
}

/* Every loop below has a fixed badge/font/caption bound, not a surface-size
   bound. A bad or very large viewport therefore cannot cause a broad scan. */
static void panel(Canvas *c, int64_t x, int64_t y, int width, int height)
{
    for (int row = 0; row < height; ++row) for (int col = 0; col < width; ++col) {
        bool edge = row == 0 || row == height - 1 || col == 0 || col == width - 1;
        bool corner = (row == 0 || row == height - 1) && (col == 0 || col == width - 1);
        pixel(c, x + col, y + row, edge && !corner ? CREAM : INK);
    }
}

#define G(a,b,c,d,e) ((a << 12) | (b << 9) | (c << 6) | (d << 3) | e)
static uint16_t glyph(unsigned char letter)
{
    /* Original 3x5 uppercase font, designed as row masks here. */
    static const uint16_t letters[] = {
        G(2,5,7,5,5), G(6,5,6,5,6), G(3,4,4,4,3), G(6,5,5,5,6),
        G(7,4,6,4,7), G(7,4,6,4,4), G(3,4,5,5,3), G(5,5,7,5,5),
        G(7,2,2,2,7), G(1,1,1,5,2), G(5,5,6,5,5), G(4,4,4,4,7),
        G(5,7,7,5,5), G(5,7,5,5,5), G(2,5,5,5,2), G(6,5,6,4,4),
        G(2,5,5,3,1), G(6,5,6,5,5), G(3,4,2,1,6), G(7,2,2,2,2),
        G(5,5,5,5,7), G(5,5,5,5,2), G(5,5,7,7,5), G(5,5,2,5,5),
        G(5,5,2,2,2), G(7,1,2,4,7)
    };
    static const uint16_t digits[] = {
        G(7,5,5,5,7), G(2,6,2,2,7), G(6,1,2,4,7), G(6,1,2,1,6),
        G(5,5,7,1,1), G(7,4,6,1,6), G(3,4,6,5,2), G(7,1,2,2,2),
        G(2,5,2,5,2), G(2,5,3,1,6)
    };
    if (letter >= 'a' && letter <= 'z') letter = (unsigned char)(letter - 'a' + 'A');
    if (letter >= 'A' && letter <= 'Z') return letters[letter - 'A'];
    if (letter >= '0' && letter <= '9') return digits[letter - '0'];
    if (letter == ' ') return 0;
    if (letter == '-') return G(0,0,7,0,0);
    if (letter == '.') return G(0,0,0,0,2);
    if (letter == '/') return G(1,1,2,4,4);
    return G(6,1,2,0,2); /* '?' also sanitizes every unsupported byte. */
}
#undef G

static void character(Canvas *c, int64_t x, int64_t y, unsigned char letter)
{
    uint16_t mask = glyph(letter);
    for (int row = 0; row < 5; ++row) for (int col = 0; col < 3; ++col)
        if (mask & (1u << (14 - row * 3 - col))) pixel(c, x + col, y + row, CREAM);
}

static void portrait(Canvas *c, int64_t x, int64_t y, bool player_one)
{
    /* Original cap, pointed ears, hair and face; no game sprite/font data. */
    static const char art[8][13] = {
        "....TTTT....", "..TTTTTTTT..", ".TTTTTTTTTH.", "TTTHSSSHHH..",
        "..HSSSSSSH..", ".SHSKSSKHS..", "..HSSSSSH...", "...HHSSHH..."
    };
    for (int row = 0; row < 8; ++row) for (int col = 0; col < 12; ++col) {
        char p = art[row][col];
        if (p == '.') continue;
        uint16_t color = p == 'T' ? (player_one ? GREEN : BLUE) :
                         p == 'H' ? HAIR : p == 'S' ? SKIN : INK;
        pixel(c, x + col, y + row, color);
    }
}

static void chevron(Canvas *c, int64_t x, int64_t y, double dx, double dy)
{
    double scale = fmax(fabs(dx), fabs(dy));
    dx /= scale; dy /= scale;
    int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
    if (fabs(dx) < fabs(dy) * 0.414213562373095) sx = 0;
    if (fabs(dy) < fabs(dx) * 0.414213562373095) sy = 0;
    if (sx && sy) {
        int tx = sx > 0 ? 17 : 0, ty = sy > 0 ? 19 : 0;
        for (int i = 0; i < 4; ++i) {
            pixel(c, x + tx - i * sx, y + ty, CREAM);
            pixel(c, x + tx, y + ty - i * sy, CREAM);
        }
    } else if (sx) {
        int tx = sx > 0 ? 17 : 0;
        for (int i = -2; i <= 2; ++i)
            pixel(c, x + tx - (i < 0 ? -i : i) * sx, y + 9 + i, CREAM);
    } else {
        int ty = sy > 0 ? 19 : 0;
        for (int i = -2; i <= 2; ++i)
            pixel(c, x + 8 + i, y + ty - (i < 0 ? -i : i) * sy, CREAM);
    }
}

bool coop_marker_draw(CoopMarkerSurface *surface, const CoopMarkerView *view,
                      const CoopMarker *marker)
{
    if (!surface || !view || !marker || !surface->pixels ||
        surface->width <= 0 || surface->height <= 0 ||
        surface->stride < (size_t)surface->width ||
        surface->stride > (size_t)PTRDIFF_MAX / sizeof(uint16_t) / (size_t)surface->height ||
        view->width <= 2 * COOP_MARKER_PADDING ||
        view->height <= COOP_MARKER_HUD_HEIGHT + 2 * COOP_MARKER_PADDING ||
        marker->width != 18 || marker->height != 20 ||
        marker->player_label[0] != 'P' ||
        (marker->player_label[1] != '1' && marker->player_label[1] != '2') ||
        marker->player_label[2] != '\0' ||
        (marker->mode != COOP_MARKER_EDGE && marker->mode != COOP_MARKER_AREA_LABEL))
        return false;
    if (marker->mode == COOP_MARKER_EDGE &&
        (!isfinite(marker->direction_x) || !isfinite(marker->direction_y) ||
         (marker->direction_x == 0 && marker->direction_y == 0))) return false;

    int64_t left = (int64_t)view->x + COOP_MARKER_PADDING;
    int64_t top = (int64_t)view->y + COOP_MARKER_HUD_HEIGHT + COOP_MARKER_PADDING;
    int64_t right = (int64_t)view->x + view->width - COOP_MARKER_PADDING;
    int64_t bottom = (int64_t)view->y + view->height - COOP_MARKER_PADDING;
    Canvas c = {surface, left > 0 ? left : 0, top > 0 ? top : 0,
                right < surface->width ? right : surface->width,
                bottom < surface->height ? bottom : surface->height, false};

    const char *caption = marker->area_name;
    int letters = 0, band_width = 0;
    int64_t band_x = 0, band_y = 0;
    if (marker->mode == COOP_MARKER_AREA_LABEL) {
        if (!caption || !caption[0]) caption = "UNKNOWN AREA";
        while (letters < COOP_MARKER_AREA_MAX_CHARS && caption[letters]) ++letters;
        int64_t fit = (right - left - 3) / 4;
        if (fit < 1) return false;
        if (letters > fit) letters = (int)fit;
        band_width = letters * 4 + 3;
        band_x = (int64_t)marker->x + 9 - band_width / 2;
        if (band_x < left) band_x = left;
        if (band_x + band_width > right) band_x = right - band_width;
        band_y = (int64_t)marker->y + 21;
        if (band_y < top || band_y + 9 > bottom) band_y = (int64_t)marker->y - 10;
        if (band_y < top || band_y + 9 > bottom) return false;
    }

    panel(&c, (int64_t)marker->x + 2, (int64_t)marker->y + 2, 14, 16);
    portrait(&c, (int64_t)marker->x + 3, (int64_t)marker->y + 3,
             marker->player_label[1] == '1');
    character(&c, (int64_t)marker->x + 5, (int64_t)marker->y + 12, 'P');
    character(&c, (int64_t)marker->x + 9, (int64_t)marker->y + 12,
              (unsigned char)marker->player_label[1]);
    if (marker->mode == COOP_MARKER_EDGE)
        chevron(&c, marker->x, marker->y, marker->direction_x, marker->direction_y);
    else {
        panel(&c, band_x, band_y, band_width, 9);
        for (int i = 0; i < letters; ++i)
            character(&c, band_x + 2 + 4 * i, band_y + 2, (unsigned char)caption[i]);
    }
    return c.wrote;
}
