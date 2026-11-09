/* The real start-screen logic against a pretend screen. Whether the game takes it this way can only be seen in the game. */
static uint8_t fe_screen[0x2800], fe_slots[10 * 0xc10];
static unsigned fe_starts, fe_loads, fe_news;
static int32_t fe_loaded, fe_newed;
static uint64_t fe_start_screen;
static unsigned fe_transports;
static unsigned fe_prepares;
static int fe_prepare_ok, fe_seed_armed, fe_fast_resume;
static int fe_prepare_stub(void) {
    CHECK(!fe_loads); /* Bind before the native load can enter RaidStart. */
    fe_prepares++;
    fe_seed_armed = fe_prepare_ok;
    return fe_prepare_ok;
}
static void fe_transport_stub(void) { fe_transports++; }
/* What this copy told the other one: the slot of each DD_MSG_LOAD waiting to be sent, newest last. */
static unsigned fe_loads_sent(uint32_t *last) {
    dd_msg m;
    unsigned n = 0;
    while (queue_pop(&g_outgoing, &m))
        if (m.type == DD_MSG_LOAD) {
            n++;
            if (last) *last = m.u.load.slot;
        }
    return n;
}
static uintptr_t fe_image(void) { return (uintptr_t)sale_image; }
static void __fastcall fe_start_stub(void *functor) {
    fe_starts++;
    memcpy(&fe_start_screen, (uint8_t *)functor + 8, 8);
}
static void __fastcall fe_load_stub(void *screen, int32_t slot) {
    CHECK(screen == fe_screen);
    fe_loads++;
    fe_loaded = slot;
    if (fe_fast_resume) {
        CHECK(!g_campaign_active && !g_hooks_installed && fe_seed_armed);
        if (fe_seed_armed) hook_raid_start(dp_app);
        else dp_start(dp_app); /* A fast native load overtakes late town hook installation. */
        CHECK(dp_reseeds == 1 && dp_rules_seed && dp_starts == 1);
    }
}
static void __fastcall fe_new_stub(void *screen, int32_t slot) {
    CHECK(screen == fe_screen);
    fe_news++;
    fe_newed = slot;
}
static void fe_state(int32_t state) { memcpy(fe_screen + DD_FE_STATE, &state, 4); }
static void fe_mark(int slot, uint8_t has, uint8_t loadable) {
    fe_slots[(slot - 1) * 0xc10 + 0x145] = has;
    fe_slots[(slot - 1) * 0xc10 + 0x146] = loadable;
}
static void fe_show(int on) { uint64_t p = on ? (uintptr_t)fe_screen : 0; memcpy(sale_image + DD_FE_OBJECT, &p, 8); }
static void fe_mode_set(int32_t mode) { memcpy(sale_image + DD_FE_START_MODE, &mode, 4); }
static int32_t fe_mode(void) { int32_t m = 0; memcpy(&m, sale_image + DD_FE_START_MODE, 4); return m; }
static void fe_settle(void) { g_fe_state_at = 1; }
static void fe_reset(int role, const char *slots) {
    uint64_t list = (uintptr_t)fe_slots;
    reset(role);
    frontend_reset();
    g_fe_image = fe_image;
    g_fe_start = fe_start_stub;
    g_fe_load = fe_load_stub;
    g_fe_new = fe_new_stub;
    g_frontend_transport = fe_transport_stub;
    g_frontend_departure_ready = fe_prepare_stub;
    fe_prepares = 0;
    fe_prepare_ok = 1;
    fe_seed_armed = fe_fast_resume = 0;
    g_departure_load_armed = 0;
    fe_transports = 0;
    /* The other copy already stands at its save screen for this session's slot, unless a test says otherwise. */
    InterlockedExchange(&g_peer_load_slot, slots[0] >= '1' && slots[0] <= '9' && !slots[1] ? slots[0] - '0' : 0);
    fe_news = 0;
    fe_newed = -1;
    snprintf(g_cfg.coop_slots, sizeof g_cfg.coop_slots, "%s", slots);
    InterlockedExchange(&g_campaign_active, 0);
    memset(fe_screen, 0, sizeof fe_screen);
    memset(fe_slots, 0, sizeof fe_slots);
    memset(sale_image + 0x2a6d4b0, 0, 0x140);
    memcpy(fe_screen + DD_FE_SLOTS, &list, 8);
    fe_show(0);
    fe_mode_set(0);
    fe_starts = fe_loads = 0;
    fe_loaded = -1;
    fe_start_screen = 0;
}
static void fast_restored_load_checks(void) {
    uint32_t seeds[2] = {0};
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        uint64_t none = 0;
        dp_reset(role);
        memcpy(dp_app + 0x1720, &none, 8);
        fe_reset(role, "3");
        g_hooks_installed = 0;
        fe_fast_resume = 1;
        fe_show(1); fe_mark(3, 1, 1); fe_state(7);
        frontend_poll(); fe_settle(); frontend_poll();
        CHECK(fe_loads == 1 && fe_prepares == 1 && !g_departure_load_armed);
        seeds[role - DD_ROLE_HOST] = dp_rules_seed;
        dd_runtime_poll(); /* Inactive slot-watch polling must not undo the reseed. */
        CHECK(dp_reseeds == 1 && dp_rules_seed == seeds[role - DD_ROLE_HOST]);
    }
    CHECK(seeds[0] && seeds[0] == seeds[1]);
    /* A town save has no RaidStart: do not leave an arm for a later unrelated load. */
    fe_reset(DD_ROLE_HOST, "3");
    fe_show(1); fe_mark(3, 1, 1); fe_state(7);
    frontend_poll(); fe_settle(); frontend_poll();
    CHECK(fe_loads == 1 && g_departure_load_armed);
    g_campaign_active = 1;
    frontend_poll();
    CHECK(!g_departure_load_armed);
    {
        uint64_t none = 0;
        dp_reset(DD_ROLE_HOST);
        memcpy(dp_app + 0x1720, &none, 8);
        g_campaign_active = 0;
        hook_raid_start(dp_app);
        CHECK(dp_starts == 1 && !dp_reseeds); /* Other inactive loads stay native. */
    }
    /* An unrecognized binding cannot advertise readiness or load the save. */
    fe_reset(DD_ROLE_HOST, "3");
    fe_prepare_ok = 0;
    fe_show(1); fe_mark(3, 1, 1); fe_state(7);
    frontend_poll(); fe_settle(); frontend_poll();
    CHECK(fe_prepares == 1 && !fe_loads && g_fe_done && !fe_loads_sent(NULL));
    /* No hook arming in a different slot, without a session, or in another mode. */
    fe_reset(DD_ROLE_HOST, "3");
    g_session_ready = 0;
    fe_show(1); fe_mark(3, 1, 1); fe_state(7);
    frontend_poll(); fe_settle(); frontend_poll();
    CHECK(!fe_prepares && !fe_loads);
    fe_reset(DD_ROLE_HOST, "3,4");
    frontend_poll();
    CHECK(!fe_prepares && !fe_loads);
    fe_reset(DD_ROLE_HOST, "3");
    sale_image[0x2a6d4b9] = 1;
    fe_show(1); fe_mark(3, 1, 1); fe_state(7);
    frontend_poll(); fe_settle(); frontend_poll();
    CHECK(!fe_prepares && !fe_loads);
    sale_image[0x2a6d4b9] = 0;
    puts("Fast restored load: RaidStart/Rules armed before save loading and slot-watch; same host/guest seed, bad binding refuses before advertising readiness");
}
static void frontend_transactions(void) {
    fast_restored_load_checks();
    /* A new campaign: no session, only this process's environment. The same way, then the empty slot's button. */
    fe_reset(DD_ROLE_OFF, "");
    g_fe_new_slot = 4;
    frontend_poll();
    CHECK(fe_mode() == 2 && !g_fe_done);
    fe_show(1);
    fe_state(5);
    frontend_poll();
    fe_settle();
    frontend_poll();
    CHECK(fe_starts == 1);
    fe_mark(4, 0, 1);
    fe_state(7);
    frontend_poll();
    fe_settle();
    frontend_poll();
    CHECK(fe_news == 1 && fe_newed == 3 && !fe_loads && g_fe_done);
    /* The tutorial was left halfway: it has a save now, to go on with. */
    fe_reset(DD_ROLE_OFF, "");
    g_fe_new_slot = 4;
    fe_show(1);
    fe_mark(4, 1, 1);
    fe_state(7);
    frontend_poll();
    fe_settle();
    frontend_poll();
    CHECK(fe_loads == 1 && fe_loaded == 3 && !fe_news);
    /* No button for it: nothing. */
    fe_reset(DD_ROLE_OFF, "");
    g_fe_new_slot = 4;
    fe_show(1);
    fe_state(7);
    frontend_poll();
    fe_settle();
    frontend_poll();
    CHECK(!fe_news && !fe_loads && g_fe_done);
    /* In a session an empty slot stays empty, also with that in the environment: the session's slot counts. */
    fe_reset(DD_ROLE_HOST, "2");
    g_fe_new_slot = 4;
    fe_show(1);
    fe_mark(2, 0, 1);
    fe_mark(4, 0, 1);
    fe_state(7);
    frontend_poll();
    fe_settle();
    frontend_poll();
    CHECK(!fe_news && !fe_loads && g_fe_done);
    /* The Circus stops this too. */
    fe_reset(DD_ROLE_OFF, "");
    g_fe_new_slot = 4;
    sale_image[0x2a6d5e0] = 1;
    fe_show(1);
    fe_mark(4, 0, 1);
    fe_state(7);
    frontend_poll();
    fe_settle();
    frontend_poll();
    CHECK(!fe_news && g_fe_done);
    sale_image[0x2a6d5e0] = 0;
    /* What the environment may say. */
    {
        static const struct { const char *value; int slot; } env[] = {{"4", 4}, {"1", 1}, {"9", 9}, {"0", 0}, {"10", 0}, {"x", 0}, {"", 0}, {NULL, 0}};
        for (size_t i = 0; i < sizeof env / sizeof env[0]; i++) {
            SetEnvironmentVariableA("DDCOOP_NEW_SLOT", env[i].value);
            frontend_read_new_slot();
            CHECK(g_fe_new_slot == env[i].slot);
        }
    }
    /* From the startup thread, before the frame poll: the same, once, and only before the screen exists. */
    {
        dd_config early;
        memset(&early, 0, sizeof early);
        early.role = DD_ROLE_GUEST;
        snprintf(early.coop_slots, sizeof early.coop_slots, "2");
        fe_reset(DD_ROLE_OFF, "");
        dd_runtime_frontend_early(&early);
        CHECK(fe_mode() == 2 && g_fe_mode_done);
        fe_mode_set(0);
        g_cfg.role = DD_ROLE_GUEST;
        snprintf(g_cfg.coop_slots, sizeof g_cfg.coop_slots, "2");
        frontend_poll();
        CHECK(!fe_mode());
        fe_reset(DD_ROLE_OFF, "");
        fe_show(1);
        dd_runtime_frontend_early(&early);
        CHECK(!fe_mode());
        /* On its way to the Circus: nothing. */
        fe_reset(DD_ROLE_OFF, "");
        sale_image[0x2a6d4b9] = 1;
        dd_runtime_frontend_early(&early);
        CHECK(!fe_mode());
        sale_image[0x2a6d4b9] = 0;
        /* Started normally: nothing. */
        fe_reset(DD_ROLE_OFF, "");
        early.role = DD_ROLE_OFF;
        dd_runtime_frontend_early(&early);
        CHECK(!fe_mode());
        SetEnvironmentVariableA("DDCOOP_NEW_SLOT", "4");
        dd_runtime_frontend_early(&early);
        CHECK(fe_mode() == 2);
        SetEnvironmentVariableA("DDCOOP_NEW_SLOT", NULL);
        g_fe_new_slot = 0;
    }
    /* The whole way, on both roles: the title first, the slot of the session after it, each once. */
    for (int r = 0; r < 2; r++) {
        fe_reset(r ? DD_ROLE_GUEST : DD_ROLE_HOST, "2");
        frontend_poll();
        CHECK(fe_mode() == 2 && !g_fe_done);
        fe_mode_set(0); /* Only once. */
        frontend_poll();
        CHECK(!fe_mode());
        fe_show(1);
        fe_state(4);
        frontend_poll();
        fe_settle();
        frontend_poll();
        CHECK(!fe_starts);
        /* The title: not while it fades in. */
        fe_state(5);
        frontend_poll();
        CHECK(!fe_starts);
        fe_settle();
        frontend_poll();
        CHECK(fe_starts == 1 && fe_start_screen == (uintptr_t)fe_screen);
        /* A question of the game keeps it on the title: that stays for the player. */
        fe_settle();
        frontend_poll();
        CHECK(fe_starts == 1);
        fe_state(6);
        frontend_poll();
        fe_settle();
        frontend_poll();
        CHECK(fe_starts == 1 && !fe_loads);
        fe_mark(2, 1, 1);
        fe_state(7);
        frontend_poll();
        CHECK(!fe_loads);
        fe_settle();
        frontend_poll();
        CHECK(fe_loads == 1 && fe_loaded == 1 && g_fe_done);
        fe_settle();
        frontend_poll();
        CHECK(fe_loads == 1);
    }
    /* Together: in a session each copy waits at the row of saves until the other one stands there for the same slot. */
    {
        uint32_t last = 0;
        int before;
        fe_reset(DD_ROLE_HOST, "2");
        InterlockedExchange(&g_peer_load_slot, 0);
        fe_show(1);
        fe_mark(2, 1, 1);
        fe_state(7);
        frontend_poll();
        CHECK(fe_transports >= 1); /* The connection starts at the start screen already. */
        fe_settle();
        before = alerts;
        frontend_poll();
        CHECK(!fe_loads && !g_fe_done && alerts == before + 1 && fe_loads_sent(&last) == 1 && last == 2);
        /* Waiting is not the screen failing: each look moves the give-up time on. */
        CHECK(g_fe_give_up_at > GetTickCount64() + DD_FE_GIVE_UP_MS - 5000);
        g_fe_load_sent_at = 1; /* A second later: said again. */
        frontend_poll();
        CHECK(!fe_loads && !g_fe_done && fe_loads_sent(NULL) == 1);
        /* The other copy arrives: this one loads, once, and says so once more for the other one. */
        InterlockedExchange(&g_peer_load_slot, 2);
        frontend_poll();
        CHECK(fe_loads == 1 && fe_loaded == 1 && g_fe_done && fe_loads_sent(&last) == 1 && last == 2);
        frontend_poll();
        CHECK(fe_loads == 1);
        /* No connection yet: nothing sent, nothing loaded. */
        fe_reset(DD_ROLE_GUEST, "2");
        InterlockedExchange(&g_peer_load_slot, 0);
        InterlockedExchange(&g_session_ready, 0);
        fe_show(1);
        fe_mark(2, 1, 1);
        fe_state(7);
        frontend_poll();
        fe_settle();
        frontend_poll();
        CHECK(!fe_loads && !g_fe_done && !fe_loads_sent(NULL));
        InterlockedExchange(&g_session_ready, 1);
        InterlockedExchange(&g_peer_load_slot, 2);
        frontend_poll();
        CHECK(fe_loads == 1 && fe_loads_sent(NULL) >= 1);
        /* The other copy waits for another slot: neither loads; left to the players. */
        fe_reset(DD_ROLE_HOST, "2");
        InterlockedExchange(&g_peer_load_slot, 5);
        fe_show(1);
        fe_mark(2, 1, 1);
        fe_state(7);
        frontend_poll();
        fe_settle();
        frontend_poll();
        CHECK(!fe_loads && g_fe_done);
        /* The player clicks a slot while waiting: that wins, and the waiting ends. */
        fe_reset(DD_ROLE_HOST, "2");
        InterlockedExchange(&g_peer_load_slot, 0);
        fe_show(1);
        fe_mark(2, 1, 1);
        fe_state(7);
        frontend_poll();
        fe_settle();
        frontend_poll();
        CHECK(!fe_loads && g_fe_waiting);
        fe_state(8);
        frontend_poll();
        CHECK(!fe_loads && g_fe_done);
        /* The game alone for a new campaign does not wait and starts no connection. */
        fe_reset(DD_ROLE_OFF, "");
        g_fe_new_slot = 4;
        fe_show(1);
        fe_mark(4, 0, 1);
        fe_state(7);
        frontend_poll();
        fe_settle();
        frontend_poll();
        CHECK(fe_news == 1 && !fe_transports && !fe_loads_sent(NULL));
        g_fe_new_slot = 0;
        InterlockedExchange(&g_peer_load_slot, 0);
    }
    /* It runs from the game's frame, before anything of a campaign, and the two functions are in the table. */
    fe_reset(DD_ROLE_GUEST, "2");
    dd_runtime_poll();
    CHECK(fe_mode() == 2);
    {
        uint64_t start = 0, load = 0, fresh = 0;
        for (size_t i = 0; i < dd_address_count; i++) {
            if (!strcmp(dd_addresses[i].name, "fe_start_button")) start = dd_addresses[i].va;
            if (!strcmp(dd_addresses[i].name, "fe_load_slot")) load = dd_addresses[i].va;
            if (!strcmp(dd_addresses[i].name, "fe_new_slot")) fresh = dd_addresses[i].va;
        }
        CHECK(start == 0x140424150ULL && load == 0x14041c390ULL && fresh == 0x14041b330ULL);
    }
    /* A menu slot is its number minus one: slot 9 is index 8. The Circus's index 9 cannot be reached. */
    fe_reset(DD_ROLE_HOST, "9");
    fe_show(1);
    fe_mark(9, 1, 1);
    fe_state(7);
    frontend_poll();
    fe_settle();
    frontend_poll();
    CHECK(fe_loads == 1 && fe_loaded == 8);
    /* Outside a session: nothing at all. */
    fe_reset(DD_ROLE_OFF, "2");
    frontend_poll();
    CHECK(!fe_mode() && g_fe_done);
    /* Not exactly one slot, or nothing readable in it: nothing. */
    static const char *const odd[] = {"", "2,3", "0", "10", "x", "2,x", "2,99"};
    for (size_t i = 0; i < sizeof odd / sizeof odd[0]; i++) {
        fe_reset(DD_ROLE_HOST, odd[i]);
        frontend_poll();
        CHECK(!fe_mode() && g_fe_done);
    }
    /* A campaign is loaded already. */
    fe_reset(DD_ROLE_HOST, "2");
    InterlockedExchange(&g_campaign_active, 1);
    frontend_poll();
    CHECK(!fe_mode() && g_fe_done);
    InterlockedExchange(&g_campaign_active, 0);
    /* The game has another start mode of its own: not overwritten. */
    fe_reset(DD_ROLE_HOST, "2");
    fe_mode_set(1);
    frontend_poll();
    CHECK(fe_mode() == 1);
    /* Late: the screen is there before the first look. The mode is not touched any more, and the rest still works. */
    fe_reset(DD_ROLE_HOST, "2");
    fe_show(1);
    fe_state(0);
    frontend_poll();
    CHECK(!fe_mode() && !g_fe_done);
    fe_show(0);
    frontend_poll();
    CHECK(!fe_mode());
    /* Anything of the Butcher's Circus stops it, whatever comes after. */
    static const unsigned circus[] = {0x2a6d5e0, 0x2a6d5e8, 0x2a6d4b9, 0x2a6d4bb};
    for (size_t i = 0; i < sizeof circus / sizeof circus[0]; i++) {
        fe_reset(DD_ROLE_HOST, "2");
        fe_show(1);
        fe_mark(2, 1, 1);
        fe_state(7);
        sale_image[circus[i]] = 1;
        frontend_poll();
        fe_settle();
        frontend_poll();
        CHECK(!fe_loads && !fe_starts && g_fe_done);
        sale_image[circus[i]] = 0;
        fe_reset(DD_ROLE_HOST, "2");
        sale_image[circus[i]] = 1;
        frontend_poll();
        CHECK(!fe_mode());
        sale_image[circus[i]] = 0;
    }
    /* An empty slot, or one that the game offers no load for: the row of saves stays, for the player. */
    fe_reset(DD_ROLE_HOST, "2");
    fe_show(1);
    fe_state(7);
    fe_mark(2, 1, 0);
    frontend_poll();
    fe_settle();
    frontend_poll();
    CHECK(!fe_loads && g_fe_done);
    fe_reset(DD_ROLE_HOST, "2");
    fe_show(1);
    fe_state(7);
    fe_mark(2, 0, 1);
    frontend_poll();
    fe_settle();
    frontend_poll();
    CHECK(!fe_loads && g_fe_done);
    /* The slots are not there. */
    fe_reset(DD_ROLE_HOST, "2");
    fe_show(1);
    fe_state(7);
    memset(fe_screen + DD_FE_SLOTS, 0, 8);
    frontend_poll();
    fe_settle();
    frontend_poll();
    CHECK(!fe_loads && g_fe_done);
    /* The player was faster. */
    fe_reset(DD_ROLE_HOST, "2");
    fe_show(1);
    fe_mark(2, 1, 1);
    fe_state(8);
    frontend_poll();
    fe_settle();
    frontend_poll();
    CHECK(!fe_loads && g_fe_done);
    /* Never comes to the saves: it stops trying. */
    fe_reset(DD_ROLE_HOST, "2");
    fe_show(1);
    fe_state(3);
    frontend_poll();
    CHECK(!g_fe_done);
    g_fe_give_up_at = 1;
    frontend_poll();
    CHECK(g_fe_done);
    fe_show(0);
    fe_mode_set(0);
    g_fe_image = image_base;
    g_fe_start = NULL;
    g_fe_load = NULL;
    g_fe_new = NULL;
    g_frontend_departure_ready = departure_start_ready;
    g_fe_new_slot = 0;
    g_cfg.coop_slots[0] = 0;
    frontend_reset();
    reset(DD_ROLE_HOST);
    puts("Start screen: straight to the chosen slot, once, in a session, never towards the Circus");
}
