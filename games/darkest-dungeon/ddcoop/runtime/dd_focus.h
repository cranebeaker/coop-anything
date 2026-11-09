/* A copy that loses focus stops its input logic and walking: a lost focus clears 118525d, which 76dff0 and others read (372067). In a shared session the other player goes on, so this
 * copy fell behind and took its Rules numbers in another order (owner's run of 6 Nov 18:09: the guest alt-tabbed, stayed two tiles behind, and got other loot from the same chest).
 * So in a session that one event does not reach the game. SDL itself still drops held keys and stops sending input to a window without focus. */
#ifndef DD_FOCUS_H
#define DD_FOCUS_H

#include <stdint.h>
#include <string.h>

#define DD_FOCUS_SDL_WINDOWEVENT 0x200u
#define DD_FOCUS_SDL_LOST 13u

static int dd_focus_lost(const void *event) {
    uint32_t type = 0;
    uint8_t what = 0;
    memcpy(&type, event, 4);
    memcpy(&what, (const uint8_t *)event + 12, 1); /* SDL_WindowEvent.event; the game reads the same byte (372067). */
    return type == DD_FOCUS_SDL_WINDOWEVENT && what == DD_FOCUS_SDL_LOST;
}
/* SDL input only: leave window close/quit and system events available after a
 * sync stop. The overlay gets first refusal, so F9 and its controls still work. */
static int dd_gameplay_input(const void *event) {
    uint32_t type = 0;
    if (!event) return 0;
    memcpy(&type, event, 4);
    return (type >= 0x300 && type <= 0x303) || (type >= 0x400 && type <= 0x403) ||
        (type >= 0x600 && type <= 0x606) || (type >= 0x650 && type <= 0x653) ||
        (type >= 0x700 && type <= 0x702);
}

#endif
