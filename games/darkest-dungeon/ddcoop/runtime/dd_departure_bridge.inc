/* Mission departure: host-authoritative departure and one map from one seed.
 * Darkest::RaidStart 3d7e80 -> Raid start 60c480 -> 5e36c0 -> map generator 5f1ff0, which seeds a local
 * prng from one rand_s call at 5f2164. The host picks that seed when it embarks and sends it through the
 * ordered town FIFO. rand_s is only replaced while the map generator runs.
 * The Rules stream (global named prng at 2add250) also draws while the raid loads and is seeded per game start, so
 * the host sends a value for it too, set through the game's own named seed function 28fc40 as RaidStart begins. */
typedef uintptr_t(__fastcall *raid_start_fn)(void *);
typedef uintptr_t(__fastcall *raid_map_fn)(void *, void *, void *);
typedef int(__cdecl *rand_s_fn)(unsigned int *);
typedef void(__fastcall *named_seed_fn)(void *, uint32_t);
typedef struct {
    uint32_t quest, heroes[4], supplies;
} departure_view;
#define DD_MAP_SEED_CALL_RVA 0x5f2164u
#define DD_RULES_RVA 0x2add250u
static raid_start_fn g_raid_start;
static dd_hook g_raid_start_hook;
static int g_raid_start_hook_installed, g_departure_load_armed;
static raid_map_fn g_raid_map;
static rand_s_fn g_rand_s_native;
static named_seed_fn g_rules_reseed;
static void *g_rules_object;
static uint32_t g_departure_seed, g_departure_rules, g_departure_open_supplies;
static int depart_clock(float *out) { return safe_read(image_base() + 0x2a6e178, out, 4); }
static int (*g_depart_clock)(float *) = depart_clock;
static uint64_t g_departure_poll_at;
static int g_departure_forcing, g_departure_have_host;
static void quest_return_reset(void);
static departure_view g_departure_host;
static void prepare_reset(void);
static void embark_reset(void);

static void departure_reset(void) {
    prepare_reset();
    embark_reset();
    g_departure_seed = g_departure_rules = g_departure_open_supplies = 0;
    g_departure_poll_at = 0;
    g_departure_forcing = g_departure_have_host = 0;
    memset(&g_departure_host, 0, sizeof g_departure_host);
}
/* app+1720 = RaidStartParameters: +0 quest definition (+40 its key), +208/+210 vector of Hero*, index = rank. */
static int departure_read(void *app, departure_view *v) {
    uint64_t params = 0, quest = 0, begin = 0, end = 0;
    memset(v, 0, sizeof *v);
    if (!safe_read((uintptr_t)app + 0x1720, &params, 8) || !params || !safe_read((uintptr_t)params, &quest, 8) ||
        !quest || !safe_read((uintptr_t)quest + 0x40, &v->quest, 4) ||
        !safe_read((uintptr_t)params + 0x208, &begin, 8) || !safe_read((uintptr_t)params + 0x210, &end, 8) ||
        end < begin || (end - begin) % 8 || (end - begin) / 8 > 4)
        return 0;
    for (unsigned i = 0; i < (end - begin) / 8; i++) {
        uint64_t hero = 0;
        if (!safe_read((uintptr_t)begin + i * 8, &hero, 8)) return 0;
        if (hero && !safe_read((uintptr_t)hero + 0x137c, &v->heroes[i], 4)) return 0;
    }
    return 1;
}
/* Bag and funds as a number both logs can show. RaidStart is too late to read them (the owner's solo log shows zero
 * there), so this keeps the last state seen with the supply panel open. */
static void departure_poll(void) {
    static dd_provisions_snapshot s;
    supply_model m;
    uint8_t hash[32];
    uint64_t now = GetTickCount64();
    if (now < g_departure_poll_at) return;
    g_departure_poll_at = now + 500;
    if (g_supply_open(&m, &s) && (s.panel_visible || g_supply_bg) && dd_supplies_digest(&s, hash)) memcpy(&g_departure_open_supplies, hash, 4);
}
static int __cdecl hook_rand_s(unsigned int *out) {
    if (g_departure_forcing && g_departure_seed && out) {
        *out = g_departure_seed;
        dd_log("Departure: map seed %u used", g_departure_seed);
        return 0;
    }
    return g_rand_s_native(out);
}
static uintptr_t __fastcall hook_raid_map(void *a, void *b, void *c) {
    g_departure_forcing = g_departure_seed != 0;
    uintptr_t result = g_raid_map(a, b, c);
    if (!g_departure_forcing && InterlockedCompareExchange(&g_session_ready, 0, 0))
        dd_log("Departure: map made without a shared seed; this copy's map will differ");
    g_departure_forcing = 0;
    return result;
}
#ifdef DD_SOLO_MAP_SEED
/* Separate test build only (never a player kit): can one seed make one map, without a second player?
 * Every embark gets one map seed; embarks 1 and 2 get one Rules seed, later ones another, to see what Rules decides. */
static uint32_t departure_solo_seed(void) {
    char path[MAX_PATH], *slash;
    HMODULE self = NULL;
    unsigned run = 0;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (const char *)departure_solo_seed, &self) ||
        !GetModuleFileNameA(self, path, sizeof path) || !(slash = strrchr(path, '\\')) ||
        (size_t)(slash + 1 - path) + sizeof "ddcoop-solo-rules-run.txt" > sizeof path)
        return 0;
    strcpy(slash + 1, "ddcoop-solo-rules-run.txt");
    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%u", &run) != 1) run = 0;
        fclose(f);
    }
    run++;
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%u\n", run);
        fclose(f);
    }
    uint32_t rules = (uint32_t)DD_SOLO_MAP_SEED + (run > 2 ? 1000u : 0u);
    dd_log("SOLO TEST: embark %u uses fixed map seed %u and Rules seed %u", run, (uint32_t)DD_SOLO_MAP_SEED, rules);
    return rules;
}
#endif
/* Leaving for the raid. The normal way ends in 6e7c70, called when the supply panel has hidden (6e2f40, 6e25b6): it only sets the
 * app's next state to 0x14, and that state builds the start parameters and calls RaidStart.
 * NEVER 6e3390, 6e3690's callers or 6debf0: that is the Butcher's Circus ("#Status_InCircus", "matchmaking_%s"). */
typedef struct {
    uint64_t town, raid, campaign;
    int32_t quest;
    uint32_t key;
} depart_view;
typedef void(__fastcall *town_depart_fn)(void);
static town_depart_fn g_town_depart;
static int depart_quest_key(uint64_t campaign, int32_t index, uint32_t *key) {
    uint64_t quest = 0;
    *key = 0;
    return supply_quest_at(campaign, index, &quest) && safe_read((uintptr_t)quest + 0x40, key, 4) && *key;
}
static int depart_read(depart_view *v) {
    memset(v, 0, sizeof *v);
    if (!safe_read(image_base() + 0x118e360, &v->town, 8) || !safe_read(image_base() + 0x118e2c8, &v->raid, 8) ||
        !safe_read(image_base() + 0x118e138, &v->campaign, 8) || !v->campaign ||
        !safe_read((uintptr_t)v->campaign + 0x1248, &v->quest, 4))
        return 0;
    (void)depart_quest_key(v->campaign, v->quest, &v->key);
    return 1;
}
static int (*g_depart_view)(depart_view *) = depart_read;
/* Normal mode: the click handler 69f990 only stores this; the panel reads it every update (67ac1d). */
static int depart_set_quest(const depart_view *v, uint32_t index, uint32_t key) {
    uint32_t mine = 0;
    if (!key || index == UINT32_MAX) return 1;
    if (!depart_quest_key(v->campaign, (int32_t)index, &mine) || mine != key) {
        dd_log("Departure: mission %u of the host is %08x here, this copy has %08x", index, key, mine);
        shop_notice("Deze kopie heeft niet dezelfde missie als de host. Sluit beide spellen en start de sessie opnieuw; "
                    "stuur de logs mee.");
        return 0;
    }
    memcpy((void *)(uintptr_t)(v->campaign + 0x1248), &index, 4);
    return 1;
}
/* What the expedition screen does on the copy that opened it, for the copy that did not: the raid takes the heroes at location 1,
 * lowest +15b8 first (5575c0, 590260, 590220). The screen writes +15b8 from its slots (67a630): the clock plus 4 - slot, and slot = the
 * index in the party vector (67a084). So party 8,2,18,1 gives the list 1,18,2,8, as in the owner's logs. */
static int depart_party(void) {
    dd_hamlet s;
    uint32_t party[4] = {0}, ids[DD_HAMLET_HEROES];
    unsigned count = 0;
    float clock = 0;
    if (!g_hamlet_location || !g_hamlet_open_model(&s) || !dd_hamlet_party(&s, party) || !hamlet_roster_ids(&s, ids, &count) ||
        !g_depart_clock(&clock))
        return 0;
    if (!(party[0] || party[1] || party[2] || party[3])) return 0;
    for (unsigned i = 0; i < count; i++) {
        dd_hamlet_hero hero;
        uint32_t want = 0;
        if (dd_hamlet_find(&s, ids[i], &hero) != 1) return 0;
        for (unsigned r = 0; r < 4; r++)
            if (party[r] == ids[i]) want = 1;
        if (hero.location <= 1 && hero.location != want)
            g_hamlet_location((void *)(uintptr_t)s.campaign, (void *)(uintptr_t)hero.entry, (int32_t)want);
    }
    for (unsigned r = 0; r < 4; r++) {
        dd_hamlet_hero hero;
        if (!party[r]) continue;
        if (dd_hamlet_find(&s, party[r], &hero) != 1) return 0;
        float order = clock + (float)(4 - r);
        memcpy((void *)(uintptr_t)(hero.entry + 0x15b8), &order, 4);
    }
    return 1;
}
static int (*g_depart_party)(void) = depart_party;
static void departure_send(uint32_t quest, uint32_t index, const uint32_t heroes[4], uint32_t supplies);
/* A raid loaded from the save (no start parameters): Rules is seeded by the clock when each game starts and not when a campaign
 * loads, so two copies resuming the same fight drew different numbers from the first turn (owner's run of 8 Nov 16:01; the
 * Battle stream comes from the save and was equal). Both copies know the session's password, so both seed Rules with the same
 * value from it as RaidStart begins, without a message. A new password per session gives a new value. */
static uint32_t departure_session_seed(const char *password) {
    uint32_t h = 2166136261u;
    static const char domain[] = "DDCoop restored raid";
    for (const char *p = domain; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    for (const char *p = password; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    h &= 0x7fffffffu;
    return h ? h : 0x17fa1;
}
static void departure_restored_seed(void) {
    if (g_cfg.role == DD_ROLE_OFF || !g_cfg.password[0] || g_departure_rules) return;
    g_departure_rules = departure_session_seed(g_cfg.password);
    dd_log("Departure: restored raid; Rules seed %u from the session, the same on both copies", g_departure_rules);
}
static uint8_t hero_owners_load(const uint32_t ids[4], uint32_t *revision);
static void hero_owners_set(const uint32_t ids[4], uint8_t mask);
static void departure_begin(void *app) {
    departure_view v;
    if (!departure_read(app, &v)) {
        dd_log("Departure: no start parameters to read (role=%s), restored or unknown raid",
               dd_role_name(g_cfg.role));
        departure_restored_seed();
        return;
    }
    v.supplies = g_departure_open_supplies;
    dd_log("Departure: role=%s quest=%08x heroes=%u,%u,%u,%u supplies=%08x", dd_role_name(g_cfg.role), v.quest,
           v.heroes[0], v.heroes[1], v.heroes[2], v.heroes[3], v.supplies);
    if (g_cfg.role == DD_ROLE_GUEST) {
        if (!g_departure_have_host) {
            dd_log("Departure: the host has not embarked yet; this copy's map will differ");
            return;
        }
        int same_quest = v.quest == g_departure_host.quest, same_party = !memcmp(v.heroes, g_departure_host.heroes, sizeof v.heroes);
        int same_supplies = v.supplies == g_departure_host.supplies;
        dd_log("Departure: compared with host quest=%d party=%d supplies=%d", same_quest, same_party, same_supplies);
        return;
    }
#ifdef DD_SOLO_MAP_SEED
    if (g_cfg.role == DD_ROLE_HOST && !InterlockedCompareExchange(&g_session_ready, 0, 0) && g_rand_s_native) {
        g_departure_rules = departure_solo_seed();
        g_departure_seed = (uint32_t)DD_SOLO_MAP_SEED;
        return;
    }
#endif
    if (g_cfg.role != DD_ROLE_HOST || !InterlockedCompareExchange(&g_session_ready, 0, 0) || g_town.needs_resync) return;
    {
        depart_view view;
        departure_send(v.quest, g_depart_view(&view) && view.key == v.quest ? (uint32_t)view.quest : UINT32_MAX, v.heroes, v.supplies);
    }
}
static void departure_send(uint32_t quest, uint32_t index, const uint32_t heroes[4], uint32_t supplies) {
    if (!g_rand_s_native) {
        dd_log("Departure: seed cannot be shared on this game file");
        return;
    }
    unsigned int value = 0, rules = 0;
    if (g_rand_s_native(&value) || g_rand_s_native(&rules)) return;
    value &= 0x7fffffffu;
    rules &= 0x7fffffffu;
    if (!value) value = 0x17fa1; /* What the game itself uses for a zero. */
    if (!rules) rules = 0x17fa1;
    dd_action a = {0};
    a.kind = DD_ACT_DEPARTURE;
    a.u.departure.seed = value;
    a.u.departure.rules = rules;
    a.u.departure.quest = quest;
    a.u.departure.quest_index = index;
    memcpy(a.u.departure.heroes, heroes, sizeof a.u.departure.heroes);
    a.u.departure.supplies = supplies;
    a.u.departure.owners = hero_owners_load(heroes, &a.u.departure.revision);
    if (!dd_action_valid(&a)) {
        dd_log("Departure: no valid party to share");
        return;
    }
    g_departure_seed = value;
    g_departure_rules = rules;
    hero_owners_set(heroes, a.u.departure.owners);
    dd_town_host_observed(&g_town, &a);
    dd_log("Departure: host shares map seed %u and Rules seed %u", value, rules);
}
static uintptr_t __fastcall hook_raid_start(void *app) {
    int armed = g_departure_load_armed;
    g_departure_load_armed = 0;
    if (!g_raid_start) return 0;
    /* Before the slot watcher catches up, only the configured normal auto-load
     * may enter here. Other campaigns or modes keep their native behavior. */
    if (!InterlockedCompareExchange(&g_campaign_active, 0, 0) &&
        !(armed && g_cfg.role != DD_ROLE_OFF && InterlockedCompareExchange(&g_session_ready, 0, 0)))
        return g_raid_start(app);
    quest_return_reset();
    embark_reset();
    departure_begin(app);
    provisions_leave_town();
    g_party_pending_mask = 0;
    if (g_departure_rules && g_rules_reseed) {
        g_rules_reseed(g_rules_object, g_departure_rules); /* The game logs "Rules Reseed seed" in app.log. */
        dd_log("Departure: Rules seed %u set", g_departure_rules);
    }
    uintptr_t result = g_raid_start(app);
    /* One seed makes one map. Another expedition gets a new one. */
    g_departure_seed = g_departure_rules = 0;
    g_departure_have_host = 0;
    return result;
}
/* Guest only: stores the seeds and leaves too. Everything before this order has already been applied. */
#include "dd_prepare_bridge.inc"
#include "dd_embark_bridge.inc"
static int departure_apply(const dd_action *a) {
    depart_view view;
    if (g_cfg.role != DD_ROLE_GUEST) return 0;
    g_departure_seed = a->u.departure.seed;
    g_departure_rules = a->u.departure.rules;
    g_departure_host.quest = a->u.departure.quest;
    memcpy(g_departure_host.heroes, a->u.departure.heroes, sizeof g_departure_host.heroes);
    g_departure_host.supplies = a->u.departure.supplies;
    g_departure_have_host = 1;
    hero_owners_set(a->u.departure.heroes, a->u.departure.owners);
    dd_log("Departure: host embarked; map seed %u and Rules seed %u stored", g_departure_seed, g_departure_rules);
    if (!g_town_depart || !g_depart_view(&view) || !view.town || view.raid) {
        dd_log("Departure: cannot follow from here");
        shop_notice("De host is vertrokken. Klik OK, kies dezelfde missie en party en vertrek nu ook.");
        return 1;
    }
    if (!depart_set_quest(&view, a->u.departure.quest_index, a->u.departure.quest)) return 0;
    if (!g_depart_party()) {
        dd_log("Departure: the party could not be prepared; not following");
        shop_notice("De host is vertrokken, maar deze kopie kon de party niet klaarzetten. Sluit beide spellen en stuur de logs mee.");
        return 0;
    }
    dd_log("Departure: following the host");
    g_embark_request = 0;
    g_embark_leaving = 1;
    g_town_depart();
    return 1;
}
static const hamlet_hook_spec g_departure_hooks[] = {
    {"provision_open", (uintptr_t)hook_prepare_open, &g_prepare_open, 13},
    {"raid_map_generate", (uintptr_t)hook_raid_map, &g_raid_map, 16},
};
/* Installed once before loading an existing campaign, separately from town
 * hooks. It must never be prepared again on top of its own jump. */
static const hamlet_hook_spec g_departure_early_hooks[] = {
    {"darkest_raid_start", (uintptr_t)hook_raid_start, &g_raid_start, 13},
};
static int departure_bind_rules(void) {
    char name[6] = {0};
    uintptr_t rules = image_base() + DD_RULES_RVA, seed = hamlet_address("rng_named_seed");
    if (!seed || !safe_read(rules + 0x10, name, sizeof name) || memcmp(name, "Rules", 6)) return 0;
    g_rules_object = (void *)rules;
    g_rules_reseed = (named_seed_fn)seed;
    return 1;
}
static int departure_start_ready(void) {
    if (g_raid_start_hook_installed) return 1;
    const hamlet_hook_spec *spec = &g_departure_early_hooks[0];
    uintptr_t target = hamlet_address(spec->name);
    if (!target || !departure_bind_rules() ||
        !hook_prepare(&g_raid_start_hook, (void *)target, spec->replacement, spec->length)) return 0;
    g_raid_start = (raid_start_fn)g_raid_start_hook.trampoline;
    if (!hook_apply(&g_raid_start_hook)) {
        VirtualFree(g_raid_start_hook.trampoline, 0, MEM_RELEASE);
        memset(&g_raid_start_hook, 0, sizeof g_raid_start_hook);
        g_raid_start = NULL;
        return 0;
    }
    g_raid_start_hook_installed = 1;
    dd_log("Departure: RaidStart and Rules binding ready before campaign load");
    return 1;
}
/* Finds the import slot through the map generator's own call. Without that exact call: no seed sharing,
 * and everything else keeps working. */
static void departure_bind_rand_s(void) {
    uint8_t call[6];
    int32_t displacement = 0;
    uintptr_t site = image_base() + DD_MAP_SEED_CALL_RVA;
    if (!safe_read(site, call, sizeof call) || call[0] != 0xff || call[1] != 0x15) {
        dd_log("Departure: rand_s call not recognised; seed sharing off");
        return;
    }
    memcpy(&displacement, call + 2, 4);
    uintptr_t slot = site + 6 + (intptr_t)displacement;
    rand_s_fn native = NULL, mine = hook_rand_s;
    DWORD old = 0;
    if (!safe_read(slot, &native, sizeof native) || !native || !VirtualProtect((void *)slot, sizeof mine, PAGE_READWRITE, &old)) {
        dd_log("Departure: rand_s slot not available; seed sharing off");
        return;
    }
    g_rand_s_native = native; /* Set before the slot changes: the game may call it at any time. */
    MemoryBarrier();
    InterlockedExchangePointer((void *volatile *)slot, (void *)mine);
    VirtualProtect((void *)slot, sizeof mine, old, &old);
    /* The stream names itself; another layout means no reseed rather than writing somewhere else. */
    if (!departure_bind_rules()) {
        dd_log("Departure: Rules stream not recognised; its seed is not shared");
    }
}
