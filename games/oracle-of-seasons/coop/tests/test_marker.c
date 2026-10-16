#include "marker.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
static int checks;
#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static CoopMarkerView normal_view(void)
{
    return (CoopMarkerView){0, 0, 160, 144, 80, 80, 0};
}

static CoopPartner east_partner(void)
{
    return (CoopPartner){true, true, false, true, 200, 0, 1, "Horon Village"};
}

/* Catches omitted clamping, centre-only clamping, and swapped axes.
   Hand geometry: whole marker centre fits x=[11,149], y=[28,132].
   From (80,80), a 45-degree diagonal meets y=28/132 at x=28/132. */
static void cardinal_and_diagonal_positions(void)
{
    static const struct { double dx, dy; int x, y; } cases[] = {
        {1, 0, 140, 70}, {-1, 0, 2, 70},
        {0, -1, 71, 18}, {0, 1, 71, 122},
        {1, -1, 123, 18}, {-1, -1, 19, 18},
        {1, 1, 123, 122}, {-1, 1, 19, 122},
        {4, -1, 140, 53}, {4, 1, 140, 87}
    };
    CoopMarkerView view = normal_view();
    CoopPartner partner = east_partner();
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        CoopMarker marker;
        partner.bearing_x = cases[i].dx;
        partner.bearing_y = cases[i].dy;
        CHECK(coop_marker_layout(&view, &partner, &marker));
        CHECK(marker.mode == COOP_MARKER_EDGE);
        CHECK(marker.x == cases[i].x);
        CHECK(marker.y == cases[i].y);
        CHECK(marker.width == 18 && marker.height == 20);
        CHECK(marker.x >= 2 && marker.x + marker.width <= 158);
        CHECK(marker.y >= 18 && marker.y + marker.height <= 142);
        CHECK(marker.direction_x == cases[i].dx / fmax(fabs(cases[i].dx), fabs(cases[i].dy)));
        CHECK(marker.direction_y == cases[i].dy / fmax(fabs(cases[i].dx), fabs(cases[i].dy)));
    }
}

/* Catches viewport-origin assumptions and ignoring the local player centre. */
static void translated_view_and_offcentre_player(void)
{
    CoopMarkerView view = {10, 20, 160, 144, 42, 68, 0};
    CoopPartner partner = east_partner();
    CoopMarker marker;
    partner.bearing_x = 2;
    partner.bearing_y = -1;
    CHECK(coop_marker_layout(&view, &partner, &marker));
    CHECK(marker.x == 73 && marker.y == 38);
    view = normal_view();
    view.player_center_x = 5;
    partner.bearing_x = -1;
    partner.bearing_y = 0;
    CHECK(coop_marker_layout(&view, &partner, &marker));
    CHECK(marker.x == 2 && marker.y == 70);
}

/* Catches labels derived from viewpoint/order rather than stable identity. */
static void both_player_viewpoints(void)
{
    CoopMarkerView view = normal_view();
    CoopPartner partner = east_partner();
    CoopMarker marker;
    CHECK(coop_marker_layout(&view, &partner, &marker));
    CHECK(strcmp(marker.player_label, "P2") == 0);
    view.player_index = 1;
    partner.player_index = 0;
    CHECK(coop_marker_layout(&view, &partner, &marker));
    CHECK(strcmp(marker.player_label, "P1") == 0);
}

/* Catches stale markers after connection/visibility/location state changes. */
static void hide_states_clear_output(void)
{
    CoopMarkerView view = normal_view();
    for (int state = 0; state < 3; ++state) {
        CoopPartner partner = east_partner();
        CoopMarker marker;
        memset(&marker, 0x7f, sizeof marker);
        if (state == 0) partner.visible = true;
        if (state == 1) partner.connected = false;
        if (state == 2) partner.location_known = false;
        CHECK(!coop_marker_layout(&view, &partner, &marker));
        CHECK(marker.mode == COOP_MARKER_HIDDEN);
        CHECK(marker.player_label[0] == '\0');
        CHECK(marker.area_name == NULL);
        CHECK(marker.width == 0 && marker.height == 0);
    }
}

/* Catches a fabricated compass direction for unrelated rooms/floors.
   Also demonstrates that same_coordinate_space does not suppress a marker. */
static void unrelated_space_is_neutral(void)
{
    CoopMarkerView view = normal_view();
    CoopPartner partner = east_partner();
    CoopMarker marker;
    partner.same_coordinate_space = false;
    partner.bearing_x = 200;
    partner.bearing_y = -100;
    CHECK(coop_marker_layout(&view, &partner, &marker));
    CHECK(marker.mode == COOP_MARKER_AREA_LABEL);
    CHECK(marker.x == 71 && marker.y == 18);
    CHECK(marker.direction_x == 0 && marker.direction_y == 0);
    CHECK(marker.area_name && strcmp(marker.area_name, "Horon Village") == 0);
    CHECK(strcmp(marker.player_label, "P2") == 0);
    partner.bearing_x = NAN;
    partner.bearing_y = INFINITY;
    CHECK(coop_marker_layout(&view, &partner, &marker));
    CHECK(marker.mode == COOP_MARKER_AREA_LABEL);
    CHECK(marker.x == 71 && marker.y == 18);
    CHECK(marker.direction_x == 0 && marker.direction_y == 0);
    CHECK(marker.area_name && strcmp(marker.area_name, "Horon Village") == 0);
    CHECK(strcmp(marker.player_label, "P2") == 0);
    partner.area_name = NULL;
    CHECK(coop_marker_layout(&view, &partner, &marker));
    CHECK(marker.area_name == NULL);
    partner = east_partner();
    CHECK(coop_marker_layout(&view, &partner, &marker));
    CHECK(marker.mode == COOP_MARKER_EDGE);
}

/* Catches partial marker overflow when there is no room for its footprint. */
static void tiny_and_invalid_viewports(void)
{
    CoopMarkerView view = {0, 0, 22, 40, 11, 28, 0};
    CoopPartner partner = east_partner();
    CoopMarker marker;
    CHECK(coop_marker_layout(&view, &partner, &marker));
    CHECK(marker.x == 2 && marker.y == 18);
    --view.width;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    view.width = 22;
    --view.height;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    view = normal_view();
    view.width = 0;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    view = normal_view();
    view.height = -1;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    view = normal_view();
    view.x = INT_MAX;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    CHECK(!coop_marker_layout(NULL, &partner, &marker));
    CHECK(marker.mode == COOP_MARKER_HIDDEN);
    CHECK(!coop_marker_layout(&view, NULL, &marker));
    CHECK(!coop_marker_layout(&view, &partner, NULL));
}

/* Catches undefined angle and nonfinite inputs reaching integer conversion. */
static void invalid_bearings_and_identities(void)
{
    CoopMarkerView view = normal_view();
    CoopPartner partner = east_partner();
    CoopMarker marker;
    partner.bearing_x = 0;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    partner.bearing_x = NAN;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    partner.bearing_x = INFINITY;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    partner = east_partner();
    view.player_center_y = NAN;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    view = normal_view();
    partner.player_index = 0;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    partner.player_index = 2;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    partner.player_index = 1;
    view.player_index = 2;
    CHECK(!coop_marker_layout(&view, &partner, &marker));
    view = normal_view();
    partner.bearing_x = 1e308;
    partner.bearing_y = 1e308;
    CHECK(coop_marker_layout(&view, &partner, &marker));
    CHECK(marker.x == 123 && marker.y == 122);
}

int main(void)
{
    cardinal_and_diagonal_positions();
    translated_view_and_offcentre_player();
    both_player_viewpoints();
    hide_states_clear_output();
    unrelated_space_is_neutral();
    tiny_and_invalid_viewports();
    invalid_bearings_and_identities();
    printf("marker layout: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
