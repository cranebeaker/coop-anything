/* Normal Overlay_Event::ShowHunger and its two buttons. An event number follows
 * each prompt through the existing ordered stream; the first choice counts. */
typedef void(__fastcall *hunger_show_fn)(void *, void *);
typedef void(__fastcall *hunger_choice_fn)(void *);
static hunger_show_fn g_hunger_show;
static hunger_choice_fn g_hunger_eat, g_hunger_refuse;
static uint64_t g_hunger_overlay, g_hunger_display;
static uint32_t g_hunger_epoch, g_hunger_done, g_hunger_area, g_hunger_type, g_hunger_cost;
static uint32_t g_hunger_request;
static uint8_t g_hunger_done_flag;
static int g_hunger_visible;
static void hunger_reset(void) {
    g_hunger_overlay = g_hunger_display = 0;
    g_hunger_epoch = g_hunger_done = g_hunger_area = g_hunger_type = g_hunger_cost = g_hunger_request = 0;
    g_hunger_done_flag = 0;
    g_hunger_visible = 0;
}
static uint32_t hunger_food_type(void) {
    uint32_t type = 0;
    safe_read(image_base() + 0x2a87b08, &type, 4);
    return type;
}
static uint32_t (*g_hunger_food_type)(void) = hunger_food_type;
/* Same sum as ShowHunger: max(0, 1 + hero.food_consumption), not a hardcoded 4. */
static int hunger_cost(const walk_view *v, uint32_t *bits) {
    uint64_t begin = 0, end = 0;
    float cost = 0;
    if (!safe_read((uintptr_t)v->raid + 8, &begin, 8) || !safe_read((uintptr_t)v->raid + 0x10, &end, 8) ||
        !begin || end <= begin || (end - begin) % 8 || (end - begin) / 8 > 4) return 0;
    for (uint64_t p = begin; p < end; p += 8) {
        uint64_t hero = 0;
        float modifier = 0;
        uint32_t raw = 0;
        if (!safe_read((uintptr_t)p, &hero, 8) || !hero || !safe_read((uintptr_t)hero + 0xbc4, &raw, 4) ||
            (raw & 0x7f800000u) == 0x7f800000u) return 0;
        memcpy(&modifier, &raw, 4);
        modifier += 1.0f;
        if (modifier > 0) cost += modifier;
    }
    if (cost > 64.0f) return 0;
    memcpy(bits, &cost, 4);
    return 1;
}
static int hunger_open(const walk_view *v) {
    uint32_t mode = 0;
    uint64_t prop = 1;
    return !v->battle && g_hunger_visible && g_hunger_display == v->display &&
           g_hunger_overlay == v->display + DD_WALK_OVERLAY && g_hunger_area == v->area &&
           safe_read((uintptr_t)g_hunger_overlay + 0xd88, &mode, 4) && mode == 2 &&
           safe_read((uintptr_t)g_hunger_overlay + 0xd70, &prop, 8) && !prop;
}
static void hunger_poll(const walk_view *v) {
    if (g_hunger_visible && !hunger_open(v)) g_hunger_visible = 0;
}
static int hunger_has_food(const walk_view *v) {
    uint64_t begin = 0, end = 0, total = 0;
    float cost = 0;
    if (!g_hunger_type || !safe_read((uintptr_t)v->raid + DD_WALK_RAID_INVENTORY + 8, &begin, 8) ||
        !safe_read((uintptr_t)v->raid + DD_WALK_RAID_INVENTORY + 0x10, &end, 8) ||
        end < begin || (end - begin) % 0x5a8 || (end - begin) / 0x5a8 > 64) return 0;
    for (uint64_t p = begin; p < end; p += 0x5a8) {
        uint32_t count = 0, type = 0;
        if (!safe_read((uintptr_t)p, &count, 4) || count > INT32_MAX || !safe_read((uintptr_t)p + 0x44, &type, 4)) return 0;
        if (type == g_hunger_type) total += count;
    }
    memcpy(&cost, &g_hunger_cost, 4);
    return total <= INT32_MAX && (float)total >= cost;
}
/* ShowHunger is rendered every frame, not a blocking dialog. Keep its identity
 * after the native call returns so a network order can act between frames. */
static void __fastcall hook_hunger_show(void *overlay, void *display) {
    walk_view v;
    uint32_t mode = 0, cost = 0;
    if (!g_hunger_show) return;
    if (walk_shared() && g_walk_view(&v) && !v.battle && (uintptr_t)display == v.display &&
        (uintptr_t)overlay == v.display + DD_WALK_OVERLAY &&
        safe_read((uintptr_t)overlay + 0xd88, &mode, 4) && (mode == 1 || mode == 2)) {
        if (!g_hunger_visible || g_hunger_overlay != (uintptr_t)overlay || g_hunger_area != v.area) {
            uint32_t type = g_hunger_food_type();
            if (g_hunger_epoch != UINT32_MAX && hunger_cost(&v, &cost) && type) {
                g_hunger_epoch++;
                g_hunger_overlay = (uintptr_t)overlay;
                g_hunger_display = v.display;
                g_hunger_area = v.area;
                g_hunger_type = type;
                g_hunger_cost = cost;
                g_hunger_request = 0;
                g_hunger_visible = 1;
                dd_log("Hunger: prompt %u opened (area=%08x role=%s)", g_hunger_epoch, v.area, dd_role_name(g_cfg.role));
            }
        }
    }
    g_hunger_show(overlay, display);
    if (g_hunger_overlay == (uintptr_t)overlay &&
        (!safe_read((uintptr_t)overlay + 0xd88, &mode, 4) || mode != 2)) g_hunger_visible = 0;
}
static int hunger_apply(const dd_action *a) {
    walk_view v;
    uint32_t cost = 0;
    if (!g_walk_view(&v)) return 0;
    if (a->u.item.round == g_hunger_done) {
        /* A crossed click must not charge the food twice. A guest may only
         * acknowledge an identical already executed host choice. */
        return g_cfg.role == DD_ROLE_GUEST && a->u.item.flag == g_hunger_done_flag &&
               a->u.item.area == g_hunger_area && a->u.item.amount == g_hunger_cost && a->u.item.type == g_hunger_type;
    }
    if (!hunger_open(&v) || a->u.item.area != v.area || a->u.item.round != g_hunger_epoch ||
        a->u.item.type != g_hunger_type || !hunger_cost(&v, &cost) || cost != g_hunger_cost || a->u.item.amount != cost ||
        (a->u.item.flag && (!g_hunger_eat || !hunger_has_food(&v))) || (!a->u.item.flag && !g_hunger_refuse)) {
        dd_log("Hunger: choice does not match this prompt (order=%u here=%u role=%s)", a->u.item.round, g_hunger_epoch, dd_role_name(g_cfg.role));
        return 0;
    }
    g_hunger_done = g_hunger_epoch;
    g_hunger_done_flag = a->u.item.flag;
    g_hunger_visible = 0;
    dd_log("Hunger: prompt %u %s (role=%s)", g_hunger_done, a->u.item.flag ? "eat" : "refuse", dd_role_name(g_cfg.role));
    if (a->u.item.flag) {
        uint8_t args[0x18] = {0}; /* std::_Func object: overlay at +8, food float at +10. */
        memcpy(args + 8, &g_hunger_overlay, 8);
        memcpy(args + 0x10, &g_hunger_cost, 4);
        g_hunger_eat(args);
    } else {
        uint8_t capture[0x10] = {0}; /* Lambda data: overlay +0, food float +8. */
        memcpy(capture, &g_hunger_overlay, 8);
        memcpy(capture + 8, &g_hunger_cost, 4);
        g_hunger_refuse(capture);
    }
    return 1;
}
static void hunger_choice(hunger_choice_fn native, int eat, void *args) {
    walk_view v;
    dd_action a;
    uint64_t overlay = 0;
    uint32_t cost = 0;
    if (!native) return;
    if (g_game_apply_depth || g_cfg.role == DD_ROLE_OFF || !InterlockedCompareExchange(&g_campaign_active, 0, 0)) {
        native(args);
        return;
    }
    if (!walk_shared()) { (void)runtime_shared_blocked(); return; }
    if (!g_walk_view(&v) || !safe_read((uintptr_t)args + (eat ? 8 : 0), &overlay, 8) ||
        overlay != v.display + DD_WALK_OVERLAY ||
        !hunger_open(&v) || g_hunger_request || !item_order(DD_ITEM_OP_HUNGER, &a, &v)) return;
    if (!safe_read((uintptr_t)args + (eat ? 0x10 : 8), &cost, 4) || cost != g_hunger_cost) {
        dd_log("Hunger: %s callback cost differs (capture=%08x prompt=%08x role=%s)", eat ? "eat" : "refuse", cost, g_hunger_cost, dd_role_name(g_cfg.role));
        return;
    }
    a.u.item.round = g_hunger_epoch;
    a.u.item.flag = (uint8_t)eat;
    a.u.item.type = g_hunger_type;
    a.u.item.amount = g_hunger_cost;
    dd_log("Hunger: prompt %u %s chosen (role=%s)", g_hunger_epoch, eat ? "eat" : "refuse", dd_role_name(g_cfg.role));
    if (g_cfg.role == DD_ROLE_GUEST) g_hunger_request = dd_town_guest_local(&g_town, &a);
    else (void)dd_town_host_local(&g_town, &a);
}
static void __fastcall hook_hunger_eat(void *args) { hunger_choice(g_hunger_eat, 1, args); }
static void __fastcall hook_hunger_refuse(void *args) { hunger_choice(g_hunger_refuse, 0, args); }
static void hunger_request_done(uint32_t request) {
    if (request && request == g_hunger_request) g_hunger_request = 0;
}
static int hunger_gate_ready(const walk_view *v, const dd_action *a) {
    return g_hunger_epoch > a->u.item.round || g_hunger_done == a->u.item.round ||
           (g_hunger_epoch == a->u.item.round && hunger_open(v));
}
