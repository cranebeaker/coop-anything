#include "marker_draw.h"
#include <stdio.h>

enum { W = 160, H = 144, COLS = 4, ROWS = 5 };
static uint16_t sheet[W * COLS * H * ROWS];

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: preview_marker OUTPUT.ppm\n"); return 2; }
    static const double directions[8][2] = {
        {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}
    };
    for (int panel = 0; panel < 20; ++panel) {
        int ox = panel % COLS * W, oy = panel / COLS * H;
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
            /* Original abstract backdrop: never a game screenshot. */
            uint16_t color = y < 16 ? 3 | (5 << 5) | (8 << 10) :
                ((x / 8 + y / 8) % 2 ? 14 | (21 << 5) | (11 << 10) : 16 | (23 << 5) | (13 << 10));
            if (x == 0 || x == W - 1 || y == H - 1) color = 0;
            sheet[(oy + y) * W * COLS + ox + x] = color;
        }
        CoopMarkerView view = {ox, oy, W, H, ox + 80, oy + 80, panel >= 8 && panel < 16 ? 1u : 0u};
        CoopPartner partner = {true, true, false, panel < 16,
            directions[panel % 8][0], directions[panel % 8][1], view.player_index == 0 ? 1u : 0u,
            panel == 16 ? "Horon Village" : panel == 17 ? "Temple / B1" :
            panel == 18 ? "A very long area name / basement floor 12345" : "unknown @ area"};
        if (panel == 17 || panel == 19) { view.player_index = 1; partner.player_index = 0; }
        CoopMarker marker;
        CoopMarkerSurface surface = {sheet, W * COLS, H * ROWS, W * COLS};
        if (!coop_marker_layout(&view, &partner, &marker) ||
            !coop_marker_draw(&surface, &view, &marker)) return 3;
    }
    FILE *out = fopen(argv[1], "wb");
    if (!out) return 4;
    if (fprintf(out, "P6\n%d %d\n255\n", W * COLS, H * ROWS) < 0) return 5;
    for (int i = 0; i < W * COLS * H * ROWS; ++i) {
        unsigned char rgb[3] = {
            (unsigned char)((sheet[i] & 31) * 255 / 31),
            (unsigned char)(((sheet[i] >> 5) & 31) * 255 / 31),
            (unsigned char)(((sheet[i] >> 10) & 31) * 255 / 31)
        };
        if (fwrite(rgb, 1, 3, out) != 3) return 6;
    }
    return fclose(out) != 0;
}
