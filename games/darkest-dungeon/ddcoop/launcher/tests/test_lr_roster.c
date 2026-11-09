/* Roster reader: a roster built here in the game's binary save format, every truncation, and 200,000
 * mutated copies (run under ASan by launcher/build-linux.sh). */
#include "lr_roster.h"

#include "dson_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x);                                                         \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

int main(void) {
    static uint8_t file[65536], copy[65536];
    static const dw_hero roster[] = {
        {"8", "Bourneville", "vestal", 13, 2},
        {"1", "Reynauld", "crusader", 6, 0},
        {"2", "Dismas", "highwayman", 48, 1},
        {"x", "Reynauld", "crusader", 6, 0}, /* a non-numeric id is skipped */
    };
    size_t n = dw_roster(file, roster, 4);
    lr_hero heroes[LR_MAX_HEROES];

    int count = lr_roster_parse(file, n, heroes, LR_MAX_HEROES);
    CHECK(count == 3);
    if (count == 3) {
        CHECK(heroes[0].id == 1 && strcmp(heroes[0].name, "Reynauld") == 0 && strcmp(heroes[0].cls, "crusader") == 0);
        CHECK(heroes[0].xp == 6 && heroes[0].status == 0 && lr_hero_level(heroes[0].xp) == 1);
        CHECK(heroes[1].id == 2 && strcmp(heroes[1].name, "Dismas") == 0 && lr_hero_level(heroes[1].xp) == 6);
        CHECK(heroes[2].id == 8 && strcmp(heroes[2].cls, "vestal") == 0 && heroes[2].status == 2);
    }
    CHECK(lr_roster_parse(file, n, heroes, 2) == 2 && heroes[0].id == 1 && heroes[1].id == 8); /* cap */
    CHECK(lr_hero_level(0) == 0 && lr_hero_level(2) == 1 && lr_hero_level(35) == 4 && lr_hero_level(999) == 6);

    /* not a roster */
    CHECK(lr_roster_parse(file, 10, heroes, LR_MAX_HEROES) == -1);
    memcpy(copy, file, n);
    copy[0] = 0;
    CHECK(lr_roster_parse(copy, n, heroes, LR_MAX_HEROES) == -1);
    uint8_t hero_only[4096];
    size_t hn = dw_hero_file(hero_only, "A", "b", 0, 0);
    CHECK(lr_roster_parse(hero_only, hn, heroes, LR_MAX_HEROES) == -1);

    /* every truncation, then mutations: never a crash or a read outside the buffer (ASan), at most cap heroes */
    for (size_t k = 0; k < n; k++) {
        uint8_t *t = malloc(k ? k : 1);
        memcpy(t, file, k);
        CHECK(lr_roster_parse(t, k, heroes, LR_MAX_HEROES) <= 3);
        free(t);
    }
    srand(12345);
    long parsed = 0;
    for (int round = 0; round < 200000; round++) {
        memcpy(copy, file, n);
        int edits = 1 + rand() % 4;
        for (int e = 0; e < edits; e++) {
            size_t at = (size_t)rand() % n;
            copy[at] = (rand() % 3 == 0) ? 0xff : (uint8_t)rand();
        }
        int c = lr_roster_parse(copy, n, heroes, LR_MAX_HEROES);
        CHECK(c >= -1 && c <= 4);
        for (int i = 0; i < c; i++) CHECK(memchr(heroes[i].name, 0, sizeof heroes[i].name) != NULL);
        if (c > 0) parsed++;
    }
    printf("mutations: 200000, %ld still gave heroes\n", parsed);

    /* persist.game.json */
    lr_game_info g;
    static const char *const one[] = {"musketeer"};
    static const char *const odd[] = {"crimson_court", "Bad Name", "color_of_madness"};
    n = dw_game(file, 1, "tutorial", one, 1);
    CHECK(lr_game_parse(file, n, &g) == 1 && !g.text && g.tutorial);
    CHECK(g.dlc_count == 1 && strcmp(g.dlc[0], "musketeer") == 0); /* presented_dlc (arena_mp) is not used */
    n = dw_game(file, 0, "", NULL, 0);
    CHECK(lr_game_parse(file, n, &g) == 1 && !g.tutorial && g.dlc_count == 0);
    n = dw_game(file, 1, "crypts", odd, 3); /* in a normal raid: not the tutorial; a bad name is dropped */
    {
        lr_game_info named;
        CHECK(lr_game_parse(file, n, &named) && !strcmp(named.estate, "Darkest")); /* What the player called it, for the launcher's list. */
    }
    CHECK(lr_game_parse(file, n, &g) == 1 && !g.tutorial && g.dlc_count == 2);
    CHECK(strcmp(g.dlc[0], "crimson_court") == 0 && strcmp(g.dlc[1], "color_of_madness") == 0);
    /* the game's own template (scripts/starting_save/persist.game.json), byte for byte */
    static const char tmpl[] = "{\n    \"version\": 2,\n    \"data\": {\n        \"totalelapsed\": 0,\n"
                               "        \"inraid\": true,\n        \"raiddungeon\": \"tutorial\",\n"
                               "        \"estatename\": \"Darkest\"\n    }\n}";
    CHECK(sizeof tmpl - 1 == 159);
    CHECK(lr_game_parse((const uint8_t *)tmpl, sizeof tmpl - 1, &g) == 1 && g.text && g.tutorial && g.dlc_count == 0);
    CHECK(lr_game_parse(hero_only, hn, &g) == 1 && !g.tutorial && g.dlc_count == 0); /* other base_root: no fields */
    CHECK(lr_game_parse((const uint8_t *)"xx", 2, &g) == 0);
    CHECK(strcmp(lr_dlc_title("musketeer"), "The Musketeer") == 0 && strcmp(lr_dlc_title("new_one"), "new_one") == 0);
    n = dw_game(file, 1, "tutorial", odd, 3);
    for (size_t k = 0; k < n; k++) lr_game_parse(file, k, &g); /* every truncation (ASan) */
    for (int round = 0; round < 200000; round++) {
        memcpy(copy, file, n);
        for (int e = 1 + rand() % 4; e > 0; e--) copy[(size_t)rand() % n] = (uint8_t)rand();
        if (lr_game_parse(copy, n, &g)) {
            CHECK(g.dlc_count >= 0 && g.dlc_count <= LR_MAX_DLC);
            for (int i = 0; i < g.dlc_count; i++) CHECK(memchr(g.dlc[i], 0, sizeof g.dlc[i]) != NULL);
        }
    }

    if (failures) {
        printf("test_lr_roster: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_lr_roster: all passed\n");
    return 0;
}
