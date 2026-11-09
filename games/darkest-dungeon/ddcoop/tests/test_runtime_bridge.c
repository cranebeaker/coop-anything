/* Exercise the real main-thread FIFO with fake readiness/game callbacks, no game process or network. */
#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
static int alerts, failures, ready, applied_count, sent;
static uint8_t kinds[8];
static int WINAPI test_message_box(HWND w, LPCSTR text, LPCSTR caption, UINT flags) {
    (void)w;
    (void)text;
    (void)caption;
    (void)flags;
    alerts++;
    return IDOK;
}
#define MessageBoxA test_message_box
#include "../runtime/dd_runtime.c"
#undef MessageBoxA
/* The panel itself is tested in test_overlay.c. A refusing panel must fall back to the popup. */
static int overlay_refuses;
int dd_overlay_notice(const char *utf8) {
    (void)utf8;
    if (overlay_refuses) return 0;
    alerts++;
    return 1;
}
void dd_overlay_notice_clear(const char *utf8) { (void)utf8; }
static int overlay_state;
void dd_overlay_session(int state) { overlay_state = state; }
static int overlay_turn_state;
void dd_overlay_turn(int turn) { overlay_turn_state = turn; }
static int overlay_party_count, overlay_party_owner[4];
static float overlay_party_stress[4];
void dd_overlay_party(int count, const int owner[4], const float stress[4]) {
    overlay_party_count = count;
    for (int i = 0; i < count && i < 4; i++) {
        overlay_party_owner[i] = owner[i];
        overlay_party_stress[i] = stress[i];
    }
}
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        if (!(c)) {                                                                                          \
            printf("FAIL line %d: %s\n", __LINE__, #c);                                                      \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)
static int can_apply(void) { return ready; }
static dd_msg last_message;
static dd_msg sent_messages[64];
static int fake_apply(void *ctx, const dd_action *a) {
    (void)ctx;
    kinds[applied_count++] = a->kind;
    return 1;
}
static void fake_send(void *ctx, int to, const dd_msg *m) {
    (void)ctx;
    (void)to;
    last_message = *m;
    if (sent < 64) sent_messages[sent] = *m;
    sent++;
}
static void reset(int role) {
    queue_clear(&g_incoming);
    queue_clear(&g_outgoing);
    memset(&g_cfg, 0, sizeof g_cfg);
    g_cfg.role = (dd_role)role;
    strcpy(g_cfg.password, "test-only");
    g_initialized = g_campaign_active = g_session_ready = g_hooks_installed = 1;
    g_session_seen = 1; g_session_lost = g_session_loss_notified = 0;
    g_queue_failed = 0;
    g_peer_sync_stop = g_sync_stop_sent = 0;
    g_recruit_waiting = 0;
    g_shop_notice[0] = 0;
    g_sell_request = 0;
    g_game_apply_depth=0;hamlet_reset();
    g_recruit_panel = 0;
    g_recruit_can_apply = can_apply;
    ready = applied_count = sent = alerts = 0;
    dd_town_init(&g_town, NULL, fake_apply, NULL, fake_send);
}
static void ordered_wait(int role) {
    reset(role);
    dd_msg a = {0}, b = {0};
    a.type = b.type = (uint16_t)(role == DD_ROLE_HOST ? DD_MSG_TOWN_REQUEST : DD_MSG_TOWN_APPLY);
    a.seq = 1;
    a.u.town.request_id = 1;
    a.u.town.action.kind = DD_ACT_RECRUIT;
    a.u.town.action.u.hero.hero_id = 40;
    b.seq = 2;
    b.u.town.request_id = 2;
    b.u.town.action.kind = DD_ACT_BUY_UPGRADE;
    b.u.town.action.u.upgrade.tree = 1;
    b.u.town.action.u.upgrade.code = 'a';
    b.u.town.action.u.upgrade.hero_id = 1;
    CHECK(queue_push(&g_incoming, &a) && queue_push(&g_incoming, &b));
    dd_runtime_poll();
    dd_runtime_poll();
    CHECK(applied_count == 0 && sent == 0 && g_incoming.count == 2 && !g_town.needs_resync && !alerts && g_recruit_waiting);
    ready = 1;
    dd_runtime_poll();
    CHECK(applied_count == 2 && kinds[0] == DD_ACT_RECRUIT && kinds[1] == DD_ACT_BUY_UPGRADE);
    CHECK(!g_incoming.count && g_town.applied_seq == 2 && !g_town.needs_resync);
    dd_runtime_poll();
    CHECK(applied_count == 2); /* no duplicate application */
}
static uint8_t world_campaign[0x2000], world_store[0x100], world_entries[0x15b0], world_panel[0x400],
    world_row[0x200], world_controller[0x100], world_regular[0x1638], world_clone[0x1638];
static uint64_t world_rows[2], world_roster[4];
static unsigned world_count, world_capacity, add_calls, finish_calls, drop_calls;
static int world_change_panel;
static unsigned world_events, world_saves;
static uint64_t world_bus_object;
static int world_no_bus;
static int world_bus(uint64_t *bus) {
    *bus = (uintptr_t)&world_bus_object;
    return !world_no_bus;
}
static uint64_t __fastcall world_clock(void) { return 77; }
static void __fastcall world_event(void *bus, const void *event) {
    const uint64_t *e = event;
    CHECK(bus == &world_bus_object && e[1] == 77 && e[2] == 40 && world_count == 2 && world_entries[0x15a8] == 1 && !world_saves);
    world_events++;
}
static void world_save_check(int index, void *object) {
    int32_t total = 0;
    memcpy(&total, world_campaign + 0x1708, 4);
    CHECK((int)world_saves == index && world_events == 1 && !world_entries[0x15a8] && total == 6);
    CHECK(index == 3 || object == world_campaign + (index == 1 ? 0x1680 : index == 2 ? 0x1138 : 0));
    world_saves++;
}
static void __fastcall world_save0(void *o, int f) { CHECK(!f); world_save_check(0, o); }
static void __fastcall world_save1(void *o, int f) { CHECK(!f); world_save_check(1, o); }
static void __fastcall world_save2(void *o, int f) { CHECK(!f); world_save_check(2, o); }
static void __fastcall world_save3(void *o, int f) { CHECK(!f); world_save_check(3, o); }
static void putptr(void *at, uint64_t value) { memcpy(at, &value, 8); }
static void putint(void *at, uint32_t value) { memcpy(at, &value, 4); }
static int world_open(dd_recruit *s) {
    memset(s, 0, sizeof *s);
    s->read = shop_read;
    s->image = 0x10000000;
    s->campaign = (uintptr_t)world_campaign;
    s->store = (uintptr_t)world_store;
    return 1;
}
static unsigned world_used(void) { return world_count; }
static unsigned world_limit(void) { return world_capacity; }
static uintptr_t __fastcall world_add(void *roster, const void *entry) {
    CHECK(roster == world_campaign && entry == world_entries && world_count == 1);
    add_calls++;
    memcpy(world_clone + 8, entry, 0x15b0);
    putint(world_clone, 999); /* wrapper prefix differs; stable Entry ID is +0x1384 */
    world_roster[world_count++] = (uintptr_t)world_clone;
    putptr(world_campaign + 0x28, (uintptr_t)(world_roster + world_count));
    if (world_change_panel) g_recruit_panel = 0;
    return (uintptr_t)(world_clone + 8);
}
static void __fastcall world_finish(void *capture, void *entry, uint32_t row) {
    CHECK(*(uint64_t *)capture == (uintptr_t)world_panel && entry == world_entries && row == 0 &&
          add_calls == 1);
    finish_calls++;
    world_entries[0x15a8] = 0;
    putptr(world_controller + 0x28, 0);
}
static int __fastcall world_drop(void *target, void *position, void *drag) {
    (void)target;
    (void)position;
    drop_calls++;
    world_add(world_campaign, (void *)(uintptr_t)((uint64_t *)drag)[1]);
    return 1;
}
static void world_reset(int role) {
    reset(role);
    ready = 1;
    world_count = 1;
    world_capacity = 9;
    add_calls = finish_calls = drop_calls = 0;
    world_change_panel = 0;
    memset(world_campaign, 0, sizeof world_campaign);
    memset(world_store, 0, sizeof world_store);
    memset(world_entries, 0, sizeof world_entries);
    memset(world_panel, 0, sizeof world_panel);
    memset(world_row, 0, sizeof world_row);
    memset(world_controller, 0, sizeof world_controller);
    memset(world_regular, 0, sizeof world_regular);
    memset(world_clone, 0, sizeof world_clone);
    world_roster[0] = (uintptr_t)world_regular;
    putint(world_regular + 0x1384, 1);
    putptr(world_campaign + 0x20, (uintptr_t)world_roster);
    putptr(world_campaign + 0x28, (uintptr_t)(world_roster + 1));
    putptr(world_store + 0x68, (uintptr_t)world_entries);
    putptr(world_store + 0x70, (uintptr_t)(world_entries + sizeof world_entries));
    putint(world_entries + 0x137c, 40);
    world_entries[0x15a8] = 1;
    putptr(world_panel, 0x10ec0cb0);
    putptr(world_panel + 0x80, (uintptr_t)world_store);
    world_rows[0] = (uintptr_t)world_row;
    putptr(world_panel + 0x350, (uintptr_t)world_rows);
    putptr(world_panel + 0x358, (uintptr_t)(world_rows + 2));
    putptr(world_row, 0x10ebcc00);
    putint(world_row + 0x58, 1);
    putptr(world_row + 0x158, (uintptr_t)world_controller);
    putptr(world_controller, 0x10ee0990);
    putptr(world_controller + 0x28, (uintptr_t)world_entries);
    g_recruit_panel = (uintptr_t)world_panel;
    g_recruit_ready_at = 0;
    g_recruit_open_model = world_open;
    g_original_recruit = world_finish;
    g_roster_add = world_add;
    g_roster_count = world_used;
    g_roster_capacity = world_limit;
    g_original_roster_drop = world_drop;
    world_events = world_saves = 0;
    world_no_bus = 0;
    putint(world_campaign + 0x1708, 5);
    g_recruit_bus = world_bus;
    g_recruit_clock = world_clock;
    g_recruit_event = world_event;
    g_recruit_save[0] = world_save0;
    g_recruit_save[1] = world_save1;
    g_recruit_save[2] = world_save2;
    g_recruit_save[3] = world_save3;
    dd_town_init(&g_town, NULL, runtime_apply, NULL, fake_send);
}
static void recruit_transactions(void) {
    world_reset(DD_ROLE_GUEST);
    uint64_t drag[] = {0, (uintptr_t)world_entries};
    CHECK(!hook_roster_drop(NULL, NULL, drag));
    CHECK(!add_calls && !finish_calls && !drop_calls && world_count == 1 && world_entries[0x15a8] == 1);
    CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST &&
          last_message.u.town.action.u.hero.hero_id == 40);
    dd_msg request = last_message;
    world_reset(DD_ROLE_HOST);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.accepted == 1 && !g_town.needs_resync && add_calls == 1 && finish_calls == 1 &&
          world_count == 2 && !world_entries[0x15a8]);
    CHECK(last_message.type == DD_MSG_TOWN_APPLY && last_message.seq == 1);
    dd_msg apply = last_message;
    CHECK(!world_events && !world_saves); /* The screen's own callback did that here. */
    request.u.town.request_id = 2;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && !g_incoming.count && add_calls == 1 &&
          finish_calls == 1); /* stale duplicate */
    world_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(g_town.applied_seq == 1 && !g_town.needs_resync && add_calls == 1 && finish_calls == 1 &&
          world_count == 2);
    world_reset(DD_ROLE_HOST);
    world_capacity = 1;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && !add_calls && !finish_calls && world_entries[0x15a8] == 1 &&
          !g_town.needs_resync);
    world_reset(DD_ROLE_HOST);
    CHECK(hook_roster_drop(NULL, NULL, drag));
    CHECK(add_calls == 1 && !finish_calls && sent == 0);
    uint64_t capture = (uintptr_t)world_panel;
    hook_recruit(&capture, world_entries, 0);
    CHECK(g_town.accepted == 1 && add_calls == 1 && finish_calls == 1 &&
          last_message.type == DD_MSG_TOWN_APPLY);
    world_reset(DD_ROLE_GUEST);
    putint(world_entries + 0x144c, 1);
    CHECK(!hook_roster_drop(NULL, NULL, drag));
    CHECK(!sent && !add_calls && world_entries[0x15a8] == 1);
    world_reset(DD_ROLE_HOST);
    world_change_panel = 1;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.needs_resync && alerts == 1 && add_calls == 1 &&
          !finish_calls); /* never use a panel invalidated by a game listener */
    /* Without the screen: at once, without a message, and everything the callback does except its row. */
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        dd_msg order = role == DD_ROLE_HOST ? request : apply;
        int32_t total = 0;
        world_reset(role);
        g_recruit_panel = 0;
        CHECK(queue_push(&g_incoming, &order));
        dd_runtime_poll();
        memcpy(&total, world_campaign + 0x1708, 4);
        CHECK(!g_incoming.count && !g_town.needs_resync && !alerts && add_calls == 1 && !finish_calls && world_count == 2);
        CHECK(!world_entries[0x15a8] && world_events == 1 && world_saves == 4 && total == 6);
        CHECK(role == DD_ROLE_HOST ? last_message.type == DD_MSG_TOWN_APPLY && g_town.accepted == 1 : g_town.applied_seq == 1);
    }
    /* Taken already, roster full, or a game file without those functions: refused, and nothing changes. */
    world_reset(DD_ROLE_HOST);
    g_recruit_panel = 0;
    world_entries[0x15a8] = 0;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && !add_calls && !world_events && !g_town.needs_resync);
    world_reset(DD_ROLE_HOST);
    g_recruit_panel = 0;
    world_capacity = 1;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && !add_calls && !world_events && world_entries[0x15a8] == 1 && !g_town.needs_resync);
    world_reset(DD_ROLE_HOST);
    g_recruit_panel = 0;
    world_no_bus = 1;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && !add_calls && !world_events && world_entries[0x15a8] == 1 && !g_town.needs_resync);
    world_reset(DD_ROLE_HOST);
    g_recruit_panel = 0;
    g_recruit_save[2] = NULL;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && !add_calls && !world_events && !g_town.needs_resync);
    /* With the screen open, the game's own disabled row refuses too. */
    world_reset(DD_ROLE_HOST);
    putint(world_row + 0x58, 0);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_town.rejected == 1 && !add_calls && !finish_calls && !g_town.needs_resync);
    /* A panel that has only just opened gets its moment, silently; the real predicate. */
    world_reset(DD_ROLE_HOST);
    g_recruit_can_apply = recruit_context_settled;
    g_recruit_ready_at = GetTickCount64() + 60000;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(g_incoming.count == 1 && !add_calls && !alerts);
    g_recruit_ready_at = 0;
    dd_runtime_poll();
    CHECK(!g_incoming.count && add_calls == 1 && finish_calls == 1);
    world_reset(DD_ROLE_HOST);
    g_recruit_can_apply = recruit_context_settled;
    g_recruit_panel = 0;
    g_recruit_ready_at = GetTickCount64() + 60000; /* Left from a panel that has closed. */
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(!g_incoming.count && add_calls == 1 && !finish_calls);
    printf("Recruit bridge: guest waits before mutation; host/guest add+complete once, with or without the Stagecoach screen; capacity, duplicate "
           "and observed host paths verified\n");
}

/* Real read adapter and real sale bridge; only the actual game sale/price calls are substituted. */
static uint8_t sale_image[0x3000000], sale_items[3 * 0x5a8], sale_wallet[0x48], sale_definitions[2][0xb0];
static uint64_t sale_library[2], sale_definition_ptrs[2];
static int sale_calls, sale_partial;
static int sale_open(dd_shop *s) {
    memset(s, 0, sizeof *s);
    s->read = shop_read;
    s->image = (uintptr_t)sale_image;
    s->campaign = (uintptr_t)world_campaign;
    s->estate = (uintptr_t)(world_campaign + 0xe88);
    return 1;
}
static int sale_price(int base, uint32_t modifier) {
    CHECK(modifier == 0);
    return base;
}
static void __fastcall fake_sale(const void *capture) {
    const sale_request *request = capture;
    CHECK(request->inventory == (uintptr_t)(world_campaign + 0xe88) && request->slot < 3);
    sale_calls++;
    putint(sale_items + request->slot * 0x5a8, 0);
    if (!sale_partial) {
        uint32_t gold;
        memcpy(&gold, sale_wallet, 4);
        putint(sale_wallet, gold + (uint32_t)request->gold);
    }
}
static void sale_reset(int role) {
    reset(role);
    sale_calls = sale_partial = 0;
    memset(world_campaign, 0, sizeof world_campaign);
    memset(sale_items, 0, sizeof sale_items);
    memset(sale_wallet, 0, sizeof sale_wallet);
    memset(sale_definitions, 0, sizeof sale_definitions);
    putptr(world_campaign + 0xe90, (uintptr_t)sale_items);
    putptr(world_campaign + 0xe98, (uintptr_t)(sale_items + sizeof sale_items));
    putptr(world_campaign + 0xe70, (uintptr_t)sale_wallet);
    putptr(world_campaign + 0xe78, (uintptr_t)(sale_wallet + sizeof sale_wallet));
    putint(sale_wallet, 100);
    putint(sale_wallet + 0x44, 33);
    putint(sale_image + 0x2bb41f0, 33);
    putint(sale_image + 0x2a6f868, 123);
    putint(sale_image + 0x2a7dca4, 0);
    putptr(sale_image + 0x118e0f8, (uintptr_t)sale_library);
    sale_library[0] = (uintptr_t)sale_definition_ptrs;
    sale_library[1] = (uintptr_t)(sale_definition_ptrs + 2);
    for (unsigned i = 0; i < 2; i++) {
        sale_definition_ptrs[i] = (uintptr_t)sale_definitions[i];
        putint(sale_definitions[i] + 0x40, 123);
        putint(sale_definitions[i] + 0x84, 101 + i);
        putint(sale_definitions[i] + 0x9c, 500 + 250 * i);
        putint(sale_definitions[i] + 0xa4, 50 + 25 * i);
        putint(sale_items + i * 0x5a8, 1);
        putint(sale_items + i * 0x5a8 + 0x44, 123);
        putint(sale_items + i * 0x5a8 + 0x88, 101 + i);
    }
    g_sale_open_model = sale_open;
    g_original_sell = fake_sale;
    g_trinket_price = sale_price;
    dd_town_init(&g_town, NULL, runtime_apply, NULL, fake_send);
}
static void sale_transactions(void) {
    sale_reset(DD_ROLE_GUEST);
    sale_request capture = {(uintptr_t)(world_campaign + 0xe88), 50, 0};
    hook_wagon_sell(&capture);
    CHECK(!sale_calls && sale_items[0] == 1 && sale_wallet[0] == 100 && sent == 1 && g_sell_request == 1);
    CHECK(last_message.type == DD_MSG_TOWN_REQUEST && last_message.u.town.action.kind == DD_ACT_WAGON_SELL &&
          last_message.u.town.action.u.items.id == 101 && last_message.u.town.action.u.items.cost == 50);
    dd_msg request = last_message;
    uint8_t wire[512];
    size_t length = dd_encode(&request, wire, sizeof wire), used = 0;
    CHECK(length && dd_decode(wire, length, &request, &used) == DD_DECODE_OK && used == length);
    sale_reset(DD_ROLE_HOST);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(sale_calls == 1 && !sale_items[0] && sale_wallet[0] == 150 && g_town.accepted == 1 &&
          !g_town.needs_resync);
    dd_msg apply = last_message;
    CHECK(apply.type == DD_MSG_TOWN_APPLY && apply.seq == 1);
    request.u.town.request_id = 2;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(sale_calls == 1 && sale_wallet[0] == 150 && g_town.rejected == 1 && !g_town.needs_resync);
    sale_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming, &apply));
    dd_runtime_poll();
    CHECK(sale_calls == 1 && !sale_items[0] && sale_wallet[0] == 150 && g_town.applied_seq == 1 &&
          !g_town.needs_resync);
    sale_reset(DD_ROLE_HOST);
    hook_wagon_sell(&capture);
    CHECK(sale_calls == 1 && g_town.accepted == 1 && last_message.type == DD_MSG_TOWN_APPLY);
    sale_reset(DD_ROLE_HOST);
    request.u.town.action.u.items.cost = 51;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(!sale_calls && sale_items[0] == 1 && sale_wallet[0] == 100 && g_town.rejected == 1);
    request.u.town.action.u.items.cost = 50;
    sale_reset(DD_ROLE_HOST);
    putint(sale_items + 0x88, 102);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(!sale_calls && g_town.rejected == 1);
    sale_reset(DD_ROLE_HOST);
    putint(sale_items, 2);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(!sale_calls && sale_items[0] == 2 && g_town.rejected == 1);
    sale_reset(DD_ROLE_HOST);
    putint(sale_wallet, INT32_MAX - 10);
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(!sale_calls && sale_items[0] == 1 && g_town.rejected == 1);
    sale_reset(DD_ROLE_HOST);
    sale_partial = 1;
    CHECK(queue_push(&g_incoming, &request));
    dd_runtime_poll();
    CHECK(sale_calls == 1 && g_town.needs_resync && alerts == 1);
    sale_reset(DD_ROLE_GUEST);
    capture.inventory += 8;
    hook_wagon_sell(&capture);
    CHECK(!sale_calls && !sent && sale_items[0] == 1 && alerts == 1);
    printf("Sale bridge: no guest mutation before approval; host/guest remove one and credit once; stale "
           "source/quote, overflow and partial results checked\n");
}

static void hook_machine_code(void) {
    uint8_t *code=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    CHECK(code!=NULL);if(!code)return;
    uint32_t first=0x12345678,second=0x23456789;
    code[0]=0xb8;memcpy(code+1,&first,4);code[5]=0xc3;
    code[96]=0xb8;memcpy(code+97,&second,4);code[101]=0xc3;
    dd_hook hook={0};hook.target=code;hook.patch_len=5;hook.trampoline=code+128;hook.relay=code+64;
    memcpy(hook.original,code,5);memcpy(hook.trampoline,code,5);
    emit_absolute_jump(code+133,code+5);emit_absolute_jump(code+64,code+96);
    FlushInstructionCache(GetCurrentProcess(),code,4096);
    typedef uint32_t (*machine_fn)(void);
    CHECK(((machine_fn)code)()==first);
    CHECK(hook_apply(&hook));CHECK(((machine_fn)code)()==second);
    CHECK(((machine_fn)hook.trampoline)()==first);
    hook_restore(&hook);CHECK(((machine_fn)code)()==first);
    VirtualFree(code,0,MEM_RELEASE);
    puts("Hook CPU fixture: near relay, register-preserving trampoline and restoration verified");
}

#include "runtime_hamlet_fixture.inc"
#include "runtime_supplies_fixture.inc"
#include "runtime_departure_fixture.inc"
#include "runtime_prepare_fixture.inc"
#include "runtime_embark_fixture.inc"
#include "runtime_frontend_fixture.inc"
#include "runtime_walk_fixture.inc"
#include "runtime_party_fixture.inc"
#include "runtime_battle_fixture.inc"
#include "runtime_desync_fixture.inc"
#include "runtime_formation_fixture.inc"
#include "runtime_fresh_raid_fixture.inc"
#include "runtime_hunger_fixture.inc"
#include "runtime_quest_return_fixture.inc"
#include "dd_focus.h"
#include "dd_duo.h"
static void duo_checks(void) {
    CHECK(!dd_duo_frame_wait(100, 0));
    CHECK(dd_duo_frame_wait(105, 100) == 11);
    CHECK(!dd_duo_frame_wait(116, 100) && !dd_duo_frame_wait(1000, 100));
    CHECK(!dd_duo_frame_wait(90, 100));
    uint8_t event[56] = {0};
    uint32_t type = DD_FOCUS_SDL_WINDOWEVENT;
    memcpy(event, &type, 4);
    event[12] = 13; /* Focus lost: not passed on. */
    CHECK(dd_focus_lost(event));
    event[12] = 12; /* Focus gained, and every other event, pass. */
    CHECK(!dd_focus_lost(event));
    type = 0x300;
    memcpy(event, &type, 4);
    event[12] = 13;
    CHECK(!dd_focus_lost(event));
    CHECK(dd_duo_role("guest", DD_ROLE_HOST) == DD_ROLE_GUEST);
    CHECK(dd_duo_role("", DD_ROLE_HOST) == DD_ROLE_HOST && dd_duo_role(NULL, DD_ROLE_OFF) == DD_ROLE_OFF);
    CHECK(dd_duo_role("host", DD_ROLE_OFF) == DD_ROLE_OFF); /* Only the second window has a say; the rest comes from the launcher. */
    CHECK(strcmp(dd_duo_title(DD_ROLE_GUEST), dd_duo_title(DD_ROLE_HOST)) && strcmp(dd_duo_title(DD_ROLE_HOST), "Darkest Dungeon"));
    /* A real window, off screen but marked visible, under a title of its own: the real game may be running on this PC.
     * The search the second copy does must not find the old title any more. */
    {
        char now[64] = "";
        HWND shown = CreateWindowExA(WS_EX_TOOLWINDOW, "STATIC", "DDCoop test window", WS_POPUP | WS_VISIBLE, -32000, -32000, 1, 1, NULL, NULL, NULL, NULL);
        HWND hidden = CreateWindowExA(WS_EX_TOOLWINDOW, "STATIC", "Hidden helper", WS_POPUP, 0, 0, 1, 1, NULL, NULL, NULL, NULL);
        CHECK(shown && hidden && FindWindowA(NULL, "DDCoop test window") == shown);
        CHECK(dd_duo_rename(dd_duo_title(DD_ROLE_HOST)) == 1);
        CHECK(!FindWindowA(NULL, "DDCoop test window"));
        GetWindowTextA(shown, now, sizeof now);
        CHECK(!strcmp(now, dd_duo_title(DD_ROLE_HOST)));
        GetWindowTextA(hidden, now, sizeof now);
        CHECK(!strcmp(now, "Hidden helper"));
        CHECK(dd_duo_rename(dd_duo_title(DD_ROLE_HOST)) == 0); /* Already right: nothing to do every half second. */
        DestroyWindow(shown);
        DestroyWindow(hidden);
    }
}
#include "runtime_guard_fixture.inc"

static void hook_targets_unique(void) {
    const hamlet_hook_spec *sets[] = {g_hamlet_hooks, g_supply_hooks, g_departure_hooks, g_walk_hooks, g_departure_early_hooks};
    const size_t counts[] = {sizeof g_hamlet_hooks / sizeof *g_hamlet_hooks,
        sizeof g_supply_hooks / sizeof *g_supply_hooks, sizeof g_departure_hooks / sizeof *g_departure_hooks,
        sizeof g_walk_hooks / sizeof *g_walk_hooks, sizeof g_departure_early_hooks / sizeof *g_departure_early_hooks};
    uint64_t seen[128] = {0};
    const char *names[128] = {0};
    unsigned n = 0;
    for (unsigned s = 0; s < sizeof sets / sizeof *sets; s++) for (size_t h = 0; h < counts[s]; h++) {
        uint64_t target = 0;
        for (size_t a = 0; a < dd_address_count; a++)
            if (!strcmp(dd_addresses[a].name, sets[s][h].name)) target = dd_addresses[a].va;
        CHECK(target && n < 128);
        for (unsigned p = 0; p < n; p++) if (seen[p] == target) {
            printf("Duplicate hook target: %s overwrites %s\n", sets[s][h].name, names[p]);
            CHECK(0);
        }
        seen[n] = target; names[n++] = sets[s][h].name;
    }
}
/* Placing a hero in a building whose screen this copy never opened: the activity comes from the town model. */
static uint8_t at_town[0x100], at_nodes[4][0x30], at_buildings[3][0xd0], at_acts[5][0x100];
static uint64_t at_lists[3][4];
static int at_kind(void *activity) {
    for (unsigned i = 0; i < 5; i++)
        if ((uint8_t *)activity == at_acts[i]) return at_acts[i][0x10];
    return -1;
}
static uint64_t at_town_object(void) { return (uintptr_t)at_town; }
static uint64_t at_no_town(void) { return 0; }
static int at_assigned;
static uint32_t at_hero;
static void *at_activity;
static int at_slot;
static uintptr_t __fastcall at_assign(void *activity, void *result, uint32_t hero_id) {
    at_assigned++;
    at_activity = activity;
    at_hero = hero_id;
    memcpy(&at_slot, (uint8_t *)activity + DD_ACTIVITY_SLOT_OFFSET, 4);
    ((uint8_t *)result)[0] = 1;
    return 0;
}
static void activity_model(void) {
    uint64_t p;
    memset(at_town, 0, sizeof at_town);
    memset(at_nodes, 0, sizeof at_nodes);
    memset(at_buildings, 0, sizeof at_buildings);
    memset(at_acts, 0, sizeof at_acts);
    /* A map head (nil) with a root and two children: tavern (bar, gambling, brothel), abbey (meditation, prayer), sanitarium (none). */
    at_nodes[0][0x19] = 1;
    p = (uintptr_t)at_nodes[0];
    memcpy(at_town + 0xc0, &p, 8);
    p = (uintptr_t)at_nodes[1];
    memcpy(at_nodes[0] + 8, &p, 8);
    for (unsigned n = 1; n < 4; n++) {
        uint64_t nil = (uintptr_t)at_nodes[0], building = (uintptr_t)at_buildings[n - 1];
        memcpy(at_nodes[n], &nil, 8);
        memcpy(at_nodes[n] + 0x10, &nil, 8);
        memcpy(at_nodes[n] + 0x28, &building, 8);
    }
    p = (uintptr_t)at_nodes[2];
    memcpy(at_nodes[1], &p, 8);
    p = (uintptr_t)at_nodes[3];
    memcpy(at_nodes[1] + 0x10, &p, 8);
    const int kinds[5] = {DD_ACTIVITY_BAR, DD_ACTIVITY_GAMBLING, DD_ACTIVITY_BROTHEL, DD_ACTIVITY_MEDITATION, DD_ACTIVITY_PRAYER};
    for (unsigned i = 0; i < 5; i++) at_acts[i][0x10] = (uint8_t)kinds[i];
    for (unsigned i = 0; i < 3; i++) at_lists[0][i] = (uintptr_t)at_acts[i];
    at_lists[1][0] = (uintptr_t)at_acts[3];
    at_lists[1][1] = (uintptr_t)at_acts[4];
    for (unsigned b = 0; b < 3; b++) {
        uint64_t begin = (uintptr_t)at_lists[b], end = begin + 8 * (b == 0 ? 3 : b == 1 ? 2 : 0);
        memcpy(at_buildings[b] + 0xc0, &begin, 8);
        memcpy(at_buildings[b] + 0xc8, &end, 8);
    }
    g_activity_kind = at_kind;
    g_town_object = at_town_object;
}
static void activity_transactions(void) {
    dd_action a;
    activity_model();
    CHECK(activity_in_town((uintptr_t)at_town, DD_ACTIVITY_PRAYER) == at_acts[4]);
    CHECK(activity_in_town((uintptr_t)at_town, DD_ACTIVITY_BAR) == at_acts[0]);
    CHECK(activity_in_town((uintptr_t)at_town, DD_ACTIVITY_BROTHEL) == at_acts[2]);
    CHECK(!activity_in_town((uintptr_t)at_town, DD_ACTIVITY_FLAGELLATION));
    CHECK(!activity_in_town(0, DD_ACTIVITY_BAR));
    /* Two of a kind: not guessed. */
    at_acts[1][0x10] = DD_ACTIVITY_BAR;
    CHECK(!activity_in_town((uintptr_t)at_town, DD_ACTIVITY_BAR));
    at_acts[1][0x10] = DD_ACTIVITY_GAMBLING;
    /* A loop in the tree does not hang it. */
    {
        uint64_t loop = (uintptr_t)at_nodes[1];
        memcpy(at_nodes[2], &loop, 8);
        CHECK(!activity_in_town((uintptr_t)at_town, DD_ACTIVITY_PRAYER));
        loop = (uintptr_t)at_nodes[0];
        memcpy(at_nodes[2], &loop, 8);
    }
    /* The guest applies the host's placement without having opened the abbey: g_activity is empty. */
    reset(DD_ROLE_GUEST);
    memset(g_activity, 0, sizeof g_activity);
    g_original_assign = at_assign;
    at_assigned = 0;
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ASSIGN_HERO;
    a.u.assign.activity = DD_ACTIVITY_PRAYER;
    a.u.assign.slot = 2;
    a.u.assign.hero_id = 7;
    CHECK(runtime_apply(NULL, &a) == 1);
    CHECK(at_assigned == 1 && at_activity == at_acts[4] && at_hero == 7 && at_slot == 2 && !g_game_apply_depth);
    /* No such activity in the model and no panel either: refused, as before. */
    a.u.assign.activity = DD_ACTIVITY_FLAGELLATION;
    CHECK(runtime_apply(NULL, &a) == 0 && at_assigned == 1);
    /* Without a model the panel's activity still works. */
    g_town_object = at_no_town;
    g_activity[DD_ACTIVITY_FLAGELLATION] = at_acts[3];
    at_acts[3][0x10] = DD_ACTIVITY_FLAGELLATION;
    CHECK(runtime_apply(NULL, &a) == 1 && at_assigned == 2 && at_activity == at_acts[3]);
    memset(g_activity, 0, sizeof g_activity);
    g_original_assign = NULL;
    g_activity_kind = activity_kind;
    g_town_object = town_object;
    puts("Building placement without the building's screen: the activity comes from the town's buildings; two of a kind, a missing one and a looping tree refuse; the panel's activity still works");
}

int main(void) {
    queue_init(&g_incoming);
    queue_init(&g_outgoing);
    ordered_wait(DD_ROLE_HOST);
    ordered_wait(DD_ROLE_GUEST);
    reset(DD_ROLE_HOST);
    dd_msg m = {0};
    for (unsigned i = 0; i < DD_QUEUE_CAP; i++)
        CHECK(queue_push(&g_outgoing, &m));
    outgoing_send(NULL, 0, &m);
    CHECK(g_queue_failed);
    dd_runtime_poll();
    CHECK(g_town.needs_resync && alerts == 1);
    recruit_transactions();
    sale_transactions();
    hook_machine_code();
    hook_targets_unique();
    hamlet_transactions();
    supplies_transactions();
    sw_drags();
    sw_stock_slot_drops();
    departure_transactions();
    prepare_transactions();
    embark_transactions();
    frontend_transactions();
    walk_transactions();
    party_publication_checks();
    battle_transactions();
    desync_regressions();
    formation_checks();
    fresh_raid_checks();
    hunger_transactions();
    quest_return_transactions();
    activity_transactions();
    duo_checks();
    guard_transactions();
    reset(DD_ROLE_HOST);
    overlay_refuses = 1;
    shop_notice("fallback test");
    overlay_refuses = 0;
    CHECK(alerts == 1); /* Through test_message_box. */
    shop_notice("fallback test");
    CHECK(alerts == 1); /* The same message is not repeated. */
    shop_notice_clear("fallback test");
    shop_notice("fallback test");
    CHECK(alerts == 2);
    DeleteCriticalSection(&g_incoming.lock);
    DeleteCriticalSection(&g_outgoing.lock);
    printf("Runtime FIFO: host/guest wait without mutation, preserve order, resume once and detect overflow: "
           "%s (%d failures)\n",
           failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
