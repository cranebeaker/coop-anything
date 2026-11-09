/* Real hunger hooks + authority/FIFO, fake native effects only. */
static uint8_t hn_bag[2][0x5a8];
static unsigned hn_eats, hn_refusals, hn_shows;
static uint32_t hn_food_key(void) { return 0x72f14069u; }
static void hn_mode(uint32_t mode) { memcpy(wk_display + DD_WALK_OVERLAY + 0xd88, &mode, 4); }
static uint32_t hn_food(void) { uint32_t a, b; memcpy(&a, hn_bag[0], 4); memcpy(&b, hn_bag[1], 4); return a + b; }
static void __fastcall hn_show(void *overlay, void *display) {
    CHECK(overlay == wk_display + DD_WALK_OVERLAY && display == wk_display);
    hn_shows++;
    hn_mode(2);
}
static void __fastcall hn_eat(void *args) {
    uint64_t overlay;
    float cost;
    memcpy(&overlay, (uint8_t *)args + 8, 8);
    memcpy(&cost, (uint8_t *)args + 0x10, 4);
    CHECK(overlay == (uintptr_t)wk_display + DD_WALK_OVERLAY);
    hn_eats++;
    uint32_t left = (uint32_t)cost;
    for (unsigned i = 0; i < 2 && left; i++) {
        uint32_t count = 0;
        memcpy(&count, hn_bag[i], 4);
        uint32_t take = count < left ? count : left;
        count -= take; left -= take;
        memcpy(hn_bag[i], &count, 4);
    }
    CHECK(!left);
    for (unsigned i = 0; i < 4; i++) {
        float hp;
        memcpy(&hp, bt_heroes[i] + DD_HERO_HP, 4); hp += 1;
        memcpy(bt_heroes[i] + DD_HERO_HP, &hp, 4);
    }
    hn_mode(3);
}
static void __fastcall hn_refuse(void *capture) {
    uint64_t overlay;
    float food;
    memcpy(&overlay, capture, 8);
    memcpy(&food, (uint8_t *)capture + 8, 4);
    CHECK(overlay == (uintptr_t)wk_display + DD_WALK_OVERLAY && (food == 4.0f || food == 5.0f));
    hn_refusals++;
    for (unsigned i = 0; i < 4; i++) {
        float hp, stress;
        memcpy(&hp, bt_heroes[i] + DD_HERO_HP, 4); hp -= 2;
        memcpy(&stress, bt_heroes[i] + DD_HERO_STRESS, 4); stress += 20;
        memcpy(bt_heroes[i] + DD_HERO_HP, &hp, 4);
        memcpy(bt_heroes[i] + DD_HERO_STRESS, &stress, 4);
    }
    hn_mode(3);
}
static float hn_hp(void) { float v; memcpy(&v, bt_heroes[0] + DD_HERO_HP, 4); return v; }
static void hn_reset(int role) {
    bt_reset(role);
    hunger_reset();
    wk_view.battle = 0;
    g_rules_object = NULL;
    memset(hn_bag, 0, sizeof hn_bag);
    uint64_t begin = (uintptr_t)hn_bag, end = (uintptr_t)(hn_bag + 2), none = 0;
    uint32_t food = hn_food_key(), count = 12;
    memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 8, &begin, 8);
    memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 0x10, &end, 8);
    memcpy(hn_bag[0], &count, 4);
    for (unsigned i = 0; i < 2; i++) memcpy(hn_bag[i] + 0x44, &food, 4);
    for (unsigned i = 0; i < 4; i++) {
        float hp = 20, stress = 30, modifier = 0;
        memcpy(bt_heroes[i] + DD_HERO_HP, &hp, 4);
        memcpy(bt_heroes[i] + DD_HERO_STRESS, &stress, 4);
        memcpy(bt_heroes[i] + 0xbc4, &modifier, 4);
    }
    memcpy(wk_display + DD_WALK_OVERLAY + 0xd70, &none, 8);
    hn_mode(0);
    g_hunger_food_type = hn_food_key;
    g_hunger_show = hn_show;
    g_hunger_eat = hn_eat;
    g_hunger_refuse = hn_refuse;
    hn_eats = hn_refusals = hn_shows = 0;
}
static void hn_open(void) {
    hn_mode(1);
    hook_hunger_show(wk_display + DD_WALK_OVERLAY, wk_display);
    CHECK(g_hunger_visible && hunger_open(&wk_view));
}
static void hn_click(int eat) {
    if (eat) {
        uint8_t args[0x18] = {0};
        uint64_t overlay = (uintptr_t)wk_display + DD_WALK_OVERLAY;
        uint32_t cost = 0;
        CHECK(hunger_cost(&wk_view, &cost));
        memcpy(args + 8, &overlay, 8); memcpy(args + 0x10, &cost, 4);
        hook_hunger_eat(args);
    } else {
        uint8_t capture[0x10] = {0};
        uint64_t overlay = (uintptr_t)wk_display + DD_WALK_OVERLAY;
        uint32_t cost = 0, padding = 0xdec0addeu;
        CHECK(hunger_cost(&wk_view, &cost));
        memcpy(capture, &overlay, 8);
        memcpy(capture + 8, &cost, 4);
        memcpy(capture + 12, &padding, 4);
        hook_hunger_refuse(capture);
    }
}
static void hunger_transactions(void) {
    /* Actual refusal capture from ShowHunger: pointer +0, FOOD FLOAT +8,
     * unrelated copied padding +12. It contains no RaidDisplay pointer. */
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        hn_reset(role);
        uint32_t two = 2, cost = wk_bits(4.0f), padding = 0xa5a5a5a5u;
        memcpy(hn_bag[0], &two, 4);
        hn_open();
        uint8_t capture[0x10] = {0};
        uint64_t overlay = (uintptr_t)wk_display + DD_WALK_OVERLAY;
        memcpy(capture, &overlay, 8);
        memcpy(capture + 8, &cost, 4);
        memcpy(capture + 12, &padding, 4);
        hook_hunger_refuse(capture);
        CHECK(sent == 1 && last_message.u.town.action.kind == DD_ACT_ITEM &&
              last_message.u.town.action.u.item.op == DD_ITEM_OP_HUNGER && !last_message.u.town.action.u.item.flag);
    }
    /* Both choices from the guest: no local consumption/damage until host approval. */
    for (int eat = 0; eat <= 1; eat++) {
        hn_reset(DD_ROLE_GUEST); hn_open();
        hook_hunger_show(wk_display + DD_WALK_OVERLAY, wk_display);
        CHECK(g_hunger_epoch == 1); /* Per-frame rendering does not create new checks. */
        hn_click(eat); hn_click(eat);
        CHECK(sent == 1 && g_hunger_request && !hn_eats && !hn_refusals && hn_food() == 12 && hn_hp() == 20);
        dd_msg request = last_message, decoded;
        CHECK(request.type == DD_MSG_TOWN_REQUEST && request.u.town.action.u.item.op == DD_ITEM_OP_HUNGER && dd_action_valid(&request.u.town.action));
        uint8_t wire[DD_PROTO_HEADER + DD_PROTO_MAX_BODY]; size_t used = 0;
        size_t length = dd_encode(&request, wire, sizeof wire);
        CHECK(length && dd_decode(wire, length, &decoded, &used) == DD_DECODE_OK && used == length &&
              decoded.u.town.action.u.item.round == 1 && decoded.u.town.action.u.item.amount == wk_bits(4.0f));
        /* Host arrives later; the first request stays queued without effects. */
        hn_reset(DD_ROLE_HOST);
        CHECK(queue_push(&g_incoming, &request));
        dd_runtime_poll();
        CHECK(g_incoming.count == 1 && !hn_eats && !hn_refusals && hn_food() == 12);
        hn_open(); dd_runtime_poll();
        CHECK(!g_incoming.count && g_town.applied_seq == 1 && !g_town.needs_resync && sent == 1);
        CHECK(hn_eats == (unsigned)eat && hn_refusals == (unsigned)!eat && hn_food() == (eat ? 8u : 12u) && hn_hp() == (eat ? 21.0f : 18.0f));
        dd_msg apply = last_message;
        /* A late/crossed request is rejected, never a second meal. */
        CHECK(queue_push(&g_incoming, &request)); dd_runtime_poll();
        CHECK(g_town.rejected == 1 && hn_eats + hn_refusals == 1);
        hn_reset(DD_ROLE_GUEST); hn_open(); hn_click(eat);
        CHECK(queue_push(&g_incoming, &apply)); dd_runtime_poll();
        CHECK(g_town.applied_seq == 1 && !g_town.needs_resync && !g_hunger_request);
        CHECK(hn_eats == (unsigned)eat && hn_refusals == (unsigned)!eat && hn_food() == (eat ? 8u : 12u) && hn_hp() == (eat ? 21.0f : 18.0f));
        /* Identical already-done host action is harmless; a conflicting one stops. */
        apply.seq = 2;
        CHECK(queue_push(&g_incoming, &apply)); dd_runtime_poll();
        CHECK(g_town.applied_seq == 2 && !g_town.needs_resync && hn_eats + hn_refusals == 1);
        apply.seq = 3; apply.u.town.action.u.item.flag ^= 1;
        CHECK(queue_push(&g_incoming, &apply)); dd_runtime_poll();
        CHECK(g_town.needs_resync && hn_eats + hn_refusals == 1);
    }
    /* Both choices from the host work identically, including guest arrival later. */
    for (int eat = 0; eat <= 1; eat++) {
        hn_reset(DD_ROLE_HOST); hn_open(); hn_click(eat); hn_click(eat);
        CHECK(sent == 1 && hn_eats + hn_refusals == 1 && g_town.applied_seq == 1);
        dd_msg apply = last_message;
        hn_reset(DD_ROLE_GUEST);
        CHECK(queue_push(&g_incoming, &apply)); dd_runtime_poll();
        CHECK(g_incoming.count == 1 && hn_food() == 12 && hn_hp() == 20);
        hn_open(); dd_runtime_poll();
        CHECK(g_town.applied_seq == 1 && !g_town.needs_resync && hn_food() == (eat ? 8u : 12u) && hn_hp() == (eat ? 21.0f : 18.0f));
    }
    /* With two or zero food: refusal still travels both ways and applies once. */
    for (uint32_t food = 0; food <= 2; food += 2) for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        hn_reset(role); memcpy(hn_bag[0], &food, 4); hn_open(); hn_click(0);
        CHECK(sent == 1 && !last_message.u.town.action.u.item.flag);
        dd_msg message = last_message;
        hn_reset(role == DD_ROLE_HOST ? DD_ROLE_GUEST : DD_ROLE_HOST);
        memcpy(hn_bag[0], &food, 4); hn_open();
        CHECK(queue_push(&g_incoming, &message)); dd_runtime_poll();
        CHECK(hn_refusals == 1 && !hn_eats && hn_food() == food && hn_hp() == 18.0f && !g_town.needs_resync);
        if (role == DD_ROLE_GUEST) {
            message = last_message;
            hn_reset(DD_ROLE_GUEST); memcpy(hn_bag[0], &food, 4); hn_open();
            CHECK(queue_push(&g_incoming, &message)); dd_runtime_poll();
            CHECK(hn_refusals == 1 && hn_food() == food && hn_hp() == 18.0f && !g_town.needs_resync);
        }
    }
    /* Quirks affect cost; split stacks count together; an unaffordable meal is refused. */
    hn_reset(DD_ROLE_HOST);
    { float modifier = 1; memcpy(bt_heroes[0] + 0xbc4, &modifier, 4); }
    hn_open(); hn_click(1);
    CHECK(hn_food() == 7 && hn_eats == 1 && last_message.u.town.action.u.item.amount == wk_bits(5.0f));
    hn_reset(DD_ROLE_HOST);
    { uint32_t a = 2, b = 3; memcpy(hn_bag[0], &a, 4); memcpy(hn_bag[1], &b, 4); }
    hn_open(); hn_click(1);
    CHECK(hn_food() == 1 && hn_eats == 1);
    hn_reset(DD_ROLE_HOST);
    { uint32_t a = 2; memcpy(hn_bag[0], &a, 4); }
    hn_open(); hn_click(1);
    CHECK(!sent && !hn_eats && hn_food() == 2);
    /* Rejected guest choice unlocks the same prompt so refusing remains possible. */
    hn_reset(DD_ROLE_GUEST); hn_open(); hn_click(1);
    dd_msg denied = {0}; denied.type = DD_MSG_TOWN_REJECT;
    denied.u.reject.request_id = g_hunger_request; denied.u.reject.reason = DD_REJECT_NOT_ALLOWED;
    CHECK(queue_push(&g_incoming, &denied)); dd_runtime_poll();
    CHECK(!g_hunger_request && hunger_open(&wk_view) && !hn_eats);
    hn_click(0);
    CHECK(sent == 2 && g_hunger_request && !last_message.u.town.action.u.item.flag);
    /* No local side effect after disconnect; ordinary single player stays native. */
    hn_reset(DD_ROLE_HOST); hn_open(); g_session_ready = 0;
    hn_click(1); hn_click(0);
    CHECK(!sent && !hn_eats && !hn_refusals && hn_food() == 12);
    hn_reset(DD_ROLE_OFF); hn_click(1);
    CHECK(!sent && hn_eats == 1 && hn_food() == 8);
    /* A curio sharing this overlay is not a hunger prompt; closed prompts expire. */
    hn_reset(DD_ROLE_HOST); hn_open();
    { uint64_t prop = 123; memcpy(wk_display + DD_WALK_OVERLAY + 0xd70, &prop, 8); }
    CHECK(!hunger_open(&wk_view));
    hunger_poll(&wk_view); CHECK(!g_hunger_visible);
    hn_reset(DD_ROLE_HOST); hn_open(); hn_mode(0); hunger_poll(&wk_view);
    CHECK(!g_hunger_visible);
    /* Reject malformed wire input, including nonfinite, negative or excessive cost. */
    hn_reset(DD_ROLE_GUEST); hn_open(); hn_click(1);
    dd_action action = last_message.u.town.action;
    CHECK(dd_action_valid(&action));
    action.u.item.round = 0; CHECK(!dd_action_valid(&action)); action.u.item.round = 1;
    action.u.item.amount = 0x7fc00000u; CHECK(!dd_action_valid(&action));
    action.u.item.amount = 0xbf800000u; CHECK(!dd_action_valid(&action));
    action.u.item.amount = wk_bits(65.0f); CHECK(!dd_action_valid(&action));
    hunger_reset();
    g_hunger_show = NULL; g_hunger_eat = g_hunger_refuse = NULL;
    g_hunger_food_type = hunger_food_type;
    puts("Hunger: eat/refuse in both directions, delayed prompts, first choice, native food/HP/stress, modifiers, split stacks, malformed/stale choices and disconnect verified");
}
