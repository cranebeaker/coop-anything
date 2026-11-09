/* Walking and using things together. See runtime/DUNGEON.md, "Walking".
 * One copy decides: the host's party moves on either player's keys, and the guest's party follows the host's position through the
 * game's own Party::Move. So its tiles, steps and tile events (traps too) come from the game's own code, in the same order. */
typedef void(__fastcall *party_move_fn)(void *, const float *);
typedef void(__fastcall *raid_move_fn)(void *, float *);
typedef uint8_t(__fastcall *raid_use_fn)(void *, void *, int32_t, uint8_t);
typedef uint8_t(__fastcall *raid_use_one_fn)(void *, void *);
typedef void(__fastcall *raid_travel_fn)(void *, const uint32_t *, void *, uint8_t, uint8_t);
typedef void(__fastcall *overlay_use_fn)(void *);
typedef void(__fastcall *party_select_fn)(void *, int32_t);
typedef struct {
    uint64_t raid, display, area_ptr;
    uint32_t area, state, battle;
    float pos;
} walk_view;
static party_move_fn g_party_move;
static raid_move_fn g_raid_move;
static raid_use_fn g_raid_use;
static raid_use_one_fn g_raid_trap;
static raid_travel_fn g_raid_travel;
static overlay_use_fn g_overlay_use, g_curio_button;
static party_select_fn g_party_select;
static volatile LONG64 g_walk_target;     /* Area id and position bits of the host's last message; written by the network thread. */
static volatile LONG g_walk_intent;       /* On the host: the direction the guest's keys ask for. */
static volatile LONG64 g_walk_intent_at;  /* When it arrived. It runs out, so a lost guest does not keep walking the party. */
static int g_walk_capture, g_walk_captured, g_walk_intent_sent;
static uint32_t g_walk_sent_area, g_walk_sent_pos, g_walk_log_area;
static int g_walk_log_tile, g_walk_sent_any, g_walk_gate_told;
static uint32_t g_battle_log_state, g_battle_log_round, g_battle_log_turn;
static int g_battle_logging;
static uint64_t g_walk_send_at, g_walk_intent_send_at, g_walk_use_wait_until;
#define DD_WALK_TORCH 0x5f14u
static uint64_t g_walk_door_pending;
static int g_walk_bag_told, g_walk_bag_asked;
static int g_walk_party_told, g_walk_trinket_told;
static int g_town_check_raid; /* a raid was seen: back in town, the host sends its town to compare */
static uint64_t g_town_check_at;
static void town_check_poll(void);
static uint32_t g_walk_asked_prop, g_walk_asked_door;
static int g_walk_selected = -1; /* The hero this copy last knew as selected, outside fights. */
static uint64_t g_walk_asked_at, g_walk_door_until;
#define DD_WALK_NEAR 8.0f /* Game units; a tile is 720 wide. */
#define DD_WALK_USE_NEAR 24.0f
#define DD_WALK_RAID_RVA 0x118e2c8u
#define DD_WALK_DISPLAY_RVA 0x118e570u
#define DD_WALK_OVERLAY 0x20b8u /* The curio overlay in the display (7748bf). */
/* Both pointers only exist from just before RaidStart until the raid is left (3d343c, 3d3813; 3d34c3, 3d385f). */
static int walk_read(walk_view *v) {
    memset(v, 0, sizeof *v);
    return safe_read(image_base() + DD_WALK_RAID_RVA, &v->raid, 8) && v->raid &&
           safe_read(image_base() + DD_WALK_DISPLAY_RVA, &v->display, 8) && v->display &&
           safe_read((uintptr_t)v->raid + 0x55d0, &v->area_ptr, 8) && v->area_ptr &&
           safe_read((uintptr_t)v->area_ptr + 0x18, &v->area, 4) && safe_read((uintptr_t)v->raid + 0x294, &v->pos, 4) &&
           safe_read((uintptr_t)v->raid + 0x4b20, &v->battle, 4) && safe_read((uintptr_t)v->display + 0x40b0, &v->state, 4);
}
static int (*g_walk_view)(walk_view *) = walk_read;
static void battle_reset_hook(void);
static void formation_reset(void);
static void walk_reset(void) {
    formation_reset();
    InterlockedExchange64(&g_walk_target, 0);
    InterlockedExchange(&g_walk_intent, 0);
    InterlockedExchange64(&g_walk_intent_at, 0);
    g_walk_capture = g_walk_captured = g_walk_intent_sent = 0;
    g_walk_sent_area = g_walk_sent_pos = g_walk_log_area = 0;
    g_walk_log_tile = -1;
    g_walk_sent_any = g_walk_gate_told = 0;
    g_battle_logging = 0;
    g_walk_send_at = g_walk_intent_send_at = g_walk_use_wait_until = 0;
    g_walk_asked_at = g_walk_door_until = g_walk_door_pending = 0;
    g_walk_bag_told = g_walk_bag_asked = 0;
    g_walk_party_told = g_walk_trinket_told = 0;
    g_town_check_raid = 0;
    g_town_check_at = 0;
    g_walk_selected = -1;
    battle_reset_hook();
}
static int walk_shared(void) {
    return g_cfg.role != DD_ROLE_OFF && InterlockedCompareExchange(&g_campaign_active, 0, 0) &&
           InterlockedCompareExchange(&g_session_ready, 0, 0) && !g_town.needs_resync && g_party_move;
}
static int walk_following(void) { return g_cfg.role == DD_ROLE_GUEST && walk_shared(); }
/* Network thread: only the newest position matters, so it does not go through the order queue. */
static void walk_receive(const dd_msg *m) {
    InterlockedExchange64(&g_walk_target, (LONG64)(((uint64_t)m->u.walk.area << 32) | m->u.walk.pos));
}
static void walk_intent_receive(const dd_msg *m) {
    InterlockedExchange(&g_walk_intent, m->u.intent.direction);
    InterlockedExchange64(&g_walk_intent_at, (LONG64)GetTickCount64());
}
static void walk_chase(const walk_view *v) {
    uint64_t target = (uint64_t)InterlockedCompareExchange64(&g_walk_target, 0, 0);
    uint32_t bits = (uint32_t)target;
    float pos, delta, direction[3] = {0, 0, 0};
    if (!target || (uint32_t)(target >> 32) != v->area) return; /* Another area: the travel order comes first. */
    memcpy(&pos, &bits, 4);
    delta = pos - v->pos;
    if (delta < DD_WALK_NEAR && delta > -DD_WALK_NEAR) return;
    direction[0] = delta > 0 ? 1.0f : -1.0f; /* What the game's own input (75b530) passes for D and A. */
    g_party_move((void *)(uintptr_t)(v->raid + 8), direction);
}
/* 604af0. While the guest's own input runs, a move is only a wish: the game has checked the corridor's ends and blocked tiles for it. */
static void __fastcall hook_party_move(void *party, const float *direction) {
    if (g_walk_capture) {
        float x = 0;
        memcpy(&x, direction, sizeof x);
        g_walk_captured = x > 0 ? 1 : x < 0 ? -1 : 0;
        return;
    }
    if (g_party_move) g_party_move(party, direction);
}
static void walk_send_intent(int direction) {
    uint64_t now = GetTickCount64();
    if (direction == g_walk_intent_sent && (!direction || now < g_walk_intent_send_at)) return;
    dd_msg m;
    memset(&m, 0, sizeof m);
    m.type = DD_MSG_WALK_INTENT;
    m.u.intent.direction = direction;
    if (!queue_push(&g_outgoing, &m)) return;
    g_walk_intent_sent = direction;
    g_walk_intent_send_at = now + 100;
}
/* 75b530 reads the keys and moves the party; 7657a0 only calls it while the player may act, so not in a dialog or fight.
 * Only this is replaced: the rest of 7657a0 also makes the other three heroes follow the first, and must keep running. */
static void __fastcall hook_raid_move(void *display, float *out) {
    walk_view v;
    if (!g_raid_move) return;
    if (!g_game_apply_depth && walk_shared() && formation_busy()) { memset(out,0,3*sizeof *out); return; }
    if (walk_following()) {
        g_walk_capture = 1;
        g_walk_captured = 0;
        g_raid_move(display, out);
        g_walk_capture = 0;
        walk_send_intent(g_walk_captured);
        memset(out, 0, 3 * sizeof *out); /* No direction for what follows in 7657a0: this copy moves through the chase. */
        if (g_walk_view(&v) && (uintptr_t)display == v.display) walk_chase(&v);
        return;
    }
    if (g_cfg.role == DD_ROLE_HOST && walk_shared() && g_walk_view(&v) && (uintptr_t)display == v.display) {
        /* Capture before moving, just as on GAST. Neither player's conflicting
         * direction wins; agreeing inputs still produce only one native move. */
        g_walk_capture = 1;
        g_walk_captured = 0;
        g_raid_move(display, out);
        g_walk_capture = 0;
        int mine = g_walk_captured;
        LONG wish = InterlockedCompareExchange(&g_walk_intent, 0, 0);
        uint64_t at = (uint64_t)InterlockedCompareExchange64(&g_walk_intent_at, 0, 0);
        if (GetTickCount64() - at > 300) wish = 0;
        int chosen = mine && wish && mine != wish ? 0 : mine ? mine : (int)wish;
        float direction[3] = {(float)chosen, 0, 0};
        memcpy(out, direction, sizeof direction);
        if (chosen) g_party_move((void *)(uintptr_t)(v.raid + 8), direction);
        return;
    }
    g_raid_move(display, out);
}
/* ---- Using a prop (curio, door, obstacle). The result is decided by the game's own code on each copy, at the same place in the order. ---- */
static int walk_prop_index(uint64_t area, uint64_t prop, uint32_t *index) {
    uint64_t begin = 0, end = 0;
    if (!safe_read((uintptr_t)area + 0x118, &begin, 8) || !safe_read((uintptr_t)area + 0x120, &end, 8) || end < begin || (end - begin) % 8 ||
        (end - begin) / 8 > 4096)
        return 0;
    for (uint32_t i = 0; begin + (uint64_t)i * 8 < end; i++) {
        uint64_t p = 0;
        if (!safe_read((uintptr_t)(begin + (uint64_t)i * 8), &p, 8)) return 0;
        if (p == prop) {
            *index = i;
            return 1;
        }
    }
    return 0;
}
static uint64_t walk_prop_at(uint64_t area, uint32_t index) {
    uint64_t begin = 0, end = 0, p = 0;
    if (!safe_read((uintptr_t)area + 0x118, &begin, 8) || !safe_read((uintptr_t)area + 0x120, &end, 8) || end < begin || (end - begin) % 8 ||
        (uint64_t)index >= (end - begin) / 8 || !safe_read((uintptr_t)(begin + (uint64_t)index * 8), &p, 8))
        return 0;
    return p;
}
static int walk_hero_rank(uint64_t raid, uint64_t hero, uint8_t *rank) {
    uint64_t begin = 0, end = 0;
    if (!safe_read((uintptr_t)raid + 8, &begin, 8) || !safe_read((uintptr_t)raid + 0x10, &end, 8) || end < begin || (end - begin) / 8 > 4) return 0;
    for (uint8_t i = 0; begin + (uint64_t)i * 8 < end; i++) {
        uint64_t p = 0;
        if (!safe_read((uintptr_t)(begin + (uint64_t)i * 8), &p, 8)) return 0;
        if (p == hero) {
            *rank = i;
            return 1;
        }
    }
    return 0;
}
static int walk_use_order(const walk_view *v, uint64_t prop, uint64_t hero, int32_t option, uint8_t flag, uint8_t door, dd_action *a) {
    memset(a, 0, sizeof *a);
    a->kind = DD_ACT_USE;
    a->u.use.area = v->area;
    a->u.use.option = (uint32_t)option;
    a->u.use.flag = flag != 0;
    a->u.use.door = door;
    memcpy(&a->u.use.pos, &v->pos, 4);
    if (!walk_prop_index(v->area_ptr, prop, &a->u.use.prop) || !safe_read((uintptr_t)prop + 0x0c, &a->u.use.prop_kind, 4)) return 0;
    if (door != 2 && !walk_hero_rank(v->raid, hero, &a->u.use.hero)) return 0;
    return dd_action_valid(a);
}
/* A key held down clicks every frame, and the guest's click does not change its screen, so the next frame clicks again (owner's run: 23 door requests in 100 ms). */
static int walk_use_repeat(const dd_action *a) {
    uint64_t now = GetTickCount64();
    if (g_cfg.role != DD_ROLE_GUEST) return 0;
    if (g_walk_asked_at && now - g_walk_asked_at < 1000 && g_walk_asked_prop == a->u.use.prop && g_walk_asked_door == a->u.use.door) return 1;
    g_walk_asked_prop = a->u.use.prop;
    g_walk_asked_door = a->u.use.door;
    g_walk_asked_at = now;
    return 0;
}
/* The host's Rules number, where the next use stands. Numbers can go apart on the way (a tile passed later, something unshared) and the use decides what a chest holds. The guest takes it over
 * when it has reached that spot, right before the use that follows in the queue. Not in a fight: that runs in step by itself. */
static void walk_rules_send(const walk_view *v) {
    dd_action a;
    uint32_t rules = 0;
    if (g_cfg.role != DD_ROLE_HOST || v->battle || !g_rules_object || !safe_read((uintptr_t)g_rules_object + 8, &rules, 4)) return;
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ITEM;
    a.u.item.op = DD_ITEM_OP_RULES;
    a.u.item.hero = 255;
    a.u.item.area = v->area;
    memcpy(&a.u.item.pos, &v->pos, 4);
    a.u.item.rules = rules;
    dd_town_host_observed(&g_town, &a);
}
static void walk_use_send(const dd_action *a) {
    if (walk_use_repeat(a)) return;
    dd_log("Walk: use prop %u of area %08x, option %u, hero rank %u, door %u (role=%s)", a->u.use.prop, a->u.use.area, a->u.use.option, a->u.use.hero, a->u.use.door,
           dd_role_name(g_cfg.role));
    if (g_cfg.role == DD_ROLE_GUEST) dd_town_guest_local(&g_town, a);
    else dd_town_host_observed(&g_town, a);
}
#define DD_WALK_DOOR_TABLE 0xeaa2d8u /* DoorProp's vtable. */
/* A door's result (4bef80) walks the party to the door and only then travels (779620, 77d7b0), with the prop in its callback. So a door must run on
 * one copy only: run on both, the first to arrive moved the other, whose callback then read a prop that was gone (owner's run on one PC, crash at 77014c). */
static int walk_is_door(uint64_t prop) {
    uint64_t table = 0;
    return safe_read((uintptr_t)prop, &table, 8) && table == image_base() + DD_WALK_DOOR_TABLE;
}
/* 7747c0: clicking a prop. Without an overlay (+124) it interacts at once. With one, a curio without a choice runs on the overlay's next update
 * (712b70, 7107c0, 7111e0) with no further click, so the click itself is the moment to share. Owner's run on one PC: curios ran on one copy only. */
static uint8_t __fastcall hook_raid_use(void *display, void *prop, int32_t option, uint8_t flag) {
    walk_view v;
    uint8_t overlay = 0;
    uint64_t hero = 0;
    dd_action a;
    if (!g_raid_use) return 0;
    if (!g_game_apply_depth && walk_shared() && formation_busy()) return 0;
    if (g_game_apply_depth || !walk_shared() || !safe_read((uintptr_t)prop + 0x124, &overlay, 1)) return g_raid_use(display, prop, option, flag);
    if (walk_is_door((uintptr_t)prop)) {
        if (g_cfg.role == DD_ROLE_HOST) {
            uint8_t opened = g_raid_use(display, prop, option, flag);
            if (opened) {
                g_walk_door_until = GetTickCount64() + 3000;
                if (g_walk_view(&v) && walk_use_order(&v, (uintptr_t)prop, 0, option, flag, 2, &a)) walk_use_send(&a);
            }
            return opened;
        }
        if (!g_walk_view(&v) || !walk_use_order(&v, (uintptr_t)prop, 0, option, flag, 2, &a)) {
            dd_log("Walk: door not readable; not shared");
            return 0;
        }
        walk_use_send(&a);
        return 1;
    }
    if (!g_walk_view(&v) || !safe_read((uintptr_t)v.raid + 0x438, &hero, 8) || !walk_use_order(&v, (uintptr_t)prop, hero, option, flag, 0, &a)) {
        dd_log("Walk: use not readable; left to the game alone");
        return g_raid_use(display, prop, option, flag);
    }
    if (g_cfg.role == DD_ROLE_GUEST) {
        walk_use_send(&a);
        return 1;
    }
    walk_rules_send(&v);
    uint8_t done = g_raid_use(display, prop, option, flag);
    if (done) walk_use_send(&a);
    return done;
}
/* 774410: RaidDisplay::InteractWithTrap, clicking a trap to disarm it. The overlay then draws the attempt by itself, so the click is the order, like a curio's.
 * Owner's run on one PC: the host disarmed alone, its Rules numbers went on without the guest, and every result after that differed. */
static uint8_t __fastcall hook_raid_trap(void *display, void *prop) {
    walk_view v;
    dd_action a;
    uint64_t hero = 0;
    if (!g_raid_trap) return 0;
    if (g_game_apply_depth || !walk_shared()) return g_raid_trap(display, prop);
    if (!g_walk_view(&v) || !safe_read((uintptr_t)v.raid + 0x438, &hero, 8) || !walk_use_order(&v, (uintptr_t)prop, hero, 0, 0, 1, &a)) {
        dd_log("Walk: trap not readable; not disarmed");
        return 0;
    }
    if (g_cfg.role == DD_ROLE_GUEST) {
        walk_use_send(&a);
        return 1;
    }
    walk_rules_send(&v);
    uint8_t done = g_raid_trap(display, prop);
    if (done) walk_use_send(&a);
    return done;
}
/* 714770: the overlay's button, a functor with the overlay at +8. The prop is at +d70 and the hero at +d78 while +d88 is 2. */
static void __fastcall hook_overlay_use(void *functor) {
    walk_view v;
    uint64_t panel = 0, prop = 0, hero = 0;
    uint32_t mode = 0;
    dd_action a;
    if (!g_overlay_use) return;
    if (g_game_apply_depth || !walk_shared() || !safe_read((uintptr_t)functor + 8, &panel, 8) || !safe_read((uintptr_t)panel + 0xd88, &mode, 4) || mode != 2 ||
        !safe_read((uintptr_t)panel + 0xd70, &prop, 8) || !safe_read((uintptr_t)panel + 0xd78, &hero, 8) || !g_walk_view(&v) ||
        !walk_use_order(&v, prop, hero, 0, 0, 0, &a)) {
        g_overlay_use(functor);
        return;
    }
    walk_rules_send(&v);
    walk_use_send(&a);
    if (g_cfg.role == DD_ROLE_HOST) g_overlay_use(functor); /* The guest's overlay closes when the order comes back. */
}
/* 714360: the curio overlay's button, once an item or the hand is picked: a functor with the overlay at +8. Prop at +6a0, hero at +d78, item at +76c and +770. */
static void __fastcall hook_curio_button(void *functor) {
    walk_view v;
    uint64_t panel = 0, prop = 0, hero = 0;
    uint32_t mode = 0;
    dd_action a;
    if (!g_curio_button) return;
    if (g_game_apply_depth || !walk_shared() || !safe_read((uintptr_t)functor + 8, &panel, 8) || !safe_read((uintptr_t)panel + 0xd88, &mode, 4) || mode != 2 ||
        !safe_read((uintptr_t)panel + 0x6a0, &prop, 8) || !prop || !safe_read((uintptr_t)panel + 0xd78, &hero, 8) || !g_walk_view(&v)) {
        g_curio_button(functor);
        return;
    }
    if (!hero) safe_read((uintptr_t)v.raid + 0x438, &hero, 8);
    if (!walk_use_order(&v, prop, hero, 0, 0, 0, &a)) {
        g_curio_button(functor);
        return;
    }
    a.u.use.door = 3;
    safe_read((uintptr_t)panel + 0x76c, &a.u.use.item_type, 4);
    safe_read((uintptr_t)panel + 0x770, &a.u.use.item_id, 4);
    dd_log("Walk: curio button, item type %u id %u", a.u.use.item_type, a.u.use.item_id);
    walk_rules_send(&v);
    walk_use_send(&a);
    if (g_cfg.role == DD_ROLE_HOST) g_curio_button(functor);
}
/* 70ff50: the overlay's button for a tool of the party's (a shovel on rubble), its functor's call 714640 jumps here. It takes one from the bag and interacts: both must happen on both copies, and
 * nothing shared them (owner's run of 6 Nov: each copy cleared the rubble by itself, 19 s apart). Capture: the overlay at +0, a copy of the item at +8 (kind at +4c, id at +90). */
typedef void(__fastcall *curio_tool_fn)(void *);
static curio_tool_fn g_curio_tool;
static void __fastcall hook_curio_tool(void *capture) {
    walk_view v;
    uint64_t panel = 0, prop = 0, hero = 0;
    uint32_t mode = 0;
    dd_action a;
    if (!g_curio_tool) return;
    if (g_game_apply_depth || !walk_shared() || !safe_read((uintptr_t)capture, &panel, 8) || !safe_read((uintptr_t)panel + 0xd88, &mode, 4) || mode != 2 ||
        !safe_read((uintptr_t)panel + 0xd70, &prop, 8) || !prop || !safe_read((uintptr_t)panel + 0xd78, &hero, 8) || !g_walk_view(&v)) {
        g_curio_tool(capture);
        return;
    }
    if (!hero) safe_read((uintptr_t)v.raid + 0x438, &hero, 8);
    if (!walk_use_order(&v, prop, hero, 0, 0, 4, &a)) {
        dd_log("Walk: tool button not readable; left to the game alone");
        g_curio_tool(capture);
        return;
    }
    safe_read((uintptr_t)capture + 0x4c, &a.u.use.item_type, 4);
    safe_read((uintptr_t)capture + 0x90, &a.u.use.item_id, 4);
    walk_rules_send(&v);
    walk_use_send(&a);
    if (g_cfg.role == DD_ROLE_HOST) g_curio_tool(capture);
}
/* Selecting a hero on this copy: the display sets the selection from its own field every frame (75a5f3). Writes down what is now selected, so it is not sent back. */
static void walk_select(const walk_view *v, uint8_t rank) {
    uint64_t begin = 0, end = 0;
    if (!g_party_select || !safe_read((uintptr_t)v->raid + 8, &begin, 8) || !safe_read((uintptr_t)v->raid + 0x10, &end, 8) || end < begin || (uint64_t)rank >= (end - begin) / 8) return;
    int32_t field = (int32_t)((end - begin) / 8) - 1 - rank;
    memcpy((void *)(uintptr_t)(v->display + 0x3f88), &field, 4);
    g_party_select((void *)(uintptr_t)(v->raid + 8), rank);
    g_walk_selected = (int)rank;
}
static uint64_t item_in_bag(uint64_t raid, uint32_t type, uint32_t id);
static int walk_use_apply(const dd_action *a) {
    walk_view v;
    uint64_t prop = 0, begin = 0, end = 0, hero = 0;
    uint32_t kind = 0;
    if (!g_walk_view(&v) || v.area != a->u.use.area || !(prop = walk_prop_at(v.area_ptr, a->u.use.prop)) ||
        !safe_read((uintptr_t)prop + 0x0c, &kind, 4) || kind != a->u.use.prop_kind) {
        dd_log("Walk: prop %u of area %08x is not here", a->u.use.prop, a->u.use.area);
        return 0;
    }
    if (a->u.use.door != 2) walk_rules_send(&v); /* A guest's wish: the numbers go first, then the order itself. */
    if (a->u.use.door == 2) {
        if (g_cfg.role == DD_ROLE_GUEST) {
            /* The door runs here too, so the walk through it shows and its numbers are drawn. Its callback then comes to hook_raid_travel, where it ends: the host's travel order moves
             * this copy, and waits for that callback, because a callback outliving its area was the crash at 77014c (duo5). */
            g_walk_door_pending = GetTickCount64() + 6000;
            if (!g_raid_use || !g_raid_use((void *)(uintptr_t)v.display, (void *)(uintptr_t)prop, (int32_t)a->u.use.option, a->u.use.flag)) {
                g_walk_door_pending = 0;
                dd_log("Walk: the game declined the door here (position %f)", v.pos);
            }
            return 1;
        }
        /* A door walks the party there and only then travels, with the prop in its callback. A second one still running after the first has travelled reads a prop that is gone (crash at 6087e9, 77d8fe). */
        if (GetTickCount64() < g_walk_door_until) {
            dd_log("Walk: a door is already being taken; prop %u refused", a->u.use.prop);
            return 0;
        }
        if (!g_raid_use || !g_raid_use((void *)(uintptr_t)v.display, (void *)(uintptr_t)prop, (int32_t)a->u.use.option, a->u.use.flag)) return 0;
        g_walk_door_until = GetTickCount64() + 3000;
        return 1;
    }
    if (!safe_read((uintptr_t)v.raid + 8, &begin, 8) || !safe_read((uintptr_t)v.raid + 0x10, &end, 8) || end < begin ||
        (uint64_t)a->u.use.hero >= (end - begin) / 8 || !safe_read((uintptr_t)(begin + (uint64_t)a->u.use.hero * 8), &hero, 8) || !hero || !g_party_select)
        return 0;
    /* The same hero acts on both copies: the display sets the selection from its own field every frame (75a5f3). */
    walk_select(&v, a->u.use.hero);
    uint64_t panel = v.display + DD_WALK_OVERLAY, open_prop = 0;
    uint32_t mode = 0;
    if (a->u.use.door == 1) {
        uint8_t disarming = g_raid_trap ? g_raid_trap((void *)(uintptr_t)v.display, (void *)(uintptr_t)prop) : 0;
        if (!disarming) dd_log("Walk: the game declined trap %u here (position %f)", a->u.use.prop, v.pos);
        return g_cfg.role == DD_ROLE_HOST ? disarming : 1;
    }
    if (a->u.use.door == 3) {
        uint64_t functor[2] = {0, panel};
        if (!safe_read((uintptr_t)panel + 0x6a0, &open_prop, 8) || open_prop != prop || !safe_read((uintptr_t)panel + 0xd88, &mode, 4) || mode != 2 || !g_curio_button) {
            dd_log("Walk: the curio overlay is not open here");
            return g_cfg.role == DD_ROLE_HOST ? 0 : 1;
        }
        memcpy((void *)(uintptr_t)(panel + 0x76c), &a->u.use.item_type, 4);
        memcpy((void *)(uintptr_t)(panel + 0x770), &a->u.use.item_id, 4);
        memcpy((void *)(uintptr_t)(panel + 0xd78), &hero, 8);
        g_curio_button(functor);
        return 1;
    }
    if (a->u.use.door == 4) {
        static uint8_t capture[8 + 0x5a8];
        uint64_t item = item_in_bag(v.raid, a->u.use.item_type, a->u.use.item_id);
        if (!safe_read((uintptr_t)panel + 0xd70, &open_prop, 8) || open_prop != prop || !safe_read((uintptr_t)panel + 0xd88, &mode, 4) || mode != 2 || !item || !g_curio_tool) {
            dd_log("Walk: no overlay or tool for prop %u here (overlay prop %llx, mode %u, item %llx)", a->u.use.prop, (unsigned long long)open_prop, mode, (unsigned long long)item);
            return g_cfg.role == DD_ROLE_HOST ? 0 : 1;
        }
        memcpy(capture, &panel, 8);
        memcpy(capture + 8, (void *)(uintptr_t)item, 0x5a8);
        memcpy((void *)(uintptr_t)(panel + 0xd78), &hero, 8);
        dd_log("Walk: tool %08x on prop %u (role=%s)", a->u.use.item_type, a->u.use.prop, dd_role_name(g_cfg.role));
        g_curio_tool(capture);
        return 1;
    }
    if (safe_read((uintptr_t)panel + 0xd88, &mode, 4) && mode == 2 && safe_read((uintptr_t)panel + 0xd70, &open_prop, 8) && open_prop == prop && g_overlay_use) {
        uint64_t functor[2] = {0, panel};
        memcpy((void *)(uintptr_t)(panel + 0xd78), &hero, 8);
        g_overlay_use(functor); /* It interacts, runs the result and closes itself. */
        return 1;
    }
    /* The game's own click, here too: it opens the same overlay or interacts, and decides by its own draws. */
    uint8_t done = g_raid_use ? g_raid_use((void *)(uintptr_t)v.display, (void *)(uintptr_t)prop, (int32_t)a->u.use.option, a->u.use.flag) : 0;
    if (!done) dd_log("Walk: the game declined prop %u here (position %f)", a->u.use.prop, v.pos);
    return g_cfg.role == DD_ROLE_HOST ? done : 1;
}
/* 770080: every move to another room or corridor. A door or the map on the guest asks the host; the game's own calls pass, unshared. */
static int walk_player_travel(uintptr_t rva) { return rva == 0x7427ae || rva == 0x77d84f || rva == 0x77e3ea; }
static int (*g_walk_player_travel)(uintptr_t) = walk_player_travel;
#define DD_WALK_DOOR_CALLBACK_RVA 0x77d84fu
#define DD_WALK_RETREAT_TRAVEL_RVA 0x5a9542u /* A retreat runs on both copies by its own order, its travel with it. */
static int walk_travel_order(const uint32_t *dest, void *from, uint8_t flip, uint8_t extra, dd_action *a) {
    uint32_t values[4] = {0}, from_area = 0;
    if (!safe_read((uintptr_t)dest, values, sizeof values) || (from && !safe_read((uintptr_t)from + 0x18, &from_area, 4))) return 0;
    memset(a, 0, sizeof *a);
    a->kind = DD_ACT_TRAVEL;
    memcpy(a->u.travel.dest, values, sizeof values);
    a->u.travel.from_area = from_area;
    a->u.travel.has_from = from != NULL;
    a->u.travel.flip = flip != 0;
    a->u.travel.extra = extra != 0;
    return dd_action_valid(a);
}
static int walk_area_pointer(uint64_t raid, uint32_t id, uint64_t *out);
/* The bag as one number: count and kind per slot, in order. */
static uint32_t walk_bag_hash(uint64_t raid) {
    uint64_t begin = 0, end = 0;
    uint32_t hash = 2166136261u;
    if (!safe_read((uintptr_t)raid + 0x220, &begin, 8) || !safe_read((uintptr_t)raid + 0x228, &end, 8) || end < begin || (end - begin) / 0x5a8 > 256) return 0;
    for (uint64_t p = begin; p + 0x5a8 <= end; p += 0x5a8) {
        int32_t count = 0;
        uint32_t values[3] = {0};
        if (!safe_read((uintptr_t)p, &count, 4) || count <= 0) continue;
        values[0] = (uint32_t)count;
        safe_read((uintptr_t)p + 0x44, &values[1], 4);
        safe_read((uintptr_t)p + 0x88, &values[2], 4);
        for (unsigned i = 0; i < 3; i++) hash = (hash ^ values[i]) * 16777619u;
    }
    return hash;
}
/* The party by rank (raid+8, a vector of Hero*). Per hero: its id (+137c), HP (+10a8, written as "current_hp" by the
 * actor's serializer 47dc20) and stress (+12fc, "m_Stress" in the hero's 5d1ad0), each a float, and its two trinket slots
 * (Hero+1280, Items of 0x5a8: count +0, type +44, id +88) as one hash. 0 = not readable. */
#define DD_HERO_HP 0x10a8u
#define DD_HERO_STRESS 0x12fcu
#define DD_HERO_TRINKETS 0x1280u
static uint32_t walk_trinket_hash(uint64_t hero) {
    uint64_t begin = 0, end = 0;
    uint32_t hash = 2166136261u;
    if (!safe_read((uintptr_t)hero + DD_HERO_TRINKETS + 8, &begin, 8) || !safe_read((uintptr_t)hero + DD_HERO_TRINKETS + 0x10, &end, 8) ||
        end < begin || (end - begin) % 0x5a8 || (end - begin) / 0x5a8 > 2)
        return 0;
    for (uint64_t p = begin; p < end; p += 0x5a8) {
        uint32_t values[3] = {0};
        if (!safe_read((uintptr_t)p, &values[0], 4)) return 0;
        if (values[0] && (!safe_read((uintptr_t)p + 0x44, &values[1], 4) || !safe_read((uintptr_t)p + 0x88, &values[2], 4))) return 0;
        if (!values[0]) values[1] = values[2] = 0;
        for (unsigned i = 0; i < 3; i++) hash = (hash ^ values[i]) * 16777619u;
    }
    return hash;
}
static int walk_party_read(uint64_t raid, uint64_t heroes[4], uint32_t ids[4], uint32_t hp[4], uint32_t stress[4], uint32_t trinkets[4]) {
    uint64_t begin = 0, end = 0;
    for (unsigned i = 0; i < 4; i++) heroes[i] = ids[i] = hp[i] = stress[i] = trinkets[i] = 0;
    if (!safe_read((uintptr_t)raid + 8, &begin, 8) || !safe_read((uintptr_t)raid + 0x10, &end, 8) || end < begin || (end - begin) % 8 || (end - begin) / 8 > 4)
        return 0;
    for (unsigned i = 0; begin + (uint64_t)i * 8 < end; i++) {
        uint64_t hero = 0;
        if (!safe_read((uintptr_t)(begin + (uint64_t)i * 8), &hero, 8)) return 0;
        if (!hero) continue;
        if (!safe_read((uintptr_t)hero + 0x137c, &ids[i], 4) || !safe_read((uintptr_t)hero + DD_HERO_HP, &hp[i], 4) ||
            !safe_read((uintptr_t)hero + DD_HERO_STRESS, &stress[i], 4) || !(trinkets[i] = walk_trinket_hash(hero)))
            return 0;
        heroes[i] = hero;
    }
    return 1;
}
/* At a door: verify the same formation before any HP/stress correction or travel.
 * Different hero order stops both peers before a new fight can generate turns. */
#include "dd_formation_bridge.inc"
static int walk_party_compare(uint64_t raid, const dd_action *a) {
    uint64_t heroes[4];
    uint32_t ids[4], hp[4], stress[4], trinkets[4];
    if (!(a->u.travel.ids[0] | a->u.travel.ids[1] | a->u.travel.ids[2] | a->u.travel.ids[3])) return 1;
    if (!walk_party_read(raid, heroes, ids, hp, stress, trinkets)) {
        dd_log("Walk: party not readable at the door; not compared");
        runtime_sync_stop(1); return 0;
    }
    if (memcmp(ids,a->u.travel.ids,sizeof ids)) {
        for(unsigned i=0;i<4;i++)dd_log("Formation before travel: rank=%u local=%u expected=%u",i,ids[i],a->u.travel.ids[i]);
        g_walk_party_told=1;
        runtime_sync_stop(1); return 0; /* Never enter a new fight with another actor order. */
    }
    for (unsigned i = 0; i < 4; i++) {
        float mine, theirs;
        if (!ids[i]) continue;
        if (hp[i] != a->u.travel.hp[i]) {
            memcpy(&mine, &hp[i], 4);
            memcpy(&theirs, &a->u.travel.hp[i], 4);
            dd_log("Walk: hero %u HP differed at the door: here %f, host %f; taken over from the host", ids[i], mine, theirs);
            memcpy((void *)(uintptr_t)(heroes[i] + DD_HERO_HP), &a->u.travel.hp[i], 4);
        }
        if (stress[i] != a->u.travel.stress[i]) {
            memcpy(&mine, &stress[i], 4);
            memcpy(&theirs, &a->u.travel.stress[i], 4);
            dd_log("Walk: hero %u stress differed at the door: here %f, host %f; taken over from the host", ids[i], mine, theirs);
            memcpy((void *)(uintptr_t)(heroes[i] + DD_HERO_STRESS), &a->u.travel.stress[i], 4);
        }
        if (trinkets[i] != a->u.travel.trinkets[i]) {
            dd_log("Walk: hero %u trinkets differ at the door: here %08x, host %08x", ids[i], trinkets[i], a->u.travel.trinkets[i]);
            if (!g_walk_trinket_told) {
                g_walk_trinket_told = 1;
                shop_notice("A hero's trinkets differ from the host. You may continue; send the logs afterwards.");
            }
        }
    }
    return 1;
}
static void __fastcall hook_raid_travel(void *display, const uint32_t *dest, void *from, uint8_t flip, uint8_t extra) {
#ifdef _MSC_VER
    uintptr_t caller = (uintptr_t)_ReturnAddress();
#else
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
#endif
    dd_action a;
    uint32_t state = 0;
    if (!g_raid_travel) return;
    if (g_game_apply_depth || !walk_shared()) {
        g_raid_travel(display, dest, from, flip, extra);
        return;
    }
    if (formation_busy()) return;
    {
        /* The game does not check: an area it cannot find is a crash at 77014c. In a shared game a callback can outlive its area. */
        walk_view v;
        uint64_t area = 0;
        uint32_t id = 0;
        if (safe_read((uintptr_t)dest, &id, 4) && g_walk_view(&v) && !walk_area_pointer(v.raid, id, &area)) {
            dd_log("Walk: travel to unknown area %08x dropped", id);
            return;
        }
    }
    if (g_cfg.role == DD_ROLE_GUEST) {
        if (g_walk_door_pending && caller - image_base() == DD_WALK_DOOR_CALLBACK_RVA) {
            g_walk_door_pending = 0;
            dd_log("Walk: this copy's door has finished; the host's order moves it");
            return;
        }
        if (!g_walk_player_travel(caller - image_base())) {
            g_raid_travel(display, dest, from, flip, extra);
            return;
        }
        if (walk_travel_order(dest, from, flip, extra, &a)) {
            dd_log("Walk: guest asks to travel to area %08x tile %u", a.u.travel.dest[0], a.u.travel.dest[1]);
            dd_town_guest_local(&g_town, &a);
        }
        return;
    }
    if (caller - image_base() == DD_WALK_RETREAT_TRAVEL_RVA) {
        g_raid_travel(display, dest, from, flip, extra);
        return;
    }
    if (!safe_read((uintptr_t)display + 0x40b0, &state, 4) || !walk_travel_order(dest, from, flip, extra, &a)) {
        dd_log("Walk: travel not readable; not shared");
        g_raid_travel(display, dest, from, flip, extra);
        return;
    }
    if (state > 1 && state != 6 && state != 7) { /* The native call ignores the others. */
        if (g_rules_object) safe_read((uintptr_t)g_rules_object + 8, &a.u.travel.rules, 4);
        {
            walk_view here;
            if (g_walk_view(&here)) {
                safe_read((uintptr_t)here.raid + DD_WALK_TORCH, &a.u.travel.torch, 4);
                a.u.travel.bag = walk_bag_hash(here.raid);
                {
                    uint64_t heroes[4];
                    if (!walk_party_read(here.raid, heroes, a.u.travel.ids, a.u.travel.hp, a.u.travel.stress, a.u.travel.trinkets))
                        memset(a.u.travel.ids, 0, sizeof a.u.travel.ids);
                }
            }
        }
        dd_town_host_observed(&g_town, &a);
        dd_log("Walk: host travels to area %08x tile %u", a.u.travel.dest[0], a.u.travel.dest[1]);
    }
    g_raid_travel(display, dest, from, flip, extra);
}
static int walk_area_pointer(uint64_t raid, uint32_t id, uint64_t *out) {
    uint64_t begin = 0, end = 0;
    *out = 0;
    if (!safe_read((uintptr_t)raid + 0x740, &begin, 8) || !safe_read((uintptr_t)raid + 0x748, &end, 8) || end < begin ||
        (end - begin) % 8 || (end - begin) / 8 > 4096)
        return 0;
    for (uint64_t p = begin; p < end; p += 8) {
        uint64_t area = 0;
        uint32_t key = 0;
        if (!safe_read((uintptr_t)p, &area, 8) || !area || !safe_read((uintptr_t)area + 0x18, &key, 4)) return 0;
        if (key == id) {
            *out = area;
            return 1;
        }
    }
    return 0;
}
/* The guest may still be loading or fading, in a fight of its own, or not yet at the prop. Then this order, and what follows it, waits. */
static int walk_gate(const dd_msg *m) {
    walk_view v = {0};
    int go;
    if (g_cfg.role != DD_ROLE_GUEST || m->type != DD_MSG_TOWN_APPLY || g_town.applied_seq == UINT32_MAX || m->seq != g_town.applied_seq + 1 ||
        !dd_action_valid(&m->u.town.action))
        return 0;
    const dd_action *a = &m->u.town.action;
    if (a->kind == DD_ACT_TRAVEL) {
        if (!g_raid_travel) return 0;
        go = g_walk_view(&v);
        if (go && g_walk_door_pending && GetTickCount64() < g_walk_door_pending) go = 0; /* This copy's door is still walking. */
        else if (go && !v.battle && v.area == a->u.travel.dest[0]) go = 1; /* Already there: apply only logs it. */
        else if (go && (v.battle || v.state <= 1 || v.state == 6 || v.state == 7)) go = 0;
    } else if (a->kind == DD_ACT_USE) {
        float pos, delta;
        memcpy(&pos, &a->u.use.pos, 4);
        go = g_walk_view(&v) && !v.battle && v.state > 1 && v.area == a->u.use.area;
        if (go) {
            uint64_t now = GetTickCount64();
            delta = pos - v.pos;
            if (!g_walk_use_wait_until) g_walk_use_wait_until = now + 3000;
            /* It may already have walked on past that spot behind the host. */
            go = (delta < DD_WALK_USE_NEAR && delta > -DD_WALK_USE_NEAR) || now > g_walk_use_wait_until;
            /* A tool needs the overlay, which opens when this copy walks up to the thing. */
            if (a->u.use.door == 4) {
                uint32_t mode = 0;
                go = (go && safe_read((uintptr_t)v.display + DD_WALK_OVERLAY + 0xd88, &mode, 4) && mode == 2) || now > g_walk_use_wait_until + 5000;
            }
        }
        if (go) g_walk_use_wait_until = 0;
    } else {
        return 0;
    }
    if (!go) {
        if (!g_walk_gate_told) {
            g_walk_gate_told = 1;
            dd_log("Walk: order %u waits for this copy (state=%u battle=%u area=%08x)", a->kind, v.state, v.battle, v.area);
        }
        return 1;
    }
    g_walk_gate_told = 0;
    return 0;
}
static int walk_travel_apply(const dd_action *a) {
    walk_view v;
    uint64_t from = 0;
    if (!g_raid_travel || !g_walk_view(&v)) return 0;
    if (v.area == a->u.travel.dest[0]) { /* The game's own call got here first. */
        dd_log("Walk: already in area %08x", v.area);
        return g_cfg.role==DD_ROLE_GUEST ? walk_party_compare(v.raid,a) : 1;
    }
    /* On the host this is the guest's wish. The game's call would do nothing in these states while the order still went out. */
    if (g_cfg.role == DD_ROLE_HOST && (v.battle || v.state <= 1 || v.state == 6 || v.state == 7)) return 0;
    if (a->u.travel.has_from && !walk_area_pointer(v.raid, a->u.travel.from_area, &from)) {
        dd_log("Walk: this copy has no area %08x", a->u.travel.from_area);
        shop_notice("This copy does not have the same map as the host. Close both games and restart the session; "
                    "send the logs.");
        return 0;
    }
    dd_log("Walk: travel to area %08x tile %u (role=%s)", a->u.travel.dest[0], a->u.travel.dest[1], dd_role_name(g_cfg.role));
    /* Something unshared may have drawn numbers on one copy, and one such draw splits every fight after it. The rooms start equal again; the log says it happened. */
    if (g_cfg.role == DD_ROLE_GUEST && a->u.travel.rules && g_rules_object) {
        uint32_t here = 0;
        if (safe_read((uintptr_t)g_rules_object + 8, &here, 4) && here != a->u.travel.rules) {
            dd_log("Walk: Rules differed at the door: here %08x, host %08x; taken over from the host", here, a->u.travel.rules);
            memcpy((uint8_t *)g_rules_object + 8, &a->u.travel.rules, 4);
        }
    }
    /* The rest of what a door compares. The torch is one number, so it can simply be set; a bag can only be reported. */
    if (g_cfg.role == DD_ROLE_GUEST && a->u.travel.rules) {
        uint32_t torch = 0, bag = walk_bag_hash(v.raid);
        if (safe_read((uintptr_t)v.raid + DD_WALK_TORCH, &torch, 4) && torch != a->u.travel.torch) {
            float mine, theirs;
            memcpy(&mine, &torch, 4);
            memcpy(&theirs, &a->u.travel.torch, 4);
            dd_log("Walk: torch differed at the door: here %f, host %f; taken over from the host", mine, theirs);
            memcpy((void *)(uintptr_t)(v.raid + DD_WALK_TORCH), &a->u.travel.torch, 4);
        }
        if (bag != a->u.travel.bag) {
            dd_action ask;
            dd_log("Walk: bag differs at the door: here %08x, host %08x; asking for the host's", bag, a->u.travel.bag);
            /* The host's bag is the truth: it sends every slot. Still different at the next door means that could not repair it. */
            if (g_walk_bag_asked && !g_walk_bag_told) {
                g_walk_bag_told = 1;
                shop_notice("The inventory differs from the host and could not be repaired. You may continue; send the logs afterwards.");
            }
            g_walk_bag_asked = 1;
            memset(&ask, 0, sizeof ask);
            ask.kind = DD_ACT_ITEM;
            ask.u.item.op = DD_ITEM_OP_BAG_ASK;
            ask.u.item.hero = 255;
            dd_town_guest_local(&g_town, &ask);
        } else {
            g_walk_bag_asked = 0;
        }
        if (!walk_party_compare(v.raid, a)) return 0;
    }
    g_raid_travel((void *)(uintptr_t)v.display, a->u.travel.dest, (void *)(uintptr_t)from, a->u.travel.flip, a->u.travel.extra);
    return 1;
}
/* Each turn in the log, so both logs show whether the fights run in step:
 * raid+4b20 is the battle's state, raid+4898 the battle, +a0 the turn, +a8 the round, +20 the turns (0x18 each, the actor first). */
static void walk_battle_log(const walk_view *v) {
    uint64_t battle = v->raid + 0x4898, turns = 0, actor = 0;
    uint32_t round = 0, turn = 0, id = 0, rules = 0;
    if (!v->battle) {
        if (g_battle_logging) dd_log("Battle: role=%s over", dd_role_name(g_cfg.role));
        g_battle_logging = 0;
        return;
    }
    if (!safe_read((uintptr_t)battle + 0xa8, &round, 4) || !safe_read((uintptr_t)battle + 0xa0, &turn, 4)) return;
    if (g_battle_logging && round == g_battle_log_round && turn == g_battle_log_turn) return;
    if (safe_read((uintptr_t)battle + 0x20, &turns, 8) && turns && safe_read((uintptr_t)(turns + (uint64_t)turn * 0x18), &actor, 8) && actor)
        safe_read((uintptr_t)actor + 0x137c, &id, 4);
    if (g_rules_object) safe_read((uintptr_t)g_rules_object + 8, &rules, 4);
    dd_log("Battle: role=%s round=%u turn=%u actor=%u state=%u rules=%08x", dd_role_name(g_cfg.role), round, turn, id, v->battle, rules);
    g_battle_logging = 1;
    g_battle_log_round = round;
    g_battle_log_turn = turn;
    g_battle_log_state = v->battle;
}
static void walk_selection_poll(const walk_view *v);
static void bag_state_forget(void);
static void bag_poll_hook(const walk_view *v);
static void battle_turn_show(const walk_view *v);
static void hero_owners_poll(const walk_view *v);
static int hero_owner(uint64_t hero);
static void hunger_poll(const walk_view *v);
#include "dd_party_bridge.inc"
/* Whose hero stands where, for the blocks at the top left; only in a session. */
static void party_marks_show(const walk_view *v) {
    uint64_t heroes[4];
    uint32_t ids[4], hp[4], stress[4], trinkets[4];
    int owner[4] = {0}, count = 0;
    float level[4] = {0};
    if (walk_shared() && walk_party_read(v->raid, heroes, ids, hp, stress, trinkets))
        for (int i = 0; i < 4 && heroes[i]; i++, count++) {
            int who = hero_owner(heroes[i]);
            owner[i] = who < 0 ? DD_OVERLAY_TURN_NONE : who == (g_cfg.role == DD_ROLE_GUEST ? 1 : 0) ? DD_OVERLAY_TURN_MINE : DD_OVERLAY_TURN_OTHER;
            memcpy(&level[i], &stress[i], 4);
        }
    dd_overlay_party(count, owner, level);
}
/* Runs every frame. Both copies log area, tile and Rules state, so the two logs can be laid side by side. */
static void walk_poll(void) {
    walk_view v;
    uint32_t bits;
    if (!g_walk_view(&v)) {
        party_publish(NULL);
        /* No raid (any more): nothing to show, and the next raid starts afresh. */
        dd_overlay_turn(DD_OVERLAY_TURN_NONE);
        dd_overlay_party(0, NULL, NULL);
        g_walk_selected = -1;
        bag_state_forget();
        town_check_poll();
        return;
    }
    g_town_check_raid = 1;
    party_publish(&v);
    g_town_check_at = 0;
    memcpy(&bits, &v.pos, 4);
    int tile = (int)((v.pos + 360.0f) / 720.0f);
    if (v.area != g_walk_log_area || tile != g_walk_log_tile) {
        uint32_t rules = 0;
        if (g_rules_object) safe_read((uintptr_t)g_rules_object + 8, &rules, 4);
        dd_log("Walk: role=%s area=%08x tile=%d rules=%08x", dd_role_name(g_cfg.role), v.area, tile, rules);
        g_walk_log_area = v.area;
        g_walk_log_tile = tile;
    }
    walk_battle_log(&v);
    walk_selection_poll(&v);
    hero_owners_poll(&v);
    hunger_poll(&v);
    battle_turn_show(&v);
    party_marks_show(&v);
    bag_poll_hook(&v);
    if (g_cfg.role != DD_ROLE_HOST) return;
    uint64_t now = GetTickCount64();
    if ((g_walk_sent_any && v.area == g_walk_sent_area && bits == g_walk_sent_pos) || now < g_walk_send_at) return;
    dd_msg m;
    memset(&m, 0, sizeof m);
    m.type = DD_MSG_WALK;
    m.u.walk.area = v.area;
    m.u.walk.pos = bits;
    if (!queue_push(&g_outgoing, &m)) return; /* The next frame tries again; nothing is lost but a step of the view. */
    g_walk_sent_any = 1;
    g_walk_sent_area = v.area;
    g_walk_sent_pos = bits;
    g_walk_send_at = now + 50;
}
#include "dd_battle_bridge.inc"
#include "dd_items_bridge.inc"
/* 28fcd0: the wrapped generator's step. Which code draws a Rules number is what the logs could not say when two copies part (owner's run, 15:42:32: one draw more on the host). */
typedef float(__fastcall *rules_uniform_fn)(void *, uint64_t, uint64_t, uint64_t);
static rules_uniform_fn g_rules_uniform;
static float __fastcall hook_rules_uniform(void *rng, uint64_t b, uint64_t c, uint64_t d) {
    float value = g_rules_uniform ? g_rules_uniform(rng, b, c, d) : 0;
    if (rng == g_rules_object && g_rules_object && walk_shared()) {
        void *frames[4] = {0};
        uintptr_t base = image_base();
        USHORT n = RtlCaptureStackBackTrace(0, 4, frames, NULL);
        uint32_t state = 0;
        safe_read((uintptr_t)rng + 8, &state, 4);
        dd_log("Rules: %08x from %llx %llx %llx", state, n > 1 ? (unsigned long long)((uintptr_t)frames[1] - base) : 0ULL, n > 2 ? (unsigned long long)((uintptr_t)frames[2] - base) : 0ULL,
               n > 3 ? (unsigned long long)((uintptr_t)frames[3] - base) : 0ULL);
    }
    return value;
}
static void bag_state_forget(void) { bag_state_reset(); }
static void bag_poll_hook(const walk_view *v) {
    if (g_loot_escape && !loot_open(v)) g_loot_escape = 0;
    bag_poll(v);
}
static void battle_reset_hook(void) {
    battle_reset();
    items_reset();
}
/* 2357e0: the game's assert report, the message box a player sees. Only its strings are read; the rest goes on as it came. The arguments behind the format are passed along unread. */
typedef int(__fastcall *game_assert_fn)(const char *, const char *, const char *, int, const char *, void *, void *, void *, void *);
static game_assert_fn g_game_assert;
static void assert_text(const char *from, char *to, size_t size) {
    size_t i = 0;
    for (; i + 1 < size; i++) if (!safe_read((uintptr_t)from + i, &to[i], 1) || !to[i]) break;
    to[i] = 0;
}
static int __fastcall hook_game_assert(const char *expression, const char *function, const char *file, int line, const char *format, void *a, void *b, void *c, void *d) {
    char what[120], where[80], text[160];
    assert_text(expression, what, sizeof what);
    assert_text(function, where, sizeof where);
    assert_text(format, text, sizeof text);
    dd_log("Game assert: %s in %s line %d: %s", what, where, line, text);
    return g_game_assert ? g_game_assert(expression, function, file, line, format, a, b, c, d) : 1;
}
static const hamlet_hook_spec g_walk_hooks[] = {
    {"formation_reset", (uintptr_t)hook_formation_reset, &g_formation_native, 11},
    {"raid_move_input", (uintptr_t)hook_raid_move, &g_raid_move, 10},
    {"party_move", (uintptr_t)hook_party_move, &g_party_move, 7},
    {"raid_use_prop", (uintptr_t)hook_raid_use, &g_raid_use, 11},
    {"raid_use_trap", (uintptr_t)hook_raid_trap, &g_raid_trap, 11},
    {"overlay_use", (uintptr_t)hook_overlay_use, &g_overlay_use, 10},
    {"curio_button", (uintptr_t)hook_curio_button, &g_curio_button, 10},
    {"curio_tool", (uintptr_t)hook_curio_tool, &g_curio_tool, 13},
    {"game_assert", (uintptr_t)hook_game_assert, &g_game_assert, 14},
    {"raid_travel", (uintptr_t)hook_raid_travel, &g_raid_travel, 12},
    {"battle_skill_button", (uintptr_t)hook_battle_skill_button, &g_battle_skill_button, 10},
    {"battle_skill_pick", (uintptr_t)hook_battle_pick, &g_battle_pick, 10},
    {"battle_target_enemy", (uintptr_t)hook_battle_enemy, &g_battle_enemy, 11},
    {"battle_target_friend", (uintptr_t)hook_battle_friend, &g_battle_friend, 11},
    {"battle_move", (uintptr_t)hook_battle_move, &g_battle_move, 10},
    {"rules_uniform", (uintptr_t)hook_rules_uniform, &g_rules_uniform, 11},
    {"curio_item", (uintptr_t)hook_curio_item, &g_curio_item, 12},
    {"bark_moving", (uintptr_t)hook_bark_moving, &g_bark_moving, 13},
    {"tutorial_show", (uintptr_t)hook_tutorial_show, &g_tutorial_show, 10},
    {"panel_close", (uintptr_t)hook_panel_close, &g_panel_close, 13},
    {"results_setup", (uintptr_t)hook_results_setup, &g_results_setup, 10},
    {"results_next", (uintptr_t)hook_results_next, &g_results_next, 11},
    {"bag_shop_drop", (uintptr_t)hook_bag_shop_drop, &g_bag_shop_drop, 10},
    {"provision_stock_drop", (uintptr_t)hook_provision_stock_drop, &g_provision_stock_drop, 9},
    {"provision_stack_return", (uintptr_t)hook_supply_stack_return, &g_supply_stack_return, 8},
    {"bag_throw_a", (uintptr_t)hook_bag_throw_a, &g_bag_throw_a, 8},
    {"bag_throw_b", (uintptr_t)hook_bag_throw_b, &g_bag_throw_b, 9},
    {"retreat_attempt", (uintptr_t)hook_retreat, &g_retreat, 10},
    {"quest_choice_one", (uintptr_t)hook_quest_one, &g_quest_one, 10},
    {"quest_choice_two", (uintptr_t)hook_quest_two, &g_quest_two, 10},
    {"camp_meal", (uintptr_t)hook_camp_meal, &g_camp_meal, 10},
    {"camp_skill", (uintptr_t)hook_camp_skill, &g_camp_skill, 11},
    {"camp_leave", (uintptr_t)hook_camp_leave, &g_camp_leave, 11},
    {"item_use", (uintptr_t)hook_item_use, &g_item_use, 11},
    {"torch_change", (uintptr_t)hook_torch, &g_torch, 10},
    {"loot_take_all", (uintptr_t)hook_loot_take_all, &g_loot_take_all, 11},
    {"loot_close", (uintptr_t)hook_loot_close, &g_loot_close, 11},
    {"loot_take_item", (uintptr_t)hook_loot_take_item, &g_loot_take_item, 9},
    {"hunger_show", (uintptr_t)hook_hunger_show, &g_hunger_show, 14},
    {"hunger_eat", (uintptr_t)hook_hunger_eat, &g_hunger_eat, 13},
    {"hunger_refuse", (uintptr_t)hook_hunger_refuse, &g_hunger_refuse, 15},
};
