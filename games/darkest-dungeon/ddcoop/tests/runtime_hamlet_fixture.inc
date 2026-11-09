/* Uses real Hamlet hooks/authority/reads. Only native game operations and read-only roots are faked. */
static uint8_t hm_entries[3][0x1638], hm_wallet[4 * 0x48], hm_town[0x4100], hm_widgets[4][0x180],
    hm_slots[4][0x300];
#define hm_roster_panel (hm_town + 0x2f48)
#define hm_embark (hm_town + 0x3640)
#define hm_character (hm_town + 0x38c0)
static uint32_t hm_levels[3][2][7], hm_selected[3][2][8], hm_party[4];
static uint64_t hm_rows[4];
static int hm_slot_calls;
static unsigned hm_display_refreshes;
static uint32_t hm_display_masks[2];
static int hm_native, hm_notify, hm_invalidate;
static uint32_t hm_dismiss_event;
static unsigned hm_index(void *hero);
static uint8_t hm_estate[4][0x5a8], hm_gear[3][2][0x5a8], hm_definitions[2][0x488], hm_class[0x100];
static int hm_item_alive, hm_refresh, hm_broken_swap;
static uintptr_t __fastcall hm_copy(void *to, const void *from) {
    CHECK(!hm_item_alive);
    hm_item_alive++;
    memcpy(to, from, 0x5a8);
    return (uintptr_t)to;
}
static void __fastcall hm_clear(void *item) {
    CHECK(hm_item_alive == 1);
    memset(item, 0, 0x5a8);
}
static void __fastcall hm_destroy(void *item) {
    (void)item;
    CHECK(hm_item_alive == 1);
    hm_item_alive--;
}
static void __fastcall hm_swap(void *inventory, uint32_t slot, void *item) {
    uint64_t begin = 0;
    uint32_t count = 0;
    uint8_t temporary[0x5a8];
    hm_native++;
    if (hm_broken_swap && hm_native == 2) return;
    memcpy(&begin, (uint8_t *)inventory + 8, 8);
    memcpy(&count, (uint8_t *)inventory + 0x24, 4);
    uint8_t *at = (uint8_t *)(uintptr_t)begin + slot * 0x5a8;
    if (at[0]) count--;
    if (((uint8_t *)item)[0]) count++;
    memcpy(temporary, at, 0x5a8);
    memcpy(at, item, 0x5a8);
    memcpy(item, temporary, 0x5a8);
    putint((uint8_t *)inventory + 0x24, count);
}
static void __fastcall hm_actor(void *hero) {
    (void)hm_index(hero);
    hm_refresh++;
}
static void __fastcall hm_pickup(void *ui, void *b, void *c, void *data) {
    (void)ui;
    (void)b;
    (void)c;
    (void)data;
}
static void __fastcall hm_unexpected_finish(void *ui, void *data) {
    (void)ui;
    (void)data;
    CHECK(0);
}
static void __fastcall hm_bulk_native(void *capture) {
    (void)capture;
    hm_native++;
}
static int hm_open(dd_hamlet *s) {
    return dd_hamlet_open(s, shop_read, NULL, (uintptr_t)sale_image, (uintptr_t)world_campaign);
}
static int hm_input(uintptr_t unused) {
    (void)unused;
    return 1;
}
static int hm_not_input(uintptr_t unused) {
    (void)unused;
    return 0;
}
static unsigned hm_index(void *hero) {
    for (unsigned i = 0; i < 3; i++)
        if (hero == hm_entries[i] + 8) return i;
    CHECK(0);
    return 0;
}
static uint8_t hm_toggle(void *pointer, uint32_t index, unsigned camping) {
    unsigned h = hm_index(pointer);
    uint8_t *hero = pointer;
    uint64_t end = 0;
    memcpy(&end, hero + (camping ? 0x13a0 : 0x13d0), 8);
    unsigned n = (unsigned)((end - (uintptr_t)hm_selected[h][camping]) / 4), at = n;
    for (unsigned i = 0; i < n; i++)
        if (hm_selected[h][camping][i] == index) at = i;
    if (at < n) {
        memmove(hm_selected[h][camping] + at, hm_selected[h][camping] + at + 1, (n - at - 1) * 4);
        n--;
    } else if (n < 4)
        hm_selected[h][camping][n++] = index;
    else
        return 0;
    putptr(hero + (camping ? 0x13a0 : 0x13d0), (uintptr_t)(hm_selected[h][camping] + n));
    hm_native++;
    return 1;
}
static uint8_t __fastcall hm_combat(void *hero, uint32_t index) { return hm_toggle(hero, index, 0); }
static uint8_t __fastcall hm_camping(void *hero, uint32_t index) { return hm_toggle(hero, index, 1); }
static void __fastcall hm_refresh_skills(void *panel, void *pointer) {
    CHECK(panel == hm_character && g_game_apply_depth);
    dd_hamlet s;
    dd_hamlet_hero hero;
    CHECK(hm_open(&s) && dd_hamlet_hero_pointer(&s, (uintptr_t)pointer, &hero));
    CHECK(dd_hamlet_skill_mask(&s, &hero, 0, &hm_display_masks[0]));
    CHECK(dd_hamlet_skill_mask(&s, &hero, 1, &hm_display_masks[1]));
    hm_display_refreshes++;
}
/* The hero's own refresh after an upgrade (5d5670, 5d3c20): what the building's callback does where its screen is open. */
static unsigned hm_equipment_calls, hm_skill_calls;
static void *hm_refreshed_hero;
static void __fastcall hm_hero_equipment(void *hero) { hm_equipment_calls++; hm_refreshed_hero = hero; }
static void __fastcall hm_hero_skills(void *hero) { hm_skill_calls++; hm_refreshed_hero = hero; }
static int __fastcall hm_buy(const void *request) {
    const uint8_t *bytes = request;
    uint32_t id = 0;
    memcpy(&id, bytes + 8, 4);
    CHECK(id >= 1 && id <= 3 && g_game_apply_depth);
    if (id < 1 || id > 3) return 0;
    unsigned camping = bytes[4] == 'b';
    hm_levels[id - 1][camping][1] = 0; /* Learn an initially locked skill. */
    CHECK(hamlet_skill_input(hm_entries[id - 1] + 8, 1, camping, 0));
    return 1;
}
static void __fastcall hm_name(void *input, void *hero) {
    hm_native++;
    memcpy((uint8_t *)hero + 8, (uint8_t *)input + 0x14f, 64);
}
static void __fastcall hm_currency(void *wallet, uint32_t type, int32_t delta) {
    CHECK(wallet == world_campaign + 0xe50);
    hm_native++;
    for (unsigned i = 0; i < 4; i++) {
        uint32_t key = 0;
        int32_t amount = 0;
        memcpy(&key, hm_wallet + i * 0x48 + 0x44, 4);
        if (key != type) continue;
        memcpy(&amount, hm_wallet + i * 0x48, 4);
        putint(hm_wallet + i * 0x48, (uint32_t)(amount + delta));
        return;
    }
    CHECK(0);
}
static void __fastcall hm_debit(void *wallet, uint32_t type, int32_t amount) {
    hm_currency(wallet, type, -amount);
}
static void __fastcall hm_changed(void *wallet) {
    CHECK(wallet == world_campaign + 0xe50);
    hm_notify++;
}
static void __fastcall hm_unused_exchange(void *capture) {
    (void)capture;
    CHECK(0);
}
static void __fastcall hm_dismiss(void *panel, uint32_t id) {
    CHECK(panel == hm_roster_panel);
    CHECK(!hm_dismiss_event);
    hm_native++;
    hm_dismiss_event = id; /* Native callback schedules removal; the model changes on a later tick. */
}
static void hm_finish_dismiss(void) {
    uint32_t id = hm_dismiss_event;
    hm_dismiss_event = 0;
    CHECK(id);
    uint64_t end = 0;
    memcpy(&end, world_campaign + 0x28, 8);
    unsigned n = (unsigned)((end - (uintptr_t)world_roster) / 8);
    for (unsigned i = 0; i < n; i++) {
        uint32_t key = 0;
        memcpy(&key, (void *)(uintptr_t)world_roster[i], 4);
        if (key != id) continue;
        memmove(world_roster + i, world_roster + i + 1, (n - i - 1) * 8);
        putptr(world_campaign + 0x28, (uintptr_t)(world_roster + n - 1));
        return;
    }
    CHECK(0);
}
static void __fastcall hm_location(void *roster, void *entry, int32_t location) {
    CHECK(roster == world_campaign && location >= 0 && location <= 1);
    hm_native++;
    putint((uint8_t *)entry + 0x15b0, (uint32_t)location);
    if (hm_invalidate) putptr(hm_embark, 0);
}
static void __fastcall hm_party_slot(void *slot, void *hero) {
    uint32_t rank = 0, id = UINT32_MAX;
    memcpy(&rank, (uint8_t *)slot + 0x230, 4);
    CHECK(rank < 4);
    if (rank >= 4) return;
    if (hero) memcpy(&id, (uint8_t *)hero + 0x137c, 4);
    hm_party[rank] = id;
    putptr((uint8_t *)slot + 0x28, (uintptr_t)hero);
    hm_native++;
    hm_slot_calls++;
}
static void hm_reset(int role) {
    reset(role);
    hm_native = hm_notify = hm_invalidate = 0;
    hm_dismiss_event = 0;
    g_party_pending_mask = 0;
    g_party_pending_at = 0;
    hm_slot_calls = 0;
    memset(world_campaign, 0, sizeof world_campaign);
    memset(hm_entries, 0, sizeof hm_entries);
    memset(hm_levels, 0, sizeof hm_levels);
    memset(hm_selected, 0, sizeof hm_selected);
    memset(hm_wallet, 0, sizeof hm_wallet);
    memset(hm_town, 0, sizeof hm_town);
    memset(hm_slots, 0, sizeof hm_slots);
    memset(hm_widgets, 0, sizeof hm_widgets);
    sale_image[0x2a6d4bb] = 0;
    putptr(sale_image + 0x118e140, 0); /* Old flat registry must not be required. */
    putptr(sale_image + 0x118e360, (uintptr_t)hm_town);
    putptr(hm_town, (uintptr_t)sale_image + 0xec37a8);
    putptr(hm_town + 0x3478, (uintptr_t)sale_image + 0xebe530);
    putptr(hm_roster_panel, (uintptr_t)sale_image + 0xec00a0);
    putptr(hm_embark, (uintptr_t)sale_image + 0xeba728);
    putptr(hm_character, (uintptr_t)sale_image + 0xee2ae8);
    hm_character[0x60] = 1;
    putptr(hm_character + 0x140, (uintptr_t)(hm_entries[0] + 8));
    hm_display_refreshes = 0;
    hm_display_masks[0] = hm_display_masks[1] = 5;
    putptr(hm_embark + 0x88, (uintptr_t)hm_rows);
    putptr(hm_embark + 0x90, (uintptr_t)(hm_rows + 4));
    for (unsigned i = 0; i < 4; i++) {
        hm_rows[i] = (uintptr_t)hm_widgets[i];
        putptr(hm_widgets[i], (uintptr_t)sale_image + 0xebcc00);
        putptr(hm_widgets[i] + 0x158, (uintptr_t)hm_slots[i]);
        putptr(hm_slots[i], (uintptr_t)sale_image + 0xee0838);
        putint(hm_slots[i] + 0x230, i);
        hm_party[i] = i == 0 ? 1 : UINT32_MAX;
    }
    putptr(hm_slots[0] + 0x28, (uintptr_t)(hm_entries[0] + 8));
    putptr(world_campaign, (uintptr_t)hm_party);
    putptr(world_campaign + 8, (uintptr_t)(hm_party + 4));
    putptr(world_campaign + 0x20, (uintptr_t)world_roster);
    putptr(world_campaign + 0x28, (uintptr_t)(world_roster + 3));
    for (unsigned i = 0; i < 3; i++) {
        uint8_t *hero = hm_entries[i] + 8;
        world_roster[i] = (uintptr_t)hm_entries[i];
        putint(hm_entries[i], i + 1);
        putint(hero + 0x137c, i + 1);
        putint(hm_entries[i] + 0x15b0, i ? 0 : 1); /* Hero 1 sits in the party: location 1. */
        snprintf((char *)hero + 8, 64, "Hero %u", i + 1);
        for (unsigned c = 0; c < 2; c++) {
            hm_levels[i][c][1] = UINT32_MAX;
            hm_selected[i][c][0] = 0;
            hm_selected[i][c][1] = 2;
            putptr(hero + (c ? 0x13b0 : 0x13e0), (uintptr_t)hm_levels[i][c]);
            putptr(hero + (c ? 0x13b8 : 0x13e8), (uintptr_t)(hm_levels[i][c] + 7));
            putptr(hero + (c ? 0x1398 : 0x13c8), (uintptr_t)hm_selected[i][c]);
            putptr(hero + (c ? 0x13a0 : 0x13d0), (uintptr_t)(hm_selected[i][c] + 2));
        }
    }
    const uint32_t types[] = {14924810, 504499961, 15176862, 798419517};
    putptr(world_campaign + 0xe70, (uintptr_t)hm_wallet);
    putptr(world_campaign + 0xe78, (uintptr_t)(hm_wallet + sizeof hm_wallet));
    for (unsigned i = 0; i < 4; i++) {
        putint(hm_wallet + i * 0x48, 20);
        putint(hm_wallet + i * 0x48 + 0x44, types[i]);
    }
    g_hamlet_open_model = hm_open;
    g_hamlet_skill_input = g_hamlet_party_input = hm_input;
    g_hamlet_toggle[0] = hm_combat;
    g_hamlet_toggle[1] = hm_camping;
    g_hamlet_skills_refresh = hm_refresh_skills;
    g_original_buy_upgrade = hm_buy;
    g_hero_equipment = hm_hero_equipment;
    g_hero_skills = hm_hero_skills;
    hm_equipment_calls = hm_skill_calls = 0;
    hm_refreshed_hero = NULL;
    g_hamlet_name = hm_name;
    g_hamlet_dismiss = hm_dismiss;
    g_hamlet_add = hm_currency;
    g_hamlet_deduct = hm_debit;
    g_hamlet_notify = hm_changed;
    g_hamlet_exchange_original = hm_unused_exchange;
    g_hamlet_party_slot = hm_party_slot;
    g_hamlet_location = hm_location;
    hm_item_alive = hm_refresh = hm_broken_swap = 0;
    memset(hm_estate, 0, sizeof hm_estate);
    memset(hm_gear, 0, sizeof hm_gear);
    memset(hm_definitions, 0, sizeof hm_definitions);
    memset(hm_class, 0, sizeof hm_class);
    putint(sale_image + 0x2a6f868, 3950276909u);
    putptr(world_campaign + 0xe90, (uintptr_t)hm_estate);
    putptr(world_campaign + 0xe98, (uintptr_t)(hm_estate + 4));
    putint(world_campaign + 0xeac, 2);
    for (unsigned i = 0; i < 2; i++) {
        putint(hm_estate[i], 1);
        putint(hm_estate[i] + 0x44, 3950276909u);
        putint(hm_estate[i] + 0x88, 101 + i);
        putint(hm_definitions[i] + 0x40, 101 + i);
    }
    putptr(sale_image + 0x2c3c570, (uintptr_t)hm_definitions);
    putptr(sale_image + 0x2c3c578, (uintptr_t)(hm_definitions + 2));
    for (unsigned i = 0; i < 3; i++) {
        putptr(hm_entries[i] + 8 + 0x1288, (uintptr_t)hm_gear[i]);
        putptr(hm_entries[i] + 8 + 0x1290, (uintptr_t)(hm_gear[i] + 2));
        putptr(hm_entries[i] + 8 + 0x1320, (uintptr_t)hm_class);
    }
    putint(hm_class + 0xd4, 123);
    g_hamlet_copy = hm_copy;
    g_hamlet_clear = hm_clear;
    g_hamlet_destroy = hm_destroy;
    g_hamlet_swap = hm_swap;
    g_hamlet_actor_refresh = hm_actor;
    g_trinket_pickup[0] = g_trinket_pickup[1] = hm_pickup;
    for (unsigned i = 0; i < 4; i++)
        g_trinket_finish[i] = hm_unexpected_finish;
    g_hamlet_bulk[0] = g_hamlet_bulk[1] = hm_bulk_native;
    dd_town_init(&g_town, NULL, runtime_apply, NULL, fake_send);
}
static dd_msg hm_host_apply(dd_msg request) {
    hm_reset(DD_ROLE_HOST);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.accepted == 1 && !g_town.needs_resync && last_message.type == DD_MSG_TOWN_APPLY &&
          !g_game_apply_depth);
    return last_message;
}
static void hm_names_and_skills(void) {
    hm_reset(DD_ROLE_GUEST);
    uint8_t input[0x14f + 64] = {0};
    strcpy((char *)input + 0x14f, "New name");
    hook_hamlet_name(input, hm_entries[0] + 8);
    CHECK(sent == 1 && !hm_native && !strcmp((char *)hm_entries[0] + 16, "Hero 1"));
    dd_msg request = last_message, apply = hm_host_apply(request);
    CHECK(hm_native == 1 && !strcmp((char *)hm_entries[0] + 16, "New name"));
    request.u.town.request_id = 2;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && hm_native == 1);
    hm_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(hm_native == 1 && !strcmp((char *)hm_entries[0] + 16, "New name"));
    for (unsigned camping = 0; camping < 2; camping++) {
        hm_reset(DD_ROLE_GUEST);
        CHECK(!hamlet_skill_input(hm_entries[0] + 8, 3, camping, 0));
        CHECK(sent == 1 && !hm_native);
        CHECK(!hamlet_skill_input(hm_entries[0] + 8, 4, camping, 0) && sent == 1);
        request = last_message;
        apply = hm_host_apply(request);
        CHECK(hm_native == 1);
        CHECK(!hm_display_refreshes && hm_display_masks[camping] == 5);
        dd_runtime_poll();
        CHECK(hm_native == 1 && hm_display_refreshes == 1 && hm_display_masks[camping] == 13);
        hm_reset(DD_ROLE_GUEST);
        CHECK(queue_push(&g_incoming, &apply));
        dd_runtime_poll();
        dd_hamlet s;
        dd_hamlet_hero hero;
        int active = 0;
        CHECK(hm_open(&s) && dd_hamlet_find(&s, 1, &hero) == 1 &&
              dd_hamlet_skill(&s, &hero, camping, 3, &active) && active);
        CHECK(!hm_display_refreshes && hm_display_masks[camping] == 5);
        dd_runtime_poll();
        CHECK(hm_native == 1 && hm_display_refreshes == 1 && hm_display_masks[camping] == 13);
        hm_reset(DD_ROLE_GUEST);
        CHECK(!hamlet_skill_input(hm_entries[0] + 8, 1, camping, 0));
        CHECK(!sent && !hm_native);
        hm_reset(DD_ROLE_HOST);
        hm_selected[0][camping][1] = 3;
        CHECK(queue_push(&g_incoming, &request));
        dd_runtime_poll();
        CHECK(!hm_native && g_town.rejected == 1);
    }
    puts("Hamlet bridge: names, combat/camping selection, stale names/masks and unlearned skills verified");
}
static void hm_upgrades_and_refresh(void) {
    for (unsigned camping = 0; camping < 2; camping++) {
        uint8_t request_bytes[12] = {0};
        putint(request_bytes, 123);
        request_bytes[4] = camping ? 'b' : 'a';
        putint(request_bytes + 8, 1);
        hm_reset(DD_ROLE_GUEST);
        CHECK(!hook_buy_upgrade(request_bytes) && sent == 1 && !hm_native &&
              hm_levels[0][camping][1] == UINT32_MAX && !hm_equipment_calls && !hm_skill_calls);
        dd_msg request = last_message, apply = hm_host_apply(request);
        CHECK(hm_native == 1 && sent == 1 && apply.u.town.action.kind == DD_ACT_BUY_UPGRADE &&
              hm_levels[0][camping][1] == 0 && !hm_display_refreshes);
        /* The host had no screen for this hero: its weapon, armour and skill levels follow the purchase all the same. */
        CHECK(hm_equipment_calls == 1 && hm_skill_calls == 1 && hm_refreshed_hero == (void *)(hm_entries[0] + 8));
        dd_runtime_poll();
        CHECK(hm_display_refreshes == 1 && hm_display_masks[camping] == 7 && sent == 1 && hm_native == 1);
        hm_reset(DD_ROLE_GUEST);
        CHECK(queue_push(&g_incoming, &apply));
        dd_runtime_poll();
        dd_runtime_poll();
        CHECK(hm_native == 1 && !sent && hm_levels[0][camping][1] == 0 && hm_display_masks[camping] == 7);
        CHECK(hm_equipment_calls == 1 && hm_skill_calls == 1 && hm_refreshed_hero == (void *)(hm_entries[0] + 8));
        hm_reset(DD_ROLE_HOST);
        CHECK(hook_buy_upgrade(request_bytes) && sent == 1 && !hm_display_refreshes && hm_native == 1);
        CHECK(hm_equipment_calls == 1 && hm_skill_calls == 1);
        CHECK(last_message.u.town.action.kind == DD_ACT_BUY_UPGRADE);
        dd_runtime_poll();
        CHECK(hm_display_refreshes == 1 && hm_native == 1 && sent == 1);
    }
    hm_reset(DD_ROLE_HOST);
    CHECK(hamlet_skill_input(hm_entries[0] + 8, 3, 0, 0));
    hm_character[0x60] = 0;
    dd_runtime_poll();
    CHECK(!hm_display_refreshes && !g_refresh_count);
    hm_character[0x60] = 1;
    hamlet_refresh_schedule(2); /* Another hero is on screen. */
    dd_runtime_poll();
    CHECK(!hm_display_refreshes);
    hamlet_refresh_schedule(1);
    putptr(hm_character + 0x508, 1); /* Malformed button vector. */
    dd_runtime_poll();
    CHECK(!hm_display_refreshes);
    CHECK(hamlet_skill_ui(image_base() + 0x8045f1) && hamlet_skill_ui(image_base() + 0x8040c8));
    CHECK(!hamlet_skill_ui(image_base() + 0x7f1000));
    puts("Hamlet upgrades: combat/camping learning uses one ordered purchase; deferred visible skill "
         "refresh, no nested toggle requests");
}
static void hm_connection_loss(void) {
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        hm_reset(role);
        g_session_seen = g_session_lost = 1;
        g_session_ready = 0;
        dd_runtime_poll();
        CHECK(alerts == 1 && g_town.needs_resync);
        uint8_t request[12] = {0};
        putint(request, 123);
        request[4] = 'a';
        putint(request + 8, 1);
        CHECK(!hook_buy_upgrade(request));
        hook_hamlet_party_slot(hm_slots[1], hm_entries[1] + 8);
        hook_hamlet_dismiss(hm_roster_panel, 2);
        CHECK(!hamlet_skill_input(hm_entries[0] + 8, 3, 0, 0));
        CHECK(!hook_assign_hero(NULL, NULL, 1));
        CHECK(!hm_native && !sent && hm_party[1] == UINT32_MAX);
        g_session_ready = 1; /* A new socket must not silently clear lost authority. */
        dd_runtime_poll();
        CHECK(!hook_buy_upgrade(request) && !hm_native && !sent && alerts == 1);
    }
    hm_reset(DD_ROLE_HOST);
    g_session_ready = 0;
    g_session_seen = 0;
    g_hamlet_party_input = hm_not_input;
    hook_hamlet_party_slot(hm_slots[1], hm_entries[1] + 8);
    CHECK(hm_party[1] == 2 && !sent && !g_town.needs_resync); /* Initial save load still works. */
    g_hamlet_party_input = hm_input;
    /* A player dragging a hero before the connection is refused, with a message, every time. */
    hm_reset(DD_ROLE_HOST);
    g_session_ready = 0;
    g_session_seen = 0;
    {
        int before = alerts;
        hook_hamlet_party_slot(hm_slots[1], hm_entries[1] + 8);
        CHECK(hm_party[1] != 2 && !sent && alerts == before + 1 && !g_town.needs_resync);
        hook_hamlet_party_slot(hm_slots[1], hm_entries[1] + 8);
        CHECK(hm_party[1] != 2 && alerts == before + 2);
    }
    puts("Hamlet disconnect: host/guest shared mutations freeze once, reconnect cannot resume stale "
         "authority; initial load allowed");
}
static void hm_exchanges_and_dismiss(void) {
    hm_reset(DD_ROLE_GUEST);
    uint8_t capture[0x28] = {0}, source[0x90] = {0};
    putint(source + 0x84, 14924810);
    putptr(capture, (uintptr_t)source);
    putptr(capture + 8, (uintptr_t)(world_campaign + 0xe50));
    putint(capture + 0x10, 504499961);
    putint(capture + 0x14, 6);
    putint(capture + 0x18, 2);
    hook_hamlet_exchange(capture);
    CHECK(sent == 1 && !hm_native);
    dd_msg request = last_message;
    dd_msg apply = hm_host_apply(request);
    CHECK(hm_native == 2 && hm_notify == 1 && hm_wallet[0] == 14 && hm_wallet[0x48] == 22);
    hm_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(hm_native == 2 && hm_wallet[0] == 14 && hm_wallet[0x48] == 22);
    hm_reset(DD_ROLE_HOST);
    putint(hm_wallet, 5);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(!hm_native && g_town.rejected == 1);
    hm_reset(DD_ROLE_HOST);
    request.u.town.action.u.exchange.dst_amount = 3;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(!hm_native && g_town.rejected == 1);
    /* Location 1 is a party slot, also when the remembered party does not name this hero. */
    hm_reset(DD_ROLE_HOST);
    putint(hm_entries[1] + 0x15b0, 1);
    hook_hamlet_dismiss(hm_roster_panel, 2);
    CHECK(!g_dismiss.active && !hm_native && alerts);
    hm_reset(DD_ROLE_GUEST);
    hook_hamlet_dismiss(hm_roster_panel, 2);
    CHECK(sent == 1 && !hm_native);
    request = last_message;
    hm_reset(DD_ROLE_HOST);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(hm_native == 1 && !sent && !g_town.accepted && !g_town.rejected && g_incoming.count == 1);
    dd_runtime_poll();
    CHECK(hm_native == 1 && !sent && !g_town.needs_resync);
    hm_finish_dismiss();
    dd_runtime_poll();
    CHECK(sent == 1 && g_town.accepted == 1 && !g_town.needs_resync);
    apply = last_message;
    hm_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(!sent && !g_town.applied_seq && !g_town.needs_resync && hm_native == 1);
    hm_finish_dismiss();
    dd_runtime_poll();
    dd_hamlet s;
    dd_hamlet_hero hero;
    CHECK(hm_open(&s) && dd_hamlet_find(&s, 2, &hero) == 0 && hm_native == 1);
    puts("Hamlet bridge: exchange ratios/funds, guest authority and dismissal on both copies verified");
}
static void hm_parties(void) {
    hm_reset(DD_ROLE_GUEST);
    hook_hamlet_party_slot(hm_slots[1], hm_entries[1] + 8);
    CHECK(sent == 1 && !hm_native && hm_party[1] == UINT32_MAX);
    dd_msg request = last_message, apply = hm_host_apply(request);
    /* Owner's two-PC test: the raid's hero list is the roster at location 1, so a shared party must set that. */
    CHECK(hm_party[0] == 1 && hm_party[1] == 2 && hm_entries[0][0x15b0] == 1 && hm_entries[1][0x15b0] == 1);
    hm_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(hm_party[0] == 1 && hm_party[1] == 2 && hm_entries[0][0x15b0] == 1 && hm_entries[1][0x15b0] == 1);
    /* Every slot redraws, also an empty one: a removed hero must not stay visible. */
    for (unsigned i = 0; i < 4; i++) CHECK(hm_slots[i][0x0c] == 1);
    /* And the roster screen looks the team's name up again, as after a real drop. */
    CHECK(hm_town[0x3478 + 0xb0] == 1);
    hm_reset(DD_ROLE_HOST);
    hm_party[0] = 3;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(!hm_native && g_town.rejected == 1);
    hm_reset(DD_ROLE_GUEST);
    hook_hamlet_party_slot(hm_slots[0], NULL);
    dd_msg remove = last_message;
    hook_hamlet_party_slot(hm_slots[0], hm_entries[0] + 8);
    dd_msg restore = last_message;
    CHECK(sent == 2 && !hm_native && g_party_request_count == 2 &&
          restore.u.town.action.u.party.before[0] == 0 && restore.u.town.action.u.party.after[0] == 1);
    hm_reset(DD_ROLE_HOST);
    CHECK(queue_push(&g_incoming, &remove));
    dd_runtime_poll();
    CHECK(hm_party[0] == UINT32_MAX && hm_entries[0][0x15b0] == 0); /* Removed: free again. */
    hm_reset(DD_ROLE_HOST);
    CHECK(queue_push(&g_incoming, &remove) && queue_push(&g_incoming, &restore));
    dd_runtime_poll();
    CHECK(g_town.accepted == 2 && hm_party[0] == 1 && !g_town.needs_resync);
    hm_reset(DD_ROLE_HOST);
    hm_invalidate = 1;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.needs_resync && alerts == 1);
    puts("Hamlet bridge: party add, stale conflicts, queued drag cancellation and UI invalidation verified");
}
static unsigned hm_roster_updates;
static void __fastcall hm_roster_update(void *panel, float seconds) {
    CHECK(panel == hm_roster_panel && seconds == 0.0f && g_game_apply_depth);
    hm_roster_updates++;
    if (hm_dismiss_event) hm_finish_dismiss();
}
/* The roster list is not showing on this copy: its update is run once, after a moment. */
/* Back from a raid: the host sends its town once, two seconds after it is there; the guest compares when it is in town too. */
static int hm_raid_now;
static int hm_back_view(walk_view *v) {
    memset(v, 0, sizeof *v);
    return hm_raid_now;
}
/* The launcher's game alone, for a new campaign: once the tutorial is over and the town has been on screen for a moment,
 * the player is told that this game shares nothing and what to do. Never in the tutorial's raid, never twice, never in a
 * normal start or a session. */
static int hm_open_none(dd_hamlet *s) { (void)s; return 0; }
static void hm_alone(void) {
    int (*saved)(walk_view *) = g_walk_view;
    hm_reset(DD_ROLE_OFF);
    g_walk_view = hm_back_view;
    g_fe_new_slot = 5;
    g_alone_told = 0;
    g_alone_town_since = 0;
    hm_raid_now = 1; /* the tutorial's raid */
    alone_town_poll();
    CHECK(!alerts && !g_alone_town_since);
    hm_raid_now = 0;
    alone_town_poll();
    CHECK(!alerts && g_alone_town_since);
    alone_town_poll();
    CHECK(!alerts);
    hm_raid_now = 1; /* back in a raid: the wait starts again */
    alone_town_poll();
    CHECK(!alerts && !g_alone_town_since);
    hm_raid_now = 0;
    alone_town_poll();
    g_alone_town_since -= DD_ALONE_TOWN_MS;
    alone_town_poll();
    CHECK(alerts == 1 && g_alone_told && !strcmp(g_shop_notice, g_alone_done) && !sent);
    g_shop_notice[0] = 0;
    g_alone_town_since = 1;
    alone_town_poll();
    CHECK(alerts == 1);
    /* No town display (the name screen, a loading screen): nothing. */
    hm_reset(DD_ROLE_OFF);
    g_walk_view = hm_back_view;
    g_alone_told = 0;
    g_alone_town_since = 1;
    g_hamlet_open_model = hm_open_none;
    alone_town_poll();
    CHECK(!alerts && !g_alone_town_since);
    /* A normal start without DDCoop, the campaign not loaded, or a session: never. */
    for (int c = 0; c < 4; c++) {
        hm_reset(c == 3 ? DD_ROLE_HOST : DD_ROLE_OFF);
        g_walk_view = hm_back_view;
        g_fe_new_slot = c == 0 ? 0 : 5;
        if (c == 1) g_campaign_active = 0;
        if (c == 2) g_alone_told = 1;
        else g_alone_told = 0;
        g_alone_town_since = 1;
        alone_town_poll();
        CHECK(!alerts);
    }
    g_fe_new_slot = 0;
    g_alone_told = 0;
    g_alone_town_since = 0;
    g_walk_view = saved;
    puts("New campaign alone: in town after the tutorial the player is told once to close the game and click Host; never in a raid, a normal start or a session");
}
static void hm_town_check(void) {
    int (*saved)(walk_view *) = g_walk_view;
    dd_action check;
    dd_msg m = {0};
    g_walk_view = hm_back_view;
    hm_raid_now = 0;
    /* Host: nothing before a raid, then once, after its two seconds. */
    hm_reset(DD_ROLE_HOST);
    g_walk_view = hm_back_view;
    town_check_poll();
    CHECK(!sent);
    g_town_check_raid = 1;
    town_check_poll();
    CHECK(!sent && g_town_check_at);
    town_check_poll();
    CHECK(!sent);
    g_town_check_at = 1;
    town_check_poll();
    CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.action.u.item.op == DD_ITEM_OP_TOWN &&
          last_message.u.town.action.u.item.round == 3 && !g_town_check_raid);
    check = last_message.u.town.action;
    town_check_poll();
    CHECK(sent == 1);
    /* The guest asks for it: only the host says what its town is. */
    m.type = DD_MSG_TOWN_REQUEST;
    m.u.town.request_id = 5;
    m.u.town.action = check;
    CHECK(queue_push(&g_incoming, &m));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && !alerts);
    /* Guest, same town: applied, nothing said. */
    hm_reset(DD_ROLE_GUEST);
    g_walk_view = hm_back_view;
    m.type = DD_MSG_TOWN_APPLY;
    m.seq = 1;
    m.u.town.request_id = 0;
    CHECK(queue_push(&g_incoming, &m));
    dd_runtime_poll();
    CHECK(g_town.applied_seq == 1 && !alerts && !g_town.needs_resync);
    /* Still in the raid here: it waits for the town. */
    hm_raid_now = 1;
    putint(hm_wallet, 25);
    m.seq = 2;
    CHECK(queue_push(&g_incoming, &m));
    dd_runtime_poll();
    CHECK(g_incoming.count == 1 && g_town.applied_seq == 1);
    /* In town, with other gold: said once, and the order is done. */
    hm_raid_now = 0;
    dd_runtime_poll();
    CHECK(!g_incoming.count && g_town.applied_seq == 2 && alerts == 1 && !g_town.needs_resync);
    /* Another hero's stress and another trinket count too. */
    putint(hm_wallet, 20);
    putint(hm_entries[1] + 8 + DD_HERO_STRESS, 0x42200000u);
    m.seq = 3;
    CHECK(queue_push(&g_incoming, &m));
    dd_runtime_poll();
    CHECK(g_town.applied_seq == 3 && alerts == 2);
    putint(hm_entries[1] + 8 + DD_HERO_STRESS, 0);
    putint(hm_gear[2][0], 1);
    putint(hm_gear[2][0] + 0x44, 3950276909u);
    putint(hm_gear[2][0] + 0x88, 101);
    g_shop_notice[0] = 0; /* the same text is not said twice in a row */
    m.seq = 4;
    CHECK(queue_push(&g_incoming, &m));
    dd_runtime_poll();
    CHECK(g_town.applied_seq == 4 && alerts == 3);
    g_walk_view = saved;
    puts("Town after a raid: the host sends currencies, heroes and count once, two seconds after the town is back; the guest compares in town and says a difference");
}
static void hm_dismiss_hidden(void) {
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        dd_msg m = {0};
        hm_reset(role);
        hm_roster_updates = 0;
        g_hamlet_roster_update = hm_roster_update;
        m.type = (uint16_t)(role == DD_ROLE_HOST ? DD_MSG_TOWN_REQUEST : DD_MSG_TOWN_APPLY);
        m.seq = 1;
        m.u.town.request_id = role == DD_ROLE_HOST ? 3 : 0;
        m.u.town.action.kind = DD_ACT_DISMISS;
        m.u.town.action.u.hero.hero_id = 2;
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        dd_runtime_poll();
        CHECK(g_dismiss.active && !hm_roster_updates && hm_dismiss_event == 2 && !alerts); /* Not at once: normally the game does it next frame. */
        g_dismiss.nudge_at = 0;
        dd_runtime_poll();
        CHECK(hm_roster_updates == 1 && !hm_dismiss_event && !g_game_apply_depth);
        dd_runtime_poll();
        dd_runtime_poll();
        CHECK(!g_dismiss.active && !g_incoming.count && !g_town.needs_resync && !alerts && hm_roster_updates == 1);
        CHECK(role == DD_ROLE_HOST ? g_town.accepted == 1 && sent == 1 : g_town.applied_seq == 1);
        /* The update did not do it after all: once only, and the time limit still stops it. */
        hm_reset(role);
        hm_roster_updates = 0;
        g_hamlet_roster_update = NULL;
        m.u.town.request_id = role == DD_ROLE_HOST ? 4 : 0;
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        g_dismiss.nudge_at = 0;
        dd_runtime_poll();
        CHECK(g_dismiss.active && !g_town.needs_resync && hm_dismiss_event == 2);
        g_hamlet_roster_update = hm_roster_update;
        hm_dismiss_event = 0; /* Its update will find nothing. */
        dd_runtime_poll();
        dd_runtime_poll();
        CHECK(hm_roster_updates == 1 && g_dismiss.active && !g_town.needs_resync);
        g_dismiss.until = 0;
        dd_runtime_poll();
        CHECK(g_town.needs_resync);
        memset(&g_dismiss, 0, sizeof g_dismiss);
        g_town.needs_resync = 0;
    }
    g_hamlet_roster_update = NULL;
    puts("Dismissal without the roster list showing: its update runs once after a moment; the order is applied on both copies; the time limit still holds");
}
static void hm_party_panel_wait(void) {
    dd_msg party = {0}, name = {0};
    party.type = name.type = DD_MSG_TOWN_APPLY;
    party.seq = 1;
    name.seq = 2;
    party.u.town.action.kind = DD_ACT_PARTY;
    party.u.town.action.u.party.before[0] = 1;
    party.u.town.action.u.party.after[0] = 1;
    party.u.town.action.u.party.after[1] = 2;
    name.u.town.action.kind = DD_ACT_RENAME;
    name.u.town.action.u.rename.hero_id = 3;
    strcpy(name.u.town.action.u.rename.before, "Hero 3");
    strcpy(name.u.town.action.u.rename.after, "Renamed");
    for (unsigned role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        hm_reset((int)role);
        party.type = name.type = (uint16_t)(role == DD_ROLE_HOST ? DD_MSG_TOWN_REQUEST : DD_MSG_TOWN_APPLY);
        party.u.town.request_id = role == DD_ROLE_HOST ? 11 : 0;
        name.u.town.request_id = role == DD_ROLE_HOST ? 12 : 0;
        putptr(hm_embark + 0x90, (uintptr_t)hm_rows); /* Normal panel, widgets not built while closed. */
        CHECK(queue_push(&g_incoming, &party) && queue_push(&g_incoming, &name));
        hook_hamlet_party_slot(hm_slots[1], hm_entries[1] + 8);
        CHECK(!hm_native && !sent && !g_town.needs_resync); /* Packet arrived before the next poll: no second request. */
        dd_runtime_poll();
        /* No screen needed: both orders are done now, in the model, and nothing is said. */
        CHECK(!g_incoming.count && !g_town.needs_resync && g_town.applied_seq == 2 && !hm_slot_calls && !alerts && !g_shop_notice[0]);
        CHECK(hm_party[0] == 1 && hm_party[1] == 2 && hm_party[2] == UINT32_MAX && hm_entries[1][0x15b0] == 1);
        CHECK(!strcmp((char *)hm_entries[2] + 16, "Renamed"));
        CHECK(sent == (role == DD_ROLE_HOST ? 2 : 0));
        CHECK(g_party_pending_mask == 0xf);
        /* While it is not built, the slots wait. */
        g_party_pending_at = 0;
        dd_runtime_poll();
        CHECK(!hm_slot_calls && g_party_pending_mask == 0xf);
        /* Opening the screen shows what the orders said, once. */
        putptr(hm_embark + 0x90, (uintptr_t)(hm_rows + 4));
        memset(hm_town + 0x3478 + 0xb0, 0, 1);
        g_party_pending_at = GetTickCount64() + 60000;
        dd_runtime_poll();
        CHECK(!hm_slot_calls); /* Not every frame. */
        g_party_pending_at = 0;
        dd_runtime_poll();
        {
            uint64_t shown = 0;
            memcpy(&shown, hm_slots[1] + 0x28, 8);
            CHECK(shown == (uintptr_t)(hm_entries[1] + 8) && hm_slots[1][0x0c] == 1 && hm_party[1] == 2);
            memcpy(&shown, hm_slots[2] + 0x28, 8);
            CHECK(!shown && hm_party[2] == UINT32_MAX);
        }
        CHECK(!g_party_pending_mask && hm_slot_calls && hm_town[0x3478 + 0xb0] == 1 && !g_town.needs_resync);
        int completed = hm_slot_calls;
        g_party_pending_at = 0;
        dd_runtime_poll();
        CHECK(hm_slot_calls == completed && g_town.applied_seq == 2);
        /* The screen was built, and can show at once again: no slots wait. */
        dd_msg next = party;
        next.seq = 3;
        next.u.town.request_id = role == DD_ROLE_HOST ? 13 : 0;
        next.u.town.action.u.party.before[1] = 2;
        next.u.town.action.u.party.after[1] = 0;
        CHECK(queue_push(&g_incoming, &next));
        dd_runtime_poll();
        CHECK(!g_incoming.count && !g_town.needs_resync && g_town.applied_seq == 3 && hm_party[1] == UINT32_MAX && hm_slot_calls > completed && !g_party_pending_mask);
        CHECK(hm_entries[1][0x15b0] == 0); /* Removed: free again. */
        /* And removing without a screen frees the hero too. */
        completed = hm_slot_calls;
        next.seq = 4;
        next.u.town.request_id = role == DD_ROLE_HOST ? 14 : 0;
        next.u.town.action.u.party.before[1] = 0;
        next.u.town.action.u.party.before[0] = 1;
        next.u.town.action.u.party.after[0] = 0;
        putptr(hm_embark + 0x90, (uintptr_t)hm_rows);
        CHECK(queue_push(&g_incoming, &next));
        dd_runtime_poll();
        CHECK(!g_town.needs_resync && hm_party[0] == UINT32_MAX && hm_entries[0][0x15b0] == 0 && hm_slot_calls == completed && g_party_pending_mask == 0xf);
        CHECK(!alerts);
    }
    hm_reset(DD_ROLE_GUEST);
    party.type = DD_MSG_TOWN_APPLY;
    party.seq = 2;
    putptr(hm_embark + 0x90, (uintptr_t)hm_rows);
    CHECK(queue_push(&g_incoming, &party));
    dd_runtime_poll();
    CHECK(g_town.needs_resync && !hm_native && !g_incoming.count); /* Gaps never hide in the wait. */
    hm_reset(DD_ROLE_GUEST);
    party.seq = 1;
    putptr(hm_slots[1], (uintptr_t)sale_image + 0x123456);
    CHECK(queue_push(&g_incoming, &party));
    dd_runtime_poll();
    CHECK(g_town.needs_resync && !hm_native && !g_incoming.count); /* Wrong type is invalid, not unbuilt. */
    hm_reset(DD_ROLE_GUEST);
    putptr(hm_embark + 0x90, (uintptr_t)hm_rows);
    hm_party[0] = 3; /* A genuine logical conflict is still refused without a screen. */
    CHECK(queue_push(&g_incoming, &party));
    dd_runtime_poll();
    CHECK(g_town.needs_resync && !hm_native && hm_party[1] == UINT32_MAX);
    /* The campaign's party is not the size it should be: not written to. */
    hm_reset(DD_ROLE_GUEST);
    putptr(hm_embark + 0x90, (uintptr_t)hm_rows);
    {
        uint64_t begin = 0;
        memcpy(&begin, world_campaign, 8);
        putptr(world_campaign + 8, (uintptr_t)begin + 12);
        CHECK(queue_push(&g_incoming, &party));
        dd_runtime_poll();
        CHECK(g_town.needs_resync && hm_party[1] == UINT32_MAX);
    }
    puts("Party without a screen: closed host/guest panel applies in the model at once, silently, ordered with a rename; "
         "the slots follow once when the screen exists; conflicts and gaps still stop");
}
/* The screen opens empty while the party is remembered: a drag to the remembered rank is sent as one slot to show. */
static void hm_party_show(void) {
    uint64_t shown = 0;
    hm_reset(DD_ROLE_HOST);
    memset(hm_slots[0] + 0x28, 0, 8);
    hook_hamlet_party_slot(hm_slots[0], hm_entries[0] + 8);
    CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.action.kind == DD_ACT_PARTY && last_message.u.town.action.u.party.show == 1);
    dd_action show = last_message.u.town.action;
    dd_msg order = last_message;
    CHECK(dd_action_valid(&show));
    /* On the other copy: the slot draws the hero and the screen looks the team's name up again. */
    hm_reset(DD_ROLE_GUEST);
    memset(hm_slots[0] + 0x28, 0, 8);
    CHECK(queue_push(&g_incoming, &order));
    dd_runtime_poll();
    CHECK(hm_slots[0][0x0c] == 1 && hm_town[0x3478 + 0xb0] == 1 && !g_town.needs_resync);
    /* The other copy has no expedition screen yet: the hero is in the party now, the slot follows, and only that slot. */
    hm_reset(DD_ROLE_GUEST);
    memset(hm_slots[0] + 0x28, 0, 8);
    putptr(hm_slots[1] + 0x28, (uintptr_t)(hm_entries[1] + 8));
    hm_entries[0][0x15b0] = 0;
    putptr(hm_embark + 0x90, (uintptr_t)hm_rows);
    CHECK(queue_push(&g_incoming, &order));
    dd_runtime_poll();
    CHECK(!hm_slot_calls && hm_entries[0][0x15b0] == 1 && g_party_pending_mask == 1 && !g_town.needs_resync && !alerts);
    putptr(hm_embark + 0x90, (uintptr_t)(hm_rows + 4));
    g_party_pending_at = 0;
    dd_runtime_poll();
    memcpy(&shown, hm_slots[0] + 0x28, 8);
    CHECK(shown == (uintptr_t)(hm_entries[0] + 8) && hm_slots[0][0x0c] == 1 && hm_slot_calls == 1 && !g_party_pending_mask);
    memcpy(&shown, hm_slots[1] + 0x28, 8);
    CHECK(shown == (uintptr_t)(hm_entries[1] + 8));
    /* Something else at that place is not written to. */
    hm_reset(DD_ROLE_GUEST);
    memset(hm_slots[0] + 0x28, 0, 8);
    putptr(hm_town + 0x3478, (uintptr_t)sale_image + 0x10);
    CHECK(queue_push(&g_incoming, &order));
    dd_runtime_poll();
    CHECK(hm_slots[0][0x0c] == 1 && !hm_town[0x3478 + 0xb0] && !g_town.needs_resync);
    /* Already shown, a redraw: no order. */
    hm_reset(DD_ROLE_HOST);
    { uint64_t pointer = (uintptr_t)(hm_entries[0] + 8); memcpy(hm_slots[0] + 0x28, &pointer, 8); }
    hook_hamlet_party_slot(hm_slots[0], hm_entries[0] + 8);
    CHECK(!sent);
    /* The guest asks and draws nothing yet. */
    hm_reset(DD_ROLE_GUEST);
    memset(hm_slots[0] + 0x28, 0, 8);
    hook_hamlet_party_slot(hm_slots[0], hm_entries[0] + 8);
    CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST && last_message.u.town.action.u.party.show == 1);
    /* Not a player's: the game's own fill. */
    hm_reset(DD_ROLE_HOST);
    memset(hm_slots[0] + 0x28, 0, 8);
    g_hamlet_party_input = hm_not_input;
    hook_hamlet_party_slot(hm_slots[0], hm_entries[0] + 8);
    CHECK(!sent);
    g_hamlet_party_input = hm_input;
    /* An invalid wish on the wire. */
    {
        dd_action bad = show;
        bad.u.party.show = 5;
        CHECK(!dd_action_valid(&bad));
        bad.u.party.show = 1;
        bad.u.party.after[0] = 0;
        bad.u.party.before[0] = 0;
        CHECK(!dd_action_valid(&bad));
    }
    (void)shown;
    puts("Hamlet bridge: dragging a hero to the rank it is remembered at reaches the other copy");
}
static void hm_party_screen_fill(void) {
    /* Opening the screen fills the slots from the party that is there: that location change is the game's own. */
    hm_reset(DD_ROLE_HOST);
    putint(hm_entries[0] + 0x15b0, 0); /* Hero 1 is in the party, freshly loaded at location 0. */
    hook_hamlet_location(world_campaign, hm_entries[0], 1);
    CHECK(hm_entries[0][0x15b0] == 1 && hm_native == 1 && !sent);
    /* A hero that is not in the party is a drag: it waits for the shared order. */
    hm_reset(DD_ROLE_HOST);
    hook_hamlet_location(world_campaign, hm_entries[1], 1);
    CHECK(hm_entries[1][0x15b0] == 0 && !hm_native);
    /* A hero in the party being taken out: also the order's work. */
    hm_reset(DD_ROLE_HOST);
    hook_hamlet_location(world_campaign, hm_entries[0], 0);
    CHECK(hm_entries[0][0x15b0] == 1 && !hm_native);
    /* Out of the party and sent back to free: nothing to wait for. */
    hm_reset(DD_ROLE_HOST);
    putint(hm_entries[1] + 0x15b0, 1);
    hook_hamlet_location(world_campaign, hm_entries[1], 0);
    CHECK(hm_entries[1][0x15b0] == 0 && hm_native == 1);
    puts("Hamlet bridge: the screen's own fill sets the location, a drag waits for its order");
}
static void hm_busy_remembered_party(void) {
    /* Real owner report: last-party IDs still contain a hero now in a building (location2).
     * Opening the expedition UI must be able to clear/replace that remembered slot. */
    hm_reset(DD_ROLE_HOST);
    putint(hm_entries[0] + 0x15b0, 2);
    hook_hamlet_party_slot(hm_slots[0], NULL);
    CHECK(sent == 1 && hm_party[0] == UINT32_MAX && hm_entries[0][0x15b0] == 2);
    dd_msg clear = last_message;
    hm_reset(DD_ROLE_GUEST);
    putint(hm_entries[0] + 0x15b0, 2);
    if (clear.type == DD_MSG_TOWN_APPLY) {
        CHECK(queue_push(&g_incoming, &clear));
        dd_runtime_poll();
    }
    CHECK(hm_party[0] == UINT32_MAX && hm_entries[0][0x15b0] == 2 && !g_town.needs_resync);

    hm_reset(DD_ROLE_GUEST);
    putint(hm_entries[0] + 0x15b0, 2);
    hook_hamlet_party_slot(hm_slots[0], hm_entries[1] + 8);
    CHECK(sent == 1 && !hm_native && hm_party[0] == 1);
    dd_msg request = last_message;
    hm_reset(DD_ROLE_HOST);
    putint(hm_entries[0] + 0x15b0, 2);
    if (request.type == DD_MSG_TOWN_REQUEST) {
        CHECK(queue_push(&g_incoming, &request));
        dd_runtime_poll();
    }
    CHECK(hm_party[0] == 2 && hm_entries[0][0x15b0] == 2 && hm_entries[1][0x15b0] == 1 && hm_party[0] == 2);
    dd_msg replace = last_message;
    hm_reset(DD_ROLE_GUEST);
    putint(hm_entries[0] + 0x15b0, 2);
    if (replace.type == DD_MSG_TOWN_APPLY) {
        CHECK(queue_push(&g_incoming, &replace));
        dd_runtime_poll();
    }
    CHECK(hm_party[0] == 2 && hm_entries[0][0x15b0] == 2 && hm_entries[1][0x15b0] == 1 && hm_party[0] == 2);

    /* A busy remembered party member must not stop dismissal of a DIFFERENT spare hero. */
    hm_reset(DD_ROLE_HOST);
    putint(hm_entries[0] + 0x15b0, 2);
    hook_hamlet_dismiss(hm_roster_panel, 2);
    CHECK(g_dismiss.active && hm_dismiss_event == 2 && !sent);
    if (hm_dismiss_event) hm_finish_dismiss();
    dd_runtime_poll();
    CHECK(sent == 1 && !g_dismiss.active && hm_entries[0][0x15b0] == 2);
    dd_msg dismiss = last_message;
    hm_reset(DD_ROLE_GUEST);
    putint(hm_entries[0] + 0x15b0, 2);
    if (dismiss.type == DD_MSG_TOWN_APPLY) {
        CHECK(queue_push(&g_incoming, &dismiss));
        dd_runtime_poll();
    }
    CHECK(g_dismiss.active && hm_dismiss_event == 2);
    if (hm_dismiss_event) hm_finish_dismiss();
    dd_runtime_poll();
    CHECK(!g_dismiss.active && g_town.applied_seq == 1 && hm_entries[0][0x15b0] == 2);
    for (unsigned role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        hm_reset((int)role);
        hook_hamlet_dismiss(hm_roster_panel, 1);
        CHECK(!hm_native && !sent && strstr(g_shop_notice, "party"));
        putint(hm_entries[2] + 0x15b0, 2);
        hook_hamlet_dismiss(hm_roster_panel, 3);
        CHECK(!hm_native && !sent && strstr(g_shop_notice, "building"));
    }
    puts("Remembered party: busy old member can be cleared/replaced without leaving building; "
         "spare dismissal completes on both copies; blocked dismissal explains why");
}
static void hm_deferred_dismissals(void) {
    /* Host-local case from the supplied logs: event first, later roster removal, THEN broadcast. */
    hm_reset(DD_ROLE_HOST);
    hook_hamlet_dismiss(hm_roster_panel, 2);
    CHECK(hm_native == 1 && hm_dismiss_event == 2 && !sent && g_dismiss.active);
    hook_hamlet_dismiss(hm_roster_panel, 2);
    CHECK(hm_native == 1 && !sent); /* Do not emit the native event twice. */
    CHECK(!hamlet_skill_input(hm_entries[0] + 8, 3, 0, 0) && hm_native == 1);
    dd_msg upgrade = {0};
    upgrade.type = DD_MSG_TOWN_REQUEST;
    upgrade.u.town.request_id = 2;
    upgrade.u.town.action.kind = DD_ACT_BUY_UPGRADE;
    upgrade.u.town.action.u.upgrade.tree = 123;
    upgrade.u.town.action.u.upgrade.code = 'a';
    upgrade.u.town.action.u.upgrade.hero_id = 1;
    CHECK(queue_push(&g_incoming, &upgrade));
    dd_runtime_poll();
    dd_runtime_poll();
    CHECK(hm_native == 1 && !sent && g_incoming.count == 1 && !g_town.accepted);
    hm_finish_dismiss();
    dd_runtime_poll();
    CHECK(!g_dismiss.active && hm_native == 2 && sent == 2 && g_town.applied_seq == 2);
    dd_msg dismissed = sent_messages[0], bought = sent_messages[1];
    CHECK(dismissed.type == DD_MSG_TOWN_APPLY && dismissed.seq == 1 && !dismissed.u.town.request_id &&
          dismissed.u.town.action.kind == DD_ACT_DISMISS && dismissed.u.town.action.u.hero.hero_id == 2);
    CHECK(bought.seq == 2 && bought.u.town.action.kind == DD_ACT_BUY_UPGRADE);
    dd_msg stale = dismissed;
    stale.type = DD_MSG_TOWN_REQUEST;
    stale.seq = 0;
    stale.u.town.request_id = 3;
    CHECK(queue_push(&g_incoming, &stale));
    dd_runtime_poll();
    CHECK(sent == 3 && g_town.rejected == 1 && hm_native == 2 && !hm_dismiss_event);

    hm_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming, &dismissed) && queue_push(&g_incoming, &bought));
    dd_runtime_poll();
    dd_runtime_poll();
    CHECK(hm_native == 1 && !g_town.applied_seq && g_incoming.count == 2 && !sent);
    hook_hamlet_party_slot(hm_slots[1], hm_entries[1] + 8);
    CHECK(hm_native == 1 && !sent && hm_party[1] == UINT32_MAX);
    hm_finish_dismiss();
    dd_runtime_poll();
    CHECK(hm_native == 2 && g_town.applied_seq == 2 && !g_incoming.count && !g_town.needs_resync);
    dd_runtime_poll();
    CHECK(hm_native == 2 && !hm_dismiss_event && !sent);
    CHECK(queue_push(&g_incoming, &dismissed));
    dd_runtime_poll();
    CHECK(g_town.needs_resync && hm_native == 2 && !hm_dismiss_event); /* No replay mutation. */

    hm_reset(DD_ROLE_GUEST);
    dismissed.seq = 2; /* Sequence gap must be detected BEFORE native side effects. */
    CHECK(queue_push(&g_incoming, &dismissed));
    dd_runtime_poll();
    CHECK(g_town.needs_resync && !hm_native && !hm_dismiss_event && !g_dismiss.active);

    hm_reset(DD_ROLE_HOST);
    hook_hamlet_dismiss(hm_roster_panel, 2);
    g_dismiss.until = 0;
    dd_runtime_poll();
    dd_runtime_poll();
    CHECK(g_town.needs_resync && !sent && !g_town.accepted && hm_native == 1 && alerts == 1);

    hm_reset(DD_ROLE_HOST);
    hook_hamlet_dismiss(hm_roster_panel, 2);
    hm_dismiss_event = 3; /* Wrong survivor removed: a matching count alone is insufficient. */
    hm_finish_dismiss();
    dd_runtime_poll();
    CHECK(g_town.needs_resync && !sent && hm_native == 1 && alerts == 1);

    hm_reset(DD_ROLE_HOST);
    hook_hamlet_dismiss(hm_roster_panel, 2);
    g_dismiss.campaign++;
    dd_runtime_poll();
    CHECK(g_town.needs_resync && !sent && hm_native == 1);

    hm_reset(DD_ROLE_HOST);
    hook_hamlet_dismiss(hm_roster_panel, 1); /* First remove the hero from the expedition party. */
    CHECK(!hm_native && !sent && !g_dismiss.active && !g_town.needs_resync);

    hm_reset(DD_ROLE_HOST);
    hook_hamlet_dismiss(hm_roster_panel, 2);
    g_session_lost = 1;
    g_session_ready = 0;
    dd_runtime_poll();
    hm_finish_dismiss();
    dd_runtime_poll();
    CHECK(g_town.needs_resync && !sent && hm_native == 1 && alerts == 1);
    puts("Deferred dismissal: host-local/guest-request authority, async guest apply, FIFO barrier, "
         "duplicate/gap refusal, exact survivors, timeout and disconnect verified");
}
static void hm_trinkets(void) {
    uint8_t source_ui[0x100] = {0}, target_ui[0x100] = {0}, drag[0x610] = {0};
    hm_reset(DD_ROLE_GUEST);
    putptr(source_ui + 0x28, (uintptr_t)(world_campaign + 0xe88));
    putptr(target_ui + 0x18, (uintptr_t)(hm_entries[0] + 8 + 0x1280));
    hook_trinket_pickup_inventory(source_ui, NULL, NULL, drag);
    CHECK(g_trinket_drag.valid);
    CHECK(!hook_trinket_drop_hero(target_ui, NULL, drag));
    CHECK(sent == 1 && !hm_native && hm_estate[0][0] == 1 && !hm_gear[0][0][0]);
    dd_msg request = last_message;
    hook_trinket_cancel_inventory(source_ui, drag);
    CHECK(g_trinket_drag.valid == 2 && !hm_native && source_ui[0xc] == 1);
    hook_trinket_finish_inventory(source_ui, drag);
    CHECK(!hm_native); /* A repeated native completion cannot restore its stale drag copy. */
    dd_msg apply = hm_host_apply(request);
    CHECK(hm_native == 3 && !hm_item_alive && hm_refresh == 1 && !hm_estate[0][0] && hm_gear[0][0][0]);
    request.u.town.request_id = 2;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && hm_native == 3);
    hm_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(hm_native == 3 && hm_refresh == 1 && !hm_item_alive);
    /* Occupied estate slots swap order with no change in item counts; each side uses its own objects. */
    hm_reset(DD_ROLE_HOST);
    dd_action action = {0};
    action.kind = DD_ACT_TRINKET_MOVE;
    action.u.move.src_slot = 0;
    action.u.move.dst_slot = 1;
    action.u.move.src_type = action.u.move.dst_type = 3950276909u;
    action.u.move.src_id = 101;
    action.u.move.dst_id = 102;
    CHECK(dd_town_host_local(&g_town, &action) && hm_estate[0][0x88] == 102 && hm_estate[1][0x88] == 101 &&
          !hm_refresh && !hm_item_alive);
    /* Unequip back into an empty estate slot. */
    hm_reset(DD_ROLE_HOST);
    memcpy(hm_gear[0][0], hm_estate[0], 0x5a8);
    memset(hm_estate[0], 0, 0x5a8);
    action.u.move.src_hero = 1;
    action.u.move.dst_slot = 2;
    action.u.move.dst_type = action.u.move.dst_id = 0;
    CHECK(dd_town_host_local(&g_town, &action) && !hm_gear[0][0][0] && hm_estate[2][0x88] == 101 &&
          hm_refresh == 1);
    hm_reset(DD_ROLE_HOST);
    action = request.u.town.action;
    uint32_t only_other_class = 456;
    putptr(hm_definitions[0] + 0x2d8, (uintptr_t)&only_other_class);
    putptr(hm_definitions[0] + 0x2e0, (uintptr_t)(&only_other_class + 1));
    CHECK(!dd_town_host_local(&g_town, &action) && !hm_native);
    hm_reset(DD_ROLE_HOST);
    memcpy(hm_gear[0][1], hm_estate[0], 0x5a8);
    CHECK(!dd_town_host_local(&g_town, &action) && !hm_native);
    hm_reset(DD_ROLE_HOST);
    hm_broken_swap = 1;
    CHECK(!dd_town_host_local(&g_town, &action) && g_town.needs_resync && !hm_item_alive);
    hm_reset(DD_ROLE_HOST);
    hook_trinket_sort(NULL);
    hook_trinket_unequip_all(NULL);
    CHECK(!hm_native && g_shop_notice[0]);
    g_cfg.role = DD_ROLE_OFF;
    hook_trinket_sort(NULL);
    CHECK(hm_native == 1);
    hm_reset(DD_ROLE_GUEST);
    putptr(source_ui + 0x30, (uintptr_t)hm_estate[0]);
    putint(hm_estate[0], 2);
    hook_trinket_pickup_inventory(source_ui, NULL, NULL, drag);
    CHECK(g_trinket_drag.valid == 3 && !sent && !hm_native);
    CHECK(!hook_trinket_drop_hero(target_ui, NULL, drag));
    hook_trinket_cancel_inventory(source_ui, drag);
    CHECK(hm_estate[0][0] == 2 && !sent && !hm_native);
    puts("Hamlet trinkets: guest drag/cancel waits; native equip, unequip and occupied-slot reorder; stale "
         "IDs, class/duplicate restrictions and partial failure verified");
}
static uint8_t hm_raid[0x5500], hm_raid_loot[3][0x5a8], hm_raid_bag[4][0x5a8];
static void hm_raid_reset(int role) {
    hm_reset(role);
    memset(hm_raid, 0, sizeof hm_raid);
    memset(hm_raid_loot, 0, sizeof hm_raid_loot);
    memset(hm_raid_bag, 0, sizeof hm_raid_bag);
    putptr(sale_image + 0x118e2c8, (uintptr_t)hm_raid);
    putptr(hm_raid + 0x5248, (uintptr_t)hm_raid_loot);
    putptr(hm_raid + 0x5250, (uintptr_t)(hm_raid_loot + 3));
    putptr(hm_raid + 0x220, (uintptr_t)hm_raid_bag);
    putptr(hm_raid + 0x228, (uintptr_t)(hm_raid_bag + 4));
    memcpy(hm_raid_loot[2], hm_estate[0], 0x5a8);
    memcpy(hm_raid_loot[0], hm_estate[1], 0x5a8);
    putint(hm_raid + 0x5264, 2);
}
static uint8_t hm_display[0x6000];
static int hm_raid_view(walk_view *v) {
    memset(v, 0, sizeof *v);
    v->raid = (uintptr_t)hm_raid;
    v->display = (uintptr_t)hm_display;
    v->state = 2;
    return 1;
}
static void hm_loot_window(uint32_t state) {
    memcpy(hm_display + DD_WALK_LOOT_OVERLAY, &state, 4);
    hm_display[DD_WALK_LOOT_OVERLAY + 0x170] = state == 2;
}
static void hm_secret_room_trinkets(void) {
    uint8_t source[0x100] = {0}, target[0x100] = {0}, drag[0x610] = {0};
    int (*normal_view)(walk_view *) = g_walk_view;
    g_walk_view = hm_raid_view;
    memset(hm_display, 0, sizeof hm_display);
    hm_loot_window(2);
    hm_raid_reset(DD_ROLE_GUEST);
    putptr(source + 0x28, (uintptr_t)(hm_raid + 0x5240));
    putptr(source + 0x30, (uintptr_t)hm_raid_loot[2]);
    putint(source + 0xb8, 2);
    putptr(target + 0x18, (uintptr_t)(hm_entries[0] + 8 + 0x1280));
    putint(target + 0xa8, 1);
    hook_trinket_pickup_inventory(source, NULL, NULL, drag);
    CHECK(g_trinket_drag.valid == 1 && !g_shop_notice[0]);
    CHECK(!hook_trinket_drop_hero(target, NULL, drag));
    CHECK(sent == 1 && last_message.u.town.action.u.move.src_hero == DD_TRINKET_LOOT && !hm_native);
    dd_msg request = last_message;
    hook_trinket_cancel_inventory(source, drag);
    CHECK(hm_raid_loot[2][0] == 1 && source[0xc] == 1);
    hm_raid_reset(DD_ROLE_HOST);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.accepted == 1 && !g_town.needs_resync && hm_native == 3 && !hm_item_alive && hm_refresh == 1);
    CHECK(!hm_raid_loot[2][0] && hm_gear[0][1][0x88] == 101 && hm_raid_loot[0][0x88] == 102);
    dd_msg apply = last_message;
    hm_raid_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(!hm_raid_loot[2][0] && hm_gear[0][1][0x88] == 101 && hm_raid_loot[0][0x88] == 102 && !hm_item_alive);
    CHECK(g_town.applied_seq == 1 && !g_town.needs_resync);
    /* The host was quicker: this copy's chest has not given its loot yet. The move waits for it, and then runs. */
    hm_raid_reset(DD_ROLE_GUEST);
    hm_loot_window(1);
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(g_incoming.count == 1 && hm_raid_loot[2][0] == 1 && !g_town.needs_resync);
    hm_loot_window(2);
    dd_runtime_poll();
    CHECK(!g_incoming.count && !hm_raid_loot[2][0] && hm_gear[0][1][0x88] == 101 && g_town.applied_seq == 1);
    /* It never comes, or holds something else: told, and the expedition goes on. */
    hm_raid_reset(DD_ROLE_GUEST);
    memset(hm_raid_loot, 0, sizeof hm_raid_loot);
    {
        int before = alerts;
        CHECK(queue_push(&g_incoming, &apply));
        dd_runtime_poll();
        CHECK(!g_incoming.count && g_town.applied_seq == 1 && !g_town.needs_resync && alerts == before + 1);
    }
    /* On the host a refusal stays a refusal. */
    hm_raid_reset(DD_ROLE_HOST);
    memset(hm_raid_loot, 0, sizeof hm_raid_loot);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && !g_town.accepted && !g_town.needs_resync);
    /* Between heroes in town the rule stays: a move that fails on the guest stops shared actions. */
    {
        dd_msg town = apply;
        town.u.town.action.u.move.src_hero = 1;
        town.u.town.action.u.move.src_slot = 0;
        CHECK(dd_action_valid(&town.u.town.action));
        hm_raid_reset(DD_ROLE_GUEST);
        memset(hm_gear, 0, sizeof hm_gear);
        CHECK(queue_push(&g_incoming, &town));
        dd_runtime_poll();
        CHECK(g_town.needs_resync);
    }
    g_walk_view = normal_view;
    hm_reset(DD_ROLE_HOST);
    putptr(sale_image + 0x118e2c8, 0);
    puts("Secret-room trinkets: loot slot to hero uses native Item lifetime on host and guest; second head stays in the chest");
}
static void hamlet_transactions(void) {
    hm_names_and_skills();
    hm_upgrades_and_refresh();
    hm_exchanges_and_dismiss();
    hm_parties();
    hm_deferred_dismissals();
    hm_party_screen_fill();
    hm_party_show();
    hm_busy_remembered_party();
    hm_dismiss_hidden();
    hm_town_check();
    hm_alone();
    hm_party_panel_wait();
    hm_trinkets();
    hm_secret_room_trinkets();
    hm_connection_loss();
}
