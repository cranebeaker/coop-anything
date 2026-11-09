/* Uses the real supply hooks, FIFO, gates and authority. Only the game's model and native calls are faked. */
#define SW_FOOD 0x72f14069u
#define SW_TORCH 0x2b1c0a11u
#define SW_WOOD 0x5d3e7788u
#define SW_OTHER 0x1234abcdu
static dd_provisions_snapshot sw;
static uint8_t sw_campaign[8], sw_panel[8], sw_stock[8], sw_bag[32], sw_items[16 * 0x5a8];
static int sw_compact, sw_shards, sw_decline, sw_partial, sw_open_fail;
static int32_t sw_paid;
static unsigned sw_buys, sw_returns, sw_swaps, sw_copies, sw_destroys, sw_cancels, sw_backs, sw_enters, sw_fills, sw_stock_fills;
static int sw_no_quest;
static uint8_t sw_quest_object[8];
static void sw_counts(void) {
    sw.stock_occupied = sw.bag_occupied = 0;
    for (unsigned i = 0; i < sw.stock_slots; i++) sw.stock_occupied += sw.stock[i].quantity != 0;
    for (unsigned i = 0; i < sw.bag_slots; i++) sw.bag_occupied += sw.bag[i].quantity != 0;
}
static void sw_fill(void) {
    memset(sw.stock, 0, sizeof sw.stock);
    memset(sw.bag, 0, sizeof sw.bag);
    sw.stock[0] = (dd_provisions_item){SW_FOOD, 0, 8};
    sw.stock[1] = (dd_provisions_item){SW_TORCH, 1, 4};
    sw.bag[0] = (dd_provisions_item){SW_WOOD, 2, 1}; /* free, refunded as nothing */
    sw.panel_visible = sw.stock_view = sw.bag_view = 1;
    sw_counts();
}
static void sw_show(int open) { sw.panel_visible = sw.stock_view = sw.bag_view = (uint8_t)open; }
static int32_t *sw_gold(void) { return &sw.wallet[0].quantity; }
static int sw_open(supply_model *m, dd_provisions_snapshot *s) {
    if (sw_open_fail) return 0;
    m->campaign = (uintptr_t)sw_campaign;
    m->panel = (uintptr_t)sw_panel;
    m->stock = (uintptr_t)sw_stock;
    m->bag = (uintptr_t)sw_bag;
    *s = sw;
    return 1;
}
static int sw_definition(uint32_t type, uint32_t id, uint64_t *out) {
    (void)id;
    *out = type;
    return type != SW_OTHER;
}
static int sw_compact_mode(void) { return sw_compact; }
static int __fastcall sw_price_gold(void *c, void *d) { (void)c; (void)d; return 75; }
static int __fastcall sw_price_shards(void *c, void *d) { (void)c; (void)d; return sw_shards; }
static int __fastcall sw_total_gold(void *c, void *d) { (void)c; (void)d; return sw_paid; }
static int __fastcall sw_total_shards(void *c, void *d) { (void)c; (void)d; return 0; }
static void __fastcall sw_buy(void *capture, int raw) {
    uint64_t *p = capture;
    CHECK(p[0] == (uintptr_t)sw_panel && p[1] == (uintptr_t)sw_stock && p[2] == (uintptr_t)sw_bag);
    sw_buys++;
    if (sw_decline) return;
    int at = raw;
    if (sw_compact) { /* Native 6960c0: a compact view counts occupied slots only. */
        int seen = 0;
        at = -1;
        for (unsigned i = 0; i < sw.stock_slots; i++)
            if (sw.stock[i].quantity && seen++ == raw) { at = (int)i; break; }
    }
    CHECK(at >= 0 && sw.stock[at].quantity > 0);
    if (at < 0) return;
    *sw_gold() -= 75;
    sw_paid += 75;
    if (sw_partial) return; /* Charged and never delivered. */
    dd_provisions_item got = sw.stock[at];
    got.quantity = 1;
    if (!--sw.stock[at].quantity) sw.stock[at] = (dd_provisions_item){0, 0, 0};
    unsigned to = sw.bag_slots;
    for (unsigned i = sw.bag_slots; i-- > 0;) {
        if (sw.bag[i].quantity && sw.bag[i].type == got.type && sw.bag[i].id == got.id) { to = i; break; }
        if (!sw.bag[i].quantity) to = i;
    }
    if (sw.bag[to].quantity) sw.bag[to].quantity++;
    else sw.bag[to] = got;
    sw_counts();
}
static uintptr_t __fastcall sw_copy(void *to, const void *from) {
    sw_copies++;
    memcpy(to, from, 0x5a8);
    return (uintptr_t)to;
}
static int __fastcall sw_return(void *temp, int count) {
    CHECK(count == 1);
    sw_returns++;
    if (sw_decline) return 0;
    unsigned at = *(uint32_t *)temp; /* sw_reset writes each fake item's own slot number. */
    CHECK(at < sw.bag_slots && sw.bag[at].quantity > 0);
    dd_provisions_item back = sw.bag[at];
    if (!--sw.bag[at].quantity) sw.bag[at] = (dd_provisions_item){0, 0, 0};
    *sw_gold() += 75;
    sw_paid -= 75;
    for (unsigned i = 0; i < sw.stock_slots; i++)
        if (sw.stock[i].quantity && sw.stock[i].type == back.type && sw.stock[i].id == back.id) {
            sw.stock[i].quantity++;
            sw_counts();
            return 1;
        }
    for (unsigned i = 0; i < sw.stock_slots; i++)
        if (!sw.stock[i].quantity) {
            sw.stock[i] = (dd_provisions_item){back.type, back.id, 1};
            break;
        }
    sw_counts();
    return 1;
}
static void __fastcall sw_swap(void *bag, uint32_t slot, void *temp) {
    (void)slot;
    (void)temp;
    CHECK(bag == sw_bag);
    sw_swaps++;
}
static void __fastcall sw_destroy(void *item) {
    (void)item;
    sw_destroys++;
}
static void __fastcall sw_cancel(void) {
    sw_cancels++;
    *sw_gold() += sw_paid;
    sw_paid = 0;
    memset(sw.stock, 0, sizeof sw.stock);
    memset(sw.bag, 0, sizeof sw.bag);
    sw_counts();
}
/* Like native 6e85e0: refund first, then the panel starts leaving. */
static void __fastcall sw_back(void *capture) {
    (void)capture;
    sw_backs++;
    hook_supply_cancel();
}
static void __fastcall sw_enter(void *panel) {
    (void)panel;
    sw_enters++;
    sw_fill();
}
/* The panel's opening, in the model only (57b100, 57b7c0). */
static void __fastcall sw_fill_free(void *bag, void *quest) {
    CHECK(bag == sw_bag && quest == sw_quest_object);
    sw_fills++;
    sw.bag[0] = (dd_provisions_item){SW_WOOD, 2, 1};
    sw_counts();
}
static void __fastcall sw_fill_stock(void *bag, void *quest) {
    CHECK(bag == sw_bag && quest == sw_quest_object);
    sw_stock_fills++;
    sw.stock[0] = (dd_provisions_item){SW_FOOD, 0, 8};
    sw.stock[1] = (dd_provisions_item){SW_TORCH, 1, 4};
    sw_counts();
}
static int sw_quest(uint64_t campaign, uint64_t *quest) {
    CHECK(campaign == (uintptr_t)sw_campaign);
    *quest = (uintptr_t)sw_quest_object;
    return !sw_no_quest;
}
/* This copy never opened the shop. */
static void sw_closed_empty(void) {
    memset(sw.stock, 0, sizeof sw.stock);
    memset(sw.bag, 0, sizeof sw.bag);
    sw_counts();
    sw_show(0);
}
static void sw_reset(int role) {
    reset(role);
    provisions_reset();
    dd_town_init(&g_town, NULL, runtime_apply, NULL, fake_send);
    memset(&sw, 0, sizeof sw);
    sw.stock_slots = 14;
    sw.bag_slots = 16;
    sw.currencies = 2;
    sw.wallet[0] = (dd_provisions_currency){DD_SUPPLIES_GOLD, 1000};
    sw.wallet[1] = (dd_provisions_currency){SW_OTHER, 5};
    sw_fill();
    sw_compact = sw_shards = sw_decline = sw_partial = sw_open_fail = sw_no_quest = 0;
    sw_fills = sw_stock_fills = 0;
    sw_paid = 0;
    sw_buys = sw_returns = sw_swaps = sw_copies = sw_destroys = sw_cancels = sw_backs = sw_enters = 0;
    uint64_t begin = (uintptr_t)sw_items;
    memcpy(sw_bag + 8, &begin, 8);
    for (uint32_t i = 0; i < 16; i++) memcpy(sw_items + i * 0x5a8, &i, 4);
    g_supply_open = sw_open;
    g_supply_definition = sw_definition;
    g_supply_compact = sw_compact_mode;
    g_supply_price[0] = sw_price_gold;
    g_supply_price[1] = sw_price_shards;
    g_supply_price[2] = sw_total_gold;
    g_supply_price[3] = sw_total_shards;
    g_supply_buy = sw_buy;
    g_supply_return = sw_return;
    g_supply_cancel = sw_cancel;
    g_supply_back = sw_back;
    g_supply_fill[0] = sw_fill_free;
    g_supply_fill[1] = sw_fill_stock;
    g_supply_quest = sw_quest;
    g_hamlet_copy = sw_copy;
    g_hamlet_swap = sw_swap;
    g_hamlet_destroy = sw_destroy;
}
static void sw_click_buy(int index) {
    uint64_t capture[3] = {(uintptr_t)sw_panel, (uintptr_t)sw_stock, (uintptr_t)sw_bag};
    hook_supply_buy(capture, index);
}
static void sw_click_return(uint32_t visible) {
    uint64_t capture[2] = {image_base() + 0xebdf38, (uintptr_t)sw_bag};
    hook_supply_return(capture, &visible);
}
static void sw_push_apply(uint32_t seq, uint32_t request, const dd_action *a) {
    dd_msg m = {0};
    m.type = DD_MSG_TOWN_APPLY;
    m.seq = seq;
    m.u.town.request_id = request;
    m.u.town.action = *a;
    CHECK(queue_push(&g_incoming, &m));
}
static dd_action sw_order(unsigned op, unsigned slot, uint32_t gold) {
    dd_action a;
    CHECK(dd_supplies_quote(&sw, op, slot, gold, 0, &a));
    return a;
}
/* A drag in the provisions screen: shop to bag is the shared purchase of one, bag to shop the shared return of one,
 * anything else refused. The native drop never runs. */
static uint8_t sw_ui_from[0xd0], sw_ui_to[0xd0]; /* the widget's flags reach +c0 */
static unsigned sw_native_drops;
static uint64_t __fastcall sw_shop_drop(void *a, void *b, void *c, void *d) { (void)a, (void)b, (void)c, (void)d; sw_native_drops++; return 7; }
static int __fastcall sw_slot_drop(void *a, void *b, void *c) { (void)a, (void)b, (void)c; sw_native_drops++; return 7; }
static void sw_pick(uint8_t *inventory, uint32_t slot) {
    uint64_t p = (uintptr_t)inventory;
    memset(sw_ui_from, 0, sizeof sw_ui_from);
    memcpy(sw_ui_from + 0x28, &p, 8);
    memcpy(sw_ui_from + 0xb8, &slot, 4);
    supply_drag_pickup(sw_ui_from);
}
static void sw_target(uint8_t *inventory) {
    uint64_t p = (uintptr_t)inventory;
    memset(sw_ui_to, 0, sizeof sw_ui_to);
    memcpy(sw_ui_to + 0x18, &p, 8);
}
static void sw_drags(void) {
    int before;
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        sw_reset(role);
        g_bag_shop_drop = sw_shop_drop;
        g_trinket_drop[0] = sw_slot_drop;
        sw_native_drops = 0;
        /* The torch from the shop onto the bag: one purchase, as a click on it. */
        sw_pick(sw_stock, 1);
        CHECK(g_supply_drag.valid);
        sw_target(sw_bag);
        CHECK(!hook_trinket_drop_hero(sw_ui_to, NULL, NULL) && !sw_native_drops && !g_supply_drag.valid);
        CHECK(sent == 1 && last_message.u.town.action.kind == DD_ACT_SUPPLIES && last_message.u.town.action.u.supplies.op == DD_SUPPLY_BUY &&
              last_message.u.town.action.u.supplies.slot == 1 && last_message.u.town.action.u.supplies.type == SW_TORCH);
        CHECK(role == DD_ROLE_HOST ? sw_buys == 1 && last_message.type == DD_MSG_TOWN_APPLY : !sw_buys && last_message.type == DD_MSG_TOWN_REQUEST);
        if (role == DD_ROLE_GUEST) { /* the host's order arrives */
            dd_action order = last_message.u.town.action;
            sw_push_apply(1, last_message.u.town.request_id, &order);
            dd_runtime_poll();
            CHECK(sw_buys == 1 && g_town.applied_seq == 1 && !g_supply_request);
        }
        /* The wood from the bag onto the shop: one return, as a click on it. */
        sw_pick(sw_bag, 0);
        CHECK(!hook_bag_shop_drop(NULL, NULL, NULL, NULL) && !sw_native_drops);
        CHECK(sent == 2 && last_message.u.town.action.u.supplies.op == DD_SUPPLY_RETURN && last_message.u.town.action.u.supplies.slot == 0 &&
              last_message.u.town.action.u.supplies.type == SW_WOOD);
        CHECK(!alerts && !g_town.needs_resync);
    }
    /* Moving inside the bag, the shop onto the shop, an empty slot, or no pickup seen: refused with a message, nothing sent. */
    sw_reset(DD_ROLE_GUEST);
    g_bag_shop_drop = sw_shop_drop;
    g_trinket_drop[0] = sw_slot_drop;
    sw_native_drops = 0;
    before = alerts;
    sw_pick(sw_bag, 0);
    sw_target(sw_bag);
    CHECK(!hook_trinket_drop_hero(sw_ui_to, NULL, NULL) && !sent && alerts == before + 1);
    sw_pick(sw_stock, 0);
    CHECK(!hook_bag_shop_drop(NULL, NULL, NULL, NULL) && !sent);
    sw_pick(sw_stock, 5); /* an empty shop slot: no quote */
    sw_target(sw_bag);
    CHECK(!hook_trinket_drop_hero(sw_ui_to, NULL, NULL) && !sent);
    sw_pick(sw_stock, 1); /* from the shop onto something that is not the bag */
    sw_target(sw_stock);
    CHECK(!hook_trinket_drop_hero(sw_ui_to, NULL, NULL) && !sent && !sw_native_drops);
    /* Two quick drags on the guest: the second waits behind the first, as a second click does. */
    sw_pick(sw_stock, 1);
    sw_target(sw_bag);
    CHECK(!hook_trinket_drop_hero(sw_ui_to, NULL, NULL) && sent == 1 && g_supply_request);
    sw_pick(sw_stock, 0);
    sw_target(sw_bag);
    CHECK(!hook_trinket_drop_hero(sw_ui_to, NULL, NULL) && sent == 1 && g_supply_click_count == 1);
    g_supply_click_count = 0;
    g_supply_request = 0;
    sent = 0;
    sw_pick(sw_panel, 0); /* not the shop or the bag: not remembered */
    CHECK(!g_supply_drag.valid);
    CHECK(!hook_bag_shop_drop(NULL, NULL, NULL, NULL) && !sent && !sw_native_drops);
    /* Panel closed or no session: the game's own drop, untouched. */
    sw_show(0);
    sw_pick(sw_stock, 1);
    CHECK(!g_supply_drag.valid);
    CHECK(hook_bag_shop_drop(NULL, NULL, NULL, NULL) == 7 && sw_native_drops == 1);
    sw_reset(DD_ROLE_OFF);
    sw_native_drops = 0;
    sw_pick(sw_stock, 1);
    sw_target(sw_bag);
    CHECK(!g_supply_drag.valid && hook_trinket_drop_hero(sw_ui_to, NULL, NULL) == 7 && sw_native_drops == 1);
    g_bag_shop_drop = NULL;
    g_trinket_drop[0] = NULL;
    memset(&g_supply_drag, 0, sizeof g_supply_drag);
    puts("Provisions drags: shop to bag is the shared purchase of one, bag to shop the shared return of one; moving inside the bag is refused; the native drop never runs while the shared panel is open");
}
/* A drop on one of the shop's own slots (694ab0) sold back natively on one copy only (8 Nov 02:23). From the bag it is
 * the shared return of one; anything else, or a screen that cannot be read, is refused. The native drop never runs while
 * shared. The bag's stack return (696a20) is refused while shared. */
static unsigned sw_stack_returns;
static unsigned sw_finishes;
static void __fastcall sw_finish(void *ui, void *data) { (void)ui, (void)data; sw_finishes++; }
static void __fastcall sw_stack_return(void *capture, int index) { (void)capture, (void)index; sw_stack_returns++; }
static void sw_stock_slot_drops(void) {
    int before;
    uint8_t slot_ui[0xc0] = {0};
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        sw_reset(role);
        g_provision_stock_drop = sw_shop_drop;
        sw_native_drops = 0;
        sw_pick(sw_bag, 0);
        CHECK(g_supply_drag.valid);
        CHECK(!hook_provision_stock_drop(slot_ui, NULL, NULL, NULL) && !sw_native_drops && !g_supply_drag.valid);
        CHECK(sent == 1 && last_message.u.town.action.kind == DD_ACT_SUPPLIES && last_message.u.town.action.u.supplies.op == DD_SUPPLY_RETURN &&
              last_message.u.town.action.u.supplies.slot == 0 && last_message.u.town.action.u.supplies.type == SW_WOOD);
        CHECK(role == DD_ROLE_HOST ? sw_returns == 1 && last_message.type == DD_MSG_TOWN_APPLY : !sw_returns && last_message.type == DD_MSG_TOWN_REQUEST);
        CHECK(!alerts && !g_town.needs_resync);
        /* The source's native cancel would set the slot from the drag's copy (the whole stack back, 8 Nov 02:41): only
         * its flags are reset, once. Then the game's own again. */
        g_trinket_finish[1] = sw_finish;
        g_trinket_finish[2] = sw_finish;
        sw_finishes = 0;
        sw_ui_from[0xc] = 0;
        sw_ui_from[0xc0] = 1;
        hook_trinket_cancel_hero(sw_ui_from, NULL);
        CHECK(!sw_finishes && sw_ui_from[0xc] == 1 && !sw_ui_from[0xc0]);
        hook_trinket_cancel_hero(sw_ui_from, NULL);
        CHECK(sw_finishes == 1);
        /* The same for a drag from the shop onto the bag (its finish on the shop's slot). */
        sw_pick(sw_stock, 1);
        sw_target(sw_bag);
        CHECK(!hook_trinket_drop_hero(sw_ui_to, NULL, NULL) && sent == (role == DD_ROLE_HOST ? 2 : 1));
        hook_trinket_finish_inventory(sw_ui_from, NULL);
        CHECK(sw_finishes == 1 && !sw_ui_from[0xc0]);
        /* A refused drag: the game's own cancel puts the item back. */
        sw_pick(sw_bag, 0);
        sw_target(sw_bag);
        CHECK(!hook_trinket_drop_hero(sw_ui_to, NULL, NULL) && sent == (role == DD_ROLE_HOST ? 2 : 1));
        hook_trinket_cancel_hero(sw_ui_from, NULL);
        CHECK(sw_finishes == 2);
        g_trinket_finish[1] = g_trinket_finish[2] = NULL;
    }
    sw_reset(DD_ROLE_GUEST);
    g_provision_stock_drop = sw_shop_drop;
    sw_native_drops = 0;
    before = alerts;
    /* From the shop onto the shop, or no pickup seen: refused with a message. */
    sw_pick(sw_stock, 1);
    CHECK(!hook_provision_stock_drop(slot_ui, NULL, NULL, NULL) && !sent && alerts == before + 1);
    memset(&g_supply_drag, 0, sizeof g_supply_drag);
    CHECK(!hook_provision_stock_drop(slot_ui, NULL, NULL, NULL) && !sent && !sw_native_drops && alerts == before + 1); /* the same message, once */
    /* The panel's flag reads closed (it flickers): still never native while shared. */
    sw_pick(sw_bag, 0);
    sw_show(0);
    CHECK(!hook_provision_stock_drop(slot_ui, NULL, NULL, NULL) && !sent && !sw_native_drops && !g_supply_drag.valid);
    /* Inside an apply, or without a session: the game's own drop. */
    g_game_apply_depth = 1;
    CHECK(hook_provision_stock_drop(slot_ui, NULL, NULL, NULL) == 7 && sw_native_drops == 1);
    g_game_apply_depth = 0;
    /* The stack return: refused with a message while shared, the game's own without a session. */
    sw_show(1);
    g_supply_stack_return = sw_stack_return;
    sw_stack_returns = 0;
    before = alerts;
    hook_supply_stack_return(NULL, 0);
    CHECK(!sw_stack_returns && !sent && alerts == before + 1 && !sw_returns);
    sw_reset(DD_ROLE_OFF);
    g_provision_stock_drop = sw_shop_drop;
    g_supply_stack_return = sw_stack_return;
    sw_native_drops = 0;
    CHECK(hook_provision_stock_drop(slot_ui, NULL, NULL, NULL) == 7 && sw_native_drops == 1);
    hook_supply_stack_return(NULL, 0);
    CHECK(sw_stack_returns == 1);
    g_provision_stock_drop = NULL;
    g_supply_stack_return = NULL;
    memset(&g_supply_drag, 0, sizeof g_supply_drag);
    puts("Provisions: a drop on a shop slot is the shared return of one from the bag, else refused; the stack return is refused while shared");
}
static void supplies_transactions(void) {
    /* Without a session the game runs as normal. */
    sw_reset(DD_ROLE_OFF);
    sw_click_buy(0);
    CHECK(sw_buys == 1 && !sent && *sw_gold() == 925);
    hook_supply_back(NULL);
    CHECK(sw_backs == 1 && sw_cancels == 1 && !sent && *sw_gold() == 1000);

    /* Host buys: applied once, ordered to the guest, bound to item and price. */
    sw_reset(DD_ROLE_HOST);
    sw_click_buy(0);
    CHECK(sw_buys == 1 && *sw_gold() == 925 && sw.bag[1].type == SW_FOOD && sent == 1);
    CHECK(last_message.type == DD_MSG_TOWN_APPLY && last_message.seq == 1);
    CHECK(last_message.u.town.action.kind == DD_ACT_SUPPLIES && last_message.u.town.action.u.supplies.op == DD_SUPPLY_BUY);
    CHECK(last_message.u.town.action.u.supplies.type == SW_FOOD && last_message.u.town.action.u.supplies.gold == 75);
    /* ... also through a compact view with a hole: visible 0 is physical slot 1 here. */
    sw.stock[0] = (dd_provisions_item){0, 0, 0};
    sw_counts();
    sw_compact = 1;
    sw_click_buy(0);
    CHECK(sw_buys == 2 && last_message.u.town.action.u.supplies.slot == 1 && last_message.u.town.action.u.supplies.type == SW_TORCH);
    /* Host returns: one item through a temporary, as native 6978b0 does. Visible 1 is the food in slot 1. */
    sw_click_return(1);
    CHECK(sw_returns == 1 && sw_copies == 1 && sw_swaps == 1 && sw_destroys == 1 && sent == 3);
    CHECK(last_message.u.town.action.u.supplies.op == DD_SUPPLY_RETURN && last_message.u.town.action.u.supplies.slot == 1);
    CHECK(!g_town.needs_resync && !alerts);

    /* Guest buys: nothing changes before the host's order; a second click follows it, on the new state. */
    sw_reset(DD_ROLE_GUEST);
    sw_click_buy(0);
    CHECK(!sw_buys && *sw_gold() == 1000 && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST && g_supply_request);
    dd_action order = last_message.u.town.action;
    uint32_t request = last_message.u.town.request_id;
    sw_click_buy(1);
    CHECK(sent == 1);
    sw_push_apply(1, request, &order);
    dd_runtime_poll();
    CHECK(sw_buys == 1 && *sw_gold() == 925 && !g_supply_request && g_town.applied_seq == 1 && !g_town.needs_resync);
    CHECK(g_supply_click_count == 1 && sent == 1);
    dd_runtime_poll();
    CHECK(sent == 2 && g_supply_request && !g_supply_click_count && last_message.u.town.action.u.supplies.type == SW_TORCH);
    {
        dd_action second = last_message.u.town.action;
        dd_provisions_snapshot now = sw;
        CHECK(dd_supplies_before(&now, &second) && !alerts); /* Bound to the state after the first. */
    }
    /* One at a time: the next click waits for this answer. */
    sw_reset(DD_ROLE_GUEST);
    sw_click_buy(0);
    sw_click_buy(1);
    sw_click_buy(1);
    order = last_message.u.town.action;
    request = last_message.u.town.request_id;
    CHECK(sent == 1 && g_supply_click_count == 2);
    sw_push_apply(1, request, &order);
    dd_runtime_poll();
    dd_runtime_poll();
    dd_runtime_poll();
    CHECK(sent == 2 && g_supply_click_count == 1 && g_supply_request);
    /* A waiting click also lets the host's order go first, and is then quoted on it. */
    sw_reset(DD_ROLE_GUEST);
    sw_click_buy(0);
    {
        dd_msg r = {0};
        r.type = DD_MSG_TOWN_REJECT;
        r.u.reject.request_id = last_message.u.town.request_id;
        CHECK(queue_push(&g_incoming, &r));
    }
    dd_runtime_poll();
    CHECK(g_supply_click_count == 1 && sent == 1);
    order = sw_order(DD_SUPPLY_BUY, 1, 75);
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    CHECK(sw_buys == 1 && sent == 1 && g_supply_click_count == 1);
    dd_runtime_poll();
    CHECK(sent == 2 && !g_supply_click_count && !g_town.needs_resync);
    {
        dd_action again = last_message.u.town.action;
        dd_provisions_snapshot now = sw;
        CHECK(dd_supplies_before(&now, &again));
    }
    /* If that item is gone by its turn, or the money, the click just ends; the game would have refused it too. */
    sw_reset(DD_ROLE_GUEST);
    sw_click_buy(0);
    sw_click_buy(1);
    order = last_message.u.town.action;
    request = last_message.u.town.request_id;
    sw_push_apply(1, request, &order);
    dd_runtime_poll();
    sw.stock[1] = (dd_provisions_item){0, 0, 0};
    sw_counts();
    dd_runtime_poll();
    CHECK(sent == 1 && !g_supply_click_count && !g_supply_request && !alerts && !g_town.needs_resync);

    /* An order for a copy with its panel closed: the shop is filled here too, the order runs at once, and nothing is said. */
    sw_reset(DD_ROLE_GUEST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    sw_closed_empty();
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    CHECK(sw_fills == 1 && sw_stock_fills == 1 && sw_buys == 1 && !g_incoming.count && !alerts && !g_supply_wait && g_supply_bg && !g_town.needs_resync);
    CHECK(*sw_gold() == 925 && sw.bag[1].type == SW_FOOD);
    /* Its panel, opened later, keeps that. */
    hook_supply_fill_free(sw_bag, sw_quest_object);
    hook_supply_fill_stock(sw_bag, sw_quest_object);
    CHECK(sw_fills == 1 && sw_stock_fills == 1);
    /* The next order runs on it. */
    order = sw_order(DD_SUPPLY_BUY, 1, 75);
    sw_push_apply(2, 0, &order);
    dd_runtime_poll();
    CHECK(sw_fills == 1 && sw_buys == 2 && !g_town.needs_resync);
    /* Without that, the panel's opening fills as ever. */
    provisions_reset();
    hook_supply_fill_free(sw_bag, sw_quest_object);
    hook_supply_fill_stock(sw_bag, sw_quest_object);
    CHECK(sw_fills == 2 && sw_stock_fills == 2);
    /* The same for the host, for the guest's request. */
    sw_reset(DD_ROLE_HOST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    sw_closed_empty();
    {
        dd_msg r = {0};
        r.type = DD_MSG_TOWN_REQUEST;
        r.u.town.request_id = 9;
        r.u.town.action = order;
        CHECK(queue_push(&g_incoming, &r));
    }
    dd_runtime_poll();
    CHECK(sw_fills == 1 && sw_buys == 1 && sent == 1 && last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.request_id == 9 && !alerts);
    /* Everything bought: the shop is still there, and is not filled again. */
    memset(sw.stock, 0, sizeof sw.stock);
    sw_counts();
    CHECK(supply_bg_begin() && sw_fills == 1);
    /* Both fills are hooked: without them a later panel would refill. */
    {
        unsigned found = 0;
        for (unsigned i = 0; i < sizeof g_supply_hooks / sizeof *g_supply_hooks; i++)
            found += g_supply_hooks[i].original == (void *)&g_supply_fill[0] || g_supply_hooks[i].original == (void *)&g_supply_fill[1];
        CHECK(found == 2);
    }
    /* A second try with stock in place fills nothing. */
    sw_reset(DD_ROLE_GUEST);
    sw_show(0);
    CHECK(supply_bg_begin() && !sw_fills && g_supply_bg);
    /* The panel's flag flickers while it is open (owner's log, 1.6 s): no wait for that. */
    sw_reset(DD_ROLE_GUEST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    sw.panel_visible = 0;
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    CHECK(sw_buys == 1 && !sw_fills && !g_supply_wait && !alerts && !g_supply_bg && !g_town.needs_resync);
    /* A panel on its way out, already refunded: wait for it, silently, then as above. */
    sw_reset(DD_ROLE_GUEST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    memset(sw.stock, 0, sizeof sw.stock);
    memset(sw.bag, 0, sizeof sw.bag);
    sw_counts();
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    dd_runtime_poll();
    CHECK(!sw_buys && !sw_fills && g_incoming.count == 1 && g_supply_wait && !alerts && !g_town.needs_resync);
    sw_show(0);
    dd_runtime_poll();
    CHECK(sw_buys == 1 && sw_fills == 1 && !g_incoming.count && !g_supply_wait && !g_town.needs_resync);
    /* Only what really stays stuck gets a message, and it goes away. */
    sw_reset(DD_ROLE_GUEST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    sw_closed_empty();
    sw_no_quest = 1;
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    CHECK(!sw_buys && g_supply_wait && !alerts && !g_town.needs_resync);
    g_supply_wait_since -= DD_SUPPLY_STUCK_MS;
    dd_runtime_poll();
    CHECK(alerts >= 1 && g_shop_notice[0]);
    sw_no_quest = 0;
    dd_runtime_poll();
    CHECK(sw_buys == 1 && !g_supply_wait && !g_shop_notice[0] && !g_town.needs_resync);

    /* Host refuses a stale request and keeps playing; a guest with another state must stop. */
    sw_reset(DD_ROLE_HOST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    *sw_gold() = 999;
    {
        dd_msg r = {0};
        r.type = DD_MSG_TOWN_REQUEST;
        r.u.town.request_id = 4;
        r.u.town.action = order;
        CHECK(queue_push(&g_incoming, &r));
    }
    dd_runtime_poll();
    CHECK(!sw_buys && last_message.type == DD_MSG_TOWN_REJECT && !g_town.needs_resync);
    sw_reset(DD_ROLE_GUEST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    *sw_gold() = 999;
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    CHECK(!sw_buys && g_town.needs_resync && alerts >= 1);
    /* A refused request: the host had acted first, so the click is quoted again on what is here now. Only if that keeps failing, a message. */
    sw_reset(DD_ROLE_GUEST);
    sw_click_buy(0);
    for (unsigned attempt = 0; attempt <= DD_SUPPLY_TRIES; attempt++) {
        dd_msg r = {0};
        r.type = DD_MSG_TOWN_REJECT;
        r.u.reject.request_id = last_message.u.town.request_id;
        CHECK(sent == (int)attempt + 1 && g_supply_request && !alerts);
        CHECK(queue_push(&g_incoming, &r));
        dd_runtime_poll();
        CHECK(!g_supply_request && !sw_buys && !g_town.needs_resync);
        if (attempt < DD_SUPPLY_TRIES) {
            CHECK(g_supply_click_count == 1 && !alerts);
            dd_runtime_poll();
        }
    }
    CHECK(!g_supply_request && !g_supply_click_count && alerts == 1 && !sw_buys && sent == (int)DD_SUPPLY_TRIES + 1 && !g_town.needs_resync);
    /* The host's own purchase crossed ours: its order comes first, ours then fits. */
    sw_reset(DD_ROLE_GUEST);
    sw_click_buy(0);
    request = last_message.u.town.request_id;
    order = sw_order(DD_SUPPLY_BUY, 1, 75);
    sw_push_apply(1, 0, &order);
    {
        dd_msg r = {0};
        r.type = DD_MSG_TOWN_REJECT;
        r.u.reject.request_id = request;
        CHECK(queue_push(&g_incoming, &r));
    }
    dd_runtime_poll();
    dd_runtime_poll();
    CHECK(sw_buys == 1 && sent == 2 && g_supply_request && !alerts && last_message.u.town.action.u.supplies.type == SW_FOOD);
    {
        dd_action again = last_message.u.town.action;
        dd_provisions_snapshot now = sw;
        CHECK(dd_supplies_before(&now, &again));
    }

    /* Back with nothing bought closes the shop for both too: it was filled on both. */
    sw_reset(DD_ROLE_HOST);
    hook_supply_back(NULL);
    CHECK(sw_backs == 1 && !sw_cancels && !sent);
    sw_show(0);
    dd_runtime_poll();
    CHECK(sw_cancels == 1 && sent == 1 && last_message.u.town.action.u.supplies.op == DD_SUPPLY_CANCEL && !g_town.needs_resync && !alerts);
    sw_reset(DD_ROLE_GUEST);
    hook_supply_back(NULL);
    sw_show(0);
    dd_runtime_poll();
    CHECK(sw_backs == 1 && !sw_cancels && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST && last_message.u.town.action.u.supplies.op == DD_SUPPLY_CANCEL);
    CHECK(!g_town.needs_resync && !alerts);
    /* Nothing in the shop at all: nothing to share. */
    sw_reset(DD_ROLE_GUEST);
    memset(sw.stock, 0, sizeof sw.stock);
    memset(sw.bag, 0, sizeof sw.bag);
    sw_counts();
    g_supply_bg = g_supply_quest_known = 1;
    hook_supply_back(NULL);
    CHECK(sw_backs == 1 && sw_cancels == 1 && !sent && !g_supply_bg && !g_supply_quest_known);

    /* Host Back after a buy: no refund while the panel leaves, then one ordered refund. */
    sw_reset(DD_ROLE_HOST);
    sw_click_buy(0);
    hook_supply_back(NULL);
    CHECK(sw_backs == 1 && !sw_cancels && sent == 1 && *sw_gold() == 925);
    dd_runtime_poll();
    CHECK(!sw_cancels && sent == 1);
    sw_show(0);
    dd_runtime_poll();
    CHECK(sw_cancels == 1 && sent == 2 && *sw_gold() == 1000 && !sw.stock_occupied && !sw.bag_occupied);
    CHECK(last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.action.u.supplies.op == DD_SUPPLY_CANCEL);
    CHECK(last_message.u.town.action.u.supplies.gold == 75 && !g_town.needs_resync);
    order = last_message.u.town.action;
    /* The guest's Back crossed ours on the wire: same order, no second refund, still answered. */
    {
        dd_msg r = {0};
        r.type = DD_MSG_TOWN_REQUEST;
        r.u.town.request_id = 6;
        r.u.town.action = order;
        CHECK(queue_push(&g_incoming, &r));
    }
    dd_runtime_poll();
    CHECK(sw_cancels == 1 && sent == 3 && *sw_gold() == 1000 && !g_town.needs_resync);
    CHECK(last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.request_id == 6);
    /* After a shared Back the shop can open again, and closes the same way. */
    sw_enter(NULL);
    sw_show(1);
    hook_supply_back(NULL);
    CHECK(sw_cancels == 1 && sent == 3);
    sw_show(0);
    dd_runtime_poll();
    CHECK(sw_cancels == 2 && sent == 4 && !g_town.needs_resync);

    /* Guest Back after a buy: asks the host and refunds on the order. */
    sw_reset(DD_ROLE_GUEST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    CHECK(sw_buys == 1);
    hook_supply_back(NULL);
    sw_show(0);
    dd_runtime_poll();
    CHECK(!sw_cancels && sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST && g_supply_request_op == DD_SUPPLY_CANCEL);
    CHECK(!alerts && !g_town.needs_resync && supply_cancel_pending());
    order = last_message.u.town.action;
    sw_push_apply(2, last_message.u.town.request_id, &order);
    dd_runtime_poll();
    CHECK(sw_cancels == 1 && !g_supply_request && *sw_gold() == 1000 && !g_town.needs_resync && !supply_cancel_pending());
    /* The host answers both crossing Backs with the same order: the second one changes nothing here. */
    sw_push_apply(3, 0, &order);
    dd_runtime_poll();
    CHECK(sw_cancels == 1 && *sw_gold() == 1000 && !g_town.needs_resync && g_town.applied_seq == 3);
    /* Back with a purchase still unanswered: it follows by itself. */
    sw_reset(DD_ROLE_GUEST);
    sw_click_buy(0);
    order = last_message.u.town.action;
    request = last_message.u.town.request_id;
    hook_supply_back(NULL);
    CHECK(!sw_backs && !sw_cancels && !alerts && !g_town.needs_resync && sent == 1 && g_supply_back_again && supply_cancel_pending());
    sw_push_apply(1, request, &order);
    dd_runtime_poll();
    CHECK(sw_buys == 1 && !sw_backs);
    dd_runtime_poll();
    CHECK(sw_backs == 1 && !g_supply_back_again && g_supply_cancel_local && sent == 1);
    sw_show(0);
    dd_runtime_poll();
    CHECK(sent == 2 && last_message.u.town.action.u.supplies.op == DD_SUPPLY_CANCEL && !alerts);
    /* Same for a waiting order from the other player. */
    sw_reset(DD_ROLE_GUEST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    sw_push_apply(1, 0, &order);
    hook_supply_back(NULL);
    CHECK(!sw_backs && !sw_cancels && !g_town.needs_resync && !alerts);
    dd_runtime_poll();
    CHECK(sw_buys == 1 && !sw_backs);
    dd_runtime_poll();
    CHECK(sw_backs == 1);
    /* If the panel has gone by then, there is nothing to press. */
    sw_reset(DD_ROLE_GUEST);
    sw_click_buy(0);
    order = last_message.u.town.action;
    request = last_message.u.town.request_id;
    hook_supply_back(NULL);
    sw_push_apply(1, request, &order);
    dd_runtime_poll();
    sw_show(0);
    dd_runtime_poll();
    CHECK(!sw_backs && !g_supply_back_again && !g_supply_cancel_local);

    /* The other player's Back with this panel open: it closes by itself, once, and then that one order refunds. */
    sw_reset(DD_ROLE_GUEST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    order = sw_order(DD_SUPPLY_CANCEL, 0, 75);
    sw_push_apply(2, 0, &order);
    dd_runtime_poll();
    CHECK(sw_backs == 1 && !sw_cancels && !alerts && g_incoming.count == 1 && !sent);
    dd_runtime_poll();
    CHECK(sw_backs == 1 && !sw_cancels);
    /* Still not gone after a while: press again, as a player would. */
    g_supply_close_at = 0;
    dd_runtime_poll();
    CHECK(sw_backs == 2 && !sw_cancels);
    sw_show(0);
    dd_runtime_poll();
    CHECK(sw_cancels == 1 && !sent && !g_incoming.count && *sw_gold() == 1000 && !g_town.needs_resync && !alerts);
    /* A panel only flickering off with the views still up is waited for, not pressed. */
    sw_reset(DD_ROLE_GUEST);
    order = sw_order(DD_SUPPLY_CANCEL, 0, 0);
    sw.panel_visible = 0;
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    CHECK(!sw_backs && !sw_cancels && g_incoming.count == 1);
    /* The same order for a copy that only has the shop in its model: refunded at once. */
    sw_reset(DD_ROLE_GUEST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    sw_closed_empty();
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    order = sw_order(DD_SUPPLY_CANCEL, 0, 75);
    g_supply_quest_known = 1;
    sw_push_apply(2, 0, &order);
    dd_runtime_poll();
    CHECK(!sw_backs && sw_cancels == 1 && !g_supply_bg && !g_supply_quest_known && *sw_gold() == 1000 && !g_town.needs_resync);
    /* An old Back, for a state that is gone: never closes a panel. The apply answers it. */
    sw_reset(DD_ROLE_HOST);
    order = sw_order(DD_SUPPLY_CANCEL, 0, 0);
    sw_click_buy(0);
    {
        dd_msg r = {0};
        r.type = DD_MSG_TOWN_REQUEST;
        r.u.town.request_id = 5;
        r.u.town.action = order;
        CHECK(queue_push(&g_incoming, &r));
    }
    dd_runtime_poll();
    CHECK(!sw_backs && !sw_cancels && last_message.type == DD_MSG_TOWN_REJECT && !g_town.needs_resync);
    /* Our Back started first and theirs arrives during the hide: no extra request, no second press. */
    sw_reset(DD_ROLE_GUEST);
    order = sw_order(DD_SUPPLY_BUY, 0, 75);
    sw_push_apply(1, 0, &order);
    dd_runtime_poll();
    hook_supply_back(NULL);
    order = sw_order(DD_SUPPLY_CANCEL, 0, 75);
    sw_push_apply(2, 0, &order);
    dd_runtime_poll();
    CHECK(!sw_cancels && !sent && sw_backs == 1);
    sw_show(0);
    dd_runtime_poll();
    CHECK(sw_cancels == 1 && !sent && !g_supply_cancel_local && !g_town.needs_resync);
    /* A second press during the hide does nothing, and stops nothing. */
    sw_reset(DD_ROLE_HOST);
    hook_supply_back(NULL);
    hook_supply_back(NULL);
    CHECK(sw_backs == 1 && !g_town.needs_resync && !alerts);
    /* The other player bought during our hide: the refund is quoted on that. */
    sw_reset(DD_ROLE_HOST);
    hook_supply_back(NULL);
    {
        dd_msg r = {0};
        r.type = DD_MSG_TOWN_REQUEST;
        r.u.town.request_id = 6;
        r.u.town.action = sw_order(DD_SUPPLY_BUY, 0, 75);
        CHECK(queue_push(&g_incoming, &r));
    }
    dd_runtime_poll();
    CHECK(sw_buys == 1 && sent == 1 && !g_town.needs_resync);
    sw_show(0);
    dd_runtime_poll();
    CHECK(sw_cancels == 1 && sent == 2 && last_message.u.town.action.u.supplies.gold == 75 && *sw_gold() == 1000 && !g_town.needs_resync);
    /* Leaving for the raid: the next shop is a new one. */
    sw_reset(DD_ROLE_GUEST);
    g_supply_bg = g_supply_quest_known = g_supply_back_again = 1;
    g_supply_click_count = 1;
    provisions_leave_town();
    CHECK(!g_supply_bg && !g_supply_quest_known && !g_supply_back_again && !g_supply_click_count);

    /* A shard price is refused, never dropped. */
    sw_reset(DD_ROLE_HOST);
    sw_shards = 3;
    sw_click_buy(0);
    CHECK(!sw_buys && !sent);
    /* Unknown item in the catalogue: refused. */
    sw_reset(DD_ROLE_HOST);
    sw.stock[0].type = SW_OTHER;
    sw_click_buy(0);
    CHECK(!sw_buys && !sent);
    /* The game declines without any effect: no order, session continues. */
    sw_reset(DD_ROLE_HOST);
    sw_decline = 1;
    sw_click_buy(0);
    CHECK(sw_buys == 1 && !sent && alerts == 1 && !g_town.needs_resync);
    /* Charged without delivery: no order, and shared actions stop. */
    sw_reset(DD_ROLE_HOST);
    sw_partial = 1;
    sw_click_buy(0);
    CHECK(sw_buys == 1 && !sent && g_town.needs_resync);
    /* A model that cannot be read refuses the click. */
    sw_reset(DD_ROLE_HOST);
    sw_open_fail = 1;
    sw_click_buy(0);
    CHECK(!sw_buys && !sent);
    /* The campaign is gone during our hide. */
    sw_reset(DD_ROLE_HOST);
    sw_click_buy(0);
    hook_supply_back(NULL);
    sw_open_fail = 1;
    dd_runtime_poll();
    CHECK(g_town.needs_resync && !sw_cancels);
    provisions_reset();
    g_town.needs_resync = 0;
    g_supply_quest = supply_quest_read;
    g_supply_fill[0] = g_supply_fill[1] = NULL;
    puts("Supply bridge: buy/return/Back through real hooks, FIFO and authority; the shop without a panel, flicker, clicks in turn, "
         "refusals quoted again, Back closing the other panel, crossings, stale state, shard price, declined and partial results checked");
}
