/* Uses the real battle hooks and FIFO on top of the walk fixture's view. Only the game's choice functions are faked. */
static uint8_t bt_heroes[4][0x1500], bt_monsters[4][0x10], bt_skill[0x10];
static uint64_t bt_party[4], bt_foes[4], bt_turns[9];
static unsigned bt_picks, bt_enemies, bt_friends, bt_moves, bt_buttons;
static int bt_pick_slot, bt_move_rank;
static uint8_t bt_pick_flag;
static void *bt_last_hero, *bt_last_target, *bt_last_skill, *bt_last_battle;
static uint64_t __fastcall bt_pick(void *hero, int32_t slot, uint8_t flag) {
    bt_picks++;
    bt_last_hero = hero;
    bt_pick_slot = slot;
    bt_pick_flag = flag;
    return 5;
}
static uint64_t __fastcall bt_enemy(void *battle, void *skill, void *actor, void *target) {
    bt_enemies++;
    bt_last_battle = battle, bt_last_skill = skill, bt_last_hero = actor, bt_last_target = target;
    return 6;
}
static uint64_t __fastcall bt_friend(void *battle, void *skill, void *actor, void *target) {
    bt_friends++;
    bt_last_battle = battle, bt_last_skill = skill, bt_last_hero = actor, bt_last_target = target;
    return 7;
}
static uint64_t __fastcall bt_move(void *battle, void *hero, int32_t rank, uint64_t spare, uint32_t fifth) {
    (void)spare, (void)fifth;
    bt_moves++;
    bt_last_battle = battle, bt_last_hero = hero, bt_move_rank = rank;
    return 8;
}
/* The button picks while it runs, like 71f1e0 does. */
static int bt_button_slot;
static void __fastcall bt_button(void *functor) {
    (void)functor;
    bt_buttons++;
    hook_battle_pick((void *)(uintptr_t)bt_party[2], bt_button_slot, 1);
}
static int bt_caller_yes(uintptr_t rva) { (void)rva; return 1; }
static int bt_caller_no(uintptr_t rva) { (void)rva; return 0; }
static void bt_at(uint32_t round, uint32_t turn, uint32_t state) {
    memcpy(wk_raid_lists + 0x4898 + 0xa8, &round, 4);
    memcpy(wk_raid_lists + 0x4898 + 0xa0, &turn, 4);
    wk_view.battle = state;
}
static void bt_reset(int role) {
    wk_reset(role);
    battle_reset();
    uint64_t party_begin = (uintptr_t)bt_party, party_end = (uintptr_t)(bt_party + 4), foes_begin = (uintptr_t)bt_foes, foes_end = (uintptr_t)(bt_foes + 4);
    uint64_t turns_begin = (uintptr_t)bt_turns, turns_end = (uintptr_t)(bt_turns + 9), skill = (uintptr_t)bt_skill;
    memset(bt_heroes, 0, sizeof bt_heroes);
    {
        /* All heroes belong to whoever this copy is, unless a test says otherwise. */
        uint32_t ids[4] = {11, 12, 13, 14};
        for (unsigned i = 0; i < 4; i++) memcpy(bt_heroes[i] + 0x137c, &ids[i], 4);
        memcpy(g_hero_owner_ids, ids, sizeof ids);
        g_hero_owner_mask = role == DD_ROLE_GUEST ? 15 : 0;
    }
    for (unsigned i = 0; i < 4; i++) {
        bt_party[i] = (uintptr_t)bt_heroes[i];
        bt_foes[i] = (uintptr_t)bt_monsters[i];
        memcpy(bt_heroes[i] + 0x8e0, &skill, 8);
    }
    memset(bt_turns, 0, sizeof bt_turns);
    bt_turns[0] = bt_party[0];
    bt_turns[3] = bt_party[2]; /* Turn 1: the third hero. Turns are 0x18 each with the actor first. */
    bt_turns[6] = bt_foes[1];  /* Turn 2: a monster. */
    memcpy(wk_raid_lists + 8, &party_begin, 8);
    memcpy(wk_raid_lists + 0x10, &party_end, 8);
    memcpy(wk_raid_lists + 0x48d0, &foes_begin, 8);
    memcpy(wk_raid_lists + 0x48d8, &foes_end, 8);
    memcpy(wk_raid_lists + 0x4898 + 0x20, &turns_begin, 8);
    memcpy(wk_raid_lists + 0x4898 + 0x28, &turns_end, 8);
    bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
    g_battle_pick = bt_pick;
    g_battle_enemy = bt_enemy;
    g_battle_friend = bt_friend;
    g_battle_move = bt_move;
    g_battle_skill_button = bt_button;
    g_battle_player_caller = bt_caller_yes;
    bt_picks = bt_enemies = bt_friends = bt_moves = bt_buttons = 0;
    bt_pick_slot = bt_move_rank = -7;
    bt_pick_flag = 0;
    bt_button_slot = 3;
    bt_last_hero = bt_last_target = bt_last_skill = bt_last_battle = NULL;
}
static void bt_unwire(void) {
    g_battle_pick = NULL;
    g_battle_enemy = g_battle_friend = NULL;
    g_battle_move = NULL;
    g_battle_skill_button = NULL;
    g_battle_player_caller = battle_player_caller;
}
static FILE *bt_owner_open(void) {
    extern const char *bt_owner_text;
    FILE *f = tmpfile();
    if (f) {
        fputs(bt_owner_text, f);
        rewind(f);
    }
    return f;
}
const char *bt_owner_text;
static FILE *bt_owner_none(void) { return NULL; }
static void owner_file_checks(void) {
    uint32_t ids[4] = {1, 18, 2, 8}, revision = 99;
    g_hero_owner_open = bt_owner_open;
    bt_owner_text = "; a comment\r\nversion=1\r\nwritten_by=host\r\nrevision=2\r\n; Reynauldo\r\nhero.1=1\r\nhero.2=1\r\nhero.7=0\r\nhero.8=0\r\nhero.18=0\r\nunknown=3\r\n";
    CHECK(hero_owners_load(ids, &revision) == 5 && revision == 2); /* The first and third of the party. */
    bt_owner_text = "version=1\nwritten_by=host\nhero.1=0\nhero.8=1\n";
    CHECK(hero_owners_load(ids, &revision) == 8); /* A hero not listed is the host's. */
    bt_owner_text = "version=1\nwritten_by=host\nhero.8=1\nhero.8=0\n";
    CHECK(!hero_owners_load(ids, &revision)); /* Duplicate hero keys are rejected by the agreed parser. */
    bt_owner_text = "version=1\nwritten_by=host\nhero.1=2\nhero.8=1\n";
    alerts = 0;
    g_shop_notice[0] = 0;
    CHECK(!hero_owners_load(ids, &revision)); /* Broken: nobody's guess, and said out loud. */
    CHECK(g_shop_notice[0] != 0);
    bt_owner_text = "version=1\nwritten_by=host\nhero.x=1\nhero.8=1\n";
    CHECK(!hero_owners_load(ids, &revision));
    g_hero_owner_open = bt_owner_none;
    g_shop_notice[0] = 0;
    CHECK(!hero_owners_load(ids, &revision) && !revision);
    CHECK(!g_shop_notice[0]); /* No file is normal. */
    g_hero_owner_open = hero_owner_open;
    /* The table reaches the guest in the departure order's wire form. */
    {
        dd_action a, b;
        uint8_t wire[256];
        memset(&a, 0, sizeof a);
        a.kind = DD_ACT_DEPARTURE;
        a.u.departure.seed = a.u.departure.rules = 5;
        memcpy(a.u.departure.heroes, ids, sizeof ids);
        a.u.departure.owners = 5;
        a.u.departure.revision = 2;
        CHECK(dd_action_valid(&a));
        a.u.departure.owners = 16;
        CHECK(!dd_action_valid(&a));
        a.u.departure.owners = 5;
        (void)b, (void)wire;
    }
}
static unsigned it_uses, it_torches, it_alls, it_closes, it_items;
static int it_slot, it_button_slot;
static float it_amount;
static void *it_inventory, *it_hero;
static uint64_t it_button_loot, it_button_widget;
static uint64_t __fastcall it_use(void *controller, void *hero, void *inventory, int32_t slot, uint32_t flag) {
    CHECK(controller == (void *)(uintptr_t)0x1234 && flag == 1);
    it_uses++;
    it_hero = hero, it_inventory = inventory, it_slot = slot;
    return 1;
}
static void __fastcall it_torch(void *raid, float amount, uint8_t flag) {
    CHECK(raid == (void *)wk_raid_lists && flag == 1);
    it_torches++;
    it_amount = amount;
}
static void __fastcall it_all(void *functor) { (void)functor; it_alls++; }
static void __fastcall it_close(void *functor) { (void)functor; it_closes++; }
static void __fastcall it_item(void *button) {
    it_items++;
    memcpy(&it_button_slot, (uint8_t *)button + 0x10, 4);
    memcpy(&it_button_loot, (uint8_t *)button + 0x20, 8);
    memcpy(&it_button_widget, (uint8_t *)button + 0x28, 8);
}
static uint64_t it_controller(void) { return 0x1234; }
static unsigned it_curios;
static void *it_curio_panel, *it_curio_item;
static uint8_t __fastcall it_curio(void *overlay, void *item) {
    it_curios++;
    it_curio_panel = overlay, it_curio_item = item;
    return 1;
}
static int it_curio_yes(uintptr_t rva) { (void)rva; return 1; }
static int it_curio_no(uintptr_t rva) { (void)rva; return 0; }
static int it_caller_yes(uintptr_t rva) { (void)rva; return 1; }
static int it_caller_no(uintptr_t rva) { (void)rva; return 0; }
static void it_reset(int role) {
    uint32_t closed = 0;
    bt_reset(role);
    items_reset();
    wk_view.battle = 0;
    /* The game's Item copy, swap and destroy, as the Hamlet fixture fakes them (the supplies fixture leaves its own behind). */
    g_hamlet_copy = hm_copy;
    g_hamlet_swap = hm_swap;
    g_hamlet_destroy = hm_destroy;
    hm_item_alive = 0;
    g_item_use = it_use;
    g_torch = it_torch;
    g_loot_take_all = it_all;
    g_loot_close = it_close;
    g_loot_take_item = it_item;
    g_item_controller = it_controller;
    g_item_torch_caller = it_caller_yes;
    g_curio_item = it_curio;
    g_item_curio_caller = it_curio_yes;
    it_curios = 0;
    it_curio_panel = it_curio_item = NULL;
    it_uses = it_torches = it_alls = it_closes = it_items = 0;
    it_slot = it_button_slot = -1;
    it_amount = 0;
    it_inventory = it_hero = NULL;
    memcpy(wk_display + DD_WALK_LOOT_OVERLAY, &closed, 4);
}
static void it_loot(uint32_t state) {
    memcpy(wk_display + DD_WALK_LOOT_OVERLAY, &state, 4);
    wk_display[DD_WALK_LOOT_OVERLAY + 0x170] = state == 2;
    wk_view.battle = 0;
}
static unsigned it_barks, it_shows, it_closes_panel;
static void __fastcall it_bark(void *self, void *text, float chance) {
    (void)self, (void)text, (void)chance;
    it_barks++;
    if (g_rules_object) { uint32_t drawn = 0x9999; memcpy((uint8_t *)g_rules_object + 8, &drawn, 4); }
}
static void __fastcall it_show(void *panel, uint64_t b, uint64_t c, uint64_t d) { (void)panel, (void)b, (void)c, (void)d; it_shows++; }
static void __fastcall it_panel_close(void *functor) { (void)functor; it_closes_panel++; }
static void selection_and_popup_checks(void) {
    /* A bark gives its number back. */
    {
        uint8_t rules[0x10] = {0};
        uint32_t state = 0x1234, now = 0;
        it_reset(DD_ROLE_GUEST);
        g_bark_moving = it_bark;
        it_barks = 0;
        g_rules_object = rules;
        memcpy(rules + 8, &state, 4);
        hook_bark_moving(NULL, NULL, 0.5f);
        memcpy(&now, rules + 8, 4);
        CHECK(it_barks == 1 && now == 0x1234);
        g_rules_object = NULL;
        g_bark_moving = NULL;
    }
    /* A hero clicked outside a fight goes out once; the first look only writes down; an order is not sent back. */
    {
        uint64_t second = bt_party[1], third = bt_party[2];
        it_reset(DD_ROLE_HOST);
        memcpy(wk_raid_lists + 0x438, &second, 8);
        walk_selection_poll(&wk_view);
        CHECK(!sent && g_walk_selected == 1);
        walk_selection_poll(&wk_view);
        CHECK(!sent);
        memcpy(wk_raid_lists + 0x438, &third, 8);
        walk_selection_poll(&wk_view);
        CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.action.u.item.op == DD_ITEM_OP_SELECT && last_message.u.town.action.u.item.hero == 2);
        walk_selection_poll(&wk_view);
        CHECK(sent == 1);
        dd_action select = last_message.u.town.action;
        /* In a fight the turn selects, on both by itself. */
        wk_view.battle = 0x1d;
        memcpy(wk_raid_lists + 0x438, &second, 8);
        walk_selection_poll(&wk_view);
        CHECK(sent == 1);
        /* The guest's click asks; the order selects and is not sent back. */
        it_reset(DD_ROLE_GUEST);
        memcpy(wk_raid_lists + 0x438, &second, 8);
        walk_selection_poll(&wk_view);
        memcpy(wk_raid_lists + 0x438, &third, 8);
        walk_selection_poll(&wk_view);
        CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
        it_reset(DD_ROLE_GUEST);
        memcpy(wk_raid_lists + 0x438, &second, 8);
        walk_selection_poll(&wk_view);
        wk_push(1, &select);
        dd_runtime_poll();
        CHECK(wk_select_index == 2 && g_walk_selected == 2 && !sent);
        { int32_t field = 0; memcpy(&field, wk_display + 0x3f88, 4); CHECK(field == 1); }
    }
    /* The tutorial popup: the close of that panel is an order; another panel's passes; it waits for the popup here. */
    {
        static uint8_t popup[0x100], other[0x100];
        uint64_t functor[2] = {0, (uintptr_t)popup}, elsewhere[2] = {0, (uintptr_t)other};
        uint32_t open = 2, closing = 6;
        g_tutorial_show = it_show;
        g_panel_close = it_panel_close;
        it_reset(DD_ROLE_HOST);
        it_shows = it_closes_panel = 0;
        hook_tutorial_show(popup, 0, 0, 0);
        CHECK(it_shows == 1 && g_tutorial_panel == (uintptr_t)popup);
        hook_panel_close(elsewhere);
        CHECK(it_closes_panel == 1 && !sent);
        hook_panel_close(functor);
        CHECK(it_closes_panel == 2 && sent == 1 && last_message.u.town.action.u.item.op == DD_ITEM_OP_TUTORIAL);
        dd_action close_it = last_message.u.town.action;
        it_reset(DD_ROLE_GUEST);
        it_closes_panel = 0;
        hook_tutorial_show(popup, 0, 0, 0);
        hook_panel_close(functor);
        CHECK(!it_closes_panel && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
        it_reset(DD_ROLE_GUEST);
        it_closes_panel = 0;
        memset(popup, 0, sizeof popup);
        hook_tutorial_show(popup, 0, 0, 0);
        wk_push(1, &close_it);
        dd_runtime_poll();
        CHECK(!it_closes_panel && g_incoming.count == 1);
        memcpy(popup + 0x58, &open, 4);
        dd_runtime_poll();
        CHECK(it_closes_panel == 1 && g_town.applied_seq == 1);
        /* Already closing here: nothing runs, the queue goes on. */
        it_reset(DD_ROLE_GUEST);
        it_closes_panel = 0;
        hook_tutorial_show(popup, 0, 0, 0);
        memcpy(popup + 0x58, &closing, 4);
        g_item_wait_until = 1;
        wk_push(1, &close_it);
        dd_runtime_poll();
        CHECK(!it_closes_panel && g_town.applied_seq == 1 && !g_town.needs_resync);
        g_tutorial_show = NULL;
        g_panel_close = NULL;
    }
    /* The strip: whose hero chooses. */
    {
        it_reset(DD_ROLE_GUEST);
        bt_at(2, 1, DD_BATTLE_CHOOSE_SKILL_STATE);
        battle_turn_show(&wk_view);
        CHECK(overlay_turn_state == DD_OVERLAY_TURN_MINE);
        g_hero_owner_mask = 0;
        battle_turn_show(&wk_view);
        CHECK(overlay_turn_state == DD_OVERLAY_TURN_OTHER);
        bt_at(2, 1, 0x20);
        battle_turn_show(&wk_view);
        CHECK(overlay_turn_state == DD_OVERLAY_TURN_NONE);
        bt_at(2, 2, DD_BATTLE_CHOOSE_SKILL_STATE); /* A monster. */
        battle_turn_show(&wk_view);
        CHECK(overlay_turn_state == DD_OVERLAY_TURN_NONE);
    }
}
static unsigned it_retreats, it_meals, it_skills, it_leaves;
static void *it_retreat_panel;
static uint8_t it_meal_args[0x28];
static uint32_t it_skill_args[3];
static uint64_t it_leave_overlay;
static void __fastcall it_retreat(void *panel) { it_retreats++; it_retreat_panel = panel; }
static void __fastcall it_meal(void *args) { it_meals++; memcpy(it_meal_args, args, sizeof it_meal_args); }
static void __fastcall it_skill(void *camp, void *party, void *loot, uint32_t x, uint32_t y, uint32_t z) {
    CHECK(camp == (void *)(wk_raid_lists + 0x5100) && party == (void *)(wk_raid_lists + 8) && loot == (void *)(wk_raid_lists + 0x5140));
    it_skills++;
    it_skill_args[0] = x, it_skill_args[1] = y, it_skill_args[2] = z;
}
static void __fastcall it_leave(void *functor) { it_leaves++; memcpy(&it_leave_overlay, (uint8_t *)functor + 8, 8); }
static int it_skill_yes(uintptr_t rva) { (void)rva; return 1; }
static int it_skill_no(uintptr_t rva) { (void)rva; return 0; }
static void it_camp_reset(int role, uint32_t stage) {
    it_reset(role);
    g_retreat = it_retreat;
    g_camp_meal = it_meal;
    g_camp_skill = it_skill;
    g_camp_leave = it_leave;
    g_camp_skill_caller = it_skill_yes;
    it_retreats = it_meals = it_skills = it_leaves = 0;
    memset(it_meal_args, 0, sizeof it_meal_args);
    memcpy(wk_raid_lists + DD_ITEM_CAMP_STAGE, &stage, 4);
}
static unsigned it_ones, it_twos;
static uint64_t it_quest_panel;
static void __fastcall it_quest_one(void *functor) { it_ones++; memcpy(&it_quest_panel, (uint8_t *)functor + 8, 8); }
static void __fastcall it_quest_two(void *functor) { (void)functor; it_twos++; }
static void retreat_and_camp_checks(void) {
    void *quest = wk_display + DD_ITEM_QUEST_INFO;
    uint64_t camp = (uintptr_t)wk_display + DD_ITEM_CAMP_OVERLAY;
    CHECK(camp_skill_caller(0x70af4e) && camp_skill_caller(0x71f085) && !camp_skill_caller(0x70af49));
    /* Retreat outside a fight: the host runs it and shares it; the guest asks; the order runs near the same spot. */
    it_camp_reset(DD_ROLE_HOST, 0);
    hook_retreat(quest);
    CHECK(it_retreats == 1 && sent == 1 && last_message.u.town.action.u.item.op == DD_ITEM_OP_RETREAT);
    dd_action retreat = last_message.u.town.action;
    it_camp_reset(DD_ROLE_GUEST, 0);
    hook_retreat(quest);
    CHECK(!it_retreats && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
    it_camp_reset(DD_ROLE_GUEST, 0);
    wk_push(1, &retreat);
    dd_runtime_poll();
    CHECK(it_retreats == 1 && it_retreat_panel == quest);
    /* In a fight: your own hero's turn, at the same point. Another panel is left alone. */
    it_camp_reset(DD_ROLE_GUEST, 0);
    bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
    g_hero_owner_mask = 0;
    hook_retreat(quest);
    CHECK(!it_retreats && !sent);
    it_camp_reset(DD_ROLE_HOST, 0);
    bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
    hook_retreat(quest);
    CHECK(it_retreats == 1 && sent == 1 && last_message.u.town.action.u.item.state == DD_BATTLE_CHOOSE_TARGET_STATE);
    retreat = last_message.u.town.action;
    it_camp_reset(DD_ROLE_GUEST, 0);
    bt_at(2, 1, 0x1c);
    wk_push(1, &retreat);
    dd_runtime_poll();
    CHECK(!it_retreats && g_incoming.count == 1);
    bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
    dd_runtime_poll();
    CHECK(it_retreats == 1);
    it_camp_reset(DD_ROLE_GUEST, 0);
    hook_retreat(wk_display);
    CHECK(it_retreats == 1 && !sent);
    /* A meal: the numbers go along, the guest builds its own arguments with its own overlay, at the same part of the camp. */
    {
        uint8_t args[0x28] = {0};
        uint32_t food = 0x40800000, what = 7, heal = 0x3f000000, stress = 0x40000000;
        args[8] = 1;
        memcpy(args + 0x0c, &food, 4);
        memcpy(args + 0x10, &what, 4);
        memcpy(args + 0x18, &heal, 4);
        memcpy(args + 0x1c, &stress, 4);
        memcpy(args + 0x20, &camp, 8);
        it_camp_reset(DD_ROLE_HOST, 3);
        hook_camp_meal(args);
        CHECK(it_meals == 1 && sent == 1 && last_message.u.town.action.u.item.op == DD_ITEM_OP_CAMP_MEAL && last_message.u.town.action.u.item.round == 3);
        dd_action meal = last_message.u.town.action;
        it_camp_reset(DD_ROLE_GUEST, 3);
        hook_camp_meal(args);
        CHECK(!it_meals && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
        it_camp_reset(DD_ROLE_GUEST, 2);
        wk_push(1, &meal);
        dd_runtime_poll();
        CHECK(!it_meals && g_incoming.count == 1);
        memcpy(wk_raid_lists + DD_ITEM_CAMP_STAGE, &(uint32_t){3}, 4);
        dd_runtime_poll();
        CHECK(it_meals == 1 && !memcmp(it_meal_args + 8, args + 8, 1) && !memcmp(it_meal_args + 0x0c, args + 0x0c, 8) && !memcmp(it_meal_args + 0x18, args + 0x18, 16));
        /* Another overlay: not ours to share. */
        { uint64_t foreign = 5; memcpy(args + 0x20, &foreign, 8); }
        it_camp_reset(DD_ROLE_GUEST, 3);
        hook_camp_meal(args);
        CHECK(it_meals == 1 && !sent);
        /* The other copy is past that part: it says so and goes on. */
        it_camp_reset(DD_ROLE_GUEST, 4);
        g_item_wait_until = 1;
        wk_push(1, &meal);
        dd_runtime_poll();
        CHECK(!it_meals && g_town.applied_seq == 1 && !g_town.needs_resync);
    }
    /* A camping skill: its three numbers; the game's own call passes; what the button does afterwards is done on the guest too. */
    {
        uint64_t hero = bt_party[1];
        int32_t chosen = 3, now = 0;
        void *c = wk_raid_lists + 0x5100, *p = wk_raid_lists + 8, *l = wk_raid_lists + 0x5140;
        it_camp_reset(DD_ROLE_HOST, 5);
        hook_camp_skill(c, p, l, 11, 12, 13);
        CHECK(it_skills == 1 && sent == 1 && last_message.u.town.action.u.item.turn == 11 && last_message.u.town.action.u.item.type == 12 && last_message.u.town.action.u.item.id == 13);
        dd_action skill = last_message.u.town.action;
        it_camp_reset(DD_ROLE_GUEST, 5);
        hook_camp_skill(c, p, l, 11, 12, 13);
        CHECK(!it_skills && sent == 1);
        it_camp_reset(DD_ROLE_GUEST, 5);
        g_camp_skill_caller = it_skill_no;
        hook_camp_skill(c, p, l, 1, 2, 3);
        CHECK(it_skills == 1 && !sent);
        it_camp_reset(DD_ROLE_GUEST, 5);
        memcpy(wk_raid_lists + 0x438, &hero, 8);
        memcpy(bt_heroes[1] + 0x141c, &chosen, 4);
        wk_push(1, &skill);
        dd_runtime_poll();
        CHECK(it_skills == 1 && it_skill_args[0] == 11 && it_skill_args[1] == 12 && it_skill_args[2] == 13);
        memcpy(&now, (uint8_t *)(uintptr_t)hero + 0x141c, 4);
        CHECK(now == -1);
    }
    /* Leave: a functor with the overlay. */
    {
        uint64_t functor[2] = {0, camp};
        it_camp_reset(DD_ROLE_HOST, 6);
        hook_camp_leave(functor);
        CHECK(it_leaves == 1 && sent == 1 && last_message.u.town.action.u.item.op == DD_ITEM_OP_CAMP_LEAVE);
        dd_action leave = last_message.u.town.action;
        it_camp_reset(DD_ROLE_GUEST, 6);
        hook_camp_leave(functor);
        CHECK(!it_leaves && sent == 1);
        it_camp_reset(DD_ROLE_GUEST, 6);
        wk_push(1, &leave);
        dd_runtime_poll();
        CHECK(it_leaves == 1 && it_leave_overlay == camp);
    }
    /* After a finished quest: the choice reaches both, each with its own panel. */
    {
        uint64_t functor[2] = {0, (uintptr_t)wk_display + DD_ITEM_QUEST_INFO}, foreign[2] = {0, 5};
        g_quest_one = it_quest_one;
        g_quest_two = it_quest_two;
        it_camp_reset(DD_ROLE_HOST, 0);
        it_ones = it_twos = 0;
        hook_quest_one(functor);
        CHECK(it_ones == 1 && !it_twos && sent == 1 && last_message.u.town.action.u.item.op == DD_ITEM_OP_QUEST_ONE);
        dd_action one = last_message.u.town.action;
        it_camp_reset(DD_ROLE_GUEST, 0);
        it_ones = it_twos = 0;
        hook_quest_two(functor);
        CHECK(!it_twos && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST && last_message.u.town.action.u.item.op == DD_ITEM_OP_QUEST_TWO);
        it_camp_reset(DD_ROLE_GUEST, 0);
        it_ones = 0;
        wk_push(1, &one);
        dd_runtime_poll();
        CHECK(it_ones == 1 && it_quest_panel == (uintptr_t)wk_display + DD_ITEM_QUEST_INFO);
        it_camp_reset(DD_ROLE_GUEST, 0);
        it_ones = 0;
        hook_quest_one(foreign);
        CHECK(it_ones == 1 && !sent);
        g_quest_one = g_quest_two = NULL;
    }
    g_retreat = NULL;
    g_camp_meal = NULL;
    g_camp_skill = NULL;
    g_camp_leave = NULL;
    g_camp_skill_caller = camp_skill_caller;
    memset(wk_raid_lists + DD_ITEM_CAMP_STAGE, 0, 4);
}
static unsigned it_nexts, it_cleared;
static int it_can;
static uint8_t it_results[0x20];
static uint64_t __fastcall it_next(void *display) { CHECK(display == it_results); it_nexts++; return 1; }
static uint8_t __fastcall it_results_can(void *display) { CHECK(display == it_results); return (uint8_t)it_can; }
static void __fastcall it_setup(void *display, uint64_t b, uint64_t c, uint64_t d) { (void)display, (void)b, (void)c, (void)d; }
static void __fastcall it_clear(void *item) { (void)item; it_cleared++; }
static uint64_t __fastcall it_drag(void *a, void *b, void *c, void *d) { (void)a, (void)b, (void)c, (void)d; return 7; }
static uint8_t it_bag[8 * DD_ITEM_SIZE], it_loot_items[3 * DD_ITEM_SIZE];
static void it_bag_slot(uint8_t *items, unsigned slot, int32_t count, uint32_t type, uint32_t id) {
    uint8_t *p = items + slot * DD_ITEM_SIZE;
    memcpy(p, &count, 4);
    memcpy(p + 0x44, &type, 4);
    memcpy(p + 0x88, &id, 4);
    p[0x100] = (uint8_t)(0x40 + type); /* Something else of the item, to see it travel. */
}
static void it_bag_set(void) {
    uint64_t b = (uintptr_t)it_bag, e = (uintptr_t)(it_bag + sizeof it_bag), lb = (uintptr_t)it_loot_items, le = (uintptr_t)(it_loot_items + sizeof it_loot_items);
    memset(it_bag, 0, sizeof it_bag);
    memset(it_loot_items, 0, sizeof it_loot_items);
    memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 8, &b, 8);
    memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 0x10, &e, 8);
    memcpy(wk_raid_lists + DD_WALK_RAID_LOOT + 8, &lb, 8);
    memcpy(wk_raid_lists + DD_WALK_RAID_LOOT + 0x10, &le, 8);
}
static void it_bag_clear_lists(void) {
    memset(wk_raid_lists + DD_WALK_RAID_INVENTORY + 8, 0, 16);
    memset(wk_raid_lists + DD_WALK_RAID_LOOT + 8, 0, 16);
}
static int32_t it_count(uint8_t *items, unsigned slot) { int32_t c = 0; memcpy(&c, items + slot * DD_ITEM_SIZE, 4); return c; }
static uint32_t it_type(uint8_t *items, unsigned slot) { uint32_t t = 0; memcpy(&t, items + slot * DD_ITEM_SIZE + 0x44, 4); return t; }
static int it_supply_view;
static int it_supply_open(supply_model *m, dd_provisions_snapshot *s) {
    memset(m, 0, sizeof *m);
    memset(s, 0, sizeof *s);
    s->stock_view = (uint8_t)(it_supply_view != 0); /* The screen's object stays after it closes. */
    s->panel_visible = (uint8_t)(it_supply_view == 1);
    return 1;
}
static unsigned it_tools;
static uint8_t it_tool_seen[0x98];
static void __fastcall it_tool(void *capture) {
    it_tools++;
    memcpy(it_tool_seen, capture, sizeof it_tool_seen);
}
static int it_assert_args[3];
static int __fastcall it_assert(const char *a, const char *b, const char *c, int line, const char *format, void *e, void *f, void *g, void *h) {
    (void)a, (void)b, (void)c, (void)format, (void)g;
    it_assert_args[0] = line;
    it_assert_args[1] = (int)(uintptr_t)e + (int)(uintptr_t)f;
    it_assert_args[2] = (int)(uintptr_t)h;
    return 5;
}
static uint64_t sc_hero(void) {
    uint64_t hero = 0, party_begin = 0;
    memcpy(&party_begin, wk_raid_lists + 8, 8);
    memcpy(&hero, (void *)(uintptr_t)party_begin, 8);
    return hero;
}
/* From the owner's run with two PCs, 6 Nov 18:02-18:10. */
static void second_copy_checks(void) {
    static uint8_t bag_items[3 * 0x5a8], capture[0x98];
    uint64_t begin = (uintptr_t)bag_items, end = (uintptr_t)(bag_items + sizeof bag_items), prop = (uintptr_t)wk_props[1];
    uint32_t type = 41, id = 42, mode = 2, two = 2, closed = 0, rules_host = 0x1111, rules_guest = 0x2222, now_rules = 0;
    uint8_t rules[0x10] = {0};
    uint8_t *panel = wk_display + DD_WALK_OVERLAY;
    uint64_t panel_at = (uintptr_t)panel;
    dd_action tool, order, use;
    int32_t count = 0;
    g_curio_tool = it_tool;
    g_item_clear = it_clear;
#define SC_BAG() do { memset(bag_items, 0, sizeof bag_items); memcpy(bag_items + 0x5a8, &two, 4); memcpy(bag_items + 0x5a8 + 0x44, &type, 4); memcpy(bag_items + 0x5a8 + 0x88, &id, 4); \
        memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 8, &begin, 8); memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 0x10, &end, 8); it_tools = 0; } while (0)
#define SC_OPEN(m) do { uint32_t mm = (m); memcpy(panel + 0xd88, &mm, 4); memcpy(panel + 0xd70, &prop, 8); memcpy(panel + 0x6a0, &prop, 8); { uint64_t doer = sc_hero(); memcpy(panel + 0xd78, &doer, 8); } } while (0)
    /* The shovel on the rubble: on both copies, once, from the host's order. */
    memcpy(capture, &panel_at, 8);
    memcpy(capture + 0x4c, &type, 4);
    memcpy(capture + 0x90, &id, 4);
    it_reset(DD_ROLE_HOST);
    SC_BAG();
    SC_OPEN(mode);
    hook_curio_tool(capture);
    CHECK(it_tools == 1 && sent == 1 && last_message.u.town.action.kind == DD_ACT_USE && last_message.u.town.action.u.use.door == 4 && last_message.u.town.action.u.use.item_type == type && last_message.u.town.action.u.use.item_id == id);
    tool = last_message.u.town.action;
    CHECK(dd_action_valid(&tool));
    it_reset(DD_ROLE_GUEST);
    SC_BAG();
    SC_OPEN(mode);
    hook_curio_tool(capture);
    CHECK(!it_tools && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST && last_message.u.town.action.u.use.door == 4);
    /* It waits until this copy's overlay is open; then a copy of its own item goes in. */
    it_reset(DD_ROLE_GUEST);
    SC_BAG();
    SC_OPEN(closed);
    wk_push(1, &tool);
    dd_runtime_poll();
    CHECK(!it_tools && g_incoming.count == 1);
    SC_OPEN(mode);
    dd_runtime_poll();
    {
        uint64_t seen = 0;
        uint32_t k = 0, i = 0;
        memcpy(&seen, it_tool_seen, 8);
        memcpy(&k, it_tool_seen + 0x4c, 4);
        memcpy(&i, it_tool_seen + 0x90, 4);
        CHECK(it_tools == 1 && seen == panel_at && k == type && i == id && g_town.applied_seq == 1);
    }
    /* No such tool here: it says so, nothing runs, the queue goes on. */
    it_reset(DD_ROLE_GUEST);
    SC_BAG();
    memset(bag_items, 0, sizeof bag_items);
    SC_OPEN(mode);
    wk_push(1, &tool);
    dd_runtime_poll();
    CHECK(!it_tools && g_town.applied_seq == 1 && !g_town.needs_resync);
    /* The host asked by a guest without its overlay open: refused, not run. */
    it_reset(DD_ROLE_HOST);
    SC_BAG();
    SC_OPEN(closed);
    {
        dd_msg m;
        memset(&m, 0, sizeof m);
        m.type = DD_MSG_TOWN_REQUEST;
        m.u.town.request_id = 9;
        m.u.town.action = tool;
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
    }
    CHECK(!it_tools && g_town.rejected == 1);
    /* The host's number goes before a use, from the place of the use. */
    it_reset(DD_ROLE_HOST);
    SC_BAG();
    SC_OPEN(mode);
    memcpy(rules + 8, &rules_host, 4);
    g_rules_object = rules;
    hook_curio_tool(capture);
    CHECK(sent == 2 && sent_messages[0].u.town.action.kind == DD_ACT_ITEM && sent_messages[0].u.town.action.u.item.op == DD_ITEM_OP_RULES && sent_messages[0].u.town.action.u.item.rules == rules_host && sent_messages[0].u.town.action.u.item.area == wk_view.area && sent_messages[1].u.town.action.kind == DD_ACT_USE);
    order = sent_messages[0].u.town.action;
    /* In a fight nothing extra goes out. */
    it_reset(DD_ROLE_HOST);
    g_rules_object = rules;
    wk_view.battle = 7;
    walk_rules_send(&wk_view);
    CHECK(!sent);
    wk_view.battle = 0;
    /* The guest, tiles behind: it takes the number over only at that spot, or those tiles would draw on top of it. */
    it_reset(DD_ROLE_GUEST);
    g_rules_object = rules;
    memcpy(rules + 8, &rules_guest, 4);
    wk_view.pos = 500.0f + 3 * DD_WALK_USE_NEAR;
    wk_push(1, &order);
    dd_runtime_poll();
    memcpy(&now_rules, rules + 8, 4);
    CHECK(now_rules == rules_guest && g_incoming.count == 1);
    memcpy(&wk_view.pos, &order.u.item.pos, 4);
    dd_runtime_poll();
    memcpy(&now_rules, rules + 8, 4);
    CHECK(now_rules == rules_host && g_town.applied_seq == 1);
    g_rules_object = NULL;
    /* A guest's second click on an item while the first is on its way uses nothing more. */
    {
        void *bag = wk_raid_lists + DD_WALK_RAID_INVENTORY;
        it_reset(DD_ROLE_GUEST);
        hook_item_use((void *)(uintptr_t)0x1234, (void *)(uintptr_t)sc_hero(), bag, 3, 1);
        CHECK(sent == 1);
        use = last_message.u.town.action;
        CHECK(!hook_item_use((void *)(uintptr_t)0x1234, (void *)(uintptr_t)sc_hero(), bag, 3, 1) && sent == 1);
        /* The order came back: the next click counts. */
        wk_push(1, &use);
        dd_runtime_poll();
        CHECK(it_uses == 1 && g_town.applied_seq == 1);
        hook_item_use((void *)(uintptr_t)0x1234, (void *)(uintptr_t)sc_hero(), bag, 3, 1);
        CHECK(sent == 2);
    }
    /* The curio's item: the copy without the drag takes one from its bag. */
    {
        dd_action curio;
        static uint8_t dragged[0x5a8];
        memcpy(dragged + 0x44, &type, 4);
        memcpy(dragged + 0x88, &id, 4);
        it_reset(DD_ROLE_HOST);
        SC_BAG();
        SC_OPEN(mode);
        CHECK(hook_curio_item(panel, dragged) == 1);
        curio = last_message.u.town.action;
        /* Host's own drag: its count is the game's business. */
        memcpy(&count, bag_items + 0x5a8, 4);
        CHECK(count == 2);
        it_reset(DD_ROLE_GUEST);
        SC_BAG();
        SC_OPEN(mode);
        wk_push(1, &curio);
        dd_runtime_poll();
        memcpy(&count, bag_items + 0x5a8, 4);
        CHECK(it_curios == 1 && count == 1 && it_curio_item == (void *)(bag_items + 0x5a8));
        /* The guest's own drag: already taken here, by the game. */
        it_reset(DD_ROLE_GUEST);
        SC_BAG();
        SC_OPEN(mode);
        CHECK(!hook_curio_item(panel, dragged) && !it_curios && sent == 1);
        wk_push(1, &curio);
        dd_runtime_poll();
        memcpy(&count, bag_items + 0x5a8, 4);
        CHECK(it_curios == 1 && count == 2);
        /* Its last one: only the overlay's hand still has it. */
        it_reset(DD_ROLE_GUEST);
        SC_BAG();
        memset(bag_items, 0, sizeof bag_items);
        SC_OPEN(mode);
        memcpy(panel + 0x7c0 + 0x44, &type, 4);
        memcpy(panel + 0x7c0 + 0x88, &id, 4);
        CHECK(!hook_curio_item(panel, dragged));
        wk_push(1, &curio);
        dd_runtime_poll();
        CHECK(it_curios == 1 && it_curio_item == (void *)(panel + 0x7c0));
        memset(panel + 0x7c0, 0, 0x90);
        /* The host, for a guest's drag: it has no drag, so it takes one; the last one empties its slot. */
        it_reset(DD_ROLE_HOST);
        SC_BAG();
        SC_OPEN(mode);
        {
            int32_t one = 1;
            dd_msg m;
            memcpy(bag_items + 0x5a8, &one, 4);
            it_cleared = 0;
            memset(&m, 0, sizeof m);
            m.type = DD_MSG_TOWN_REQUEST;
            m.u.town.request_id = 9;
            m.u.town.action = curio;
            CHECK(queue_push(&g_incoming, &m));
            dd_runtime_poll();
        }
        memcpy(&count, bag_items + 0x5a8, 4);
        CHECK(it_curios == 1 && !count && it_cleared == 1);
    }
    /* Asked for the bag: the host sends every slot, filled first, and the guest ends up with the host's. */
    {
        dd_msg m;
        dd_action ask;
        memset(&ask, 0, sizeof ask);
        ask.kind = DD_ACT_ITEM;
        ask.u.item.op = DD_ITEM_OP_BAG_ASK;
        ask.u.item.hero = 255;
        CHECK(dd_action_valid(&ask));
        it_reset(DD_ROLE_HOST);
        SC_BAG();
        memset(&m, 0, sizeof m);
        m.type = DD_MSG_TOWN_REQUEST;
        m.u.town.request_id = 9;
        m.u.town.action = ask;
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        CHECK(sent == 6 && sent_messages[0].u.town.action.u.item.op == DD_ITEM_OP_BAG_CHECK && sent_messages[1].u.town.action.u.item.slot == 1 && sent_messages[1].u.town.action.u.item.amount == 2 && sent_messages[2].u.town.action.u.item.slot == 0 && !sent_messages[2].u.town.action.u.item.amount && sent_messages[4].u.town.action.u.item.op == DD_ITEM_OP_BAG_CHECK && sent_messages[5].u.town.action.u.item.op == DD_ITEM_OP_BAG_ASK);
        order = sent_messages[1].u.town.action;
        it_reset(DD_ROLE_GUEST);
        SC_BAG();
        {
            int32_t four = 4;
            memcpy(bag_items + 0x5a8, &four, 4);
        }
        wk_push(1, &order);
        dd_runtime_poll();
        memcpy(&count, bag_items + 0x5a8, 4);
        CHECK(count == 2);
        /* The ask itself does nothing on the guest. */
        wk_push(2, &ask);
        dd_runtime_poll();
        CHECK(g_town.applied_seq == 2 && !sent);
    }
    /* What the game asserts goes in the log; all its arguments go on. */
    g_game_assert = it_assert;
    CHECK(hook_game_assert("x", NULL, "z", 3, "a %d", (void *)(uintptr_t)4, (void *)(uintptr_t)5, NULL, (void *)(uintptr_t)6) == 5 && it_assert_args[0] == 3 && it_assert_args[1] == 9 && it_assert_args[2] == 6);
    g_game_assert = NULL;
    CHECK(hook_game_assert("x", "y", "z", 3, "a", NULL, NULL, NULL, NULL) == 1);
    /* An order running while the game pumps events: the next waits, nothing fails. */
    {
        dd_action a;
        it_reset(DD_ROLE_GUEST);
        CHECK(walk_use_order(&wk_view, prop, sc_hero(), 0, 0, 0, &a));
        wk_use_nest = 1;
        wk_push(1, &a);
        wk_push(2, &a);
        dd_runtime_poll();
        wk_use_nest = 0;
        CHECK(wk_uses == 2 && g_town.applied_seq == 2 && !g_town.needs_resync);
    }
    memset(panel + 0xd70, 0, 8);
    memset(panel + 0xd88, 0, 4);
    memset(panel + 0x6a0, 0, 8);
    g_curio_tool = NULL;
    g_item_clear = NULL;
    memset(wk_raid_lists + DD_WALK_RAID_INVENTORY, 0, 0x18);
    puts("Second copy: tool button, Rules before a use, one click, curio count, the host's bag, nested pump");
#undef SC_BAG
#undef SC_OPEN
}
static int __fastcall it_drop_shared(void *a, void *b, void *c) { return (int)it_drag(a, b, c, NULL); }
static void results_and_bag_checks(void) {
    uint8_t rules[0x10] = {0};
    uint32_t state = 0x1111, now = 0;
    /* The results screen: the host moves on, with its numbers first; the guest asks once and goes on alone if nobody answers. */
    g_results_setup = it_setup;
    g_results_next = it_next;
    g_results_can = it_results_can;
    it_reset(DD_ROLE_HOST);
    it_nexts = 0;
    it_can = 1;
    g_rules_object = rules;
    memcpy(rules + 8, &state, 4);
    hook_results_setup(it_results, 0, 0, 0);
    CHECK(g_results_display == (uintptr_t)it_results);
    CHECK(hook_results_next(it_results) == 1 && it_nexts == 1 && sent == 2 && last_message.u.town.action.u.item.op == DD_ITEM_OP_RESULTS_NEXT);
    dd_action next = last_message.u.town.action;
    it_reset(DD_ROLE_GUEST);
    it_nexts = 0;
    g_rules_object = rules;
    hook_results_setup(it_results, 0, 0, 0);
    CHECK(!hook_results_next(it_results) && !it_nexts && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
    CHECK(!hook_results_next(it_results) && !it_nexts && sent == 1); /* Once. */
    g_results_asked_at = GetTickCount64() - 3500;
    CHECK(hook_results_next(it_results) == 1 && it_nexts == 1 && sent == 1 && !g_results_asked_at);
    /* The Rules order only changes the guest; the step waits until the screen can go on, and for at most 10 s. */
    {
        dd_action r;
        memset(&r, 0, sizeof r);
        r.kind = DD_ACT_ITEM;
        r.u.item.op = DD_ITEM_OP_RULES;
        r.u.item.hero = 255;
        r.u.item.rules = 0x2222;
        it_reset(DD_ROLE_GUEST);
        it_nexts = 0;
        g_rules_object = rules;
        memcpy(rules + 8, &state, 4);
        hook_results_setup(it_results, 0, 0, 0);
        wk_push(1, &r);
        dd_runtime_poll();
        memcpy(&now, rules + 8, 4);
        CHECK(now == 0x2222);
        it_can = 0;
        wk_push(2, &next);
        dd_runtime_poll();
        CHECK(!it_nexts && g_incoming.count == 1);
        it_can = 1;
        dd_runtime_poll();
        CHECK(it_nexts == 1 && !g_incoming.count);
        /* It never can: the queue goes on, without a step. */
        it_reset(DD_ROLE_GUEST);
        it_nexts = 0;
        it_can = 0;
        hook_results_setup(it_results, 0, 0, 0);
        wk_push(1, &next);
        dd_runtime_poll();
        CHECK(g_incoming.count == 1);
        g_item_wait_until = 1;
        dd_runtime_poll();
        CHECK(!it_nexts && !g_incoming.count && g_town.applied_seq == 1 && !g_town.needs_resync);
    }
    /* A guest's click at the host: the numbers go out first, then the step. */
    {
        dd_msg m;
        memset(&m, 0, sizeof m);
        m.type = DD_MSG_TOWN_REQUEST;
        m.u.town.request_id = 8;
        m.u.town.action = next;
        it_reset(DD_ROLE_HOST);
        it_nexts = 0;
        it_can = 1;
        g_rules_object = rules;
        hook_results_setup(it_results, 0, 0, 0);
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        CHECK(it_nexts == 1 && sent == 2 && last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.action.u.item.op == DD_ITEM_OP_RESULTS_NEXT);
    }
    g_rules_object = NULL;
    g_results_setup = NULL;
    g_results_next = NULL;
    g_results_can = results_can_native;
    /* The bag. A drag is sent as the changed slots, filled ones first. A change without a drag is not sent. */
    g_item_clear = it_clear;
    g_trinket_drop[0] = it_drop_shared;
    it_reset(DD_ROLE_HOST);
    it_bag_set();
    it_bag_slot(it_bag, 0, 2, 11, 21);
    it_bag_slot(it_bag, 1, 1, 12, 22);
    bag_poll(&wk_view);
    CHECK(!sent);
    it_bag_slot(it_bag, 1, 2, 12, 22); /* The game's own. */
    bag_poll(&wk_view);
    CHECK(!sent);
    /* What an order does to the slots is no drag. */
    g_game_apply_depth = 1;
    CHECK(hook_trinket_drop_hero(NULL, NULL, NULL) == 7 && !g_bag_moved);
    g_game_apply_depth = 0;
    /* A move from slot 0 to empty slot 3. */
    CHECK(hook_trinket_drop_hero(NULL, NULL, NULL) == 7 && g_bag_moved);
    { uint8_t held[DD_ITEM_SIZE]; memcpy(held, it_bag, DD_ITEM_SIZE); memcpy(it_bag + 3 * DD_ITEM_SIZE, held, DD_ITEM_SIZE); memset(it_bag, 0, DD_ITEM_SIZE); }
    bag_poll(&wk_view);
    CHECK(sent == 2 && !g_bag_moved);
    {
        dd_msg first, second;
        first = sent_messages[0];
        second = sent_messages[1];
        CHECK(first.u.town.action.u.item.slot == 3 && first.u.town.action.u.item.amount == 2 && first.u.town.action.u.item.type == 11);
        CHECK(second.u.town.action.u.item.slot == 0 && !second.u.town.action.u.item.amount);
        /* The guest: the item itself moves, with all that is in it. */
        it_reset(DD_ROLE_GUEST);
        it_bag_set();
        it_cleared = 0;
        it_bag_slot(it_bag, 0, 2, 11, 21);
        it_bag_slot(it_bag, 1, 2, 12, 22);
        bag_poll(&wk_view);
        CHECK(!sent);
        wk_push(1, &first.u.town.action);
        wk_push(2, &second.u.town.action);
        dd_runtime_poll();
        CHECK(it_count(it_bag, 3) == 2 && it_type(it_bag, 3) == 11 && it_bag[3 * DD_ITEM_SIZE + 0x100] == 0x40 + 11 && !it_count(it_bag, 0));
        CHECK(it_count(it_bag, 1) == 2 && g_town.applied_seq == 2);
        { int32_t filled = 0; memcpy(&filled, wk_raid_lists + DD_WALK_RAID_INVENTORY + 0x24, 4); CHECK(filled == 2); }
        /* And it is not sent back. */
        bag_poll(&wk_view);
        CHECK(!sent);
    }
    /* Merging: the count is taken; the emptied slot is cleared the game's way. */
    {
        dd_action a;
        memset(&a, 0, sizeof a);
        a.kind = DD_ACT_ITEM;
        a.u.item.op = DD_ITEM_OP_BAG_SLOT;
        a.u.item.hero = 255;
        a.u.item.slot = 1;
        a.u.item.amount = 5;
        a.u.item.type = 11;
        a.u.item.id = 21;
        it_reset(DD_ROLE_GUEST);
        it_bag_set();
        it_cleared = 0;
        it_bag_slot(it_bag, 0, 2, 11, 21);
        it_bag_slot(it_bag, 1, 3, 11, 21);
        wk_push(1, &a);
        a.u.item.slot = 0;
        a.u.item.amount = 0;
        wk_push(2, &a);
        dd_runtime_poll();
        CHECK(it_count(it_bag, 1) == 5 && !it_count(it_bag, 0) && it_cleared == 1);
        /* A swap of two slots. */
        a.u.item.slot = 0;
        a.u.item.amount = 3;
        a.u.item.type = 12;
        a.u.item.id = 22;
        it_reset(DD_ROLE_GUEST);
        it_bag_set();
        it_bag_slot(it_bag, 0, 2, 11, 21);
        it_bag_slot(it_bag, 1, 3, 12, 22);
        wk_push(1, &a);
        dd_runtime_poll();
        CHECK(it_type(it_bag, 0) == 12 && it_count(it_bag, 0) == 3 && it_type(it_bag, 1) == 11 && it_count(it_bag, 1) == 2);
        /* From the loot window into the bag. */
        a.u.item.slot = 4;
        a.u.item.amount = 1;
        a.u.item.type = 30;
        a.u.item.id = 31;
        it_reset(DD_ROLE_GUEST);
        it_bag_set();
        it_bag_slot(it_loot_items, 1, 1, 30, 31);
        wk_push(1, &a);
        dd_runtime_poll();
        CHECK(it_type(it_bag, 4) == 30 && it_count(it_bag, 4) == 1 && !it_count(it_loot_items, 1));
        /* Not here at all, or no such slot: nothing breaks. */
        a.u.item.type = 99;
        it_reset(DD_ROLE_GUEST);
        it_bag_set();
        wk_push(1, &a);
        dd_runtime_poll();
        CHECK(!it_count(it_bag, 4) && g_town.applied_seq == 1 && !g_town.needs_resync);
        a.u.item.slot = 40;
        it_reset(DD_ROLE_GUEST);
        it_bag_set();
        wk_push(1, &a);
        dd_runtime_poll();
        CHECK(g_town.applied_seq == 1 && !g_town.needs_resync);
    }
    /* A split: the new stack names its source and is copied there, so the source keeps its share. Two stacks of one kind: the
     * moved one names its own slot, and the other stack stays. */
    {
        dd_msg first, second;
        hamlet_copy_fn saved_copy = g_hamlet_copy;
        hamlet_swap_fn saved_swap = g_hamlet_swap;
        hamlet_notify_fn saved_destroy = g_hamlet_destroy;
        it_reset(DD_ROLE_HOST);
        it_bag_set();
        it_bag_slot(it_bag, 0, 5, 11, 21);
        bag_poll(&wk_view);
        g_bag_moved = 1;
        it_bag_slot(it_bag, 0, 3, 11, 21);
        it_bag_slot(it_bag, 2, 2, 11, 21);
        bag_poll(&wk_view);
        CHECK(sent == 2);
        first = sent_messages[0];
        second = sent_messages[1];
        CHECK(first.u.town.action.u.item.slot == 0 && first.u.town.action.u.item.amount == 3 && !first.u.town.action.u.item.round);
        CHECK(second.u.town.action.u.item.slot == 2 && second.u.town.action.u.item.amount == 2 && second.u.town.action.u.item.round == 1 &&
              second.u.town.action.u.item.turn == 0 && second.u.town.action.u.item.flag == 1);
        /* The guest, with the game's own Item copy: both stacks, each with what is in it. */
        it_reset(DD_ROLE_GUEST);
        it_bag_set();
        it_bag_slot(it_bag, 0, 5, 11, 21);
        g_hamlet_copy = hm_copy;
        g_hamlet_swap = hm_swap;
        g_hamlet_destroy = hm_destroy;
        hm_item_alive = 0;
        wk_push(1, &first.u.town.action);
        wk_push(2, &second.u.town.action);
        dd_runtime_poll();
        CHECK(it_count(it_bag, 0) == 3 && it_count(it_bag, 2) == 2 && it_type(it_bag, 2) == 11 && it_bag[2 * DD_ITEM_SIZE + 0x100] == 0x40 + 11);
        CHECK(!hm_item_alive && g_town.applied_seq == 2);
        { int32_t filled = 0; memcpy(&filled, wk_raid_lists + DD_WALK_RAID_INVENTORY + 0x24, 4); CHECK(filled == 2); }
        /* The other order: the new stack first. */
        it_reset(DD_ROLE_GUEST);
        it_bag_set();
        it_bag_slot(it_bag, 0, 5, 11, 21);
        wk_push(1, &second.u.town.action);
        wk_push(2, &first.u.town.action);
        dd_runtime_poll();
        CHECK(it_count(it_bag, 0) == 3 && it_count(it_bag, 2) == 2);
        /* Two stacks of food; the second one moves to slot 3. */
        it_reset(DD_ROLE_HOST);
        it_bag_set();
        it_bag_slot(it_bag, 0, 5, 11, 21);
        it_bag_slot(it_bag, 1, 2, 11, 21);
        bag_poll(&wk_view);
        g_bag_moved = 1;
        { uint8_t held[DD_ITEM_SIZE]; memcpy(held, it_bag + DD_ITEM_SIZE, DD_ITEM_SIZE); memcpy(it_bag + 3 * DD_ITEM_SIZE, held, DD_ITEM_SIZE); memset(it_bag + DD_ITEM_SIZE, 0, DD_ITEM_SIZE); }
        bag_poll(&wk_view);
        CHECK(sent == 2);
        first = sent_messages[0];
        second = sent_messages[1];
        CHECK(first.u.town.action.u.item.slot == 3 && first.u.town.action.u.item.round == 2 && !first.u.town.action.u.item.flag);
        it_reset(DD_ROLE_GUEST);
        it_bag_set();
        it_bag_slot(it_bag, 0, 5, 11, 21);
        it_bag_slot(it_bag, 1, 2, 11, 21);
        wk_push(1, &first.u.town.action);
        wk_push(2, &second.u.town.action);
        dd_runtime_poll();
        CHECK(it_count(it_bag, 0) == 5 && it_count(it_bag, 3) == 2 && !it_count(it_bag, 1) && !hm_item_alive);
        /* A split without the copy functions: not copied (the old way), and nothing breaks. */
        g_hamlet_copy = NULL;
        it_reset(DD_ROLE_GUEST);
        it_bag_set();
        it_bag_slot(it_bag, 0, 5, 11, 21);
        {
            dd_action split = first.u.town.action;
            split.u.item.slot = 2;
            split.u.item.amount = 2;
            split.u.item.round = 1;
            split.u.item.flag = 1;
            wk_push(1, &split);
            dd_runtime_poll();
            CHECK(it_count(it_bag, 2) == 2 && g_town.applied_seq == 1 && !g_town.needs_resync);
        }
        g_hamlet_copy = saved_copy;
        g_hamlet_swap = saved_swap;
        g_hamlet_destroy = saved_destroy;
    }
    /* In the provisions screen a drag does nothing, on the bag and on the shop; a message once. Outside it, it goes on. */
    {
        int before;
        it_reset(DD_ROLE_GUEST);
        g_bag_shop_drop = it_drag;
        g_supply_open = it_supply_open;
        it_supply_view = 1;
        /* In a raid the bag is shared, whatever the town's panel says. */
        CHECK(hook_trinket_drop_hero(NULL, NULL, NULL) == 7 && g_bag_moved);
        g_bag_moved = 0;
        wk_view_fail = 1;
        before = alerts;
        CHECK(!hook_trinket_drop_hero(NULL, NULL, NULL) && !g_bag_moved && alerts == before + 1);
        CHECK(!hook_bag_shop_drop(NULL, NULL, NULL, NULL) && alerts == before + 1);
        it_supply_view = 2;
        CHECK(hook_trinket_drop_hero(NULL, NULL, NULL) == 7 && g_bag_moved);
        it_supply_view = 0;
        CHECK(hook_bag_shop_drop(NULL, NULL, NULL, NULL) == 7);
        g_game_apply_depth = 1;
        it_supply_view = 1;
        CHECK(hook_bag_shop_drop(NULL, NULL, NULL, NULL) == 7);
        g_game_apply_depth = 0;
        wk_view_fail = 0;
        g_supply_open = supply_open;
        g_bag_shop_drop = NULL;
    }
    /* In town a drag means nothing: there is no raid to share it in. */
    it_reset(DD_ROLE_HOST);
    it_bag_set();
    g_bag_moved = 1;
    wk_view_fail = 1;
    overlay_turn_state = DD_OVERLAY_TURN_MINE;
    g_walk_selected = 2;
    walk_poll();
    wk_view_fail = 0;
    CHECK(!g_bag_moved && !sent && overlay_turn_state == DD_OVERLAY_TURN_NONE && g_walk_selected == -1);
    it_bag_clear_lists();
    g_item_clear = NULL;
    g_trinket_drop[0] = NULL;
}
static void item_transactions(void) {
    void *bag = wk_raid_lists + DD_WALK_RAID_INVENTORY, *loot = wk_raid_lists + DD_WALK_RAID_LOOT, *hero;
    uint64_t functor[2] = {0, (uintptr_t)wk_display + DD_WALK_LOOT_OVERLAY};
    CHECK(item_torch_caller(0x783d2b) && item_torch_caller(0x783dd6) && !item_torch_caller(0x430dc0) && !item_torch_caller(0x4328e7));
    /* Using an item: the host runs it and shares it; the guest asks; the order runs on the guest with the same hero, slot and bag. */
    it_reset(DD_ROLE_HOST);
    hero = (void *)(uintptr_t)bt_party[1];
    CHECK(hook_item_use((void *)(uintptr_t)0x1234, hero, bag, 3, 1) == 1 && it_uses == 1 && sent == 1 && last_message.type == DD_MSG_TOWN_APPLY);
    dd_action use = last_message.u.town.action;
    CHECK(use.kind == DD_ACT_ITEM && use.u.item.op == DD_ITEM_OP_USE && use.u.item.slot == 3 && !use.u.item.inv && use.u.item.hero == 1 && !use.u.item.state && use.u.item.flag == 1);
    it_reset(DD_ROLE_GUEST);
    CHECK(!hook_item_use((void *)(uintptr_t)0x1234, hero, bag, 3, 1) && !it_uses && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
    it_reset(DD_ROLE_GUEST);
    wk_push(1, &use);
    dd_runtime_poll();
    CHECK(it_uses == 1 && it_slot == 3 && it_inventory == bag && it_hero == hero && g_town.applied_seq == 1);
    /* From the loot window. */
    it_reset(DD_ROLE_HOST);
    CHECK(hook_item_use((void *)(uintptr_t)0x1234, hero, loot, 2, 1) == 1 && last_message.u.town.action.u.item.inv == 1);
    use = last_message.u.town.action;
    it_reset(DD_ROLE_GUEST);
    wk_push(1, &use);
    dd_runtime_poll();
    CHECK(it_uses == 1 && it_inventory == loot && it_slot == 2);
    /* Another inventory is not guessed at. */
    it_reset(DD_ROLE_GUEST);
    CHECK(hook_item_use((void *)(uintptr_t)0x1234, hero, wk_display, 2, 1) == 1 && it_uses == 1 && !sent);
    /* Inventory use is independent of corridor pixel position; waiting for it caused late battle replay. */
    it_reset(DD_ROLE_GUEST);
    wk_view.pos = 2000.0f;
    wk_push(1, &use);
    dd_runtime_poll();
    CHECK(it_uses == 1 && !g_incoming.count);
    wk_view.pos = 500.0f;
    dd_runtime_poll();
    CHECK(it_uses == 1 && !g_incoming.count);
    /* In a fight: at the turn's choosing only, for your own hero, and at the same point on the other copy. */
    it_reset(DD_ROLE_HOST);
    bt_at(2, 1, 0x1f);
    CHECK(!hook_item_use((void *)(uintptr_t)0x1234, hero, bag, 3, 1) && !it_uses && !sent);
    it_reset(DD_ROLE_HOST);
    bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
    g_hero_owner_mask = 2;
    CHECK(!hook_item_use((void *)(uintptr_t)0x1234, hero, bag, 3, 1) && !it_uses && !sent);
    it_reset(DD_ROLE_HOST);
    bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
    CHECK(hook_item_use((void *)(uintptr_t)0x1234, hero, bag, 3, 1) == 1 && it_uses == 1 && last_message.u.town.action.u.item.state == DD_BATTLE_CHOOSE_TARGET_STATE);
    use = last_message.u.town.action;
    it_reset(DD_ROLE_GUEST);
    bt_at(2, 1, 0x1c);
    wk_push(1, &use);
    dd_runtime_poll();
    CHECK(!it_uses && g_incoming.count == 1);
    bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
    dd_runtime_poll();
    CHECK(it_uses == 1 && g_town.applied_seq == 1);
    /* A guest asking for the host's hero in a fight is told no. */
    {
        dd_msg m;
        memset(&m, 0, sizeof m);
        m.type = DD_MSG_TOWN_REQUEST;
        m.u.town.request_id = 4;
        m.u.town.action = use;
        it_reset(DD_ROLE_HOST);
        bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        CHECK(!it_uses && last_message.type == DD_MSG_TOWN_REJECT);
    }
    /* The torch: the player's click is an order with the amount; the game's own burning passes. */
    it_reset(DD_ROLE_HOST);
    hook_torch(wk_raid_lists, -6.0f, 1);
    CHECK(it_torches == 1 && sent == 1 && last_message.u.town.action.u.item.op == DD_ITEM_OP_TORCH);
    dd_action torch = last_message.u.town.action;
    it_reset(DD_ROLE_GUEST);
    hook_torch(wk_raid_lists, -6.0f, 1);
    CHECK(!it_torches && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
    it_reset(DD_ROLE_GUEST);
    wk_push(1, &torch);
    dd_runtime_poll();
    CHECK(it_torches == 1 && it_amount == -6.0f);
    it_reset(DD_ROLE_GUEST);
    g_item_torch_caller = it_caller_no;
    hook_torch(wk_raid_lists, -1.0f, 1);
    CHECK(it_torches == 1 && !sent);
    /* Loot: the host's take-all sets its own flag and goes out; the guest's only asks; the order sets the flag once the window is open. */
    it_reset(DD_ROLE_HOST);
    it_loot(2);
    hook_loot_take_all(functor);
    CHECK(it_alls == 1 && sent == 1 && last_message.u.town.action.u.item.op == DD_ITEM_OP_LOOT_ALL);
    dd_action all = last_message.u.town.action;
    /* Actual two-PC timing: a visible loot overlay precedes the end of
     * battle/presentation. The button flag would be erased in those frames. */
    it_reset(DD_ROLE_GUEST);
    it_loot(2);
    bt_at(3, 3, 34);
    wk_push(1, &all);
    dd_runtime_poll();
    CHECK(g_incoming.count == 1 && !wk_display[DD_WALK_LOOT_OVERLAY + 0x218]);
    wk_view.battle = 0;
    wk_display[DD_WALK_LOOT_OVERLAY + 0x170] = 0;
    dd_runtime_poll();
    CHECK(g_incoming.count == 1);
    wk_display[DD_WALK_LOOT_OVERLAY + 0x170] = 1;
    dd_runtime_poll();
    CHECK(!g_incoming.count && g_town.applied_seq == 1 && wk_display[DD_WALK_LOOT_OVERLAY + 0x218] == 1);
    it_reset(DD_ROLE_GUEST);
    it_loot(2);
    hook_loot_take_all(functor);
    CHECK(!it_alls && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
    it_reset(DD_ROLE_GUEST);
    memset(wk_display + DD_WALK_LOOT_OVERLAY + 0x218, 0, 2);
    wk_push(1, &all);
    dd_runtime_poll();
    CHECK(g_incoming.count == 1 && !wk_display[DD_WALK_LOOT_OVERLAY + 0x218]);
    it_loot(2);
    dd_runtime_poll();
    CHECK(wk_display[DD_WALK_LOOT_OVERLAY + 0x218] == 1 && !wk_display[DD_WALK_LOOT_OVERLAY + 0x219] && g_town.applied_seq == 1);
    /* The shared click must not enable unshared clicks in the next loot window. */
    hook_loot_close(functor);
    CHECK(!it_closes && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
    /* Close. */
    it_reset(DD_ROLE_HOST);
    it_loot(2);
    hook_loot_close(functor);
    CHECK(it_closes == 1 && last_message.u.town.action.u.item.op == DD_ITEM_OP_LOOT_CLOSE);
    dd_action close_order = last_message.u.town.action;
    it_reset(DD_ROLE_GUEST);
    it_loot(2);
    memset(wk_display + DD_WALK_LOOT_OVERLAY + 0x218, 0, 2);
    wk_push(1, &close_order);
    dd_runtime_poll();
    CHECK(wk_display[DD_WALK_LOOT_OVERLAY + 0x219] == 1 && !wk_display[DD_WALK_LOOT_OVERLAY + 0x218]);
    /* A missing loot window does not acknowledge a click silently, but the expedition need not stop for it. */
    it_reset(DD_ROLE_GUEST);
    wk_push(1, &close_order);
    dd_runtime_poll();
    CHECK(g_incoming.count == 1 && g_item_wait_until);
    g_item_wait_until = 1;
    {
        int before = alerts;
        dd_runtime_poll();
        CHECK(!g_incoming.count && g_town.applied_seq == 1 && !g_town.needs_resync && alerts == before + 1 && !g_loot_escape);
    }
    /* Open but never ready: told, and this window may now be closed here. */
    it_reset(DD_ROLE_GUEST);
    it_loot(1);
    wk_push(1, &close_order);
    dd_runtime_poll();
    g_item_wait_until = 1;
    dd_runtime_poll();
    CHECK(g_town.applied_seq == 1 && g_loot_escape && !sent);
    hook_loot_close(functor);
    CHECK(it_closes == 1 && !sent);
    it_loot(0);
    bag_poll_hook(&wk_view);
    CHECK(!g_loot_escape);
    /* A second click on a window that is closing goes nowhere, on either copy: an order for it would wait for a window that is gone. */
    it_reset(DD_ROLE_HOST);
    it_loot(3);
    hook_loot_take_all(functor);
    hook_loot_close(functor);
    CHECK(!sent && !it_alls && !it_closes);
    it_reset(DD_ROLE_GUEST);
    it_loot(3);
    hook_loot_take_all(functor);
    CHECK(!sent && !it_alls);
    /* The guest's double click: once. Another button or the answer makes it count again. */
    it_reset(DD_ROLE_GUEST);
    it_loot(2);
    hook_loot_take_all(functor);
    hook_loot_take_all(functor);
    CHECK(sent == 1 && g_loot_request);
    hook_loot_close(functor);
    CHECK(sent == 2);
    {
        dd_msg no;
        memset(&no, 0, sizeof no);
        no.type = DD_MSG_TOWN_REJECT;
        no.u.reject.request_id = last_message.u.town.request_id;
        /* Refused with this window open: nothing shared can close it any more. */
        CHECK(queue_push(&g_incoming, &no));
        dd_runtime_poll();
        CHECK(g_loot_escape && !g_loot_asked_at);
        it_alls = 0;
        hook_loot_take_all(functor);
        CHECK(it_alls == 1 && sent == 2);
    }
    /* Someone else's refusal says nothing about it. */
    it_reset(DD_ROLE_GUEST);
    it_loot(2);
    hook_loot_take_all(functor);
    loot_request_done(g_loot_request + 5);
    CHECK(g_loot_request && !g_loot_escape);
    /* Refused, but the host's order had already closed it here. */
    it_loot(3);
    loot_request_done(g_loot_request);
    CHECK(!g_loot_request && !g_loot_escape);
    /* Both click: the host's came first. The guest's is told no at once, and nothing it asks for waits behind it. */
    it_reset(DD_ROLE_HOST);
    it_loot(2);
    hook_loot_take_all(functor);
    it_loot(3);
    {
        dd_msg m;
        memset(&m, 0, sizeof m);
        m.type = DD_MSG_TOWN_REQUEST;
        m.u.town.request_id = 7;
        m.u.town.action = close_order;
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        CHECK(!g_incoming.count && g_town.rejected == 1 && !g_item_wait_until);
    }
    /* The next window is opening already: that click is for it, and it waits. */
    it_reset(DD_ROLE_HOST);
    it_loot(2);
    hook_loot_take_all(functor);
    it_loot(1);
    {
        dd_msg m;
        memset(&m, 0, sizeof m);
        m.type = DD_MSG_TOWN_REQUEST;
        m.u.town.request_id = 7;
        m.u.town.action = close_order;
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        CHECK(g_incoming.count == 1 && !g_town.rejected);
        queue_clear(&g_incoming);
    }
    /* The host's window is not there yet: the guest was first, and it waits. */
    it_reset(DD_ROLE_HOST);
    {
        dd_msg m;
        memset(&m, 0, sizeof m);
        m.type = DD_MSG_TOWN_REQUEST;
        m.u.town.request_id = 7;
        m.u.town.action = close_order;
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        CHECK(g_incoming.count == 1 && !g_town.rejected);
        it_loot(2);
        dd_runtime_poll();
        CHECK(!g_incoming.count && g_town.accepted == 1 && wk_display[DD_WALK_LOOT_OVERLAY + 0x219] == 1);
    }
    /* Too late at the host: the window has closed there, so the guest's click is told no. */
    {
        dd_msg m;
        memset(&m, 0, sizeof m);
        m.type = DD_MSG_TOWN_REQUEST;
        m.u.town.request_id = 6;
        m.u.town.action = close_order;
        it_reset(DD_ROLE_HOST);
        it_loot(3);
        memset(wk_display + DD_WALK_LOOT_OVERLAY + 0x218, 0, 2);
        g_item_wait_until = 1;
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_REJECT && !wk_display[DD_WALK_LOOT_OVERLAY + 0x219]);
    }
    /* One item: the place goes along; the guest's button is made from its own raid, with an empty widget. */
    {
        uint64_t button[6] = {0};
        int32_t place = 2;
        memcpy((uint8_t *)button + 0x10, &place, 4);
        button[4] = (uintptr_t)wk_raid_lists + DD_WALK_RAID_LOOT - 0x100;
        it_reset(DD_ROLE_HOST);
        it_loot(2);
        hook_loot_take_item(button);
        CHECK(it_items == 1 && sent == 1 && last_message.u.town.action.u.item.op == DD_ITEM_OP_LOOT_ITEM && last_message.u.town.action.u.item.slot == 2);
        dd_action one = last_message.u.town.action;
        it_reset(DD_ROLE_GUEST);
        it_loot(2);
        hook_loot_take_item(button);
        CHECK(!it_items && sent == 1);
        it_reset(DD_ROLE_GUEST);
        it_loot(2);
        wk_push(1, &one);
        dd_runtime_poll();
        CHECK(it_items == 1 && it_button_slot == 2 && it_button_loot == (uintptr_t)wk_raid_lists + DD_WALK_RAID_LOOT - 0x100 && it_button_widget == (uintptr_t)g_loot_dummy_widget);
        /* Another button shape is left alone. */
        button[4] = 5;
        it_reset(DD_ROLE_GUEST);
        hook_loot_take_item(button);
        CHECK(it_items == 1 && !sent);
    }
    /* A curio's item slot: the drop is the order, by kind; the guest finds that kind in its own bag, once its curio is open. */
    {
        static uint8_t dragged[0x100], bag_items[3 * 0x5a8];
        uint64_t begin = (uintptr_t)bag_items, end = (uintptr_t)(bag_items + sizeof bag_items), prop = 0x77;
        uint32_t type = 41, id = 42, mode = 2, none = 0, one = 1, other = 7;
        CHECK(item_curio_caller(DD_ITEM_CURIO_DROP_RVA) && !item_curio_caller(0x4298d6));
        memcpy(dragged + 0x44, &type, 4);
        memcpy(dragged + 0x88, &id, 4);
        it_reset(DD_ROLE_HOST);
        CHECK(hook_curio_item(wk_display + DD_WALK_OVERLAY, dragged) == 1 && it_curios == 1 && sent == 1 && last_message.u.town.action.u.item.op == DD_ITEM_OP_CURIO && last_message.u.town.action.u.item.type == 41 && last_message.u.town.action.u.item.id == 42);
        dd_action curio = last_message.u.town.action;
        it_reset(DD_ROLE_GUEST);
        CHECK(!hook_curio_item(wk_display + DD_WALK_OVERLAY, dragged) && !it_curios && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
        /* From inside the game's item use it is no new order. */
        it_reset(DD_ROLE_GUEST);
        g_item_curio_caller = it_curio_no;
        CHECK(hook_curio_item(wk_display + DD_WALK_OVERLAY, dragged) == 1 && it_curios == 1 && !sent);
        /* The guest: a used-up stack and another kind are passed over. */
        memset(bag_items, 0, sizeof bag_items);
        memcpy(bag_items + 0x44, &type, 4);
        memcpy(bag_items + 0x88, &id, 4); /* Right kind, empty. */
        memcpy(bag_items + 0x5a8, &one, 4);
        memcpy(bag_items + 0x5a8 + 0x44, &type, 4);
        memcpy(bag_items + 0x5a8 + 0x88, &other, 4);
        memcpy(bag_items + 2 * 0x5a8, &one, 4);
        memcpy(bag_items + 2 * 0x5a8 + 0x44, &type, 4);
        memcpy(bag_items + 2 * 0x5a8 + 0x88, &id, 4);
        it_reset(DD_ROLE_GUEST);
        memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 8, &begin, 8);
        memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 0x10, &end, 8);
        memcpy(wk_display + DD_WALK_OVERLAY + 0x6a0, &none, 4);
        memset(wk_display + DD_WALK_OVERLAY + 0x6a0, 0, 8);
        wk_push(1, &curio);
        dd_runtime_poll();
        CHECK(!it_curios && g_incoming.count == 1); /* Its curio is not open yet. */
        memcpy(wk_display + DD_WALK_OVERLAY + 0x6a0, &prop, 8);
        memcpy(wk_display + DD_WALK_OVERLAY + 0xd88, &mode, 4);
        dd_runtime_poll();
        CHECK(it_curios == 1 && it_curio_panel == (void *)(wk_display + DD_WALK_OVERLAY) && it_curio_item == (void *)(bag_items + 2 * 0x5a8) && g_town.applied_seq == 1);
        /* Not in its bag: nothing runs, the queue goes on. */
        memset(bag_items, 0, sizeof bag_items);
        it_reset(DD_ROLE_GUEST);
        memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 8, &begin, 8);
        memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 0x10, &end, 8);
        memcpy(wk_display + DD_WALK_OVERLAY + 0x6a0, &prop, 8);
        memcpy(wk_display + DD_WALK_OVERLAY + 0xd88, &mode, 4);
        wk_push(1, &curio);
        dd_runtime_poll();
        CHECK(!it_curios && g_town.applied_seq == 1 && !g_town.needs_resync);
        memset(wk_display + DD_WALK_OVERLAY + 0x6a0, 0, 8);
        memset(wk_display + DD_WALK_OVERLAY + 0xd88, 0, 4);
        g_curio_item = NULL;
        g_item_curio_caller = item_curio_caller;
    }
    selection_and_popup_checks();
    results_and_bag_checks();
    second_copy_checks();
    retreat_and_camp_checks();
    g_item_use = NULL;
    g_torch = NULL;
    g_loot_take_all = g_loot_close = NULL;
    g_loot_take_item = NULL;
    g_item_controller = item_controller;
    g_item_torch_caller = item_torch_caller;
    puts("Items, torch and loot: a click is one order on both copies, in a fight at the same point and only for your own hero; the first loot click counts");
}
static void restored_owner_checks(void) {
    dd_action choice;
    bt_reset(DD_ROLE_HOST);
    battle_reset(); /* A saved raid has no departure parameters or ownership table. */
    g_hero_owner_open = bt_owner_open;
    bt_owner_text = "version=1\nwritten_by=host\nrevision=8\nhero.11=1\nhero.12=1\n";
    bt_at(2, 1, 0x20);
    hero_owners_poll(&wk_view);
    CHECK(!sent && !g_hero_owner_ids[0]); /* Finish the running attack first. */
    bt_at(2, 1, DD_BATTLE_CHOOSE_SKILL_STATE);
    hero_owners_poll(&wk_view);
    CHECK(sent == 1 && g_hero_owner_mask == 3 && g_hero_owner_revision == 8);
    CHECK(!hero_is_mine(bt_party[0]) && hero_is_mine(bt_party[2]));
    dd_msg table = last_message;
    CHECK(table.type == DD_MSG_TOWN_APPLY && table.u.town.action.kind == DD_ACT_OWNERS);
    g_hero_owner_poll_at = 0;
    hero_owners_poll(&wk_view);
    CHECK(sent == 1); /* No repeat without a change. */

    bt_reset(DD_ROLE_GUEST);
    battle_reset();
    bt_at(2, 0, DD_BATTLE_CHOOSE_SKILL_STATE);
    CHECK(!battle_order(DD_BATTLE_OP_SKILL, bt_party[0], &choice));
    party_marks_show(&wk_view);
    CHECK(overlay_party_count == 4 && !overlay_party_owner[0]); /* Unknown until the host's table. */
    CHECK(queue_push(&g_incoming, &table));
    dd_runtime_campaign_active(0); /* Native load and slot-watch thread are not simultaneous. */
    dd_runtime_poll();
    CHECK(g_incoming.count == 1 && !g_town.applied_seq && !g_hero_owner_ids[0]);
    g_campaign_active = 1;
    wk_view_fail = 1;
    dd_runtime_poll();
    CHECK(g_incoming.count == 1 && !g_town.needs_resync); /* Host finished loading first. */
    wk_view_fail = 0;
    /* Local formation may be in another order: ownership is a persistent ID, not a rank. */
    uint64_t saved = bt_party[0];
    bt_party[0] = bt_party[3];
    bt_party[3] = saved;
    bt_owner_text = "version=1\nwritten_by=guest\nhero.14=1\n"; /* Never read for authority. */
    dd_runtime_poll();
    CHECK(!g_incoming.count && !g_town.needs_resync && g_town.applied_seq == 1);
    CHECK(g_hero_owner_mask == 3 && hero_is_mine((uintptr_t)bt_heroes[0]) && !hero_is_mine((uintptr_t)bt_heroes[3]));
    CHECK(battle_order(DD_BATTLE_OP_SKILL, (uintptr_t)bt_heroes[0], &choice) == 1 && choice.u.battle.hero == 3);
    party_marks_show(&wk_view);
    CHECK(overlay_party_owner[0] == DD_OVERLAY_TURN_OTHER && overlay_party_owner[3] == DD_OVERLAY_TURN_MINE);
    /* A new active actor affects the large strip, never the four ownership blocks. */
    bt_turns[0] = (uintptr_t)bt_heroes[3];
    battle_turn_show(&wk_view);
    party_marks_show(&wk_view);
    CHECK(overlay_turn_state == DD_OVERLAY_TURN_OTHER && overlay_party_owner[3] == DD_OVERLAY_TURN_MINE);
    bt_turns[0] = (uintptr_t)bt_heroes[0];
    battle_turn_show(&wk_view);
    CHECK(overlay_turn_state == DD_OVERLAY_TURN_MINE);

    /* Both front ranks belong to this guest, on the right of the four blocks. */
    saved = bt_party[0]; bt_party[0] = bt_party[3]; bt_party[3] = saved;
    party_marks_show(&wk_view);
    CHECK(overlay_party_owner[0] == DD_OVERLAY_TURN_MINE && overlay_party_owner[1] == DD_OVERLAY_TURN_MINE &&
          overlay_party_owner[2] == DD_OVERLAY_TURN_OTHER && overlay_party_owner[3] == DD_OVERLAY_TURN_OTHER);
    g_cfg.role = DD_ROLE_HOST;
    party_marks_show(&wk_view);
    CHECK(overlay_party_owner[0] == DD_OVERLAY_TURN_OTHER && overlay_party_owner[1] == DD_OVERLAY_TURN_OTHER &&
          overlay_party_owner[2] == DD_OVERLAY_TURN_MINE && overlay_party_owner[3] == DD_OVERLAY_TURN_MINE);
    g_cfg.role = DD_ROLE_GUEST;
    saved = bt_party[0]; bt_party[0] = bt_party[3]; bt_party[3] = saved;

    /* A guest's restored hero can actually choose: request -> host -> both native picks. */
    g_battle_in_button = 1;
    CHECK(!hook_battle_pick(bt_heroes[0], 1, 1) && !bt_picks && sent == 1);
    dd_msg request = last_message;
    CHECK(request.type == DD_MSG_TOWN_REQUEST && request.u.town.action.u.battle.hero == 3);
    bt_reset(DD_ROLE_HOST);
    hero_owners_set(table.u.town.action.u.owners.heroes, 3);
    g_hero_owner_revision = 8;
    g_town.applied_seq = 1;
    g_town.next_apply_seq = 2;
    saved = bt_party[0]; bt_party[0] = bt_party[3]; bt_party[3] = saved;
    bt_at(2, 0, DD_BATTLE_CHOOSE_SKILL_STATE);
    CHECK(!battle_order(DD_BATTLE_OP_SKILL, (uintptr_t)bt_heroes[0], &choice));
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(bt_picks == 1 && g_town.applied_seq == 2 && !g_town.needs_resync && sent == 1);
    dd_msg accepted = last_message;
    bt_reset(DD_ROLE_GUEST);
    hero_owners_set(table.u.town.action.u.owners.heroes, 3);
    g_town.applied_seq = 1;
    saved = bt_party[0]; bt_party[0] = bt_party[3]; bt_party[3] = saved;
    bt_at(2, 0, DD_BATTLE_CHOOSE_SKILL_STATE);
    CHECK(queue_push(&g_incoming, &accepted));
    dd_runtime_poll();
    CHECK(bt_picks == 1 && g_town.applied_seq == 2 && !g_town.needs_resync);

    /* A different campaign's IDs must never bootstrap authority. */
    bt_reset(DD_ROLE_GUEST);
    battle_reset();
    table.u.town.action.u.owners.heroes[0] = 99;
    CHECK(queue_push(&g_incoming, &table));
    dd_runtime_poll();
    CHECK(g_town.needs_resync && !g_hero_owner_ids[0]);
    /* Missing files use the usual host defaults, but still send the initial table. */
    bt_reset(DD_ROLE_HOST);
    battle_reset();
    g_hero_owner_open = bt_owner_none;
    hero_owners_poll(&wk_view);
    CHECK(sent == 1 && g_hero_owner_ids[0] == 11 && !g_hero_owner_mask);
    /* A living subset, empty ranks, duplicate identities and bad reads. */
    {
        uint32_t ids[4], duplicate = 12;
        bt_party[0] = 0;
        CHECK(hero_owners_restored_ids(&wk_view, ids) && ids[0] == 12 && ids[2] == 14 && !ids[3]);
        memcpy(bt_heroes[2] + 0x137c, &duplicate, 4);
        CHECK(!hero_owners_restored_ids(&wk_view, ids));
    }
    g_hero_owner_open = hero_owner_open;
    puts("Restored owners: host bootstraps from live persistent IDs; guest waits for loading, rejects other parties and can choose through the host; formation blocks follow heroes, not turns");
}
static void live_owner_checks(void) {
    bt_reset(DD_ROLE_HOST);
    g_hero_owner_open = bt_owner_open;
    bt_owner_text = "version=1\nwritten_by=host\nrevision=8\nhero.13=1\n";
    /* A running attack completes first; the change reaches the very next
     * choice, without starting another expedition or even another fight. */
    bt_at(2, 1, 0x20);
    hero_owners_poll(&wk_view);
    CHECK(!sent && hero_is_mine(bt_party[2]));
    bt_at(2, 1, DD_BATTLE_CHOOSE_SKILL_STATE);
    hero_owners_poll(&wk_view);
    CHECK(sent == 1 && g_hero_owner_mask == 4 && !hero_is_mine(bt_party[2]));
    dd_msg apply = last_message;
    CHECK(apply.u.town.action.kind == DD_ACT_OWNERS && apply.u.town.action.u.owners.revision == 8);
    g_hero_owner_poll_at = 0;
    bt_owner_text = "version=1\nwritten_by=host\nhero.13=7\n";
    hero_owners_poll(&wk_view);
    CHECK(sent == 1 && g_hero_owner_mask == 4);
    g_hero_owner_poll_at = 0;
    bt_owner_text = "version=1\nwritten_by=guest\nhero.13=0\n";
    hero_owners_poll(&wk_view);
    CHECK(sent == 1 && g_hero_owner_mask == 4);
    bt_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(g_town.applied_seq == 1 && g_hero_owner_revision == 8 && g_hero_owner_mask == 4);
    CHECK(hero_is_mine(bt_party[2]) && !hero_is_mine(bt_party[0]));
    bt_reset(DD_ROLE_HOST);
    g_hero_owner_open = bt_owner_none;
    apply.type = DD_MSG_TOWN_REQUEST;
    apply.u.town.request_id = 1;
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && g_hero_owner_mask == 0);
    g_hero_owner_open = hero_owner_open;
    puts("Live owners: host file changes shared at a choice boundary; invalid/guest files retain authority; guest cannot forge an assignment");
}
static void battle_transactions(void) {
    restored_owner_checks();
    live_owner_checks();
    uint64_t functor[3] = {0, 0, 0};
    void *battle = wk_raid_lists + 0x4898;
    /* The addresses of the players' calls stay the ones read from the game. */
    CHECK(battle_player_caller(0x70787c) && battle_player_caller(0x707b06) && battle_player_caller(0x707b98) && battle_player_caller(0x71f41d) && battle_player_caller(0x780083));
    CHECK(!battle_player_caller(0x5b9f16) && !battle_player_caller(0x769055) && !battle_player_caller(0x76955c));
    /* The game's own picks and target calls pass on both copies, without an order. */
    bt_reset(DD_ROLE_GUEST);
    CHECK(hook_battle_pick((void *)(uintptr_t)bt_party[2], 1, 1) == 5 && bt_picks == 1 && !sent);
    g_battle_player_caller = bt_caller_no;
    CHECK(hook_battle_enemy(battle, bt_skill, (void *)(uintptr_t)bt_party[2], (void *)(uintptr_t)bt_foes[1]) == 6 && bt_enemies == 1 && !sent);
    CHECK(hook_battle_friend(battle, bt_skill, (void *)(uintptr_t)bt_party[2], (void *)(uintptr_t)bt_party[0]) == 7 && bt_friends == 1 && !sent);
    CHECK(hook_battle_move(battle, (void *)(uintptr_t)bt_party[2], 1, 0, 0) == 8 && bt_moves == 1 && !sent);
    /* Host: its skill button runs natively and shares the slot. */
    bt_reset(DD_ROLE_HOST);
    hook_battle_skill_button(functor);
    CHECK(bt_buttons == 1 && bt_picks == 1 && bt_pick_slot == 3 && sent == 1 && last_message.type == DD_MSG_TOWN_APPLY);
    dd_action skill_order = last_message.u.town.action;
    CHECK(skill_order.kind == DD_ACT_BATTLE && skill_order.u.battle.op == DD_BATTLE_OP_SKILL && skill_order.u.battle.round == 2 && skill_order.u.battle.turn == 1 &&
          skill_order.u.battle.state == DD_BATTLE_CHOOSE_TARGET_STATE && skill_order.u.battle.hero == 2 && skill_order.u.battle.value == 3 && skill_order.u.battle.flag == 1);
    CHECK(!g_battle_in_button);
    /* Guest: the same button only asks. */
    bt_reset(DD_ROLE_GUEST);
    hook_battle_skill_button(functor);
    CHECK(bt_buttons == 1 && !bt_picks && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST && last_message.u.town.action.u.battle.value == 3);
    /* In a corridor a skill button is only looking: no order. */
    bt_reset(DD_ROLE_GUEST);
    wk_view.battle = 0;
    hook_battle_skill_button(functor);
    CHECK(bt_picks == 1 && !sent);
    /* A pick while it is not this hero's turn or not the moment to choose is dropped on both, so neither copy draws alone. */
    bt_reset(DD_ROLE_HOST);
    bt_at(2, 1, 0x1f);
    hook_battle_skill_button(functor);
    CHECK(!bt_picks && !sent);
    bt_reset(DD_ROLE_HOST);
    bt_at(2, 0, DD_BATTLE_CHOOSE_TARGET_STATE);
    hook_battle_skill_button(functor);
    CHECK(!bt_picks && !sent);
    /* The order runs on the guest when it is at that point: the slot and flag as ordered, targets of an earlier choice gone. */
    bt_reset(DD_ROLE_GUEST);
    {
        uint64_t begin = 0x1000, end = 0x1008, now = 0;
        memcpy(bt_heroes[2] + 0x8f0, &begin, 8);
        memcpy(bt_heroes[2] + 0x8f8, &end, 8);
        wk_push(1, &skill_order);
        dd_runtime_poll();
        memcpy(&now, bt_heroes[2] + 0x8f8, 8);
        CHECK(bt_picks == 1 && bt_pick_slot == 3 && bt_pick_flag == 1 && bt_last_hero == (void *)(uintptr_t)bt_party[2] && g_town.applied_seq == 1 && now == begin);
    }
    /* Not there yet: it waits, and the queue behind it too. Each earlier point is tried. */
    {
        static const uint32_t early[][3] = {{1, 1, DD_BATTLE_CHOOSE_TARGET_STATE}, {2, 0, DD_BATTLE_CHOOSE_TARGET_STATE}, {2, 1, 0x1c}, {2, 1, DD_BATTLE_CHOOSE_SKILL_STATE}};
        for (unsigned i = 0; i < 4; i++) {
            bt_reset(DD_ROLE_GUEST);
            bt_at(early[i][0], early[i][1], early[i][2]);
            wk_push(1, &skill_order);
            dd_runtime_poll();
            CHECK(!bt_picks && !g_town.applied_seq && g_incoming.count == 1);
            bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
            dd_runtime_poll();
            CHECK(bt_picks == 1 && g_town.applied_seq == 1 && !g_incoming.count);
        }
    }
    /* No fight here yet: it waits too. */
    bt_reset(DD_ROLE_GUEST);
    wk_view.battle = 0;
    wk_push(1, &skill_order);
    dd_runtime_poll();
    CHECK(!bt_picks && g_incoming.count == 1);
    /* Past that point: the guest says so, keeps the queue going and does not run it elsewhere. */
    bt_reset(DD_ROLE_GUEST);
    bt_at(2, 2, DD_BATTLE_CHOOSE_TARGET_STATE);
    wk_push(1, &skill_order);
    dd_runtime_poll();
    CHECK(!bt_picks && !g_town.applied_seq && g_town.needs_resync && !g_incoming.count && g_sync_stop_sent);
    /* After 30 s of waiting it gives up the same way. */
    bt_reset(DD_ROLE_GUEST);
    wk_view.battle = 0;
    wk_push(1, &skill_order);
    dd_runtime_poll();
    CHECK(g_incoming.count == 1 && g_battle_wait_until);
    g_battle_wait_until = 1;
    dd_runtime_poll();
    CHECK(!bt_picks && !g_town.applied_seq && !g_incoming.count && !g_battle_wait_until && g_town.needs_resync);
    /* Target on the enemy, host: native with the click's arguments; the order names the place. */
    bt_reset(DD_ROLE_HOST);
    CHECK(hook_battle_enemy(battle, bt_skill, (void *)(uintptr_t)bt_party[2], (void *)(uintptr_t)bt_foes[3]) == 6 && bt_enemies == 1 && sent == 1);
    dd_action enemy_order = last_message.u.town.action;
    CHECK(enemy_order.u.battle.op == DD_BATTLE_OP_ENEMY && enemy_order.u.battle.side == 1 && enemy_order.u.battle.place == 3 && enemy_order.u.battle.hero == 2);
    /* Guest: asks; then the order runs with its own skill and the same target. */
    bt_reset(DD_ROLE_GUEST);
    CHECK(!hook_battle_enemy(battle, bt_skill, (void *)(uintptr_t)bt_party[2], (void *)(uintptr_t)bt_foes[3]) && !bt_enemies && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST);
    bt_reset(DD_ROLE_GUEST);
    wk_push(1, &enemy_order);
    dd_runtime_poll();
    CHECK(bt_enemies == 1 && !bt_friends && bt_last_target == (void *)(uintptr_t)bt_foes[3] && bt_last_skill == (void *)bt_skill && bt_last_hero == (void *)(uintptr_t)bt_party[2] && bt_last_battle == battle);
    /* Target in the party. */
    bt_reset(DD_ROLE_HOST);
    CHECK(hook_battle_friend(battle, bt_skill, (void *)(uintptr_t)bt_party[2], (void *)(uintptr_t)bt_party[0]) == 7 && sent == 1);
    dd_action friend_order = last_message.u.town.action;
    CHECK(friend_order.u.battle.op == DD_BATTLE_OP_FRIEND && !friend_order.u.battle.side && !friend_order.u.battle.place);
    bt_reset(DD_ROLE_GUEST);
    wk_push(1, &friend_order);
    dd_runtime_poll();
    CHECK(bt_friends == 1 && !bt_enemies && bt_last_target == (void *)(uintptr_t)bt_party[0]);
    /* An unknown target is not guessed. */
    bt_reset(DD_ROLE_HOST);
    CHECK(!hook_battle_enemy(battle, bt_skill, (void *)(uintptr_t)bt_party[2], wk_props[0]) && !bt_enemies && !sent);
    /* No skill picked on this copy: nothing runs, nothing breaks. */
    bt_reset(DD_ROLE_GUEST);
    memset(bt_heroes[2] + 0x8e0, 0, 8);
    wk_push(1, &enemy_order);
    dd_runtime_poll();
    CHECK(!bt_enemies && !g_town.applied_seq && g_town.needs_resync);
    /* Move and Pass. */
    bt_reset(DD_ROLE_HOST);
    CHECK(hook_battle_move(battle, (void *)(uintptr_t)bt_party[2], 1, 0, 0) == 8 && bt_moves == 1 && bt_move_rank == 1 && sent == 1);
    dd_action move_order = last_message.u.town.action;
    CHECK(move_order.u.battle.op == DD_BATTLE_OP_MOVE && move_order.u.battle.value == 1);
    bt_reset(DD_ROLE_GUEST);
    CHECK(!hook_battle_move(battle, (void *)(uintptr_t)bt_party[2], 0, 0, 0) && !bt_moves && sent == 1);
    bt_reset(DD_ROLE_GUEST);
    wk_push(1, &move_order);
    dd_runtime_poll();
    CHECK(bt_moves == 1 && bt_move_rank == 1 && bt_last_hero == (void *)(uintptr_t)bt_party[2] && bt_last_battle == battle);
    /* Each player only for their own heroes. The third hero is at turn 1. */
    bt_reset(DD_ROLE_GUEST);
    g_hero_owner_mask = 11; /* Not the third. */
    hook_battle_skill_button(functor);
    CHECK(!bt_picks && !sent);
    CHECK(!hook_battle_enemy(battle, bt_skill, (void *)(uintptr_t)bt_party[2], (void *)(uintptr_t)bt_foes[1]) && !bt_enemies && !sent);
    CHECK(!hook_battle_move(battle, (void *)(uintptr_t)bt_party[2], 1, 0, 0) && !bt_moves && !sent);
    bt_reset(DD_ROLE_HOST);
    g_hero_owner_mask = 4; /* The third is the guest's. */
    hook_battle_skill_button(functor);
    CHECK(!bt_picks && !sent);
    CHECK(!hook_battle_friend(battle, bt_skill, (void *)(uintptr_t)bt_party[2], (void *)(uintptr_t)bt_party[0]) && !bt_friends && !sent);
    /* And the host checks: a guest asking for the host's hero gets a no; for its own, a yes. */
    {
        dd_msg m;
        memset(&m, 0, sizeof m);
        m.type = DD_MSG_TOWN_REQUEST;
        m.u.town.request_id = 9;
        m.u.town.action = enemy_order;
        bt_reset(DD_ROLE_HOST);
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        CHECK(!bt_enemies && sent == 1 && last_message.type == DD_MSG_TOWN_REJECT);
        bt_reset(DD_ROLE_HOST);
        g_hero_owner_mask = 4;
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        CHECK(bt_enemies == 1 && sent == 1 && last_message.type == DD_MSG_TOWN_APPLY);
    }
    /* An order from the host runs on the guest whoever owns the hero. */
    bt_reset(DD_ROLE_GUEST);
    g_hero_owner_mask = 0;
    wk_push(1, &enemy_order);
    dd_runtime_poll();
    CHECK(bt_enemies == 1);
    /* A monster's turn has no choices. */
    bt_reset(DD_ROLE_HOST);
    bt_at(2, 2, DD_BATTLE_CHOOSE_TARGET_STATE);
    CHECK(!hook_battle_move(battle, (void *)(uintptr_t)bt_party[2], 0, 0, 0) && !bt_moves && !sent);
    /* The host and a guest's choice: it waits for the host to get there, then runs and goes out to both. */
    {
        dd_msg m;
        bt_reset(DD_ROLE_HOST);
        g_hero_owner_mask = 15;
        bt_at(2, 1, 0x1c);
        memset(&m, 0, sizeof m);
        m.type = DD_MSG_TOWN_REQUEST;
        m.u.town.request_id = 9;
        m.u.town.action = enemy_order;
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        CHECK(!bt_enemies && !sent && g_incoming.count == 1);
        bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
        dd_runtime_poll();
        CHECK(bt_enemies == 1 && sent == 1 && last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.action.u.battle.place == 3);
        /* Too late, the host has gone on: it says no, and nothing runs. */
        bt_reset(DD_ROLE_HOST);
        g_hero_owner_mask = 15;
        bt_at(2, 2, DD_BATTLE_CHOOSE_TARGET_STATE);
        CHECK(queue_push(&g_incoming, &m));
        dd_runtime_poll();
        CHECK(!bt_enemies && sent == 1 && last_message.type == DD_MSG_TOWN_REJECT && !g_town.needs_resync);
    }
    bt_unwire();
    owner_file_checks();
    item_transactions();
    puts("Fights: a skill, target, move or pass is one order, run at the same round, turn and state on both copies; the game's own calls pass; the guest asks, the host decides");
}
