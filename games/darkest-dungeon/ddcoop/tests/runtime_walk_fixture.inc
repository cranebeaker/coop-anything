/* Uses the real walk hooks and FIFO. Only the game's view, move call, input check and travel call are faked. */
static uint8_t wk_display[0x4200], wk_areas[3][0x140], wk_props[2][0x140], wk_heroes[4][0x1400], wk_result[0x10];
static uint64_t wk_prop_list[2], wk_party[4], wk_prop_table[6], wk_result_table[3];
static unsigned wk_interacts, wk_runs, wk_frees, wk_overlays, wk_traps, wk_selects;
static int wk_interact_option, wk_select_index, wk_use_done;
static void *wk_interact_hero;
static uint64_t wk_area_list[3];
static uint8_t wk_raid_lists[0x6000];
static walk_view wk_view;
static int wk_view_fail, wk_refuse_caller;
static unsigned wk_moves, wk_natives, wk_uses, wk_travels;
static float wk_direction[3];
static uint32_t wk_dest[4];
static void *wk_from;
static uint8_t wk_flip, wk_extra;
static int wk_read(walk_view *v) {
    if (wk_view_fail) return 0;
    *v = wk_view;
    return 1;
}
static void __fastcall wk_move(void *party, const float *direction) {
    CHECK(party == (void *)(wk_raid_lists + 8));
    wk_moves++;
    memcpy(wk_direction, direction, sizeof wk_direction);
}
static void __fastcall wk_native_move_none(void *display, float *out) {
    CHECK(display == wk_display);
    wk_natives++;
    out[0] = 0;
}
static void __fastcall wk_native_move(void *display, float *out) {
    CHECK(display == wk_display);
    wk_natives++;
    out[0] = 1.0f;
}
static int wk_use_nest;
static uint8_t __fastcall wk_use(void *display, void *prop, int32_t a, uint8_t b) {
    (void)display, (void)prop, (void)a, (void)b;
    wk_uses++;
    if (wk_use_nest) dd_runtime_poll(); /* The game's message dialog pumps events from inside. */
    return (uint8_t)wk_use_done;
}
static int wk_trap_done;
static uint8_t __fastcall wk_use_trap(void *display, void *prop) {
    (void)display, (void)prop;
    wk_traps++;
    return (uint8_t)wk_trap_done;
}
static void __fastcall wk_overlay(void *functor) {
    uint64_t panel = 0;
    memcpy(&panel, (uint8_t *)functor + 8, 8);
    CHECK(panel == (uintptr_t)wk_display + DD_WALK_OVERLAY);
    wk_overlays++;
}
static unsigned wk_curios;
static void __fastcall wk_curio(void *functor) {
    (void)functor;
    wk_curios++;
}
static void __fastcall wk_select(void *party, int32_t index) {
    CHECK(party == (void *)(wk_raid_lists + 8));
    wk_selects++;
    wk_select_index = index;
}
static uintptr_t __fastcall wk_interact(void *prop, int32_t option, void *hero) {
    CHECK(prop == wk_props[1]);
    wk_interacts++;
    wk_interact_option = option;
    wk_interact_hero = hero;
    return (uintptr_t)wk_result;
}
static void __fastcall wk_result_run(void *result) {
    CHECK(result == wk_result);
    wk_runs++;
}
static void __fastcall wk_result_free(void *result, int32_t how) {
    CHECK(result == wk_result && how == 1 && wk_runs);
    wk_frees++;
}
static void __fastcall wk_travel(void *display, const uint32_t *dest, void *from, uint8_t flip, uint8_t extra) {
    CHECK(display == wk_display);
    wk_travels++;
    memcpy(wk_dest, dest, sizeof wk_dest);
    wk_from = from;
    wk_flip = flip;
    wk_extra = extra;
}
static int wk_player_caller(uintptr_t rva) { (void)rva; return wk_refuse_caller; }
static float wk_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, 4);
    return value;
}
static uint32_t wk_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    return bits;
}
static void wk_reset(int role) {
    sw_reset(role);
    walk_reset();
    { uint32_t walking = 2; memcpy(wk_display + 0x40b0, &walking, 4); }
    uint64_t begin = (uintptr_t)wk_area_list, end = (uintptr_t)(wk_area_list + 3);
    memset(wk_raid_lists, 0, sizeof wk_raid_lists);
    memcpy(wk_raid_lists + 0x740, &begin, 8);
    memcpy(wk_raid_lists + 0x748, &end, 8);
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t id = 100 + i;
        wk_area_list[i] = (uintptr_t)wk_areas[i];
        memcpy(wk_areas[i] + 0x18, &id, 4);
    }
    memset(&wk_view, 0, sizeof wk_view);
    wk_view.raid = (uintptr_t)wk_raid_lists;
    wk_view.display = (uintptr_t)wk_display;
    wk_view.area = 100;
    wk_view.area_ptr = (uintptr_t)wk_areas[0];
    wk_view.state = 2;
    wk_view.pos = 500.0f;
    wk_view_fail = wk_refuse_caller = 0;
    wk_moves = wk_natives = wk_uses = wk_travels = 0;
    wk_interacts = wk_runs = wk_frees = wk_overlays = wk_traps = wk_selects = 0;
    wk_trap_done = 1;
    wk_interact_option = wk_select_index = -1;
    wk_interact_hero = NULL;
    wk_use_done = 1;
    memset(wk_props, 0, sizeof wk_props);
    memset(wk_display + DD_WALK_OVERLAY + 0xd70, 0, 0x20);
    {
        uint64_t prop_begin = (uintptr_t)wk_prop_list, prop_end = (uintptr_t)(wk_prop_list + 2);
        uint64_t party_begin = (uintptr_t)wk_party, party_end = (uintptr_t)(wk_party + 4), selected;
        uint64_t table = (uintptr_t)wk_prop_table, rtable = (uintptr_t)wk_result_table;
        uint32_t kind = 1;
        wk_prop_list[0] = (uintptr_t)wk_props[0];
        wk_prop_list[1] = (uintptr_t)wk_props[1];
        memcpy(wk_areas[0] + 0x118, &prop_begin, 8);
        memcpy(wk_areas[0] + 0x120, &prop_end, 8);
        for (unsigned i = 0; i < 4; i++) wk_party[i] = (uintptr_t)wk_heroes[i];
        memcpy(wk_raid_lists + 8, &party_begin, 8);
        memcpy(wk_raid_lists + 0x10, &party_end, 8);
        selected = wk_party[2];
        memcpy(wk_raid_lists + 0x438, &selected, 8);
        memcpy(wk_props[1], &table, 8);
        memcpy(wk_props[1] + 0x0c, &kind, 4);
        wk_prop_table[5] = (uintptr_t)wk_interact;
        memcpy(wk_result, &rtable, 8);
        wk_result_table[0] = (uintptr_t)wk_result_free;
        wk_result_table[2] = (uintptr_t)wk_result_run;
    }
    wk_from = NULL;
    memset(wk_dest, 0, sizeof wk_dest);
    g_walk_view = wk_read;
    g_party_move = wk_move;
    g_raid_move = wk_native_move;
    g_raid_use = wk_use;
    g_raid_trap = wk_use_trap;
    g_overlay_use = wk_overlay;
    g_party_select = wk_select;
    g_raid_travel = wk_travel;
    g_walk_player_travel = wk_player_caller;
    g_rules_object = NULL;
    while (queue_pop(&g_outgoing, &last_message)) {}
}
static void wk_target(uint32_t area, float pos) {
    dd_msg m;
    memset(&m, 0, sizeof m);
    m.type = DD_MSG_WALK;
    m.u.walk.area = area;
    m.u.walk.pos = wk_bits(pos);
    walk_receive(&m);
}
static dd_action wk_order(uint32_t area, uint32_t tile, uint32_t from_area) {
    dd_action a = {0};
    a.kind = DD_ACT_TRAVEL;
    a.u.travel.dest[0] = area;
    a.u.travel.dest[1] = tile;
    a.u.travel.dest[2] = 1;
    a.u.travel.from_area = from_area;
    a.u.travel.has_from = from_area != 0;
    a.u.travel.flip = 1;
    return a;
}
static void __fastcall wk_native_keys(void *display, float *out) {
    float wish[3] = {1.0f, 0, 0};
    CHECK(display == wk_display);
    wk_natives++;
    hook_party_move(wk_raid_lists + 8, wish); /* What 75b530 does when D is held. */
    out[0] = wish[0];
}
static void wk_push(uint32_t seq, const dd_action *a) {
    dd_msg m = {0};
    m.type = DD_MSG_TOWN_APPLY;
    m.seq = seq;
    m.u.town.action = *a;
    CHECK(queue_push(&g_incoming, &m));
}
static void use_transactions(void) {
    dd_msg out;
    float way[3] = {0, 0, 0};
    /* The guest's keys: the game's own input decides, nothing moves here, and the host hears the direction. */
    wk_reset(DD_ROLE_GUEST);
    g_raid_move = wk_native_keys;
    hook_raid_move(wk_display, way);
    CHECK(wk_natives == 1 && !wk_moves && way[0] == 0);
    CHECK(queue_pop(&g_outgoing, &out) && out.type == DD_MSG_WALK_INTENT && out.u.intent.direction == 1);
    /* Same direction soon after: no second message. Keys released: one message saying so. */
    hook_raid_move(wk_display, way);
    CHECK(!queue_pop(&g_outgoing, &out));
    g_raid_move = wk_native_move;
    hook_raid_move(wk_display, way);
    CHECK(queue_pop(&g_outgoing, &out) && !out.u.intent.direction);
    hook_raid_move(wk_display, way);
    CHECK(!queue_pop(&g_outgoing, &out));
    /* Outside the guest's input a move is a move. */
    {
        float wish[3] = {-1.0f, 0, 0};
        hook_party_move(wk_raid_lists + 8, wish);
        CHECK(wk_moves == 1 && wk_direction[0] == -1.0f);
    }
    /* The host: the guest's wish moves the party, and goes on in 7657a0 as a direction. */
    wk_reset(DD_ROLE_HOST);
    {
        dd_msg wish;
        memset(&wish, 0, sizeof wish);
        wish.type = DD_MSG_WALK_INTENT;
        wish.u.intent.direction = -1;
        g_raid_move = wk_native_move_none;
        walk_intent_receive(&wish);
        way[0] = 0;
        hook_raid_move(wk_display, way);
        CHECK(wk_moves == 1 && wk_direction[0] == -1.0f && way[0] == -1.0f);
        /* Opposing inputs stop both; HOST has no priority. */
        g_raid_move = wk_native_keys;
        way[0] = 0;
        hook_raid_move(wk_display, way);
        CHECK(wk_moves == 1 && way[0] == 0);
        /* Equal directions move only once; either player can also move alone. */
        wish.u.intent.direction = 1;
        walk_intent_receive(&wish);
        hook_raid_move(wk_display, way);
        CHECK(wk_moves == 2 && way[0] == 1.0f);
        wish.u.intent.direction = 0;
        walk_intent_receive(&wish);
        hook_raid_move(wk_display, way);
        CHECK(wk_moves == 3 && way[0] == 1.0f);
        /* A wish that is too old: the guest may be gone. */
        g_raid_move = wk_native_move_none;
        InterlockedExchange64(&g_walk_intent_at, (LONG64)GetTickCount64() - 1000);
        way[0] = 0;
        hook_raid_move(wk_display, way);
        CHECK(wk_moves == 3 && way[0] == 0);
        /* Stopped: nothing. */
        wish.u.intent.direction = 0;
        walk_intent_receive(&wish);
        hook_raid_move(wk_display, way);
        CHECK(wk_moves == 3);
    }
    /* A prop without a choice, on the host: the game does it, the order goes out. */
    wk_reset(DD_ROLE_HOST);
    CHECK(hook_raid_use(wk_display, wk_props[1], 3, 1) == 1 && wk_uses == 1 && sent == 1);
    dd_action used = last_message.u.town.action;
    CHECK(used.kind == DD_ACT_USE && used.u.use.area == 100 && used.u.use.prop == 1 && used.u.use.prop_kind == 1 && used.u.use.option == 3 && used.u.use.hero == 2 && used.u.use.flag == 1 && !used.u.use.door);
    /* The game said no (out of range): nothing goes out. */
    wk_reset(DD_ROLE_HOST);
    wk_use_done = 0;
    CHECK(!hook_raid_use(wk_display, wk_props[1], 3, 1) && !sent);
    /* The same on the guest's copy: same hero selected, the same interaction, run once and freed. */
    wk_reset(DD_ROLE_GUEST);
    wk_view.pos = 500.0f;
    { float at = 500.0f; memcpy(&used.u.use.pos, &at, 4); }
    wk_push(1, &used);
    dd_runtime_poll();
    CHECK(wk_uses == 1 && g_town.applied_seq == 1);
    CHECK(wk_select_index == 2);
    { int32_t field = 0; memcpy(&field, wk_display + 0x3f88, 4); CHECK(field == 1); }
    /* Its game declines the click (774650): the host already did it, so this copy logs it and the orders go on. */
    wk_reset(DD_ROLE_GUEST);
    wk_view.pos = 500.0f;
    wk_use_done = 0;
    wk_push(1, &used);
    dd_runtime_poll();
    CHECK(wk_uses == 1 && g_town.applied_seq == 1 && !g_town.needs_resync);
    wk_use_done = 1;
    /* On the guest a click asks; nothing happens here until the order returns. */
    wk_reset(DD_ROLE_GUEST);
    CHECK(hook_raid_use(wk_display, wk_props[1], 0, 0) == 1 && !wk_uses && sent == 1);
    CHECK(last_message.type == DD_MSG_TOWN_REQUEST && last_message.u.town.action.kind == DD_ACT_USE);
    /* That request on the host. */
    {
        dd_msg request = last_message;
        wk_reset(DD_ROLE_HOST);
        CHECK(queue_push(&g_incoming, &request));
        dd_runtime_poll();
        CHECK(wk_uses == 1 && last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.action.kind == DD_ACT_USE);
    }
    /* A curio with an overlay: the click is shared as well, because it may run without any further click. */
    wk_reset(DD_ROLE_GUEST);
    wk_props[1][0x124] = 1;
    CHECK(hook_raid_use(wk_display, wk_props[1], 0, 0) == 1 && !wk_uses && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
    wk_reset(DD_ROLE_HOST);
    wk_props[1][0x124] = 1;
    CHECK(hook_raid_use(wk_display, wk_props[1], 0, 0) == 1 && wk_uses == 1 && sent == 1 && last_message.type == DD_MSG_TOWN_APPLY);
    /* The curio's button: the picked item goes along, and the guest repeats it on its own overlay. */
    {
        uint64_t panel = (uintptr_t)wk_display + DD_WALK_OVERLAY, functor[2] = {0, panel}, prop = (uintptr_t)wk_props[1], hero = 0;
        uint32_t mode = 2, type = 77, id = 88, other = 1;
        memset((void *)(uintptr_t)(panel + 0x6a0), 0, 8);
        wk_reset(DD_ROLE_HOST);
        hero = wk_party[3];
        g_curio_button = wk_curio;
        memcpy((void *)(uintptr_t)(panel + 0x6a0), &prop, 8);
        memcpy((void *)(uintptr_t)(panel + 0xd78), &hero, 8);
        memcpy((void *)(uintptr_t)(panel + 0xd88), &mode, 4);
        memcpy((void *)(uintptr_t)(panel + 0x76c), &type, 4);
        memcpy((void *)(uintptr_t)(panel + 0x770), &id, 4);
        wk_curios = 0;
        hook_curio_button(functor);
        CHECK(wk_curios == 1 && sent == 1 && last_message.u.town.action.u.use.door == 3 && last_message.u.town.action.u.use.item_type == 77 && last_message.u.town.action.u.use.item_id == 88 && last_message.u.town.action.u.use.hero == 3);
        dd_action picked = last_message.u.town.action;
        { float at = 500.0f; memcpy(&picked.u.use.pos, &at, 4); }
        /* Guest's click: it asks, and does nothing yet. */
        wk_reset(DD_ROLE_GUEST);
        g_curio_button = wk_curio;
        wk_curios = 0;
        memcpy((void *)(uintptr_t)(panel + 0x6a0), &prop, 8);
        memcpy((void *)(uintptr_t)(panel + 0xd88), &mode, 4);
        hook_curio_button(functor);
        CHECK(!wk_curios && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
        /* The order comes back: item and hero as ordered. */
        wk_reset(DD_ROLE_GUEST);
        g_curio_button = wk_curio;
        wk_curios = 0;
        memcpy((void *)(uintptr_t)(panel + 0x6a0), &prop, 8);
        memcpy((void *)(uintptr_t)(panel + 0xd88), &mode, 4);
        memcpy((void *)(uintptr_t)(panel + 0x76c), &other, 4);
        wk_push(1, &picked);
        dd_runtime_poll();
        { uint32_t now = 0; memcpy(&now, (void *)(uintptr_t)(panel + 0x76c), 4); CHECK(wk_curios == 1 && now == 77 && g_town.applied_seq == 1); }
        /* No overlay open here: nothing runs, nothing breaks. */
        wk_reset(DD_ROLE_GUEST);
        g_curio_button = wk_curio;
        wk_curios = 0;
        memset((void *)(uintptr_t)(panel + 0x6a0), 0, 8);
        wk_push(1, &picked);
        dd_runtime_poll();
        CHECK(!wk_curios && g_town.applied_seq == 1 && !g_town.needs_resync);
        g_curio_button = NULL;
    }
    /* The overlay's button. */
    {
        uint64_t panel = (uintptr_t)wk_display + DD_WALK_OVERLAY, functor[2] = {0, panel}, prop = (uintptr_t)wk_props[1], hero;
        uint32_t mode = 2;
        wk_reset(DD_ROLE_HOST);
        hero = wk_party[1];
        memcpy((void *)(uintptr_t)(panel + 0xd70), &prop, 8);
        memcpy((void *)(uintptr_t)(panel + 0xd78), &hero, 8);
        memcpy((void *)(uintptr_t)(panel + 0xd88), &mode, 4);
        hook_overlay_use(functor);
        CHECK(wk_overlays == 1 && sent == 1 && last_message.u.town.action.u.use.hero == 1 && !last_message.u.town.action.u.use.option);
        dd_action button = last_message.u.town.action;
        /* The guest asks, and its overlay waits. */
        wk_reset(DD_ROLE_GUEST);
        memcpy((void *)(uintptr_t)(panel + 0xd70), &prop, 8);
        memcpy((void *)(uintptr_t)(panel + 0xd78), &hero, 8);
        memcpy((void *)(uintptr_t)(panel + 0xd88), &mode, 4);
        hook_overlay_use(functor);
        CHECK(!wk_overlays && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
        /* The order comes back while it is open: the overlay itself does it, with the order's hero. */
        hero = wk_party[3];
        memcpy((void *)(uintptr_t)(panel + 0xd78), &hero, 8);
        { float at = 500.0f; memcpy(&button.u.use.pos, &at, 4); }
        wk_reset(DD_ROLE_GUEST);
        memcpy((void *)(uintptr_t)(panel + 0xd70), &prop, 8);
        memcpy((void *)(uintptr_t)(panel + 0xd78), &hero, 8);
        memcpy((void *)(uintptr_t)(panel + 0xd88), &mode, 4);
        wk_push(1, &button);
        dd_runtime_poll();
        CHECK(wk_overlays == 1 && g_town.applied_seq == 1);
        memcpy(&hero, (void *)(uintptr_t)(panel + 0xd78), 8);
        CHECK(hero == wk_party[1]);
        /* Another prop open, or none: the interaction runs without it. */
        wk_reset(DD_ROLE_GUEST);
        wk_push(1, &button);
        dd_runtime_poll();
        CHECK(!wk_overlays && wk_uses == 1);
        /* Not this overlay state: the game alone. */
        wk_reset(DD_ROLE_HOST);
        hook_overlay_use(functor);
        CHECK(wk_overlays == 1 && !sent);
    }
    /* Disarming a trap: the guest asks and does nothing itself; the host does it; then the guest does it too, with the same hero. */
    wk_reset(DD_ROLE_GUEST);
    CHECK(hook_raid_trap(wk_display, wk_props[1]) == 1 && !wk_traps && sent == 1 && last_message.u.town.action.u.use.door == 1 && last_message.u.town.action.u.use.hero == 2);
    {
        dd_msg trap = last_message;
        wk_reset(DD_ROLE_HOST);
        CHECK(queue_push(&g_incoming, &trap));
        dd_runtime_poll();
        CHECK(wk_traps == 1 && last_message.type == DD_MSG_TOWN_APPLY);
        dd_action back = last_message.u.town.action;
        wk_reset(DD_ROLE_GUEST);
        wk_push(1, &back);
        dd_runtime_poll();
        CHECK(wk_traps == 1 && wk_select_index == 2 && g_town.applied_seq == 1 && !g_town.needs_resync);
        /* Not near that spot yet: it waits, as for a curio. */
        wk_reset(DD_ROLE_GUEST);
        wk_view.pos = 2000.0f;
        wk_push(1, &back);
        dd_runtime_poll();
        CHECK(!wk_traps && !g_town.applied_seq && g_incoming.count == 1);
        wk_view.pos = 500.0f;
        dd_runtime_poll();
        CHECK(wk_traps == 1 && g_town.applied_seq == 1);
        /* The guest's game declines: the host has already done it, so the orders go on. */
        wk_reset(DD_ROLE_GUEST);
        wk_trap_done = 0;
        wk_push(1, &back);
        dd_runtime_poll();
        CHECK(wk_traps == 1 && g_town.applied_seq == 1 && !g_town.needs_resync);
        /* The host's game declines the guest's click: no order goes out. */
        wk_reset(DD_ROLE_HOST);
        wk_trap_done = 0;
        CHECK(queue_push(&g_incoming, &trap));
        dd_runtime_poll();
        CHECK(wk_traps == 1 && sent == 1 && last_message.type == DD_MSG_TOWN_REJECT);
    }
    /* A key held down: one request a second from the guest, and the host takes one door at a time. */
    wk_reset(DD_ROLE_GUEST);
    CHECK(hook_raid_trap(wk_display, wk_props[1]) == 1 && sent == 1);
    for (unsigned i = 0; i < 20; i++) CHECK(hook_raid_trap(wk_display, wk_props[1]) == 1);
    CHECK(sent == 1);
    g_walk_asked_at = GetTickCount64() - 1500;
    CHECK(hook_raid_trap(wk_display, wk_props[1]) == 1 && sent == 2);
    wk_reset(DD_ROLE_GUEST);
    CHECK(hook_raid_trap(wk_display, wk_props[0]) == 1 && hook_raid_trap(wk_display, wk_props[1]) == 1 && sent == 2); /* Another prop is another click. */
    /* A door prop reached through the other way: one copy only as well. */
    wk_reset(DD_ROLE_GUEST);
    { uint64_t table = (uintptr_t)image_base() + DD_WALK_DOOR_TABLE; memcpy(wk_props[1], &table, 8); }
    CHECK(hook_raid_use(wk_display, wk_props[1], 2, 1) == 1 && !wk_uses && sent == 1 && last_message.u.town.action.u.use.door == 2);
    {
        dd_msg door = last_message;
        wk_reset(DD_ROLE_HOST);
        { uint64_t table = (uintptr_t)image_base() + DD_WALK_DOOR_TABLE; memcpy(wk_props[1], &table, 8); }
        CHECK(queue_push(&g_incoming, &door));
        dd_runtime_poll();
        CHECK(wk_uses == 1 && !wk_traps && last_message.type == DD_MSG_TOWN_APPLY);
        /* The same door again while the first is under way: refused; after that it may run again. */
        CHECK(queue_push(&g_incoming, &door));
        dd_runtime_poll();
        CHECK(wk_uses == 1 && last_message.type == DD_MSG_TOWN_REJECT);
        g_walk_door_until = 1;
        CHECK(queue_push(&g_incoming, &door));
        dd_runtime_poll();
        CHECK(wk_uses == 2);
        dd_action back = last_message.u.town.action;
        wk_reset(DD_ROLE_GUEST);
        { uint64_t table = (uintptr_t)image_base() + DD_WALK_DOOR_TABLE; memcpy(wk_props[1], &table, 8); }
        wk_push(1, &back);
        dd_runtime_poll();
        CHECK(wk_uses == 1 && g_town.applied_seq == 1 && !g_town.needs_resync && g_walk_door_pending);
        /* Its callback ends here, and until then the host's travel order waits. */
        {
            uint32_t dest[4] = {101, 2, 0, 0};
            dd_action travel;
            memset(&travel, 0, sizeof travel);
            travel.kind = DD_ACT_TRAVEL;
            memcpy(travel.u.travel.dest, dest, sizeof dest);
            wk_push(2, &travel);
            dd_runtime_poll();
            CHECK(!wk_travels && g_incoming.count == 1);
            g_walk_door_pending = GetTickCount64() - 1; /* Timed out: the order goes on. */
            dd_runtime_poll();
            CHECK(wk_travels == 1 && !g_incoming.count);
        }
        /* Not near that door yet: it waits, as for a curio. */
        wk_reset(DD_ROLE_GUEST);
        { uint64_t table = (uintptr_t)image_base() + DD_WALK_DOOR_TABLE; memcpy(wk_props[1], &table, 8); }
        wk_view.pos = 2000.0f;
        wk_push(1, &back);
        dd_runtime_poll();
        CHECK(!wk_uses && g_incoming.count == 1);
        wk_view.pos = 500.0f;
        dd_runtime_poll();
        CHECK(wk_uses == 1 && !g_incoming.count);
        /* The guest's game declines the door: nothing to wait for. */
        wk_reset(DD_ROLE_GUEST);
        { uint64_t table = (uintptr_t)image_base() + DD_WALK_DOOR_TABLE; memcpy(wk_props[1], &table, 8); }
        wk_use_done = 0;
        wk_push(1, &back);
        dd_runtime_poll();
        CHECK(wk_uses == 1 && !g_walk_door_pending && g_town.applied_seq == 1);
        /* The host's own: the game, and an order. */
        wk_reset(DD_ROLE_HOST);
        { uint64_t table = (uintptr_t)image_base() + DD_WALK_DOOR_TABLE; memcpy(wk_props[1], &table, 8); }
        CHECK(hook_raid_use(wk_display, wk_props[1], 2, 1) == 1 && wk_uses == 1 && sent == 1 && last_message.u.town.action.u.use.door == 2);
        wk_reset(DD_ROLE_HOST);
        { uint64_t table = (uintptr_t)image_base() + DD_WALK_DOOR_TABLE; memcpy(wk_props[1], &table, 8); }
        wk_use_done = 0;
        CHECK(!hook_raid_use(wk_display, wk_props[1], 2, 1) && wk_uses == 1 && !sent);
    }
    /* A travel to an area that does not exist is dropped: the game would crash on it. */
    {
        uint32_t lost[4] = {0x0a560bc8u, 456, 0, 0};
        wk_reset(DD_ROLE_HOST);
        hook_raid_travel(wk_display, lost, NULL, 0, 0);
        CHECK(!wk_travels && !sent);
        wk_reset(DD_ROLE_GUEST);
        hook_raid_travel(wk_display, lost, NULL, 0, 0);
        CHECK(!wk_travels && !sent);
        /* Without a session the game keeps its own behaviour. */
        wk_reset(DD_ROLE_HOST);
        g_session_ready = 0;
        hook_raid_travel(wk_display, lost, NULL, 0, 0);
        CHECK(wk_travels == 1);
    }
    /* The host's own disarm runs and goes out; a click its game declines does not. */
    wk_reset(DD_ROLE_HOST);
    CHECK(hook_raid_trap(wk_display, wk_props[1]) == 1 && wk_traps == 1 && sent == 1 && last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.action.u.use.door == 1);
    wk_reset(DD_ROLE_HOST);
    wk_trap_done = 0;
    CHECK(!hook_raid_trap(wk_display, wk_props[1]) && wk_traps == 1 && !sent);
    /* The order waits while the guest is elsewhere, loading, fighting or not at the prop. */
    {
        static const struct {int fail; uint32_t state, battle, area;} waits[] = {{1, 2, 0, 100}, {0, 0, 0, 100}, {0, 2, 1, 100}, {0, 2, 0, 101}};
        for (unsigned i = 0; i < sizeof waits / sizeof *waits; i++) {
            wk_reset(DD_ROLE_GUEST);
            wk_view_fail = waits[i].fail;
            wk_view.state = waits[i].state;
            wk_view.battle = waits[i].battle;
            wk_view.area = waits[i].area;
            wk_push(1, &used);
            dd_runtime_poll();
            CHECK(!wk_uses && !g_town.applied_seq && g_incoming.count == 1);
            wk_view_fail = 0;
            wk_view.state = 2;
            wk_view.battle = 0;
            wk_view.area = 100;
            dd_runtime_poll();
            CHECK(wk_uses == 1 && g_town.applied_seq == 1);
        }
        /* Not there yet: waits; walked on past it for too long: goes on anyway. */
        wk_reset(DD_ROLE_GUEST);
        wk_view.pos = 500.0f + DD_WALK_USE_NEAR + 10.0f;
        wk_push(1, &used);
        dd_runtime_poll();
        CHECK(!wk_uses && g_incoming.count == 1);
        g_walk_use_wait_until = 1;
        dd_runtime_poll();
        CHECK(wk_uses == 1);
    }
    /* A prop that is not there, or of another kind: stopped. */
    wk_reset(DD_ROLE_GUEST);
    { uint32_t other = 2; memcpy(wk_props[1] + 0x0c, &other, 4); }
    wk_push(1, &used);
    dd_runtime_poll();
    CHECK(!wk_uses && g_town.needs_resync);
    /* The guest's map or door travel asks the host, who can do it unless it is busy. */
    {
        uint32_t dest[4] = {101, 3, 1, 0};
        wk_reset(DD_ROLE_GUEST);
        wk_refuse_caller = 1;
        hook_raid_travel(wk_display, dest, wk_areas[0], 1, 0);
        CHECK(!wk_travels && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST && last_message.u.town.action.kind == DD_ACT_TRAVEL);
        dd_msg asked = last_message;
        wk_reset(DD_ROLE_HOST);
        CHECK(queue_push(&g_incoming, &asked));
        dd_runtime_poll();
        CHECK(wk_travels == 1 && wk_dest[0] == 101 && last_message.type == DD_MSG_TOWN_APPLY);
        wk_reset(DD_ROLE_HOST);
        wk_view.battle = 1;
        CHECK(queue_push(&g_incoming, &asked));
        dd_runtime_poll();
        CHECK(!wk_travels && last_message.type == DD_MSG_TOWN_REJECT);
    }
    puts("Using props: the same prop, option and hero on both copies at the same place in the order; the guest asks, the host decides");
}
/* The party at a door: the host sends id, HP, stress and trinkets per rank; the guest takes HP and stress over and reports
 * the rest once. */
static uint8_t wk_trinkets[4][2 * 0x5a8];
static void wk_hero_state(unsigned i, uint32_t id, float hp, float stress, uint32_t trinket) {
    uint64_t begin = (uintptr_t)wk_trinkets[i], end = begin + 2 * 0x5a8;
    uint32_t one = 1, kind = 7;
    memset(wk_heroes[i], 0, sizeof wk_heroes[i]);
    memset(wk_trinkets[i], 0, sizeof wk_trinkets[i]);
    memcpy(wk_heroes[i] + 0x137c, &id, 4);
    memcpy(wk_heroes[i] + DD_HERO_HP, &hp, 4);
    memcpy(wk_heroes[i] + DD_HERO_STRESS, &stress, 4);
    memcpy(wk_heroes[i] + DD_HERO_TRINKETS + 8, &begin, 8);
    memcpy(wk_heroes[i] + DD_HERO_TRINKETS + 0x10, &end, 8);
    if (trinket) {
        memcpy(wk_trinkets[i], &one, 4);
        memcpy(wk_trinkets[i] + 0x44, &kind, 4);
        memcpy(wk_trinkets[i] + 0x88, &trinket, 4);
    }
}
static void wk_party_state(float hp0, float stress1, uint32_t trinket2) {
    wk_hero_state(0, 11, hp0, 10.0f, 0);
    wk_hero_state(1, 12, 20.0f, stress1, 0x500);
    wk_hero_state(2, 13, 15.0f, 0.0f, trinket2);
    wk_hero_state(3, 14, 9.5f, 99.0f, 0);
}
static void walk_party_door(void) {
    static uint8_t rules[0x10];
    uint32_t state = 0x1111, dest[4] = {101, 2, 0, 0};
    dd_action travel;
    float hp = 0, stress = 0;
    wk_reset(DD_ROLE_HOST);
    wk_party_state(30.0f, 40.0f, 0x600);
    g_rules_object = rules;
    memcpy(rules + 8, &state, 4);
    hook_raid_travel(wk_display, dest, NULL, 0, 0);
    CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_APPLY);
    travel = last_message.u.town.action;
    CHECK(travel.u.travel.ids[0] == 11 && travel.u.travel.ids[3] == 14 && travel.u.travel.trinkets[2] != travel.u.travel.trinkets[3]);
    memcpy(&hp, &travel.u.travel.hp[0], 4);
    memcpy(&stress, &travel.u.travel.stress[1], 4);
    CHECK(hp == 30.0f && stress == 40.0f);
    /* The same party on the guest: nothing changes, nothing said. */
    wk_reset(DD_ROLE_GUEST);
    wk_party_state(30.0f, 40.0f, 0x600);
    g_rules_object = rules;
    travel.u.travel.bag = walk_bag_hash((uintptr_t)wk_raid_lists);
    { unsigned before = (unsigned)alerts; wk_push(1, &travel); dd_runtime_poll(); CHECK(wk_travels == 1 && (unsigned)alerts == before); }
    /* HP and stress differ: the host's are taken over, quietly. Another trinket is said once. */
    wk_reset(DD_ROLE_GUEST);
    wk_party_state(25.0f, 55.0f, 0x601);
    g_rules_object = rules;
    {
        unsigned before = (unsigned)alerts;
        wk_push(1, &travel);
        dd_runtime_poll();
        memcpy(&hp, wk_heroes[0] + DD_HERO_HP, 4);
        memcpy(&stress, wk_heroes[1] + DD_HERO_STRESS, 4);
        CHECK(wk_travels == 1 && hp == 30.0f && stress == 40.0f && (unsigned)alerts == before + 1 && g_walk_trinket_told);
        memcpy(&hp, wk_heroes[2] + DD_HERO_HP, 4);
        CHECK(hp == 15.0f);
        wk_push(2, &travel);
        dd_runtime_poll();
        CHECK((unsigned)alerts == before + 1);
    }
    /* Another hero at a rank: reported once, and that rank's numbers are not touched. */
    wk_reset(DD_ROLE_GUEST);
    wk_party_state(25.0f, 40.0f, 0x600);
    {
        uint32_t other = 99;
        unsigned before = (unsigned)alerts;
        memcpy(wk_heroes[0] + 0x137c, &other, 4);
        g_rules_object = rules;
        wk_push(1, &travel);
        dd_runtime_poll();
        memcpy(&hp, wk_heroes[0] + DD_HERO_HP, 4);
        CHECK(hp == 25.0f && g_walk_party_told && (unsigned)alerts == before + 1);
    }
    /* A host that could not read its party sends no ids: nothing compared. */
    wk_reset(DD_ROLE_GUEST);
    wk_party_state(25.0f, 40.0f, 0x600);
    g_rules_object = rules;
    memset(travel.u.travel.ids, 0, sizeof travel.u.travel.ids);
    wk_push(1, &travel);
    dd_runtime_poll();
    memcpy(&hp, wk_heroes[0] + DD_HERO_HP, 4);
    CHECK(hp == 25.0f && !g_walk_party_told);
    /* The trinket hash sees the id, and an empty slot hashes as empty whatever is left in it. */
    wk_hero_state(0, 11, 1.0f, 1.0f, 0x700);
    {
        uint32_t first = walk_trinket_hash((uintptr_t)wk_heroes[0]), zero = 0, junk = 0x55;
        wk_hero_state(0, 11, 1.0f, 1.0f, 0x701);
        CHECK(walk_trinket_hash((uintptr_t)wk_heroes[0]) != first && first);
        wk_hero_state(0, 11, 1.0f, 1.0f, 0);
        first = walk_trinket_hash((uintptr_t)wk_heroes[0]);
        memcpy(wk_trinkets[0] + 0x88, &junk, 4);
        memcpy(wk_trinkets[0], &zero, 4);
        CHECK(walk_trinket_hash((uintptr_t)wk_heroes[0]) == first);
    }
    /* The blocks at the top left: whose hero, and its stress. */
    wk_reset(DD_ROLE_HOST);
    wk_party_state(30.0f, 40.0f, 0x600);
    g_hero_owner_ids[0] = 11;
    g_hero_owner_ids[1] = 12;
    g_hero_owner_ids[2] = 13;
    g_hero_owner_ids[3] = 14;
    g_hero_owner_mask = 2; /* hero 12 is the guest's */
    party_marks_show(&wk_view);
    CHECK(overlay_party_count == 4 && overlay_party_owner[0] == DD_OVERLAY_TURN_MINE && overlay_party_owner[1] == DD_OVERLAY_TURN_OTHER &&
          overlay_party_stress[1] == 40.0f && overlay_party_stress[3] == 99.0f);
    g_cfg.role = DD_ROLE_GUEST;
    party_marks_show(&wk_view);
    CHECK(overlay_party_owner[0] == DD_OVERLAY_TURN_OTHER && overlay_party_owner[1] == DD_OVERLAY_TURN_MINE);
    /* Two heroes: two blocks. No raid: none. */
    memset(wk_heroes[2], 0, sizeof wk_heroes[2]);
    wk_party[2] = wk_party[3] = 0;
    party_marks_show(&wk_view);
    CHECK(overlay_party_count == 2);
    wk_party[2] = (uintptr_t)wk_heroes[2];
    wk_party[3] = (uintptr_t)wk_heroes[3];
    wk_view_fail = 1;
    walk_poll();
    CHECK(overlay_party_count == 0);
    wk_view_fail = 0;
    memset(g_hero_owner_ids, 0, sizeof g_hero_owner_ids);
    g_hero_owner_mask = 0;
    for (unsigned i = 0; i < 4; i++) memset(wk_heroes[i], 0, sizeof wk_heroes[i]);
    g_rules_object = NULL;
}
static void walk_transactions(void) {
    walk_party_door();
    /* Guest: its own keys do nothing, the direction it reports is none, and it walks towards the host's position. */
    float way[3] = {1.0f, 1.0f, 1.0f};
    wk_reset(DD_ROLE_GUEST);
    hook_raid_move(wk_display, way);
    CHECK(!wk_moves && way[0] == 0 && way[1] == 0 && way[2] == 0); /* No position yet: stands still. */
    wk_target(100, 2000.0f);
    hook_raid_move(wk_display, way);
    CHECK(wk_moves == 1 && wk_direction[0] == 1.0f && wk_direction[1] == 0 && wk_direction[2] == 0 && way[0] == 0);
    wk_view.pos = 2500.0f;
    hook_raid_move(wk_display, way);
    CHECK(wk_moves == 2 && wk_direction[0] == -1.0f);
    wk_view.pos = 2000.0f + DD_WALK_NEAR - 1.0f; /* Near enough: stops, so it cannot shake around the position. */
    hook_raid_move(wk_display, way);
    CHECK(wk_moves == 2);
    wk_view.pos = 500.0f;
    wk_target(101, 2000.0f); /* The host is already elsewhere: the travel order comes first. */
    hook_raid_move(wk_display, way);
    CHECK(wk_moves == 2);
    /* Without a session or co-op, and for the host: the game's own input and use. */
    wk_reset(DD_ROLE_GUEST);
    g_session_ready = 0;
    wk_target(100, 2000.0f);
    hook_raid_move(wk_display, way);
    CHECK(wk_natives == 1 && !wk_moves && way[0] == 1.0f);
    CHECK(hook_raid_trap(wk_display, wk_props[1]) == 1);
    wk_reset(DD_ROLE_HOST);
    wk_target(100, 2000.0f);
    hook_raid_move(wk_display, way);
    CHECK(wk_natives == 1 && !wk_moves);
        wk_reset(DD_ROLE_OFF);
    hook_raid_move(wk_display, way);
    CHECK(wk_natives == 1 && !wk_moves);
    /* After drift the guest gets its keys back rather than following a dead order. */
    wk_reset(DD_ROLE_GUEST);
    g_town.needs_resync = 1;
    hook_raid_move(wk_display, way);
    CHECK(wk_natives == 1);

    /* Host: every changed position leaves, at most every 100 ms, and one that stands still sends nothing new. */
    wk_reset(DD_ROLE_HOST);
    dd_msg out;
    walk_poll();
    CHECK(queue_pop(&g_outgoing, &out) && out.type == DD_MSG_WALK && out.u.walk.area == 100 && wk_float(out.u.walk.pos) == 500.0f);
    walk_poll();
    CHECK(!queue_pop(&g_outgoing, &out));
    wk_view.pos = 600.0f;
    walk_poll();
    CHECK(!queue_pop(&g_outgoing, &out)); /* Too soon. */
    g_walk_send_at = 0;
    walk_poll();
    CHECK(queue_pop(&g_outgoing, &out) && wk_float(out.u.walk.pos) == 600.0f);
    g_walk_send_at = 0;
    walk_poll();
    CHECK(!queue_pop(&g_outgoing, &out)); /* Standing still: also nothing after the wait. */
    wk_view.area = 101;
    walk_poll();
    CHECK(queue_pop(&g_outgoing, &out) && out.u.walk.area == 101);
    /* The guest sends none. No raid: nothing. */
    wk_reset(DD_ROLE_GUEST);
    walk_poll();
    CHECK(!queue_pop(&g_outgoing, &out));
    wk_reset(DD_ROLE_HOST);
    wk_view_fail = 1;
    walk_poll();
    CHECK(!queue_pop(&g_outgoing, &out));
    /* The host's Rules state goes along with a travel, and the guest takes it over only if it differs. */
    {
        uint8_t rules[0x10] = {0};
        uint32_t state = 0x1111, dest[4] = {101, 2, 0, 0};
        dd_action travel;
        wk_reset(DD_ROLE_HOST);
        g_rules_object = rules;
        memcpy(rules + 8, &state, 4);
        hook_raid_travel(wk_display, dest, NULL, 0, 0);
        CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.action.u.travel.rules == 0x1111);
        travel = last_message.u.town.action;
        wk_reset(DD_ROLE_GUEST);
        g_rules_object = rules;
        state = 0x2222;
        memcpy(rules + 8, &state, 4);
        wk_push(1, &travel);
        dd_runtime_poll();
        memcpy(&state, rules + 8, 4);
        CHECK(wk_travels == 1 && state == 0x1111);
        /* The torch is taken over too; a different bag is reported once, not changed. */
        {
            uint32_t host_torch = 0x42c80000, here = 0x42480000, now = 0;
            dd_action t = travel;
            t.u.travel.torch = host_torch;
            t.u.travel.bag = 0x77;
            wk_reset(DD_ROLE_GUEST);
            g_rules_object = rules;
            memcpy(wk_raid_lists + DD_WALK_TORCH, &here, 4);
            /* A different bag: the host's is asked for, nothing said yet. */
            { unsigned before = (unsigned)alerts; wk_push(1, &t); dd_runtime_poll(); CHECK((unsigned)alerts == before && g_walk_bag_asked && !g_walk_bag_told && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST && last_message.u.town.action.u.item.op == DD_ITEM_OP_BAG_ASK); }
            memcpy(&now, wk_raid_lists + DD_WALK_TORCH, 4);
            CHECK(now == host_torch);
            /* Still different at the next door: that could not be repaired, and it is said once. */
            { unsigned before = (unsigned)alerts; wk_push(2, &t); dd_runtime_poll(); CHECK((unsigned)alerts == before + 1 && g_walk_bag_told); }
            { unsigned before = (unsigned)alerts; wk_push(3, &t); dd_runtime_poll(); CHECK((unsigned)alerts == before); }
            /* An empty bag hashes the same on both: nothing said. */
            t.u.travel.bag = walk_bag_hash((uintptr_t)wk_raid_lists);
            wk_reset(DD_ROLE_GUEST);
            g_rules_object = rules;
            wk_push(1, &t);
            dd_runtime_poll();
            CHECK(!g_walk_bag_told);
            /* The hash sees count and kind. */
            {
                static uint8_t items[2 * 0x5a8];
                uint64_t begin = (uintptr_t)items, end = (uintptr_t)(items + sizeof items);
                uint32_t one = 1, two = 2;
                memset(items, 0, sizeof items);
                memcpy(items, &one, 4);
                memcpy(wk_raid_lists + 0x220, &begin, 8);
                memcpy(wk_raid_lists + 0x228, &end, 8);
                uint32_t first = walk_bag_hash((uintptr_t)wk_raid_lists);
                memcpy(items, &two, 4);
                CHECK(walk_bag_hash((uintptr_t)wk_raid_lists) != first);
                memcpy(items, &one, 4);
                memcpy(items + 0x44, &two, 4);
                CHECK(walk_bag_hash((uintptr_t)wk_raid_lists) != first);
                memset(wk_raid_lists + 0x220, 0, 16);
            }
        }
        /* Not known: left alone. */
        travel.u.travel.rules = 0;
        wk_reset(DD_ROLE_GUEST);
        g_rules_object = rules;
        state = 0x2222;
        memcpy(rules + 8, &state, 4);
        wk_push(1, &travel);
        dd_runtime_poll();
        memcpy(&state, rules + 8, 4);
        CHECK(wk_travels == 1 && state == 0x2222);
        g_rules_object = NULL;
    }
    /* Both logs show area, tile and Rules state; one line per change. */
    {
        uint8_t rules[0x10] = {0};
        wk_reset(DD_ROLE_GUEST);
        g_rules_object = rules;
        walk_poll();
        CHECK(g_walk_log_area == 100 && g_walk_log_tile == 1);
        wk_view.pos = 1100.0f;
        walk_poll();
        CHECK(g_walk_log_tile == 2);
        g_rules_object = NULL;
    }

    /* A fight is written down turn by turn, and only that. */
    {
        uint64_t turns = (uintptr_t)wk_party;
        uint32_t turn = 0, round = 1;
        wk_reset(DD_ROLE_GUEST);
        memcpy(wk_raid_lists + 0x4898 + 0x20, &turns, 8);
        memcpy(wk_raid_lists + 0x4898 + 0xa0, &turn, 4);
        memcpy(wk_raid_lists + 0x4898 + 0xa8, &round, 4);
        wk_view.battle = 0x1c;
        walk_poll();
        CHECK(g_battle_logging && g_battle_log_round == 1 && !g_battle_log_turn && !sent);
        turn = 1;
        memcpy(wk_raid_lists + 0x4898 + 0xa0, &turn, 4);
        walk_poll();
        CHECK(g_battle_log_turn == 1);
        wk_view.battle = 0;
        walk_poll();
        CHECK(!g_battle_logging && !sent);
    }
    /* Host travels: an order goes first with the same values, then the game's own call. */
    uint32_t dest[4] = {101, 3, 1, 0};
    wk_reset(DD_ROLE_HOST);
    hook_raid_travel(wk_display, dest, wk_areas[0], 1, 0);
    CHECK(sent == 1 && wk_travels == 1 && wk_from == wk_areas[0] && wk_flip == 1 && !wk_extra);
    dd_action order = last_message.u.town.action;
    CHECK(last_message.type == DD_MSG_TOWN_APPLY && order.kind == DD_ACT_TRAVEL && order.u.travel.dest[0] == 101 &&
          order.u.travel.dest[1] == 3 && order.u.travel.dest[2] == 1 && order.u.travel.from_area == 100 && order.u.travel.has_from == 1);
    /* In these states the game's call does nothing, so nothing is shared. */
    for (uint32_t state = 0; state <= 7; state++) {
        wk_reset(DD_ROLE_HOST);
        memcpy(wk_display + 0x40b0, &state, 4);
        hook_raid_travel(wk_display, dest, NULL, 0, 0);
        CHECK(wk_travels == 1 && sent == (state > 1 && state != 6 && state != 7 ? 1 : 0));
    }
    /* The guest follows with the same values, and the area pointer of its own copy. */
    wk_reset(DD_ROLE_GUEST);
    { dd_msg m = {0}; m.type = DD_MSG_TOWN_APPLY; m.seq = 1; m.u.town.action = order; CHECK(queue_push(&g_incoming, &m)); }
    dd_runtime_poll();
    CHECK(wk_travels == 1 && wk_dest[0] == 101 && wk_dest[1] == 3 && wk_dest[2] == 1 && wk_from == wk_areas[0] && wk_flip == 1);
    CHECK(g_town.applied_seq == 1 && !g_town.needs_resync);
    /* No from-area stays none. */
    wk_reset(DD_ROLE_GUEST);
    { dd_action plain = wk_order(101, 0, 0); dd_msg m = {0}; m.type = DD_MSG_TOWN_APPLY; m.seq = 1; m.u.town.action = plain; CHECK(queue_push(&g_incoming, &m)); }
    dd_runtime_poll();
    CHECK(wk_travels == 1 && !wk_from);
    /* It waits while loading, fading or fighting, and what follows waits too. */
    {
        static const struct {int fail; uint32_t state, battle;} waits[] = {{1, 2, 0}, {0, 0, 0}, {0, 1, 0}, {0, 6, 0}, {0, 7, 0}, {0, 2, 1}};
        for (unsigned i = 0; i < sizeof waits / sizeof *waits; i++) {
            wk_reset(DD_ROLE_GUEST);
            wk_view_fail = waits[i].fail;
            wk_view.state = waits[i].state;
            wk_view.battle = waits[i].battle;
            { dd_msg m = {0}; m.type = DD_MSG_TOWN_APPLY; m.seq = 1; m.u.town.action = order; CHECK(queue_push(&g_incoming, &m)); }
            dd_runtime_poll();
            CHECK(!wk_travels && !g_town.applied_seq && g_incoming.count == 1 && !g_town.needs_resync);
            wk_view_fail = 0;
            wk_view.state = 2;
            wk_view.battle = 0;
            dd_runtime_poll();
            CHECK(wk_travels == 1 && g_town.applied_seq == 1 && !g_incoming.count);
        }
    }
    /* Already there because it walked to the corridor's end: no second move, also when the fade is still running. */
    wk_reset(DD_ROLE_GUEST);
    wk_view.area = 101;
    wk_view.state = 1;
    { dd_msg m = {0}; m.type = DD_MSG_TOWN_APPLY; m.seq = 1; m.u.town.action = order; CHECK(queue_push(&g_incoming, &m)); }
    dd_runtime_poll();
    CHECK(!wk_travels && g_town.applied_seq == 1 && !g_town.needs_resync);
    /* Another map: stopped, and the player is told. */
    wk_reset(DD_ROLE_GUEST);
    { uint32_t other = 999; memcpy(wk_areas[0] + 0x18, &other, 4); }
    { dd_msg m = {0}; m.type = DD_MSG_TOWN_APPLY; m.seq = 1; m.u.town.action = order; CHECK(queue_push(&g_incoming, &m)); }
    dd_runtime_poll();
    CHECK(!wk_travels && g_town.needs_resync && alerts);
    /* The game's own travel call passes on the guest, unshared. */
    wk_reset(DD_ROLE_GUEST);
    wk_refuse_caller = 0;
    hook_raid_travel(wk_display, dest, NULL, 0, 0);
    CHECK(wk_travels == 1 && !sent);
    /* Without a session: the game alone. */
    wk_reset(DD_ROLE_HOST);
    g_session_ready = 0;
    hook_raid_travel(wk_display, dest, NULL, 0, 0);
    CHECK(wk_travels == 1 && !sent);
    /* A real area search, on its bounds. */
    {
        uint64_t found = 0, end = (uintptr_t)(wk_area_list + 3), begin = (uintptr_t)wk_area_list + 1;
        wk_reset(DD_ROLE_HOST);
        CHECK(walk_area_pointer((uintptr_t)wk_raid_lists, 102, &found) && found == (uintptr_t)wk_areas[2]);
        CHECK(!walk_area_pointer((uintptr_t)wk_raid_lists, 103, &found) && !found);
        memcpy(wk_raid_lists + 0x740, &begin, 8);
        CHECK(!walk_area_pointer((uintptr_t)wk_raid_lists, 102, &found)); /* A list that is not whole pointers. */
        (void)end;
    }
    g_walk_view = walk_read;
    g_walk_player_travel = walk_player_travel;
    g_party_move = NULL;
    use_transactions();
    g_raid_move = NULL;
    g_raid_use = NULL;
    g_raid_trap = NULL;
    g_overlay_use = NULL;
    g_party_select = NULL;
    g_raid_travel = NULL;
    walk_reset();
    puts("Walking: the guest follows the host's position through the game's own move; travel is one ordered "
         "call with the same values, waits for loading, fades and fights");
}
