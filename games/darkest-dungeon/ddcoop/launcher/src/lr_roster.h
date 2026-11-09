/* Reads the hero list from the game's own roster save (persist.roster.json), and the tutorial state and DLC
 * from persist.game.json; platform independent; launcher/tests/test_lr_roster.c.
 *
 * Darkest Dungeon saves are binary ("DSON", magic 01 B1 00 00): a header, an object table, a field table
 * and a data block with field names and untyped values. The roster keeps every hero under
 * base_root.heroes.<roster id>.hero_file_data.raw_data, which is itself an embedded DSON file. The roster
 * id is the id the game (and so the runtime) uses for that hero. Every offset is bounds checked: a damaged
 * or hostile file gives an error, never a read outside the buffer. tools/dson_dump.py dumps a whole file. */
#ifndef LR_ROSTER_H
#define LR_ROSTER_H

#include <stddef.h>
#include <stdint.h>

#define LR_MAX_HEROES 64        /* the game's roster holds at most 28 (fully upgraded stagecoach) */
#define LR_HERO_NAME_MAX 47
#define LR_HERO_CLASS_MAX 31

typedef struct {
    uint32_t id;                         /* roster id */
    char name[LR_HERO_NAME_MAX + 1];     /* UTF-8, control characters replaced */
    char cls[LR_HERO_CLASS_MAX + 1];     /* "crusader", "plague_doctor", ... */
    int32_t xp;                          /* resolveXp */
    int32_t status;                      /* roster.status: 0 = in the party, 1 = in town, 2 = in a building */
} lr_hero;

/* Parses a roster file. Returns the number of heroes stored in out (sorted by id), -1 if the file is
 * not a readable roster. Heroes beyond cap are dropped. */
int lr_roster_parse(const uint8_t *buf, size_t len, lr_hero *out, int cap);

/* Level shown in the game (0-6) for a resolveXp value. */
int lr_hero_level(int32_t xp);

/* ---- persist.game.json: checks before a session ---- */

#define LR_MAX_DLC 16
#define LR_DLC_NAME_MAX 31

typedef struct {
    int text;       /* 1 = still the game's text template (scripts\starting_save): the game never saved this campaign */
    int tutorial;   /* 1 = in the tutorial raid (inraid, raiddungeon "tutorial"): the tutorial was never finished */
    int dlc_count;  /* DLC this campaign uses (base_root.dlc), e.g. "musketeer"; [a-z0-9_] only */
    char dlc[LR_MAX_DLC][LR_DLC_NAME_MAX + 1];
    char estate[64]; /* The name the player gave the estate (base_root.estatename), UTF-8; empty in the text template. */
} lr_game_info;

/* Parses persist.game.json. Returns 1 and fills out, or 0 if the file is neither the binary save nor the
 * text template. DLC beyond LR_MAX_DLC, and names that are not [a-z0-9_], are dropped. */
int lr_game_parse(const uint8_t *buf, size_t len, lr_game_info *out);

/* The store name of a DLC ("musketeer" -> "The Musketeer"); the name itself if unknown. */
const char *lr_dlc_title(const char *name);

#endif
