/* Unit tests for src/dd_saves.c:
 *   gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -Isrc \
 *       -o build/test_saves tests/test_saves.c src/dd_saves.c && setarch x86_64 -R build/test_saves */
#include "dd_saves.h"

#include <stdio.h>

static int failures;
#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static void test_parse(void) {
    int bad;
    CHECK(dd_coop_slots_parse("3", &bad) == (1u << 3) && !bad, "3");
    CHECK(dd_coop_slots_parse(" 3 , 7 ", &bad) == ((1u << 3) | (1u << 7)) && !bad, "3,7 with spaces");
    CHECK(dd_coop_slots_parse("1,2,3,4,5,6,7,8,9", &bad) == 0x3fe && !bad, "all nine");
    CHECK(dd_coop_slots_parse("", &bad) == 0 && !bad, "empty");
    CHECK(dd_coop_slots_parse(NULL, &bad) == 0 && !bad, "missing");
    /* Malformed settings only need to be reported: the caller then ignores the whole setting. */
    const char *malformed[] = {"0", "10", "3,x", "3 4", "-3", "99999999999", "3;4", "slot3"};
    for (unsigned i = 0; i < sizeof malformed / sizeof malformed[0]; i++) {
        dd_coop_slots_parse(malformed[i], &bad);
        CHECK(bad, "\"%s\" must be reported as malformed", malformed[i]);
    }
}

static void test_names(void) {
    CHECK(dd_profile_from_device_name("steam_profile_2") == 2, "steam_profile_2");
    CHECK(dd_profile_from_device_name("profile_0") == 0, "profile_0");
    CHECK(dd_profile_from_device_name("steam_profile_9") == 9, "steam_profile_9");
    CHECK(dd_profile_from_device_name("steam_storage") == -1, "steam_storage is no campaign");
    CHECK(dd_profile_from_device_name("local_user") == -1, "local_user");
    CHECK(dd_profile_from_device_name("profile_") == -1, "no digit");
    CHECK(dd_profile_from_device_name("profile_12") == -1, "two digits");
    CHECK(dd_profile_from_device_name("steam_profile_2x") == -1, "trailing junk");
    CHECK(dd_profile_from_device_name("") == -1 && dd_profile_from_device_name(NULL) == -1, "empty / null");
}

static void test_decide(void) {
    uint32_t mask = 1u << 3; /* the owner's setup: co-op test in menu slot 3 */
    int pos;
    CHECK(dd_slot_decide("steam_profile_2", mask, &pos) == DD_SLOT_COOP && pos == 3, "slot 3 is co-op");
    CHECK(dd_slot_decide("steam_profile_0", mask, &pos) == DD_SLOT_SINGLE_PLAYER && pos == 1, "slot 1 stays single player");
    CHECK(dd_slot_decide("steam_profile_1", mask, &pos) == DD_SLOT_SINGLE_PLAYER && pos == 2, "slot 2 stays single player");
    CHECK(dd_slot_decide("steam_profile_9", 0xffffffffu, &pos) == DD_SLOT_REFUSED && pos == 0, "restore area always refused");
    CHECK(dd_slot_decide("profile_2", mask, &pos) == DD_SLOT_COOP && pos == 3, "without Steam the same rule");
    CHECK(dd_slot_decide("steam_storage", mask, &pos) == DD_SLOT_NO_CAMPAIGN, "no campaign loaded");
    CHECK(dd_slot_decide("steam_profile_2", 0, &pos) == DD_SLOT_SINGLE_PLAYER, "empty coop_slots: nothing is co-op");
}

int main(void) {
    test_parse();
    test_names();
    test_decide();
    printf("%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
