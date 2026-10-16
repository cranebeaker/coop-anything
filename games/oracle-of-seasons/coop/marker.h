#ifndef COOP_MARKER_H
#define COOP_MARKER_H

#include <stdbool.h>

enum {
    COOP_MARKER_WIDTH = 18,
    COOP_MARKER_HEIGHT = 20,
    COOP_MARKER_HUD_HEIGHT = 16,
    COOP_MARKER_PADDING = 2
};

typedef struct {
    int x, y, width, height;
    /* Local player's centre, already transformed to native viewport pixels,
       in the same absolute coordinate system as x/y; must lie in viewport. */
    double player_center_x, player_center_y;
    unsigned player_index; /* Stable zero-based session identity: 0=P1, 1=P2. */
} CoopMarkerView;

typedef struct {
    bool connected;
    bool location_known;
    bool visible; /* Actual partner is visible, not merely in the same room. */
    bool same_coordinate_space;
    /* Bearing from local player to partner, in camera-aligned coordinates.
       Ignored for different coordinate spaces. Positive y points down. */
    double bearing_x, bearing_y;
    unsigned player_index;
    const char *area_name; /* Borrowed; may be NULL. */
} CoopPartner;

typedef enum {
    COOP_MARKER_HIDDEN = 0,
    COOP_MARKER_EDGE,
    COOP_MARKER_AREA_LABEL
} CoopMarkerMode;

typedef struct {
    CoopMarkerMode mode;
    int x, y, width, height; /* Entire portrait/label/chevron footprint. */
    char player_label[3];
    /* Outward chevron vector scaled so its largest magnitude is 1.
       Both zero for AREA_LABEL: never infer a bearing in this mode. */
    double direction_x, direction_y;
    const char *area_name; /* Borrowed; renderer owns area text layout/clipping. */
} CoopMarker;

/* Pure layout, no allocation or rendering. False clears *out to HIDDEN.
   Invalid pointers/viewport/identity/bearing also return false. For a different
   coordinate space, returns AREA_LABEL at the padded top centre; when area_name
   is absent the renderer should show a neutral unknown-area label.
   Inputs and output must not overlap. x/y are native coordinates, not scaled
   window pixels. HUD height and padding are relative to the viewport origin.
   Centres too close to a viewport edge for a full marker are clamped to the
   nearest legal marker placement when their bearing points farther outward.
   Rounds half pixels toward positive infinity. The 18x20 footprint includes
   portrait, player label and chevron, not an optional longer area name. */
bool coop_marker_layout(const CoopMarkerView *view,
                        const CoopPartner *partner, CoopMarker *out);

#endif
