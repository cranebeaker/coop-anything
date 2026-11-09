/* Fights together: a choice is an order, and each copy runs it through the game's own function at the same point of the same turn.
 * The results are not sent: from equal choices both copies draw the same numbers (runtime/DUNGEON.md, Combat).
 * Each player chooses only for their own heroes: the host's table, live changes ordered at an input boundary (launcher/OWNERSHIP.md).
 * Reached only from the normal fight's buttons (see g_battle_player_caller). Nothing here touches the arena code in the same functions. */
#include <stdio.h>
#include <stdlib.h>
#include "dd_dungeon.h"
enum {
    DD_BATTLE_CHOOSE_SKILL_STATE = 0x1d, /* bs_turn_select_skill */
    DD_BATTLE_CHOOSE_TARGET_STATE = 0x1e /* bs_turn_select_targets */
};
typedef uint64_t(__fastcall *battle_pick_fn)(void *, int32_t, uint8_t);
typedef uint64_t(__fastcall *battle_target_fn)(void *, void *, void *, void *);
typedef uint64_t(__fastcall *battle_move_fn)(void *, void *, int32_t, uint64_t, uint32_t);
typedef void(__fastcall *battle_button_fn)(void *);
typedef struct {
    uint32_t round, turn, state;
    uint64_t actor;
} battle_view;
static battle_pick_fn g_battle_pick;
static battle_target_fn g_battle_enemy, g_battle_friend;
static battle_move_fn g_battle_move;
static battle_button_fn g_battle_skill_button;
static int g_battle_in_button, g_battle_gate_told;
static uint64_t g_battle_wait_until;
static uint32_t g_hero_owner_ids[4];
static uint8_t g_hero_owner_mask;
static uint32_t g_hero_owner_revision;
static uint64_t g_hero_owner_poll_at;
static int g_hero_owner_read_ok;
static uint64_t g_hero_owner_wait_until;
static int g_hero_owner_wait_told;
static void battle_reset(void) {
    g_battle_in_button = g_battle_gate_told = 0;
    g_battle_wait_until = 0;
    memset(g_hero_owner_ids, 0, sizeof g_hero_owner_ids);
    g_hero_owner_mask = 0;
    g_hero_owner_revision = 0;
    g_hero_owner_poll_at = 0;
    g_hero_owner_wait_until = 0;
    g_hero_owner_wait_told = 0;
}
/* The launcher's file next to this DLL: hero.<roster id>=<0 host, 1 guest>. No file, or a hero not in it: the host's. A broken file: all the host's, and said out loud. */
static int hero_owner_line(const char *line, uint32_t *id, uint32_t *owner) {
    char *end;
    unsigned long a, b;
    if (strncmp(line, "hero.", 5)) return 0;
    a = strtoul(line + 5, &end, 10);
    if (end == line + 5 || *end != '=') return -1;
    line = end + 1;
    b = strtoul(line, &end, 10);
    if (end == line || (*end && *end != '\r' && *end != '\n') || b > 1 || a > 0xffffffffUL) return -1;
    *id = (uint32_t)a;
    *owner = (uint32_t)b;
    return 1;
}
static FILE *hero_owner_open(void) {
    char path[MAX_PATH];
    HMODULE self = NULL;
    char *slash;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)hero_owner_line, &self) ||
        !GetModuleFileNameA(self, path, sizeof path) || !(slash = strrchr(path, '\\')) || (size_t)(slash + 1 - path) + 18 >= sizeof path)
        return NULL;
    strcpy(slash + 1, "ddcoop-heroes.ini");
    return fopen(path, "rb");
}
static FILE *(*g_hero_owner_open)(void) = hero_owner_open;
/* Reuse the bounded, versioned launcher-contract parser. A transient or
 * malformed file during a raid never replaces the last agreed ownership. */
static int hero_owners_read(const uint32_t ids[4], uint8_t *mask, uint32_t *revision) {
    char text[16385];
    uint8_t owners[4];
    FILE *f = g_hero_owner_open();
    *mask = 0;
    *revision = 0;
    if (!f) return 0;
    size_t n = fread(text, 1, sizeof text, f);
    int failed = ferror(f);
    fclose(f);
    if (failed) return -1;
    int result = dd_dng_owners_parse(text, n, ids, owners, revision);
    if (result != 1) return result;
    for (unsigned i = 0; i < 4; i++) *mask |= (uint8_t)(owners[i] << i);
    return 1;
}
static uint8_t hero_owners_load(const uint32_t ids[4], uint32_t *revision) {
    uint8_t mask = 0;
    int read = hero_owners_read(ids, &mask, revision);
    g_hero_owner_read_ok = read == 1;
    g_hero_owner_revision = *revision;
    if (!g_hero_owner_read_ok) dd_log("Heroes: no valid host ownership file at departure; host defaults retained");
    /* No file is normal. A file that cannot be read means someone's choices are being ignored: say so. */
    if (read < 0) shop_notice("De lijst met helden per speler is niet leesbaar. Deze expeditie zijn alle helden van de host.");
    return mask;
}
static void hero_owners_set(const uint32_t ids[4], uint8_t mask) {
    memcpy(g_hero_owner_ids, ids, sizeof g_hero_owner_ids);
    g_hero_owner_mask = mask;
    dd_log("Heroes: guest owns%s%s%s%s of %u,%u,%u,%u", mask & 1 ? " 1st" : "", mask & 2 ? " 2nd" : "", mask & 4 ? " 3rd" : "", mask & 8 ? " 4th" : "", ids[0], ids[1], ids[2], ids[3]);
}
static int hero_owners_known(void) {
    return g_hero_owner_ids[0] || g_hero_owner_ids[1] || g_hero_owner_ids[2] || g_hero_owner_ids[3];
}
/* Restoring a raid skips RaidStartParameters and therefore departure_send/apply.
 * Recover membership from the live normal party, sorted by persistent ID so a
 * formation change cannot change the identity of an ownership bit. Read only. */
static int hero_owners_restored_ids(const walk_view *v, uint32_t ids[4]) {
    uint64_t heroes[4];
    uint32_t live[4], hp[4], stress[4], trinkets[4];
    unsigned count = 0;
    memset(ids, 0, 4 * sizeof *ids);
    if (!walk_party_read(v->raid, heroes, live, hp, stress, trinkets)) return 0;
    for (unsigned i = 0; i < 4; i++) if (heroes[i]) {
        if (!live[i] || live[i] == UINT32_MAX) return 0;
        unsigned at = count;
        for (unsigned j = 0; j < count; j++) if (ids[j] == live[i]) return 0;
        while (at && ids[at - 1] > live[i]) { ids[at] = ids[at - 1]; at--; }
        ids[at] = live[i];
        count++;
    }
    return count != 0;
}
/* Host changes are serialized alongside skills and targets. Poll only at
 * an input boundary; an animation/attack already running completes first. */
static void hero_owners_poll(const walk_view *v) {
    if (g_cfg.role != DD_ROLE_HOST || !walk_shared() || g_game_apply_depth ||
        (v->battle && v->battle != DD_BATTLE_CHOOSE_SKILL_STATE && v->battle != DD_BATTLE_CHOOSE_TARGET_STATE)) return;
    uint64_t now = GetTickCount64();
    if (now < g_hero_owner_poll_at) return;
    g_hero_owner_poll_at = now + 500;
    int restored = !hero_owners_known();
    uint32_t ids[4];
    if (restored) {
        if (!hero_owners_restored_ids(v, ids)) return;
    } else memcpy(ids, g_hero_owner_ids, sizeof ids);
    uint8_t mask = 0;
    uint32_t revision = 0;
    int read = hero_owners_read(ids, &mask, &revision);
    if (read != 1) {
        if (!restored) return; /* Retain the last agreed table on a transient file failure. */
        mask = 0; revision = 0; /* Same host defaults as a normal departure. */
    }
    if (!restored && mask == g_hero_owner_mask && revision == g_hero_owner_revision) return;
    dd_action a = {0};
    a.kind = DD_ACT_OWNERS;
    memcpy(a.u.owners.heroes, ids, sizeof ids);
    a.u.owners.mask = mask;
    a.u.owners.revision = revision;
    if (!dd_action_valid(&a)) return;
    uint32_t before = g_town.applied_seq;
    dd_town_host_observed(&g_town, &a);
    if (g_town.applied_seq == before || g_town.needs_resync) return;
    hero_owners_set(a.u.owners.heroes, mask);
    g_hero_owner_revision = revision;
    if (restored) dd_log("Heroes: restored raid ownership shared from the host (%s)", read == 1 ? "host file" : "host defaults; no valid host file");
    dd_log("Heroes: live ownership revision=%u mask=%u shared", revision, mask);
}
/* A saved campaign can finish loading slightly earlier on the host. Keep its
 * first table in the ordered FIFO until the guest has a readable raid party. */
static int hero_owners_gate(const dd_msg *m) {
    if (g_cfg.role != DD_ROLE_GUEST || hero_owners_known() || m->type != DD_MSG_TOWN_APPLY ||
        g_town.applied_seq == UINT32_MAX || m->seq != g_town.applied_seq + 1 ||
        m->u.town.action.kind != DD_ACT_OWNERS || !dd_action_valid(&m->u.town.action)) return 0;
    walk_view v;
    uint32_t ids[4];
    if (g_walk_view(&v) && hero_owners_restored_ids(&v, ids)) {
        g_hero_owner_wait_until = 0;
        g_hero_owner_wait_told = 0;
        return 0; /* A different party is refused by apply, never silently adopted. */
    }
    uint64_t now = GetTickCount64();
    if (!g_hero_owner_wait_until) g_hero_owner_wait_until = now + 30000;
    if (now > g_hero_owner_wait_until) return 0;
    if (!g_hero_owner_wait_told) {
        g_hero_owner_wait_told = 1;
        dd_log("Heroes: restored ownership waits for this copy's raid to load");
    }
    return 1;
}
static int hero_owners_apply(const dd_action *a) {
    if (g_cfg.role != DD_ROLE_GUEST) return 0;
    if (!hero_owners_known()) {
        walk_view v;
        uint32_t ids[4];
        if (!g_walk_view(&v) || !hero_owners_restored_ids(&v, ids) || memcmp(a->u.owners.heroes, ids, sizeof ids)) return 0;
        dd_log("Heroes: restored raid ownership accepted from the host");
    } else if (memcmp(a->u.owners.heroes, g_hero_owner_ids, sizeof g_hero_owner_ids)) return 0;
    hero_owners_set(a->u.owners.heroes, a->u.owners.mask);
    g_hero_owner_revision = a->u.owners.revision;
    dd_log("Heroes: live ownership revision=%u mask=%u applied", g_hero_owner_revision, g_hero_owner_mask);
    return 1;
}
/* -1 while the initial table is unknown, 0 the host, 1 the guest. The roster id is at hero+137c. */
static int hero_owner(uint64_t hero) {
    uint32_t id = 0;
    if (!hero_owners_known()) return -1; /* Neither player chooses before the first agreed table. */
    if (!hero || !safe_read((uintptr_t)hero + 0x137c, &id, 4)) return 0;
    for (unsigned i = 0; i < 4; i++)
        if (id && g_hero_owner_ids[i] == id) return (g_hero_owner_mask >> i) & 1;
    return 0;
}
static int hero_is_mine(uint64_t hero) { return hero_owner(hero) == (g_cfg.role == DD_ROLE_GUEST ? 1 : 0); }
static int battle_read(const walk_view *v, battle_view *b) {
    uint64_t battle = v->raid + 0x4898, begin = 0, end = 0;
    memset(b, 0, sizeof *b);
    b->state = v->battle;
    return v->battle && safe_read((uintptr_t)battle + 0xa8, &b->round, 4) && safe_read((uintptr_t)battle + 0xa0, &b->turn, 4) &&
           safe_read((uintptr_t)battle + 0x20, &begin, 8) && safe_read((uintptr_t)battle + 0x28, &end, 8) && end >= begin &&
           (uint64_t)b->turn < (end - begin) / 0x18 && safe_read((uintptr_t)(begin + (uint64_t)b->turn * 0x18), &b->actor, 8) && b->actor;
}
/* A target by its place in the party (raid+8) or the monsters (raid+48d0), which are the same on both copies while they are in step. */
static int battle_target_place(uint64_t raid, uint64_t target, uint8_t *side, uint8_t *place) {
    for (uint8_t s = 0; s < 2; s++) {
        uint64_t begin = 0, end = 0;
        if (!safe_read((uintptr_t)raid + (s ? 0x48d0 : 8), &begin, 8) || !safe_read((uintptr_t)raid + (s ? 0x48d8 : 0x10), &end, 8) || end < begin) return 0;
        for (uint8_t i = 0; i < 16 && begin + (uint64_t)i * 8 < end; i++) {
            uint64_t p = 0;
            if (!safe_read((uintptr_t)(begin + (uint64_t)i * 8), &p, 8)) return 0;
            if (p == target) {
                *side = s;
                *place = i;
                return 1;
            }
        }
    }
    return 0;
}
static uint64_t battle_target_at(uint64_t raid, uint8_t side, uint8_t place) {
    uint64_t begin = 0, end = 0, p = 0;
    if (!safe_read((uintptr_t)raid + (side ? 0x48d0 : 8), &begin, 8) || !safe_read((uintptr_t)raid + (side ? 0x48d8 : 0x10), &end, 8) || end < begin ||
        (uint64_t)place >= (end - begin) / 8 || !safe_read((uintptr_t)(begin + (uint64_t)place * 8), &p, 8))
        return 0;
    return p;
}
static uint32_t battle_rules(void) {
    uint32_t rules = 0;
    if (g_rules_object) safe_read((uintptr_t)g_rules_object + 8, &rules, 4);
    return rules;
}
/* 1 an order, 0 this choice cannot be shared now, -1 no fight is running here. */
static int battle_order(uint8_t op, uint64_t hero, dd_action *a) {
    walk_view v;
    battle_view b;
    if (!g_walk_view(&v) || !v.battle) return -1;
    memset(a, 0, sizeof *a);
    a->kind = DD_ACT_BATTLE;
    a->u.battle.op = op;
    if (!battle_read(&v, &b) || (b.state != DD_BATTLE_CHOOSE_SKILL_STATE && b.state != DD_BATTLE_CHOOSE_TARGET_STATE) || b.actor != hero ||
        !walk_hero_rank(v.raid, hero, &a->u.battle.hero)) {
        dd_log("Battle: choice %u outside this hero's turn (round=%u turn=%u state=%u); dropped", op, b.round, b.turn, b.state);
        return 0;
    }
    if (!hero_is_mine(hero)) {
        dd_log("Battle: this hero belongs to the other player; choice %u dropped", op);
        return 0;
    }
    a->u.battle.round = b.round;
    a->u.battle.turn = b.turn;
    a->u.battle.state = (uint8_t)b.state;
    a->u.battle.rules = battle_rules();
    return 1;
}
static void battle_send(const dd_action *a) {
    dd_log("Battle: choice op=%u round=%u turn=%u state=%u hero=%u value=%d side=%u place=%u rules=%08x (role=%s)", a->u.battle.op, a->u.battle.round, a->u.battle.turn,
           a->u.battle.state, a->u.battle.hero, a->u.battle.value, a->u.battle.side, a->u.battle.place, a->u.battle.rules, dd_role_name(g_cfg.role));
    if (g_cfg.role == DD_ROLE_GUEST) dd_town_guest_local(&g_town, a);
    else dd_town_host_observed(&g_town, a);
}
/* 71f1e0: a skill button. It picks through 5cdf70, which the game also calls for its own reasons; only a pick from inside this button is a choice. */
static void __fastcall hook_battle_skill_button(void *functor) {
    if (!g_battle_skill_button) return;
    g_battle_in_button++;
    g_battle_skill_button(functor);
    g_battle_in_button--;
}
static uint64_t __fastcall hook_battle_pick(void *hero, int32_t slot, uint8_t flag) {
    dd_action a;
    if (!g_battle_pick) return 0;
    if (!g_battle_in_button || g_game_apply_depth || !walk_shared()) return g_battle_pick(hero, slot, flag);
    int order = battle_order(DD_BATTLE_OP_SKILL, (uintptr_t)hero, &a);
    if (order < 0) return g_battle_pick(hero, slot, flag); /* Looking at skills in a corridor. */
    if (!order) return 0;
    a.u.battle.value = slot;
    a.u.battle.flag = flag != 0;
    battle_send(&a);
    return g_cfg.role == DD_ROLE_HOST ? g_battle_pick(hero, slot, flag) : 0; /* The guest's pick comes back as the host's order. */
}
/* The players' calls, by where they return to: a target button (707850, 707ae0), moving (707b40), Pass (71f3c0), the controller's Pass (780058).
 * The fight's own calls to these functions (state code, a skill's own move at 5b9f11) run as they are. */
static int battle_player_caller(uintptr_t rva) { return rva == 0x70787c || rva == 0x707b06 || rva == 0x707b98 || rva == 0x71f41d || rva == 0x780083; }
static int (*g_battle_player_caller)(uintptr_t) = battle_player_caller;
static uint64_t battle_target_choice(battle_target_fn native, uint8_t op, void *battle, void *skill, void *actor, void *target) {
    walk_view v;
    dd_action a;
    int order = battle_order(op, (uintptr_t)actor, &a);
    if (order < 0) return native(battle, skill, actor, target);
    if (!order) return 0;
    if (!g_walk_view(&v) || !battle_target_place(v.raid, (uintptr_t)target, &a.u.battle.side, &a.u.battle.place)) {
        dd_log("Battle: target not found in party or monsters; dropped");
        return 0;
    }
    battle_send(&a);
    return g_cfg.role == DD_ROLE_HOST ? native(battle, skill, actor, target) : 0;
}
#ifdef _MSC_VER
#define DD_BATTLE_CALLER() ((uintptr_t)_ReturnAddress())
#else
#define DD_BATTLE_CALLER() ((uintptr_t)__builtin_return_address(0))
#endif
/* 5abae0: add a target for a skill on the enemy. It may redirect by its own draws, on both copies alike. */
static uint64_t __fastcall hook_battle_enemy(void *battle, void *skill, void *actor, void *target) {
    uintptr_t caller = DD_BATTLE_CALLER();
    if (!g_battle_enemy) return 0;
    if (g_game_apply_depth || !walk_shared() || !g_battle_player_caller(caller - image_base())) return g_battle_enemy(battle, skill, actor, target);
    return battle_target_choice(g_battle_enemy, DD_BATTLE_OP_ENEMY, battle, skill, actor, target);
}
/* 5ab6d0: the same for a skill on the party. */
static uint64_t __fastcall hook_battle_friend(void *battle, void *skill, void *actor, void *target) {
    uintptr_t caller = DD_BATTLE_CALLER();
    if (!g_battle_friend) return 0;
    if (g_game_apply_depth || !walk_shared() || !g_battle_player_caller(caller - image_base())) return g_battle_friend(battle, skill, actor, target);
    return battle_target_choice(g_battle_friend, DD_BATTLE_OP_FRIEND, battle, skill, actor, target);
}
/* 5b9540: move to a rank; its own rank is passing the turn. */
static uint64_t __fastcall hook_battle_move(void *battle, void *hero, int32_t rank, uint64_t spare, uint32_t fifth) {
    uintptr_t caller = DD_BATTLE_CALLER();
    dd_action a;
    if (!g_battle_move) return 0;
    if (g_game_apply_depth || !walk_shared() || !g_battle_player_caller(caller - image_base())) return g_battle_move(battle, hero, rank, spare, fifth);
    int order = battle_order(DD_BATTLE_OP_MOVE, (uintptr_t)hero, &a);
    if (order < 0) return g_battle_move(battle, hero, rank, spare, fifth);
    if (!order) return 0;
    a.u.battle.value = rank;
    battle_send(&a);
    return g_cfg.role == DD_ROLE_HOST ? g_battle_move(battle, hero, rank, spare, fifth) : 0;
}
/* Whose choice it is now, shown as a strip at the top of the window. */
static void battle_turn_show(const walk_view *v) {
    battle_view b;
    uint8_t rank = 0;
    int turn = 0;
    if (walk_shared() && hero_owners_known() && v->battle && battle_read(v, &b) && (b.state == DD_BATTLE_CHOOSE_SKILL_STATE || b.state == DD_BATTLE_CHOOSE_TARGET_STATE) && walk_hero_rank(v->raid, b.actor, &rank))
        turn = hero_is_mine(b.actor) ? DD_OVERLAY_TURN_MINE : DD_OVERLAY_TURN_OTHER;
    dd_overlay_turn(turn);
}
/* Where this copy stands against an order: <0 not there yet, 0 at its point, >0 past it. */
static int battle_place_compare(const battle_view *b, uint32_t round, uint32_t turn, uint32_t state) {
    if (b->round != round) return b->round < round ? -1 : 1;
    if (b->turn != turn) return b->turn < turn ? -1 : 1;
    if (b->state != state) return b->state < state ? -1 : 1;
    return 0;
}
static int battle_compare(const battle_view *b, const dd_action *a) { return battle_place_compare(b, a->u.battle.round, a->u.battle.turn, a->u.battle.state); }
/* Both copies wait for the same point: presentation takes its own time on each. The host waits as well, for a choice the guest made ahead of it. */
static int battle_gate(const dd_msg *m) {
    walk_view v = {0};
    battle_view b;
    int go = 0;
    if (m->type != DD_MSG_TOWN_REQUEST && m->type != DD_MSG_TOWN_APPLY) return 0;
    if (m->u.town.action.kind != DD_ACT_BATTLE || !dd_action_valid(&m->u.town.action)) return 0;
    if (g_cfg.role == DD_ROLE_GUEST ? (m->type != DD_MSG_TOWN_APPLY || g_town.applied_seq == UINT32_MAX || m->seq != g_town.applied_seq + 1) : m->type != DD_MSG_TOWN_REQUEST)
        return 0;
    const dd_action *a = &m->u.town.action;
    uint64_t now = GetTickCount64();
    if (!g_battle_wait_until) g_battle_wait_until = now + 30000;
    if (g_walk_view(&v) && battle_read(&v, &b)) go = battle_compare(&b, a) >= 0;
    if (!go && now <= g_battle_wait_until) {
        if (!g_battle_gate_told) {
            g_battle_gate_told = 1;
            dd_log("Battle: order round=%u turn=%u state=%u waits for this copy (battle state=%u)", a->u.battle.round, a->u.battle.turn, a->u.battle.state, v.battle);
        }
        return 1;
    }
    g_battle_gate_told = 0;
    g_battle_wait_until = 0;
    return 0;
}
static int battle_apply(const dd_action *a) {
    walk_view v;
    battle_view b = {0};
    uint64_t hero = 0, skill = 0, begin = 0, end = 0;
    /* The host may simply say no: the guest's click then did nothing. The guest must not: the host has already done it. */
    int refused = g_cfg.role == DD_ROLE_HOST ? 0 : 1;
    if (!g_walk_view(&v) || !battle_read(&v, &b) || battle_compare(&b, a) || !(hero = battle_target_at(v.raid, 0, a->u.battle.hero)) || hero != b.actor) {
        dd_log("Battle: order op=%u round=%u turn=%u state=%u does not fit here (round=%u turn=%u state=%u)", a->u.battle.op, a->u.battle.round, a->u.battle.turn, a->u.battle.state,
               b.round, b.turn, b.state);
        if (refused) shop_notice("Het gevecht loopt niet meer gelijk met de host. Sluit beide spellen en stuur de logs mee.");
        return refused;
    }
    if (g_cfg.role == DD_ROLE_HOST && hero_owner(hero) != 1) {
        dd_log("Battle: the guest asked for a hero that is not theirs");
        return 0;
    }
    uint32_t rules = battle_rules();
    if (rules != a->u.battle.rules) dd_log("Battle: Rules differ at this order: here %08x, there %08x", rules, a->u.battle.rules);
    dd_log("Battle: order op=%u round=%u turn=%u hero=%u value=%d side=%u place=%u (role=%s)", a->u.battle.op, a->u.battle.round, a->u.battle.turn, a->u.battle.hero, a->u.battle.value,
           a->u.battle.side, a->u.battle.place, dd_role_name(g_cfg.role));
    switch (a->u.battle.op) {
    case DD_BATTLE_OP_SKILL:
        if (!g_battle_pick) return refused;
        g_battle_pick((void *)(uintptr_t)hero, a->u.battle.value, a->u.battle.flag);
        /* What the button does after picking a skill (71f2da): no targets from an earlier choice. */
        if (a->u.battle.value != -1 && safe_read((uintptr_t)hero + 0x8f0, &begin, 8) && safe_read((uintptr_t)hero + 0x8f8, &end, 8) && begin != end)
            memcpy((void *)(uintptr_t)(hero + 0x8f8), &begin, 8);
        return 1;
    case DD_BATTLE_OP_ENEMY:
    case DD_BATTLE_OP_FRIEND: {
        battle_target_fn native = a->u.battle.op == DD_BATTLE_OP_ENEMY ? g_battle_enemy : g_battle_friend;
        uint64_t target = battle_target_at(v.raid, a->u.battle.side, a->u.battle.place);
        if (!native || !target || !safe_read((uintptr_t)hero + 0x8e0, &skill, 8) || !skill) {
            dd_log("Battle: no skill or target here for this order");
            return refused;
        }
        native((void *)(uintptr_t)(v.raid + 0x4898), (void *)(uintptr_t)skill, (void *)(uintptr_t)hero, (void *)(uintptr_t)target);
        return 1;
    }
    case DD_BATTLE_OP_MOVE:
        if (!g_battle_move) return refused;
        g_battle_move((void *)(uintptr_t)(v.raid + 0x4898), (void *)(uintptr_t)hero, a->u.battle.value, 0, 0);
        return 1;
    default:
        return refused;
    }
}
