/* Which campaigns may run in co-op (platform independent; tests/test_saves.c).
 *
 * The game mounts the loaded campaign as a storage device in device slot 5; its name is
 * "steam_profile_N" (with Steam) or "profile_N" (without), and campaign-menu position k = profile k-1
 * (runtime test 8). Profile 9 is the game's own save-restore area and is never a campaign.
 * The player lists the co-op campaigns as menu positions in ddcoop.ini: coop_slots = 3   or   3,7 */
#ifndef DD_SAVES_H
#define DD_SAVES_H

#include <stdint.h>

#define DD_MENU_SLOTS 9          /* campaign menu positions 1..9 = profiles 0..8 */
#define DD_RESTORE_PROFILE 9     /* the game's restore area: never co-op */

/* Parses "3", "3,7", " 3 , 7 " into a bit mask of menu positions (bit k = position k).
 * Returns 0 for an empty list. *bad is set if anything in it was not a position 1..9; the caller must
 * then ignore the whole setting (co-op off everywhere) rather than use a partial result. */
uint32_t dd_coop_slots_parse(const char *text, int *bad);

/* "steam_profile_2" or "profile_2" -> 2; anything else -> -1. */
int dd_profile_from_device_name(const char *name);

typedef enum {
    DD_SLOT_NO_CAMPAIGN = 0, /* nothing (recognisable) is loaded */
    DD_SLOT_SINGLE_PLAYER,   /* a campaign that is not in coop_slots */
    DD_SLOT_COOP,            /* a campaign listed in coop_slots */
    DD_SLOT_REFUSED,         /* the restore area or an out-of-range profile */
} dd_slot_decision;

/* Decides from the device name mounted in slot 5. *menu_position gets 1..9 when it is a campaign. */
dd_slot_decision dd_slot_decide(const char *device_name, uint32_t coop_mask, int *menu_position);

const char *dd_slot_decision_name(dd_slot_decision d);

#endif
