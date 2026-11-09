#ifndef DD_OVERLAY_H
#define DD_OVERLAY_H
/* Called only on the SDL game thread. Returns true for input owned by the panel. */
int dd_overlay_event(const void *event);
void dd_overlay_draw(void *sdl_window);
/* Shows a message in the F9 panel and opens it. 0 = the panel cannot draw here; the caller needs another way. */
int dd_overlay_notice(const char *utf8);
/* What the runtime knows about the game-to-game connection, shown at the top of the panel. */
enum { DD_OVERLAY_UNKNOWN = 0, DD_OVERLAY_WAITING, DD_OVERLAY_CONNECTED, DD_OVERLAY_LOST, DD_OVERLAY_REFUSED };
void dd_overlay_session(int state);
/* A strip at the top of the window in a fight: whose hero chooses now. */
enum { DD_OVERLAY_TURN_NONE = 0, DD_OVERLAY_TURN_MINE, DD_OVERLAY_TURN_OTHER };
void dd_overlay_turn(int turn);
/* In a raid: one block per hero at the top left, in the order they stand on screen (the front rank on the right), in
 * the colour of whose hero it is (owner: DD_OVERLAY_TURN_MINE, _OTHER, or 0 unknown), with its stress (0..200) along
 * the bottom. count 0 hides it. */
void dd_overlay_party(int count, const int owner[4], const float stress[4]);
/* A waiting message that no longer applies. */
void dd_overlay_notice_clear(const char *utf8);
#endif
