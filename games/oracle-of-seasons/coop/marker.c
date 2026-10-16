#include "marker.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>

static double clamp(double value, double low, double high)
{
    return fmax(low, fmin(high, value));
}

bool coop_marker_layout(const CoopMarkerView *view,
                        const CoopPartner *partner, CoopMarker *out)
{
    if (!out) return false;
    *out = (CoopMarker){0};
    if (!view || !partner || !partner->connected ||
        !partner->location_known || partner->visible ||
        view->player_index > 1 || partner->player_index > 1 ||
        view->player_index == partner->player_index)
        return false;

    /* Use wide intermediates so hostile or stale dimensions cannot overflow. */
    int64_t right = (int64_t)view->x + view->width;
    int64_t bottom = (int64_t)view->y + view->height;
    if (view->width < COOP_MARKER_WIDTH + 2 * COOP_MARKER_PADDING ||
        view->height < COOP_MARKER_HUD_HEIGHT + COOP_MARKER_HEIGHT +
                       2 * COOP_MARKER_PADDING ||
        right > INT_MAX || bottom > INT_MAX)
        return false;

    double left_x = (double)view->x + COOP_MARKER_PADDING;
    double right_x = (double)right - COOP_MARKER_PADDING - COOP_MARKER_WIDTH;
    double top_y = (double)view->y + COOP_MARKER_HUD_HEIGHT + COOP_MARKER_PADDING;
    double bottom_y = (double)bottom - COOP_MARKER_PADDING - COOP_MARKER_HEIGHT;
    double x = (left_x + right_x) / 2;
    double y = top_y;
    double dx = 0, dy = 0;

    if (partner->same_coordinate_space) {
        if (!isfinite(view->player_center_x) || !isfinite(view->player_center_y) ||
            view->player_center_x < view->x || view->player_center_x > right ||
            view->player_center_y < view->y || view->player_center_y > bottom ||
            !isfinite(partner->bearing_x) || !isfinite(partner->bearing_y))
            return false;
        double magnitude = fmax(fabs(partner->bearing_x), fabs(partner->bearing_y));
        if (magnitude == 0) return false;
        dx = partner->bearing_x / magnitude;
        dy = partner->bearing_y / magnitude;

        /* Shrink the legal rectangle by half the FULL marker footprint, then
           intersect the player's bearing ray with its far edge. */
        double min_x = left_x + COOP_MARKER_WIDTH / 2.0;
        double max_x = right_x + COOP_MARKER_WIDTH / 2.0;
        double min_y = top_y + COOP_MARKER_HEIGHT / 2.0;
        double max_y = bottom_y + COOP_MARKER_HEIGHT / 2.0;
        double distance = INFINITY;
        if (dx > 0) distance = (max_x - view->player_center_x) / dx;
        if (dx < 0) distance = (min_x - view->player_center_x) / dx;
        if (dy > 0) distance = fmin(distance, (max_y - view->player_center_y) / dy);
        if (dy < 0) distance = fmin(distance, (min_y - view->player_center_y) / dy);
        /* A player may stand closer to an edge/HUD than half a marker. If
           already beyond that edge and pointing out, clamp at the nearest
           legal rectangle point rather than running the ray backwards. */
        distance = fmax(0, distance);
        x = view->player_center_x + distance * dx - COOP_MARKER_WIDTH / 2.0;
        y = view->player_center_y + distance * dy - COOP_MARKER_HEIGHT / 2.0;
    }

    out->mode = partner->same_coordinate_space ? COOP_MARKER_EDGE : COOP_MARKER_AREA_LABEL;
    /* Round to native pixels; final clamp also guards half-pixel rounding. */
    out->x = (int)clamp(floor(x + 0.5), left_x, right_x);
    out->y = (int)clamp(floor(y + 0.5), top_y, bottom_y);
    out->width = COOP_MARKER_WIDTH;
    out->height = COOP_MARKER_HEIGHT;
    out->player_label[0] = 'P';
    out->player_label[1] = (char)('1' + partner->player_index);
    out->direction_x = dx;
    out->direction_y = dy;
    out->area_name = partner->area_name;
    return true;
}
