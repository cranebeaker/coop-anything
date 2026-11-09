/* Real ordered provisions opener; fake only this copy's native UI and roots. */
static uint8_t pp_town[0x4700], pp_campaign[0x1800], pp_quests[2][0x2f0];
static unsigned pp_opens, pp_fills, pp_stock_fills;
static int pp_ready;
static int pp_stock;
static int pp_supply_open(supply_model *m, dd_provisions_snapshot *s) {
    memset(s, 0, sizeof *s);
    m->campaign = (uintptr_t)pp_campaign;
    m->panel = (uintptr_t)pp_town + 0x3770;
    m->stock = (uintptr_t)pp_campaign + 0x1618;
    m->bag = (uintptr_t)pp_campaign + 0x15b0;
    s->stock_occupied = (uint32_t)pp_stock;
    return 1;
}
static void __fastcall pp_fill_free(void *bag, void *quest) {
    int32_t chosen = -1;
    memcpy(&chosen, pp_campaign + 0x1248, 4);
    CHECK(bag == pp_campaign + 0x15b0 && chosen >= 0 && chosen < 2 && quest == pp_quests[chosen]);
    pp_fills++;
}
static void __fastcall pp_fill_stock(void *bag, void *quest) {
    (void)quest;
    CHECK(bag == pp_campaign + 0x15b0);
    pp_stock_fills++;
    pp_stock = 1;
}
static int pp_read(depart_view *v) {
    memset(v, 0, sizeof *v);
    if (!pp_ready) return 0;
    v->town = (uintptr_t)pp_town;
    v->campaign = (uintptr_t)pp_campaign;
    memcpy(&v->quest, pp_campaign + 0x1248, 4);
    return depart_quest_key(v->campaign, v->quest, &v->key);
}
static void __fastcall pp_open(void *capture) {
    uint64_t town = 0;
    memcpy(&town, (uint8_t *)capture + 8, 8);
    CHECK(town == (uintptr_t)pp_town && g_game_apply_depth);
    pp_opens++;
    putint(pp_town + 0x37c8, 1); /* Native transition; subsequent buys wait for its views. */
}
static void pp_reset(int role, unsigned mission) {
    reset(role);
    prepare_reset();
    provisions_reset();
    g_supply_open = pp_supply_open;
    g_supply_quest = supply_quest_read;
    g_supply_fill[0] = pp_fill_free;
    g_supply_fill[1] = pp_fill_stock;
    pp_fills = pp_stock_fills = 0;
    pp_stock = 0;
    memset(pp_town, 0, sizeof pp_town);
    memset(pp_campaign, 0, sizeof pp_campaign);
    memset(pp_quests, 0, sizeof pp_quests);
    putptr(pp_campaign + 0x1230, (uintptr_t)pp_quests);
    putptr(pp_campaign + 0x1238, (uintptr_t)(pp_quests + 2));
    putint(pp_quests[0] + 0x40, 7);
    putint(pp_quests[1] + 0x40, 8);
    putint(pp_campaign + 0x1248, mission);
    pp_ready = 1;
    pp_opens = 0;
    g_prepare_read = pp_read;
    g_prepare_open = pp_open;
    dd_town_init(&g_town, NULL, runtime_apply, NULL, fake_send);
}
static void prepare_transactions(void) {
    uint64_t capture[2] = {0, (uintptr_t)pp_town};
    pp_reset(DD_ROLE_GUEST, 1);
    hook_prepare_open(capture);
    CHECK(!pp_opens && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
    dd_msg request = last_message;
    /* The other player clicked: the shop fills here, for that mission, and this screen stays what it is. */
    pp_reset(DD_ROLE_HOST, 0);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(!pp_opens && pp_fills == 1 && pp_stock_fills == 1 && g_supply_bg && g_town.accepted == 1 && last_message.type == DD_MSG_TOWN_APPLY);
    { uint32_t chosen = 0; memcpy(&chosen, pp_campaign + 0x1248, 4); CHECK(chosen == 1); }
    CHECK(g_supply_quest_known && g_supply_quest_index == 1 && g_supply_quest_key == 8);
    dd_msg apply = last_message;
    /* The guest that clicked opens, when its order comes, and the panel fills. */
    pp_reset(DD_ROLE_GUEST, 0);
    g_prepare_own_until = GetTickCount64() + DD_PREPARE_OWN_MS;
    pp_ready = 0;
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(!pp_opens && g_incoming.count == 1 && g_town.applied_seq == 0);
    pp_ready = 1;
    dd_runtime_poll();
    CHECK(pp_opens == 1 && !pp_fills && !g_supply_bg && g_town.applied_seq == 1 && !g_town.needs_resync && !g_prepare_own_until);
    apply.seq = 2;
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(pp_opens == 1 && !pp_fills && g_town.applied_seq == 2); /* Already showing: never refill twice. */
    /* A click that was refused long ago does not open anything later. */
    pp_reset(DD_ROLE_GUEST, 0);
    g_prepare_own_until = GetTickCount64() - 1;
    apply.seq = 1;
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(!pp_opens && pp_fills == 1 && g_supply_bg && !g_town.needs_resync);
    /* That guest clicks after all: it joins that shop, whatever it has selected meanwhile; the panel then does not fill again. */
    putint(pp_campaign + 0x1248, 0);
    hook_prepare_open(capture);
    CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST && last_message.u.town.action.u.prepare.index == 1 && last_message.u.town.action.u.prepare.key == 8);
    apply.seq = 2;
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(pp_opens == 1 && pp_fills == 1 && g_supply_bg);
    { uint32_t chosen = 0; memcpy(&chosen, pp_campaign + 0x1248, 4); CHECK(chosen == 1); }
    /* The shop's mission decides what it is filled with, not what this player has selected. */
    {
        uint64_t quest = 0;
        putint(pp_campaign + 0x1248, 0);
        g_supply_quest_known = 1;
        g_supply_quest_index = 1;
        CHECK(supply_quest_read((uintptr_t)pp_campaign, &quest) && quest == (uintptr_t)pp_quests[1]);
        g_supply_quest_known = 0;
        CHECK(supply_quest_read((uintptr_t)pp_campaign, &quest) && quest == (uintptr_t)pp_quests[0]);
    }
    /* The host's click opens its own screen. */
    pp_reset(DD_ROLE_HOST, 1);
    hook_prepare_open(capture);
    CHECK(pp_opens == 1 && !pp_fills && sent == 1 && last_message.type == DD_MSG_TOWN_APPLY && !g_prepare_host_click);
    /* The fill cannot be done: a stale request is refused; on the guest that stops. */
    pp_reset(DD_ROLE_HOST, 0);
    g_supply_fill[0] = NULL;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(!pp_opens && g_town.rejected == 1 && !g_town.needs_resync);
    pp_reset(DD_ROLE_GUEST, 0);
    g_supply_fill[1] = NULL;
    apply.seq = 1;
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(g_town.needs_resync);
    /* A refund on its way: this click does nothing, and the other player's click waits for it. */
    pp_reset(DD_ROLE_HOST, 1);
    g_supply_cancel_local = 1;
    hook_prepare_open(capture);
    CHECK(!pp_opens && !sent && !alerts);
    CHECK(prepare_gate(&request) == 1);
    provisions_reset();
    pp_reset(DD_ROLE_HOST, 0);
    request.u.town.action.u.prepare.key = 99;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(!pp_opens && !pp_fills && g_town.rejected == 1);
    pp_reset(DD_ROLE_GUEST, 1);
    capture[1] = 1;
    hook_prepare_open(capture);
    CHECK(!sent && !pp_opens);
    g_prepare_read = prepare_read;
    g_prepare_open = NULL;
    g_supply_open = supply_open;
    g_supply_fill[0] = g_supply_fill[1] = NULL;
    provisions_reset();
    puts("Prepare: a click opens that player's screen and the shop on both copies, shares the mission, waits for town, refuses stale missions and never refills an open panel");
}
