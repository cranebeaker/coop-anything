#include "dd_saves.h"

#include <string.h>

uint32_t dd_coop_slots_parse(const char *text, int *bad) {
    uint32_t mask = 0;
    *bad = 0;
    if (!text) return 0;
    const char *p = text;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        if (*p < '0' || *p > '9') { *bad = 1; break; }
        int v = 0;
        while (*p >= '0' && *p <= '9') {
            v = v * 10 + (*p - '0');
            if (v > 99) { *bad = 1; return mask; }
            p++;
        }
        while (*p == ' ' || *p == '\t') p++;
        if (*p && *p != ',') { *bad = 1; break; }
        if (v >= 1 && v <= DD_MENU_SLOTS) mask |= 1u << v;
        else *bad = 1;
    }
    return mask;
}

int dd_profile_from_device_name(const char *name) {
    if (!name) return -1;
    if (strncmp(name, "steam_", 6) == 0) name += 6;
    if (strncmp(name, "profile_", 8) != 0) return -1;
    name += 8;
    if (name[0] < '0' || name[0] > '9' || name[1] != 0) return -1; /* exactly one digit: profile_0..profile_9 */
    return name[0] - '0';
}

dd_slot_decision dd_slot_decide(const char *device_name, uint32_t coop_mask, int *menu_position) {
    *menu_position = 0;
    int profile = dd_profile_from_device_name(device_name);
    if (profile < 0) return DD_SLOT_NO_CAMPAIGN;
    if (profile == DD_RESTORE_PROFILE || profile >= DD_MENU_SLOTS) return DD_SLOT_REFUSED;
    *menu_position = profile + 1;
    return (coop_mask >> *menu_position) & 1 ? DD_SLOT_COOP : DD_SLOT_SINGLE_PLAYER;
}

const char *dd_slot_decision_name(dd_slot_decision d) {
    switch (d) {
    case DD_SLOT_SINGLE_PLAYER: return "single player";
    case DD_SLOT_COOP: return "co-op allowed";
    case DD_SLOT_REFUSED: return "refused";
    default: return "no campaign";
    }
}
