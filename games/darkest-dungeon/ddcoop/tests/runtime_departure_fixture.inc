/* Uses the real departure hooks and FIFO. Only the game's start parameters, map generator and rand_s are faked. */
static uint8_t dp_app[0x2000], dp_params[0x300], dp_quest[0x60], dp_heroes[4][0x1400];
static uint64_t dp_party[4];
static unsigned dp_native_calls, dp_maps, dp_starts, dp_reseeds;
static uint32_t dp_rules_seed;
static uint8_t dp_rules[0x30];
static void __fastcall dp_reseed(void *object, uint32_t seed) {
    CHECK(object == dp_rules && dp_maps == 0 && dp_starts == 0); /* Before RaidStart itself runs. */
    dp_reseeds++;
    dp_rules_seed = seed;
}
static unsigned dp_map_seed[4];
static int __cdecl dp_rand_s(unsigned int *out) {
    dp_native_calls++;
    *out = 0x80000000u + 1000u + dp_native_calls;
    return 0;
}
static uintptr_t __fastcall dp_map(void *a, void *b, void *c) {
    (void)a;
    (void)b;
    (void)c;
    unsigned value = 0;
    hook_rand_s(&value);
    if (dp_maps < 4) dp_map_seed[dp_maps] = value;
    dp_maps++;
    return 7;
}
static uintptr_t __fastcall dp_start(void *app) {
    CHECK(app == dp_app);
    dp_starts++;
    return hook_raid_map(NULL, NULL, NULL);
}
static void dp_world(uint32_t quest, uint32_t first_hero) {
    uint64_t params = (uintptr_t)dp_params, questp = (uintptr_t)dp_quest;
    uint64_t begin = (uintptr_t)dp_party, end = (uintptr_t)(dp_party + 4);
    memcpy(dp_app + 0x1720, &params, 8);
    memcpy(dp_params, &questp, 8);
    memcpy(dp_quest + 0x40, &quest, 4);
    memcpy(dp_params + 0x208, &begin, 8);
    memcpy(dp_params + 0x210, &end, 8);
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t id = first_hero + i;
        dp_party[i] = (uintptr_t)dp_heroes[i];
        memcpy(dp_heroes[i] + 0x137c, &id, 4);
    }
}
/* The town: only reading the game's state and its departure call are faked. */
static uint8_t dp_town[0x10], dp_campaign[0x1400];
static __declspec(align(16)) uint8_t dp_quests[3][0x2f0];
static unsigned dp_departs, dp_party_calls;
static int dp_view_fail, dp_party_ok, dp_in_raid;
static int dp_view(depart_view *v) {
    memset(v, 0, sizeof *v);
    if (dp_view_fail) return 0;
    v->town = (uintptr_t)dp_town;
    v->raid = dp_in_raid ? 1 : 0;
    v->campaign = (uintptr_t)dp_campaign;
    memcpy(&v->quest, dp_campaign + 0x1248, 4);
    (void)depart_quest_key(v->campaign, v->quest, &v->key);
    return 1;
}
static void __fastcall dp_depart(void) { dp_departs++; }
static void dp_town_world(void) {
    uint64_t begin = (uintptr_t)dp_quests, end = (uintptr_t)(dp_quests + 3);
    int32_t none = -1, selected = 1;
    memset(dp_quests, 0, sizeof dp_quests);
    memset(dp_campaign, 0, sizeof dp_campaign);
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t key = 0xa000u + i;
        memcpy(dp_quests[i] + 0x40, &key, 4);
    }
    memcpy(dp_campaign + 0x1230, &begin, 8);
    memcpy(dp_campaign + 0x1238, &end, 8);
    memcpy(dp_campaign + 0x1220, &none, 4);
    memcpy(dp_campaign + 0x1248, &selected, 4);
    dp_departs = dp_party_calls = 0;
    dp_view_fail = dp_in_raid = 0;
    dp_party_ok = 1;
    g_depart_view = dp_view;
    g_town_depart = dp_depart;
}
static int dp_party_prepare(void) {
    dp_party_calls++;
    return dp_party_ok;
}
static int dp_clock(float *out) {
    *out = 100.0f;
    return 1;
}
static int32_t dp_selected(void) {
    int32_t value = 0;
    memcpy(&value, dp_campaign + 0x1248, 4);
    return value;
}
static void dp_reset(int role) {
    sw_reset(role);
    departure_reset();
    dp_town_world();
    g_depart_party = dp_party_prepare;
    dp_native_calls = dp_maps = dp_starts = dp_reseeds = 0;
    dp_rules_seed = 0;
    g_rules_object = dp_rules;
    g_rules_reseed = dp_reseed;
    memset(dp_map_seed, 0, sizeof dp_map_seed);
    g_rand_s_native = dp_rand_s;
    g_raid_start = dp_start;
    g_raid_map = dp_map;
    dp_world(0x51515151, 8);
}
static void departure_transactions(void) {
    /* The host picks one seed, sends it, and its own map uses it. rand_s stays native outside the map. */
    dp_reset(DD_ROLE_HOST);
    hook_raid_start(dp_app);
    CHECK(dp_starts == 1 && dp_maps == 1 && dp_native_calls == 2 && sent == 1);
    CHECK(last_message.type == DD_MSG_TOWN_APPLY && last_message.seq == 1);
    dd_action order = last_message.u.town.action;
    CHECK(!memcmp(g_hero_owner_ids, order.u.departure.heroes, sizeof g_hero_owner_ids) && g_hero_owner_mask == order.u.departure.owners);
    CHECK(order.kind == DD_ACT_DEPARTURE && order.u.departure.seed == dp_map_seed[0] && order.u.departure.seed <= 0x7fffffffu);
    CHECK(order.u.departure.quest == 0x51515151 && order.u.departure.heroes[0] == 8 && order.u.departure.heroes[3] == 11);
    CHECK(!g_departure_seed && !g_departure_rules && !g_departure_forcing && !g_town.needs_resync);
    CHECK(dp_reseeds == 1 && dp_rules_seed == order.u.departure.rules && order.u.departure.rules <= 0x7fffffffu);
    CHECK(order.u.departure.rules != order.u.departure.seed);
    unsigned plain = 0;
    hook_rand_s(&plain);
    CHECK(dp_native_calls == 3 && plain != order.u.departure.seed);
    /* The order survives the wire. */
    {
        dd_msg input = last_message, decoded = {0};
        uint8_t wire[512];
        size_t used = 0, len = dd_encode(&input, wire, sizeof wire);
        CHECK(len && dd_decode(wire, len, &decoded, &used) == DD_DECODE_OK && used == len);
        CHECK(decoded.u.town.action.u.departure.seed == order.u.departure.seed);
        CHECK(decoded.u.town.action.u.departure.rules == order.u.departure.rules);
    }
    /* Each embark gets its own seed. */
    dp_starts = dp_maps = 0;
    hook_raid_start(dp_app);
    CHECK(sent == 2 && last_message.u.town.action.u.departure.seed != order.u.departure.seed);

    /* The guest stores the order and its next map uses exactly that seed, once. */
    dp_reset(DD_ROLE_GUEST);
    sw_push_apply(1, 0, &order);
    dd_runtime_poll(); /* Still in the Hamlet: opens the first screen, then the next pass leaves. */
    dd_runtime_poll();
    CHECK(!memcmp(g_hero_owner_ids, order.u.departure.heroes, sizeof g_hero_owner_ids));
    CHECK(g_departure_seed == order.u.departure.seed && g_town.applied_seq == 1 && !alerts && !dp_maps);
    /* It leaves by itself, once, with its party prepared. */
    CHECK(dp_departs == 1 && dp_party_calls == 1);
    hook_raid_start(dp_app);
    CHECK(dp_maps == 1 && dp_map_seed[0] == order.u.departure.seed && !dp_native_calls && !sent && !g_departure_seed);
    CHECK(dp_reseeds == 1 && dp_rules_seed == order.u.departure.rules && !g_departure_rules);
    dp_starts = dp_maps = 0;
    hook_raid_start(dp_app);
    CHECK(dp_maps == 1 && dp_map_seed[0] != order.u.departure.seed && dp_native_calls == 1 && dp_reseeds == 1);
    /* A different mission/party may not start even with an approved seed. */
    dp_reset(DD_ROLE_GUEST);
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    dd_runtime_poll();
    dp_world(0x62626262, 20);
    hook_raid_start(dp_app);
    CHECK(!dp_starts && !dp_maps && g_town.needs_resync);
    /* A guest embarking first has no seed, and sends nothing. */
    dp_reset(DD_ROLE_GUEST);
    hook_raid_start(dp_app);
    CHECK(dp_maps == 1 && dp_native_calls == 1 && !sent && !dp_reseeds);
    /* Without a session: the game alone. */
    dp_reset(DD_ROLE_OFF);
    hook_raid_start(dp_app);
    CHECK(dp_maps == 1 && dp_native_calls == 1 && !sent && !dp_reseeds);
    /* A raid loaded from the save has no start parameters: both copies seed Rules with the same value from the session's
     * password, before RaidStart runs, and send nothing. Another session, another value; without a session, nothing. */
    {
        uint32_t seen[2] = {0};
        for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
            uint64_t none = 0;
            dp_reset(role);
            memcpy(dp_app + 0x1720, &none, 8);
            hook_raid_start(dp_app);
            CHECK(dp_reseeds == 1 && dp_rules_seed && dp_rules_seed <= 0x7fffffffu && !sent && !g_departure_rules);
            CHECK(dp_maps == 1 && dp_native_calls == 1); /* The map of a loaded raid is not generated from a shared seed. */
            seen[role - DD_ROLE_HOST] = dp_rules_seed;
        }
        CHECK(seen[0] == seen[1]);
        {
            uint64_t none = 0;
            dp_reset(DD_ROLE_GUEST);
            strcpy(g_cfg.password, "another-session");
            memcpy(dp_app + 0x1720, &none, 8);
            hook_raid_start(dp_app);
            CHECK(dp_reseeds == 1 && dp_rules_seed != seen[0]);
            dp_reset(DD_ROLE_OFF);
            memcpy(dp_app + 0x1720, &none, 8);
            hook_raid_start(dp_app);
            CHECK(!dp_reseeds);
        }
    }
    /* A Rules stream that was not recognised: the map seed still works, the stream is left alone. */
    dp_reset(DD_ROLE_HOST);
    g_rules_reseed = NULL;
    hook_raid_start(dp_app);
    CHECK(dp_maps == 1 && sent == 1 && !dp_reseeds);
    /* Leaving for the raid ends the shop, and what the expedition screen was still to show. */
    dp_reset(DD_ROLE_HOST);
    g_supply_bg = g_supply_quest_known = 1;
    g_party_pending_mask = 3;
    hook_raid_start(dp_app);
    CHECK(!g_supply_bg && !g_supply_quest_known && !g_party_pending_mask);
    /* The shop's state goes with the departure from a copy that only has it in its model too. */
    dp_reset(DD_ROLE_HOST);
    sw_show(0);
    g_supply_bg = 1;
    g_departure_poll_at = 0;
    departure_poll();
    CHECK(g_departure_open_supplies != 0);
    g_supply_bg = 0;
    /* Supplies are remembered while the panel is open, and the last open state goes with the departure. */
    dp_reset(DD_ROLE_HOST);
    g_departure_poll_at = 0;
    departure_poll();
    {
        uint32_t remembered = g_departure_open_supplies;
        CHECK(remembered != 0);
        sw_show(0);
        *sw_gold() = 500; /* Closed panel: not this state. */
        g_departure_poll_at = 0;
        departure_poll();
        CHECK(g_departure_open_supplies == remembered);
        sw_open_fail = 1; /* What RaidStart really sees: the model is gone. */
        hook_raid_start(dp_app);
        CHECK(sent == 1 && last_message.u.town.action.u.departure.supplies == remembered);
        sw_open_fail = 0;
    }
    /* A host cannot be told a departure. */
    dp_reset(DD_ROLE_HOST);
    {
        dd_msg r = {0};
        r.type = DD_MSG_TOWN_REQUEST;
        r.u.town.request_id = 3;
        r.u.town.action = order;
        CHECK(queue_push(&g_incoming, &r));
    }
    dd_runtime_poll();
    CHECK(last_message.type == DD_MSG_TOWN_REJECT && !g_departure_seed && !g_town.needs_resync);
    /* Unreadable start parameters: nothing shared, the game continues; as for a raid loaded from the save, Rules gets the
     * session's seed. */
    dp_reset(DD_ROLE_HOST);
    memset(dp_app + 0x1720, 0, 8);
    hook_raid_start(dp_app);
    CHECK(dp_maps == 1 && !sent && dp_native_calls == 1 && dp_reseeds == 1);
    /* An import slot that could not be bound: no sharing. */
    dp_reset(DD_ROLE_HOST);
    g_rand_s_native = NULL;
    g_raid_map = NULL;
    g_raid_start = NULL;
    departure_view v;
    CHECK(departure_read(dp_app, &v) && v.quest == 0x51515151 && v.heroes[1] == 9);
    departure_begin(dp_app);
    CHECK(!sent && !g_departure_seed);
    /* Oversized party vector: refused, not read. */
    {
        uint64_t end = (uintptr_t)(dp_party + 5);
        memcpy(dp_params + 0x210, &end, 8);
        CHECK(!departure_read(dp_app, &v));
    }
    /* The host names the mission; the guest takes it. */
    dp_reset(DD_ROLE_HOST);
    dp_world(0xa001, 8);
    hook_raid_start(dp_app);
    CHECK(sent == 1 && last_message.u.town.action.u.departure.quest == 0xa001 && last_message.u.town.action.u.departure.quest_index == 1);
    {
        dd_action leave = last_message.u.town.action;
        dp_reset(DD_ROLE_GUEST);
        { int32_t other = 0; memcpy(dp_campaign + 0x1248, &other, 4); }
        sw_push_apply(1, 0, &leave);
        dd_runtime_poll();
        CHECK(dp_selected() == 1 && dp_departs == 1 && dp_party_calls == 1 && !g_town.needs_resync);
        /* Another mission at that place: drift, and it stays in the Hamlet. */
        dp_reset(DD_ROLE_GUEST);
        { uint32_t other = 0xbeef; memcpy(dp_quests[1] + 0x40, &other, 4); }
        sw_push_apply(1, 0, &leave);
        dd_runtime_poll();
        CHECK(!dp_departs && g_town.needs_resync && alerts);
        /* A party that cannot be prepared would give the empty list: stays too. */
        dp_reset(DD_ROLE_GUEST);
        dp_party_ok = 0;
        sw_push_apply(1, 0, &leave);
        dd_runtime_poll();
        CHECK(!dp_departs && g_town.needs_resync && alerts);
        /* Not in the Hamlet, or the host's other mission unknown: told, nothing started. */
        dp_reset(DD_ROLE_GUEST);
        dp_in_raid = 1;
        sw_push_apply(1, 0, &leave);
        dd_runtime_poll();
        CHECK(!dp_departs && !dp_party_calls && alerts == 1 && g_departure_seed == leave.u.departure.seed && !g_town.needs_resync);
        dp_reset(DD_ROLE_GUEST);
        dp_view_fail = 1;
        sw_push_apply(1, 0, &leave);
        dd_runtime_poll();
        CHECK(!dp_departs && alerts == 1 && !g_town.needs_resync);
    }
    /* The mission lookup, on its bounds, and the single quest of +12c0. */
    dp_reset(DD_ROLE_HOST);
    {
        uint32_t key = 0, single_key = 0xc0de;
        int32_t single = 9;
        CHECK(!depart_quest_key((uintptr_t)dp_campaign, 3, &key) && !key);
        memcpy(dp_campaign + 0x1220, &single, 4);
        memcpy(dp_campaign + 0x12c0 + 0x40, &single_key, 4);
        CHECK(depart_quest_key((uintptr_t)dp_campaign, 9, &key) && key == single_key);
    }
    /* The party on a copy that never opened the screen. */
    hm_reset(DD_ROLE_GUEST);
    g_depart_clock = dp_clock;
    g_depart_party = depart_party;
    hm_party[0] = 2;  /* Rank 0: hero 2, free. */
    hm_party[1] = UINT32_MAX;
    hm_party[2] = 1;  /* Rank 2: hero 1, already at location 1. */
    putint(hm_entries[2] + 0x15b0, 1); /* Hero 3: at location 1 but not in the party. */
    CHECK(depart_party());
    CHECK(hm_entries[1][0x15b0] == 1 && hm_entries[0][0x15b0] == 1 && hm_entries[2][0x15b0] == 0);
    {
        float rank0, rank2;
        memcpy(&rank0, hm_entries[1] + 0x15b8, 4);
        memcpy(&rank2, hm_entries[0] + 0x15b8, 4);
        CHECK(rank0 == 104.0f && rank2 == 102.0f); /* Lowest comes first: the last of the vector leads the list, as on the host. */
    }
    /* A hero in a building is not moved out of it. */
    hm_reset(DD_ROLE_GUEST);
    putint(hm_entries[2] + 0x15b0, 2);
    CHECK(depart_party() && hm_entries[2][0x15b0] == 2);
    g_depart_clock = depart_clock;
#ifdef DD_SOLO_MAP_SEED
    /* Solo test build: host without a guest; embarks 1 and 2 share a seed, the third differs. Sends nothing. */
    {
        char path[MAX_PATH], *slash;
        GetModuleFileNameA(NULL, path, sizeof path);
        slash = strrchr(path, '\\');
        strcpy(slash + 1, "ddcoop-solo-rules-run.txt");
        DeleteFileA(path);
        for (unsigned i = 0; i < 3; i++) {
            dp_reset(DD_ROLE_HOST);
            g_session_ready = 0;
            hook_raid_start(dp_app);
            CHECK(dp_maps == 1 && !sent && !dp_native_calls && !g_departure_seed && dp_reseeds == 1);
            CHECK(dp_map_seed[0] == (uint32_t)DD_SOLO_MAP_SEED);
            CHECK(dp_rules_seed == (uint32_t)DD_SOLO_MAP_SEED + (i == 2 ? 1000u : 0u));
        }
        DeleteFileA(path);
        puts("Solo seed: map always equal; Rules on embarks 1 and 2 equal, 3 different");
    }
#endif
    departure_reset();
    g_depart_view = depart_read;
    g_town_depart = NULL;
    puts("Departure bridge: one host map and Rules seed per embark, same seeds on the guest once, rand_s native "
         "elsewhere, supplies from the open panel, no party or no session shares nothing");
}
