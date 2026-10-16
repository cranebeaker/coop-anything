#ifndef COOP_MARKER_DRAW_H
#define COOP_MARKER_DRAW_H

#include "marker.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t *pixels;
    int width, height;
    size_t stride; /* uint16_t pixels, not bytes; must be >= width. */
} CoopMarkerSurface;

enum { COOP_MARKER_AREA_MAX_CHARS = 38 };

/* Draw original pixel art in CGB RGB555: R bits 0..4, G 5..9, B 10..14.
   Caller owns at least height*stride uint16_t pixels, with no overlap with
   marker/view/text inputs. No allocation, SDL, engine state, or external assets.
   Returns true if at least one pixel was written. Hidden/invalid returns false
   and changes nothing. Only 18x20 markers labelled P1/P2 are accepted.

   Every write is clipped to the surface AND the view's padded gameplay region:
   [x+2,x+width-2), [y+18,y+height-2). EDGE writes only inside its 18x20 footprint.
   AREA_LABEL draws no chevron, and adds an opaque 9px-high caption band, below
   the badge when possible or above otherwise, wholly in the gameplay region.
   If that region cannot fit both badge and caption, AREA_LABEL changes nothing.
   Caption is centred near the badge and clamped horizontally; uppercase ASCII
   A-Z/0-9/space/-/./? and / use an original 3x5 font. Lowercase is uppercased;
   other bytes become '?'. At most 38 bytes are read, then truncated to the band
   width (no scrolling). NULL/empty text displays UNKNOWN AREA. Surface clipping
   may crop the caption if the provided surface covers only part of the view.
   Pixel-centre directions are quantized to nearest of eight compass bearings. */
bool coop_marker_draw(CoopMarkerSurface *surface, const CoopMarkerView *view,
                      const CoopMarker *marker);

#endif
