/* Included on the game thread after the basic runtime helpers. No peer pointers enter this file. */
typedef uint8_t(__fastcall *hamlet_toggle_fn)(void *, uint32_t);
typedef void(__fastcall *hamlet_name_fn)(void *, void *);
typedef void(__fastcall *hamlet_dismiss_fn)(void *, uint32_t);
typedef void(__fastcall *hamlet_roster_update_fn)(void *, float);
typedef void(__fastcall *hamlet_wallet_fn)(void *, uint32_t, int32_t);
typedef void(__fastcall *hamlet_notify_fn)(void *);
typedef void(__fastcall *hamlet_location_fn)(void *, void *, int32_t);
typedef void(__fastcall *hamlet_party_slot_fn)(void *, void *);
typedef void(__fastcall *hamlet_skills_refresh_fn)(void *, void *);
typedef void(__fastcall *hamlet_swap_fn)(void *, uint32_t, void *);
typedef uintptr_t(__fastcall *hamlet_copy_fn)(void *, const void *);
static hamlet_toggle_fn g_hamlet_toggle[2];
static hamlet_name_fn g_hamlet_name;
static hamlet_dismiss_fn g_hamlet_dismiss;
static hamlet_roster_update_fn g_hamlet_roster_update;
static hamlet_wallet_fn g_hamlet_add, g_hamlet_deduct;
static hamlet_notify_fn g_hamlet_notify;
static hamlet_location_fn g_hamlet_location;
static hamlet_party_slot_fn g_hamlet_party_slot;
static hamlet_skills_refresh_fn g_hamlet_skills_refresh;
static hamlet_swap_fn g_hamlet_swap;
static hamlet_copy_fn g_hamlet_copy;
static hamlet_notify_fn g_hamlet_clear, g_hamlet_destroy, g_hamlet_actor_refresh;
typedef void(__fastcall *hamlet_pickup_fn)(void *, void *, void *, void *);
typedef void(__fastcall *hamlet_finish_fn)(void *, void *);
typedef int(__fastcall *hamlet_drop_fn)(void *, void *, void *);
static hamlet_pickup_fn g_trinket_pickup[2];
static hamlet_finish_fn g_trinket_finish[4];
static hamlet_drop_fn g_trinket_drop[4];
static struct {
    uintptr_t ui, data;
    uint32_t owner, slot, type, id;
    int valid;
} g_trinket_drag;
static hamlet_notify_fn g_hamlet_exchange_original;
static hamlet_notify_fn g_hamlet_bulk[2];
static uint32_t g_hamlet_requests[DD_ACT_COUNT_];
static uint32_t g_skill_requests[2], g_refresh_heroes[DD_HAMLET_HEROES];
static unsigned g_refresh_count;
static struct {
    int active, local, complete, nudged;
    dd_action action;
    uint64_t campaign, until, nudge_at;
    uint32_t ids[DD_HAMLET_HEROES];
    unsigned count;
} g_dismiss;
static int hamlet_dismiss_busy(void) { return g_dismiss.active; }
static int hamlet_dismiss_begin(const dd_action *a, int local);
static uint32_t g_party_intent[4], g_party_requests[DD_QUEUE_CAP];
static unsigned g_party_request_count;
static int g_party_intent_valid;
static unsigned g_party_failure;
/* What the other player's party changes should show when this copy's expedition screen is built for the first time. */
static uint32_t g_party_pending[4];
static unsigned g_party_pending_mask;
static uint64_t g_party_pending_at;
static int hamlet_party_message(const dd_msg *m) {
    return ((g_cfg.role == DD_ROLE_HOST && m->type == DD_MSG_TOWN_REQUEST) ||
            (g_cfg.role == DD_ROLE_GUEST && m->type == DD_MSG_TOWN_APPLY &&
             g_town.applied_seq != UINT32_MAX && m->seq == g_town.applied_seq + 1)) &&
           m->u.town.action.kind == DD_ACT_PARTY && dd_action_valid(&m->u.town.action);
}
static int hamlet_party_busy(void) {
    dd_msg head;
    /* Also guard the event-frame interval before poll has taken the order: SetSlot is model+view binding, and the order binds the slots itself. */
    return queue_peek(&g_incoming, &head) && hamlet_party_message(&head);
}

static int hamlet_open(dd_hamlet *s) {
    uint64_t campaign = 0;
    return safe_read(image_base() + 0x118e138, &campaign, 8) &&
           dd_hamlet_open(s, shop_read, NULL, image_base(), campaign);
}
static int (*g_hamlet_open_model)(dd_hamlet *) = hamlet_open;
static int hamlet_read(const dd_hamlet *s, uint64_t p, void *out, size_t n) {
    return p && p <= UINT64_MAX - n && s->read(s->ctx, p, out, n);
}
/* A hero's upgrade (Blacksmith weapon/armour, Guild skills) changes the hero only through a callback that the building's
 * screen registers for that hero, upgrade and level (59b2f0; 59ad80 calls and removes it after the purchase). The copy
 * whose screen never showed that hero has no callback: in the owner's run of 8 Nov 02:50 the guest bought weapon upgrades,
 * the host's Dismas kept his old weapon (3-6 damage against 4-7 on the guest) and the first fight parted. So after every
 * hero upgrade both copies do what those callbacks do to the model: 5d5670 sets the weapon and armour levels from the
 * purchases (Blacksmith callback 66aa20, then the hero's own vt+88 with 0), 5d3c20 the skill levels (Guild callback
 * 673210). Both only read the purchases; doing them again where the callback already ran changes nothing. Not mirrored:
 * the Guild's selecting a newly learned camping skill when fewer than four are selected. */
static hamlet_notify_fn g_hero_equipment, g_hero_skills;
typedef void(__fastcall *hero_virtual_fn)(void *, uint64_t);
static void hamlet_upgrade_refresh(uint32_t id) {
    dd_hamlet s;
    dd_hamlet_hero hero;
    uint64_t vt = 0, entry = 0;
    int32_t before[2] = {0}, after[2] = {0};
    if (!id || !g_hero_equipment || !g_hero_skills || !g_hamlet_open_model(&s) || dd_hamlet_find(&s, id, &hero) != 1 || !hero.hero) {
        if (id) dd_log("Upgrade: hero %u not refreshed; hero not found", id);
        return;
    }
    hamlet_read(&s, hero.hero + 0x132c, before, sizeof before);
    g_hero_equipment((void *)(uintptr_t)hero.hero);
    g_hero_skills((void *)(uintptr_t)hero.hero);
    if (hamlet_read(&s, hero.hero, &vt, 8) && hamlet_read(&s, vt + 0x88, &entry, 8) && image_range((uintptr_t)entry, 1))
        ((hero_virtual_fn)(uintptr_t)entry)((void *)(uintptr_t)hero.hero, 0);
    hamlet_read(&s, hero.hero + 0x132c, after, sizeof after);
    dd_log("Upgrade: hero %u refreshed, weapon %d->%d, armour %d->%d, skills", id, before[0], after[0], before[1], after[1]);
}
static void hamlet_refresh_schedule(uint32_t id) {
    if (!id) return; /* Estate/building upgrade has no selected hero. */
    for (unsigned i = 0; i < g_refresh_count; i++)
        if (g_refresh_heroes[i] == id) return;
    if (g_refresh_count < DD_HAMLET_HEROES) g_refresh_heroes[g_refresh_count++] = id;
}
static void hamlet_refresh_poll(void) {
    if (!g_refresh_count) return;
    dd_hamlet s;
    dd_hamlet_hero hero;
    uint64_t panel = 0, displayed = 0;
    uint8_t visible = 0;
    /* Rebinding rebuilds skill widgets. Never do this inside their click callback: defer until
     * the next event pump. Only the real, visible normal CharacterDisplay may be rebound. */
    if (g_hamlet_skills_refresh && g_hamlet_open_model(&s) && dd_hamlet_panel(&s, 0xee2ae8, &panel) == 1 &&
        hamlet_read(&s, panel + 0x60, &visible, 1) && visible == 1 &&
        hamlet_read(&s, panel + 0x140, &displayed, 8) && dd_hamlet_hero_pointer(&s, displayed, &hero)) {
        int pending = 0, valid = 1;
        for (unsigned i = 0; i < g_refresh_count; i++)
            if (g_refresh_heroes[i] == hero.id) pending = 1;
        for (unsigned i = 0; i < 2 && pending; i++) {
            uint64_t begin = 0, end = 0;
            uint8_t tail = 0;
            uint64_t at = panel + (i ? 0x518 : 0x500);
            if (!hamlet_read(&s, at, &begin, 8) || !hamlet_read(&s, at + 8, &end, 8) || end < begin ||
                (end - begin) % 0x130 || (end - begin) / 0x130 > DD_HAMLET_SKILLS || (!begin && end) ||
                (end != begin && !hamlet_read(&s, end - 1, &tail, 1)))
                valid = 0;
        }
        if (pending && valid) {
            g_game_apply_depth++;
            g_hamlet_skills_refresh((void *)(uintptr_t)panel, (void *)(uintptr_t)hero.hero);
            g_game_apply_depth--;
            dd_log("Hamlet skills display refreshed hero=%u", hero.id);
        }
    }
    g_refresh_count = 0; /* Hidden/other hero: the game's next normal OnEnter loads the model. */
}
static int hamlet_count(const dd_hamlet *s, unsigned *count) {
    uint64_t begin = 0, end = 0;
    if (!hamlet_read(s, s->campaign + 0x20, &begin, 8) || !hamlet_read(s, s->campaign + 0x28, &end, 8) ||
        end < begin || (end - begin) % 8 || (end - begin) / 8 > DD_HAMLET_HEROES)
        return 0;
    *count = (unsigned)((end - begin) / 8);
    return 1;
}
static int hamlet_local_active(void) {
    return g_cfg.role != DD_ROLE_OFF && InterlockedCompareExchange(&g_campaign_active, 0, 0);
}
static int hamlet_input_ready(void) {
    if (runtime_shared_blocked()) return 0;
    if (InterlockedCompareExchange(&g_session_ready, 0, 0) && !g_town.needs_resync) return 1;
    shop_notice("Wait until both games are connected. After a synchronization error, close both games and "
                "restart the co-op session.");
    return 0;
}
static int hamlet_fail(const char *why, int changed) {
    dd_log("Hamlet command refused: %s%s", why, changed ? "; rejoin required" : "");
    if (changed) g_town.needs_resync = 1;
    return 0;
}
static int hamlet_submit(dd_action *a) {
    if (!dd_action_valid(a) || !hamlet_input_ready()) return 0;
    if (g_cfg.role == DD_ROLE_GUEST) {
        if (a->kind == DD_ACT_DISMISS && g_hamlet_requests[DD_ACT_DISMISS]) return 0;
        uint32_t request = dd_town_guest_local(&g_town, a);
        g_hamlet_requests[a->kind] = request;
        if (a->kind == DD_ACT_SKILL) g_skill_requests[a->u.skill.camping] = request;
        if (request && a->kind == DD_ACT_PARTY && g_party_request_count < DD_QUEUE_CAP)
            g_party_requests[g_party_request_count++] = request;
        dd_log("guest sent Hamlet command before mutation kind=%u request=%u", a->kind, request);
        return request != 0;
    }
    int ok = a->kind == DD_ACT_DISMISS ? hamlet_dismiss_begin(a, 1) : dd_town_host_local(&g_town, a);
    if (!ok)
        shop_notice(g_town.needs_resync ? "The change could not be fully verified. Close both "
                                          "games and restart the co-op session. Send the logs."
                                        : "The change was refused: the state has changed, or "
                                          "the game does not currently allow this action.");
    return ok;
}
static void hamlet_request_done(uint32_t request, int rejected) {
    for (unsigned i = 0; i < 2; i++)
        if (request && g_skill_requests[i] == request) g_skill_requests[i] = 0;
    for (unsigned i = 0; i < g_party_request_count; i++)
        if (g_party_requests[i] == request) {
            memmove(g_party_requests + i, g_party_requests + i + 1,
                    (g_party_request_count - i - 1) * sizeof *g_party_requests);
            g_party_request_count--;
            if (rejected || !g_party_request_count) g_party_intent_valid = 0;
            break;
        }
    for (unsigned i = DD_ACT_DISMISS; i < DD_ACT_COUNT_; i++)
        if (request && g_hamlet_requests[i] == request) {
            g_hamlet_requests[i] = 0;
            if (rejected)
                shop_notice("The host refused this change. Check the current state and "
                            "try the action again.");
        }
}
static int hamlet_skill_apply(const dd_hamlet *s, const dd_action *a) {
    dd_hamlet_hero hero;
    int active = 0;
    uint32_t before = 0, after = 0;
    if (!g_hamlet_toggle[a->u.skill.camping] || dd_hamlet_find(s, a->u.skill.hero_id, &hero) != 1 ||
        !dd_hamlet_skill(s, &hero, a->u.skill.camping, a->u.skill.index, &active) ||
        active != a->u.skill.before || !dd_hamlet_skill_mask(s, &hero, a->u.skill.camping, &before) ||
        before != a->u.skill.selected_mask)
        return hamlet_fail("skill not learned, stale selection or invalid hero", 0);
    g_hamlet_toggle[a->u.skill.camping]((void *)(uintptr_t)hero.hero, a->u.skill.index);
    if (!dd_hamlet_skill_mask(s, &hero, a->u.skill.camping, &after))
        return hamlet_fail("skill post-state unreadable", 1);
    dd_log("Hamlet skill result hero=%u camping=%u index=%u mask=%08x->%08x", hero.id, a->u.skill.camping,
           a->u.skill.index, before, after);
    if (after == (before ^ (1u << a->u.skill.index))) {
        hamlet_refresh_schedule(hero.id);
        return 1;
    }
    return hamlet_fail("game declined or partially changed skill selection", after != before);
}
static int hamlet_name_apply(const dd_hamlet *s, const dd_action *a) {
    dd_hamlet_hero hero;
    char before[64], after[64];
    if (!g_hamlet_name || dd_hamlet_find(s, a->u.rename.hero_id, &hero) != 1 ||
        !dd_hamlet_name(s, &hero, before) || strcmp(before, a->u.rename.before))
        return hamlet_fail("stale hero name", 0);
    /* The pinned normal-mode function reads ONLY this bounded input string at +0x14f. It refreshes
     * genuine registered panels itself; no vtable, UI pointer or Arena data is reconstructed. */
    uint8_t input[0x14f + 64] = {0};
    memcpy(input + 0x14f, a->u.rename.after, 64);
    g_hamlet_name(input, (void *)(uintptr_t)hero.hero);
    if (!dd_hamlet_name(s, &hero, after)) return hamlet_fail("name post-state unreadable", 1);
    dd_log("Hamlet name result hero=%u bytes=%u accepted=%d", hero.id, (unsigned)strlen(a->u.rename.after),
           !strcmp(after, a->u.rename.after));
    if (!strcmp(after, a->u.rename.after)) return 1;
    return hamlet_fail("name update incomplete", strcmp(before, after) != 0);
}
static int hamlet_exchange_apply(const dd_hamlet *s, const dd_action *a) {
    uint32_t from = (uint32_t)a->u.exchange.src_type, to = (uint32_t)a->u.exchange.dst_type;
    int32_t debit = a->u.exchange.src_amount, credit = a->u.exchange.dst_amount, old_from = 0, old_to = 0,
            new_from = 0, new_to = 0;
    if (!g_hamlet_add || !g_hamlet_deduct || !g_hamlet_notify ||
        !dd_hamlet_exchange_valid(from, debit, to, credit) || !dd_hamlet_currency(s, from, &old_from) ||
        !dd_hamlet_currency(s, to, &old_to) || debit > old_from || credit > INT32_MAX - old_to)
        return hamlet_fail("unsupported exchange rate, insufficient funds or overflow", 0);
    void *wallet = (void *)(uintptr_t)(s->campaign + 0xe50);
    g_hamlet_deduct(wallet, from, debit);
    g_hamlet_add(wallet, to, credit);
    g_hamlet_notify(wallet);
    if (!dd_hamlet_currency(s, from, &new_from) || !dd_hamlet_currency(s, to, &new_to))
        return hamlet_fail("exchange post-state unreadable", 1);
    dd_log("Hamlet exchange result from=%08x %d->%d to=%08x %d->%d", from, old_from, new_from, to, old_to,
           new_to);
    if (new_from == old_from - debit && new_to == old_to + credit) return 1;
    return hamlet_fail("exchange incomplete", old_from != new_from || old_to != new_to);
}
static int hamlet_roster_ids(const dd_hamlet *s, uint32_t ids[DD_HAMLET_HEROES], unsigned *count) {
    uint64_t begin = 0;
    if (!hamlet_count(s, count) || !hamlet_read(s, s->campaign + 0x20, &begin, 8)) return 0;
    for (unsigned i = 0; i < *count; i++) {
        uint64_t entry = 0;
        dd_hamlet_hero hero;
        if (!hamlet_read(s, begin + 8 * i, &entry, 8) || !hamlet_read(s, entry, &ids[i], 4) ||
            dd_hamlet_find(s, ids[i], &hero) != 1 || hero.entry != entry)
            return 0;
    }
    return 1;
}
static const char *hamlet_dismiss_problem(const dd_hamlet *s, uint32_t id, dd_hamlet_hero *hero) {
    uint32_t party[4];
    if (dd_hamlet_find(s, id, hero) != 1)
        return "DDCoop could not verify this hero in the roster. Reopen the roster.";
    if (hero->location > 1)
        return "This hero is in a building or unavailable and cannot be dismissed now.";
    if (!dd_hamlet_party(s, party))
        return "DDCoop could not verify the party. Open the expedition screen and send the logs "
               "if this persists.";
    for (unsigned i = 0; i < 4; i++)
        if (party[i] == id)
            return "Remove this hero from the expedition party first, then dismiss them.";
    if (hero->location != 0) /* 1 = still in a party slot, whatever the remembered vector says. */
        return "Remove this hero from the expedition party first, then dismiss them.";
    return NULL;
}
static int hamlet_dismiss_begin(const dd_action *a, int local) {
    dd_hamlet s;
    dd_hamlet_hero hero;
    uint64_t panel = 0;
    uint32_t ids[DD_HAMLET_HEROES];
    unsigned count = 0;
    if (g_dismiss.active || !dd_action_valid(a) || a->kind != DD_ACT_DISMISS || !g_hamlet_dismiss ||
        !g_hamlet_open_model(&s) || dd_hamlet_panel(&s, 0xec00a0, &panel) != 1 ||
        !hamlet_roster_ids(&s, ids, &count))
        return hamlet_fail("dismiss requires a spare roster hero and genuine normal panel", 0);
    const char *problem = hamlet_dismiss_problem(&s, a->u.hero.hero_id, &hero);
    if (problem) return hamlet_fail(problem, 0);
    g_dismiss.active = 1;
    g_dismiss.local = local;
    g_dismiss.complete = 0;
    g_dismiss.action = *a;
    g_dismiss.campaign = s.campaign;
    g_dismiss.until = GetTickCount64() + 5000;
    g_dismiss.nudge_at = GetTickCount64() + 400;
    g_dismiss.count = count;
    memcpy(g_dismiss.ids, ids, count * sizeof *ids);
    dd_log("Hamlet dismiss pending hero=%u roster=%u role=%s", hero.id, count, dd_role_name(g_cfg.role));
    /* The normal callback fires HeroDismissedEvent and marks the row for a later update.
     * Do not report refusal, broadcast an APPLY, or call it again while that update is pending. */
    g_game_apply_depth++;
    g_hamlet_dismiss((void *)(uintptr_t)panel, hero.id);
    g_game_apply_depth--;
    return 1;
}
static int hamlet_dismiss_poll(void) {
    if (!g_dismiss.active || g_dismiss.complete) return 0;
    dd_hamlet s;
    unsigned count = 0;
    uint32_t ids[DD_HAMLET_HEROES];
    int valid =
        g_hamlet_open_model(&s) && s.campaign == g_dismiss.campaign && hamlet_roster_ids(&s, ids, &count);
    int present = 0;
    if (valid) {
        for (unsigned i = 0; i < count; i++) {
            int known = 0;
            for (unsigned j = 0; j < g_dismiss.count; j++)
                if (ids[i] == g_dismiss.ids[j]) known = 1;
            if (!known) valid = 0;
            if (ids[i] == g_dismiss.action.u.hero.hero_id) present = 1;
        }
    }
    if (valid && !present && count + 1 == g_dismiss.count) {
        g_dismiss.complete = 1;
        dd_log("Hamlet dismiss completed hero=%u roster=%u->%u", g_dismiss.action.u.hero.hero_id,
               g_dismiss.count, count);
        if (g_dismiss.local) {
            dd_town_host_observed(&g_town, &g_dismiss.action);
            log_town_action("host committed completed dismissal", &g_dismiss.action, 0, g_town.applied_seq);
            memset(&g_dismiss, 0, sizeof g_dismiss);
        }
        return 0;
    }
    if (valid && present && count == g_dismiss.count && GetTickCount64() < g_dismiss.until) {
        /* The hero leaves in the roster list's own update (6b4e30, its row marked by 6b46b0). A copy on a screen without that list does not run it, and the session used to
         * stop after 5 s. Run that one update now, with no time passing, as the game would on the next frame with the list showing. */
        uint64_t panel = 0;
        if (!g_dismiss.nudged && GetTickCount64() >= g_dismiss.nudge_at && g_hamlet_roster_update && dd_hamlet_panel(&s, 0xec00a0, &panel) == 1) {
            g_dismiss.nudged = 1;
            dd_log("Hamlet dismiss: the roster list did not update by itself, updating it once");
            g_game_apply_depth++;
            g_hamlet_roster_update((void *)(uintptr_t)panel, 0.0f);
            g_game_apply_depth--;
        }
        return 1;
    }
    hamlet_fail(valid && present && count == g_dismiss.count
                    ? "dismiss timed out; queued native event may still run"
                    : "dismiss changed unexpected roster/campaign state",
                1);
    shop_notice("Dismissal could not be fully verified. Shared actions have stopped. "
                "Close both games and start a new co-op session. Send the logs.");
    return 1;
}
static int hamlet_dismiss_gate(const dd_msg *m) {
    if (m->u.town.action.kind != DD_ACT_DISMISS || !dd_action_valid(&m->u.town.action)) return 0;
    if (!((g_cfg.role == DD_ROLE_HOST && m->type == DD_MSG_TOWN_REQUEST) ||
          (g_cfg.role == DD_ROLE_GUEST && m->type == DD_MSG_TOWN_APPLY && m->seq == g_town.applied_seq + 1)))
        return 0; /* In particular, never start a native dismissal for a gap/replayed guest APPLY. */
    if (!g_dismiss.active && !hamlet_dismiss_begin(&m->u.town.action, 0)) return 0;
    return !g_dismiss.complete;
}
static int hamlet_dismiss_apply(const dd_hamlet *s, const dd_action *a) {
    if (!g_dismiss.active || !g_dismiss.complete || g_dismiss.local || g_dismiss.campaign != s->campaign ||
        g_dismiss.action.u.hero.hero_id != a->u.hero.hero_id)
        return hamlet_fail("dismiss not completed through ordered runtime gate", 0);
    memset(&g_dismiss, 0, sizeof g_dismiss);
    return 1; /* Native deletion was verified; commit its original request/sequence exactly once. */
}
/* The team's name ("The Hellhounds"): the roster screen (vtable ebe530, town+3478) looks the party's classes up in the name list (697fa0) in its next update (699190) when its byte +b0 is set.
 * A real drop sets it through the character_add event (7e36ae, listener 6a0480). An order does not raise that event, so without this only the copy that dragged showed the name. */
static void hamlet_party_name_refresh(const dd_hamlet *s) {
    uintptr_t base = (uintptr_t)s->image;
    uint64_t town = 0, vtable = 0;
    uint8_t flag = 1;
    if (!safe_read(base + 0x118e360, &town, 8) || !town || !safe_read((uintptr_t)town + 0x3478, &vtable, 8) || vtable != base + 0xebe530) return;
    memcpy((void *)(uintptr_t)(town + 0x3478 + 0xb0), &flag, 1);
}
/* The party is the campaign's vector (+0, what native SetSlot 7e3820 writes: the id, or FFFFFFFF for none) and the roster locations. The slots only show it.
 * So a copy that never opened the expedition screen (no slots yet) takes the change in the model; hamlet_party_show_poll binds the slots once they exist. */
static int hamlet_party_write(const dd_hamlet *s, const uint32_t ids[4]) {
    uint64_t begin = 0, end = 0;
    if (!hamlet_read(s, s->campaign, &begin, 8) || !hamlet_read(s, s->campaign + 8, &end, 8) || end < begin || end - begin != 16) return 0;
    for (unsigned i = 0; i < 4; i++) {
        uint32_t id = ids[i] ? ids[i] : UINT32_MAX;
        memcpy((void *)(uintptr_t)(begin + 4 * i), &id, 4);
    }
    return 1;
}
static int hamlet_party_apply(const dd_hamlet *s, const dd_action *a) {
    uint32_t before[4] = {0}, after[4];
    uint64_t slots[4];
    dd_hamlet_hero heroes[4];
    int old_ok = dd_hamlet_party(s, before);
    int matches = old_ok && !memcmp(before, a->u.party.before, sizeof before);
    int available = dd_hamlet_party_valid(s, a->u.party.after);
    int slot_status = dd_hamlet_party_slots_status(s, slots);
    if (!g_hamlet_party_slot || !g_hamlet_location || !matches || !available || slot_status < 0) {
        dd_log(
            "party preflight old=%d matches=%d available=%d slots=%d actual=%u,%u,%u,%u expected=%u,%u,%u,%u",
            old_ok, matches, available, slot_status, before[0], before[1], before[2], before[3],
            a->u.party.before[0], a->u.party.before[1], a->u.party.before[2], a->u.party.before[3]);
        return hamlet_fail("stale party, unavailable heroes or genuine party slots", 0);
    }
    memset(heroes, 0, sizeof heroes);
    for (unsigned i = 0; i < 4; i++)
        if (a->u.party.after[i] && dd_hamlet_find(s, a->u.party.after[i], &heroes[i]) != 1) return 0;
    if (a->u.party.show) {
        unsigned rank = a->u.party.show - 1u;
        uint8_t refresh = 1;
        if (!heroes[rank].id) return 0;
        if (heroes[rank].location != 1) g_hamlet_location((void *)(uintptr_t)s->campaign, (void *)(uintptr_t)heroes[rank].entry, 1);
        if (slot_status == 1) {
            g_hamlet_party_slot((void *)(uintptr_t)slots[rank], (void *)(uintptr_t)heroes[rank].hero);
            memcpy((void *)(uintptr_t)(slots[rank] + 0x0c), &refresh, 1);
            hamlet_party_name_refresh(s);
            g_party_pending_mask &= ~(1u << rank);
        } else {
            g_party_pending[rank] = a->u.party.after[rank];
            g_party_pending_mask |= 1u << rank;
        }
        dd_log("Hamlet party slot %u shows hero %u (slots=%d)", rank, a->u.party.after[rank], slot_status);
        return 1;
    }
    if (slot_status == 1)
        for (unsigned i = 0; i < 4; i++)
            g_hamlet_party_slot((void *)(uintptr_t)slots[i], NULL);
    for (unsigned i = 0; i < 4; i++)
        if (before[i]) {
            int retained = 0;
            for (unsigned j = 0; j < 4; j++)
                if (before[i] == a->u.party.after[j]) retained = 1;
            if (!retained) {
                dd_hamlet_hero hero;
                if (dd_hamlet_find(s, before[i], &hero) != 1)
                    return hamlet_fail("party hero disappeared during update", 1);
                /* A remembered ID may now belong to a building. Clearing that old slot must
                 * not cancel treatment, free the hero or change the building assignment.
                 * Location 1 = in the party, 0 = free: the raid's hero list is exactly the roster
                 * entries at location 1 (predicate 590260, builder 5575c0), as in the native drop 7e3540. */
                if (hero.location == 1)
                    g_hamlet_location((void *)(uintptr_t)s->campaign, (void *)(uintptr_t)hero.entry, 0);
            }
        }
    for (unsigned i = 0; i < 4; i++)
        if (heroes[i].id)
            g_hamlet_location((void *)(uintptr_t)s->campaign, (void *)(uintptr_t)heroes[i].entry, 1);
    if (slot_status == 1) {
        if (!dd_hamlet_party_slots(s, slots)) return hamlet_fail("party UI changed during location update", 1);
        for (unsigned i = 0; i < 4; i++) {
            uint8_t refresh = 1;
            g_hamlet_party_slot((void *)(uintptr_t)slots[i], (void *)(uintptr_t)heroes[i].hero);
            /* An empty slot does not mark itself for redrawing; the native drop does that (7e36c1). Without it a removed hero stayed
             * visible on the other copy until the next change (owner's run on one PC). */
            memcpy((void *)(uintptr_t)(slots[i] + 0x0c), &refresh, 1);
        }
        hamlet_party_name_refresh(s);
        g_party_pending_mask = 0;
    } else {
        if (!hamlet_party_write(s, a->u.party.after)) return hamlet_fail("party vector unavailable", 1);
        memcpy(g_party_pending, a->u.party.after, sizeof g_party_pending);
        g_party_pending_mask = 0xf;
    }
    if (!dd_hamlet_party(s, after)) return hamlet_fail("party post-state unreadable", 1);
    dd_log("Hamlet party result ids=%u,%u,%u,%u slots=%d", after[0], after[1], after[2], after[3], slot_status);
    return !memcmp(after, a->u.party.after, sizeof after) ? 1 : hamlet_fail("party update incomplete", 1);
}
/* The slots of a screen opened after the model changed. Only what an order said: the screen opens empty while the vector remembers an older party, and that stays so. */
static void hamlet_party_show_poll(void) {
    dd_hamlet s;
    uint64_t slots[4], now;
    int changed = 0;
    if (!g_party_pending_mask) return;
    now = GetTickCount64();
    if (now < g_party_pending_at) return;
    g_party_pending_at = now + 200;
    if (!g_hamlet_party_slot || !g_hamlet_open_model(&s) || dd_hamlet_party_slots_status(&s, slots) != 1) return;
    for (unsigned i = 0; i < 4; i++) {
        dd_hamlet_hero hero;
        uint64_t shown = 0;
        uint8_t refresh = 1;
        if (!(g_party_pending_mask & (1u << i))) continue;
        memset(&hero, 0, sizeof hero);
        if (g_party_pending[i] && dd_hamlet_find(&s, g_party_pending[i], &hero) != 1) continue;
        if (!hamlet_read(&s, slots[i] + 0x28, &shown, 8) || shown == hero.hero) continue;
        g_game_apply_depth++;
        g_hamlet_party_slot((void *)(uintptr_t)slots[i], (void *)(uintptr_t)hero.hero);
        g_game_apply_depth--;
        memcpy((void *)(uintptr_t)(slots[i] + 0x0c), &refresh, 1);
        changed = 1;
    }
    g_party_pending_mask = 0;
    if (changed) {
        hamlet_party_name_refresh(&s);
        dd_log("Hamlet party slots filled now that the expedition screen exists");
    }
}
static int hamlet_move_apply(const dd_hamlet *s, const dd_action *a) {
    uint64_t inv[2] = {0}, items[2] = {0};
    unsigned sizes[2] = {0};
    uint32_t type[2] = {0}, id[2] = {0}, q[2] = {0}, normal = 0;
    uint32_t owners[2] = {a->u.move.src_hero, a->u.move.dst_hero};
    unsigned slots[2] = {a->u.move.src_slot, a->u.move.dst_slot};
    if (!g_hamlet_copy || !g_hamlet_clear || !g_hamlet_destroy || !g_hamlet_swap || !g_hamlet_actor_refresh ||
        !hamlet_read(s, s->image + 0x2a6f868, &normal, 4))
        return hamlet_fail("trinket native operations unavailable", 0);
    for (unsigned i = 0; i < 2; i++) {
        dd_hamlet_hero hero;
        if ((DD_TRINKET_IS_HERO(owners[i]) && (dd_hamlet_find(s, owners[i], &hero) != 1 || hero.location > 1)) ||
            !dd_hamlet_inventory(s, owners[i], &inv[i], &sizes[i]) ||
            !dd_hamlet_item(s, inv[i], slots[i], &type[i], &id[i], &q[i]) || (q[i] && type[i] != normal) ||
            !hamlet_read(s, inv[i] + 8, &items[i], 8))
            return hamlet_fail("trinket inventory or endpoint unavailable", 0);
        items[i] += (uint64_t)slots[i] * 0x5a8;
    }
    if (q[0] != 1 || type[0] != a->u.move.src_type || id[0] != a->u.move.src_id ||
        type[1] != a->u.move.dst_type || id[1] != a->u.move.dst_id ||
        !dd_hamlet_can_equip(s, owners[1], slots[1], id[0], owners[0] == owners[1] ? slots[0] : UINT32_MAX) ||
        !dd_hamlet_can_equip(s, owners[0], slots[0], id[1], owners[0] == owners[1] ? slots[1] : UINT32_MAX))
        return hamlet_fail("stale trinket endpoints, duplicate equipment or class restriction", 0);
    /* Item owns C++ containers. Construct, clear, swap and destroy using the pinned game functions;
     * never memcpy a live Item or use a zero-initialized Item as a native argument. */
#ifdef _MSC_VER
    __declspec(align(16))
#else
    _Alignas(16)
#endif
    uint8_t temporary[0x5a8] = {0};
    g_hamlet_copy(temporary, (const void *)(uintptr_t)items[0]);
    g_hamlet_clear(temporary);
    g_hamlet_swap((void *)(uintptr_t)inv[0], slots[0], temporary);
    g_hamlet_swap((void *)(uintptr_t)inv[1], slots[1], temporary);
    g_hamlet_swap((void *)(uintptr_t)inv[0], slots[0], temporary);
    g_hamlet_destroy(temporary);
    for (unsigned i = 0; i < 2; i++)
        if (DD_TRINKET_IS_HERO(owners[i]) && (i == 0 || owners[i] != owners[0])) {
            dd_hamlet_hero hero;
            if (dd_hamlet_find(s, owners[i], &hero) != 1) return hamlet_fail("equipped hero disappeared", 1);
            g_hamlet_actor_refresh((void *)(uintptr_t)hero.hero);
        }
    uint32_t result_type[2], result_id[2], result_q[2];
    for (unsigned i = 0; i < 2; i++)
        if (!dd_hamlet_item(s, inv[i], slots[i], &result_type[i], &result_id[i], &result_q[i]) ||
            result_type[i] != type[1 - i] || result_id[i] != id[1 - i] || result_q[i] != q[1 - i])
            return hamlet_fail("trinket move incomplete", 1);
    dd_log("Hamlet trinket move result source=%u:%u destination=%u:%u ids=%08x/%08x", owners[0], slots[0],
           owners[1], slots[1], id[0], id[1]);
    return 1;
}
static int hamlet_apply(const dd_action *a) {
    dd_hamlet s;
    if (!g_hamlet_open_model(&s)) return hamlet_fail("normal campaign unavailable", 0);
    switch (a->kind) {
    case DD_ACT_SKILL:
        return hamlet_skill_apply(&s, a);
    case DD_ACT_RENAME:
        return hamlet_name_apply(&s, a);
    case DD_ACT_HEIRLOOM_EXCHANGE:
        return hamlet_exchange_apply(&s, a);
    case DD_ACT_DISMISS:
        return hamlet_dismiss_apply(&s, a);
    case DD_ACT_PARTY:
        return hamlet_party_apply(&s, a);
    case DD_ACT_TRINKET_MOVE: {
        int moved = hamlet_move_apply(&s, a);
        /* From the raid's loot or bag, on the guest: the copies differ, but only by that item, and the expedition can go on. A hero's trinket in town still stops: that is the campaign. */
        if (!moved && g_cfg.role == DD_ROLE_GUEST && !g_town.needs_resync && (a->u.move.src_hero >= DD_TRINKET_LOOT || a->u.move.dst_hero >= DD_TRINKET_LOOT)) {
            dd_log("Hamlet: the host's trinket from raid inventory %u slot %u could not be done here", a->u.move.src_hero, a->u.move.src_slot);
            shop_notice("A loot trinket could not be applied here, so equipment may differ. You may continue; send the logs afterwards.");
            return 1;
        }
        return moved;
    }
    default:
        return 0;
    }
}
static void hamlet_reset(void) {
    g_party_pending_mask = 0;
    if (g_dismiss.active) {
        g_town.needs_resync = 1;
        dd_log("unfinished dismissal invalidated by campaign reset; rejoin required");
    }
    memset(&g_dismiss, 0, sizeof g_dismiss);
    memset(g_hamlet_requests, 0, sizeof g_hamlet_requests);
    memset(g_skill_requests, 0, sizeof g_skill_requests);
    g_refresh_count = 0;
    g_party_request_count = 0;
    g_party_intent_valid = 0;
    g_party_failure = 0;
    memset(&g_trinket_drag, 0, sizeof g_trinket_drag);
}

static int hamlet_skill_ui(uintptr_t caller) {
    uintptr_t base = image_base();
    return caller == base + 0x8045f1 || caller == base + 0x8040c8;
}
static int (*g_hamlet_skill_input)(uintptr_t) = hamlet_skill_ui;
static int hamlet_party_ui(uintptr_t caller) {
    uintptr_t rva = caller - image_base();
    return (rva >= 0x7e3500 && rva < 0x7e3f60) || (rva >= 0x6b3ee0 && rva < 0x6b44b0);
}
static int (*g_hamlet_party_input)(uintptr_t) = hamlet_party_ui;
static uint8_t hamlet_skill_input(void *pointer, uint32_t index, unsigned camping, uintptr_t caller) {
    if (!g_hamlet_toggle[camping]) return 0;
    if (g_game_apply_depth || !hamlet_local_active() || !g_hamlet_skill_input(caller))
        return g_hamlet_toggle[camping](pointer, index);
    if (g_cfg.role == DD_ROLE_GUEST && g_skill_requests[camping]) return 0;
    dd_hamlet s;
    dd_hamlet_hero hero;
    int active = 0;
    uint32_t mask = 0;
    if (!g_hamlet_open_model(&s) || !dd_hamlet_hero_pointer(&s, (uintptr_t)pointer, &hero) ||
        !dd_hamlet_skill(&s, &hero, camping, index, &active) ||
        !dd_hamlet_skill_mask(&s, &hero, camping, &mask))
        return 0;
    dd_action a = {0};
    a.kind = DD_ACT_SKILL;
    a.u.skill.hero_id = hero.id;
    a.u.skill.index = (uint8_t)index;
    a.u.skill.camping = (uint8_t)camping;
    a.u.skill.before = (uint8_t)active;
    a.u.skill.after = (uint8_t)!active;
    a.u.skill.selected_mask = mask;
    int accepted = hamlet_submit(&a);
    return (uint8_t)(g_cfg.role == DD_ROLE_HOST && accepted);
}
static uint8_t __fastcall hook_hamlet_combat(void *hero, uint32_t index) {
#ifdef _MSC_VER
    uintptr_t caller = (uintptr_t)_ReturnAddress();
#else
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
#endif
    return hamlet_skill_input(hero, index, 0, caller);
}
static uint8_t __fastcall hook_hamlet_camping(void *hero, uint32_t index) {
#ifdef _MSC_VER
    uintptr_t caller = (uintptr_t)_ReturnAddress();
#else
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
#endif
    return hamlet_skill_input(hero, index, 1, caller);
}
static void __fastcall hook_hamlet_name(void *input, void *pointer) {
    if (!g_hamlet_name) return;
    if (g_game_apply_depth || !hamlet_local_active()) {
        g_hamlet_name(input, pointer);
        return;
    }
    dd_hamlet s;
    dd_hamlet_hero hero;
    dd_action a = {0};
    a.kind = DD_ACT_RENAME;
    if (!g_hamlet_open_model(&s) || !dd_hamlet_hero_pointer(&s, (uintptr_t)pointer, &hero) ||
        !dd_hamlet_name(&s, &hero, a.u.rename.before) ||
        !safe_read((uintptr_t)input + 0x14f, a.u.rename.after, 64) ||
        !dd_hamlet_name_valid(a.u.rename.after)) {
        dd_log("Hamlet rename input refused: live hero/name/input unavailable");
        return;
    }
    if (!strcmp(a.u.rename.before, a.u.rename.after)) return;
    a.u.rename.hero_id = hero.id;
    hamlet_submit(&a);
}
static void __fastcall hook_hamlet_exchange(void *capture) {
    if (!g_hamlet_exchange_original) return;
    if (g_game_apply_depth || !hamlet_local_active()) {
        g_hamlet_exchange_original(capture);
        return;
    }
    dd_hamlet s;
    uint64_t source = 0, wallet = 0;
    dd_action a = {0};
    a.kind = DD_ACT_HEIRLOOM_EXCHANGE;
    if (!g_hamlet_open_model(&s) || !safe_read((uintptr_t)capture, &source, 8) || !source ||
        !safe_read((uintptr_t)capture + 8, &wallet, 8) || wallet != s.campaign + 0xe50 ||
        !safe_read((uintptr_t)source + 0x84, &a.u.exchange.src_type, 4) ||
        !safe_read((uintptr_t)capture + 0x10, &a.u.exchange.dst_type, 4) ||
        !safe_read((uintptr_t)capture + 0x14, &a.u.exchange.src_amount, 4) ||
        !safe_read((uintptr_t)capture + 0x18, &a.u.exchange.dst_amount, 4)) {
        dd_log("Hamlet exchange input refused: normal wallet/capture unavailable");
        return;
    }
    hamlet_submit(&a);
}
static void __fastcall hook_hamlet_dismiss(void *panel, uint32_t id) {
    if (!g_hamlet_dismiss) return;
    if (g_game_apply_depth || !hamlet_local_active()) {
        g_hamlet_dismiss(panel, id);
        return;
    }
    dd_hamlet s;
    uint64_t canonical = 0;
    if (!g_hamlet_open_model(&s) || dd_hamlet_panel(&s, 0xec00a0, &canonical) != 1 ||
        canonical != (uintptr_t)panel) {
        shop_notice("DDCoop could not verify the roster. Reopen it and send the logs "
                    "if this persists.");
        return;
    }
    dd_hamlet_hero hero;
    const char *problem = hamlet_dismiss_problem(&s, id, &hero);
    if (problem) {
        dd_log("Hamlet dismiss input refused hero=%u: %s", id, problem);
        shop_notice(problem);
        return;
    }
    dd_action a = {0};
    a.kind = DD_ACT_DISMISS;
    a.u.hero.hero_id = id;
    hamlet_submit(&a);
}
static void __fastcall hook_hamlet_party_slot(void *slot, void *pointer) {
#ifdef _MSC_VER
    uintptr_t caller = (uintptr_t)_ReturnAddress();
#else
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
#endif
    if (!g_hamlet_party_slot) return;
    if (g_game_apply_depth || !hamlet_local_active()) {
        g_hamlet_party_slot(slot, pointer);
        return;
    }
    if (hamlet_party_busy()) return; /* Opening the waiting panel must not create a second request. */
    dd_hamlet s;
    uint32_t current[4] = {0}, rank = 0, id = 0;
    uint64_t slots[4];
    dd_hamlet_hero hero = {0};
    int model_ok = g_hamlet_open_model(&s);
    int party_ok = model_ok && dd_hamlet_party(&s, current);
    int slots_ok = model_ok && dd_hamlet_party_slots(&s, slots);
    int slot_ok = slots_ok && safe_read((uintptr_t)slot + 0x230, &rank, 4) && rank < 4 &&
                  slots[rank] == (uintptr_t)slot;
    int hero_ok = !pointer || (model_ok && dd_hamlet_hero_pointer(&s, (uintptr_t)pointer, &hero));
    if (!party_ok || !slot_ok || !hero_ok) {
        unsigned failure = 1u + (unsigned)model_ok + 2u * (unsigned)party_ok + 4u * (unsigned)slots_ok +
                           8u * (unsigned)slot_ok + 16u * (unsigned)hero_ok;
        if (failure != g_party_failure) {
            dd_log("Hamlet party input unavailable model=%d old_party=%d slots=%d slot=%d hero=%d rank=%u",
                   model_ok, party_ok, slots_ok, slot_ok, hero_ok, rank);
            g_party_failure = failure;
        }
        if (!InterlockedCompareExchange(&g_session_ready, 0, 0))
            g_hamlet_party_slot(slot, pointer);
        else
            shop_notice(
                "DDCoop could not verify the party slots. Open the expedition screen and try again.");
        return;
    }
    g_party_failure = 0;
    if (pointer) id = hero.id;
    int party_input = g_hamlet_party_input(caller);
    int restoring_intent =
        g_cfg.role == DD_ROLE_GUEST && g_party_intent_valid && g_party_intent[rank] != id && party_input;
    if (current[rank] == id && !restoring_intent) {
        /* The screen opens empty while the party is remembered (slot+28 holds what is shown). Dragging a hero back to the rank it is remembered at changed nothing in
         * memory, so nothing was sent, and the other copy stayed empty until a real change (owner's runs: "after a few seconds and changes"). */
        uint64_t shown = 0;
        if (id && party_input && safe_read((uintptr_t)slot + 0x28, &shown, 8) && shown != (uintptr_t)pointer && InterlockedCompareExchange(&g_session_ready, 0, 0) &&
            !runtime_shared_blocked()) {
            dd_action show = {0};
            show.kind = DD_ACT_PARTY;
            memcpy(show.u.party.before, current, sizeof current);
            memcpy(show.u.party.after, current, sizeof current);
            show.u.party.show = (uint8_t)(rank + 1);
            hamlet_submit(&show);
            return;
        }
        g_hamlet_party_slot(slot, pointer);
        return;
    } /* UI refresh */
    if (runtime_shared_blocked()) return;
    if (!InterlockedCompareExchange(&g_session_ready, 0, 0)) {
        if (party_input && runtime_waiting_for_peer()) return; /* The owner's first test: dragged in, never shared. */
        g_hamlet_party_slot(slot, pointer);
        return;
    }
    if (g_cfg.role == DD_ROLE_GUEST && g_party_request_count >= DD_QUEUE_CAP) {
        shop_notice("Wait until previous party changes are confirmed.");
        return;
    }
    dd_action a = {0};
    a.kind = DD_ACT_PARTY;
    memcpy(a.u.party.before, g_cfg.role == DD_ROLE_GUEST && g_party_intent_valid ? g_party_intent : current,
           sizeof current);
    memcpy(a.u.party.after, a.u.party.before, sizeof current);
    if (id)
        for (unsigned i = 0; i < 4; i++)
            if (a.u.party.after[i] == id) a.u.party.after[i] = 0;
    a.u.party.after[rank] = id;
    if (!memcmp(a.u.party.before, a.u.party.after, sizeof current)) return;
    if (hamlet_submit(&a) && g_cfg.role == DD_ROLE_GUEST) {
        memcpy(g_party_intent, a.u.party.after, sizeof g_party_intent);
        g_party_intent_valid = 1;
    }
}
static void __fastcall hook_hamlet_location(void *roster, void *entry, int32_t location) {
#ifdef _MSC_VER
    uintptr_t caller = (uintptr_t)_ReturnAddress();
#else
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
#endif
    if (!g_hamlet_location) return;
    if (!g_game_apply_depth && hamlet_local_active() &&
        (InterlockedCompareExchange(&g_session_ready, 0, 0) ||
         InterlockedCompareExchange(&g_session_seen, 0, 0))) {
        if (g_hamlet_party_input(caller)) {
            dd_hamlet s;
            dd_hamlet_hero hero;
            uint32_t id = 0, party[4] = {0};
            /* Only a change the shared party order brings waits for that order. The screen filling its slots from the party
             * that is already there is no change: without this, a party nobody dragged stays at location 0 and the raid starts
             * with an empty hero list (owner's run on one PC, 6 Nov: party 8,2,18,1, all at location 0). */
            if (g_hamlet_open_model(&s) && s.campaign == (uintptr_t)roster &&
                safe_read((uintptr_t)entry, &id, 4) && dd_hamlet_find(&s, id, &hero) == 1 &&
                hero.entry == (uintptr_t)entry && hero.location <= 1 && location >= 0 && location <= 1 &&
                hero.location != (uint32_t)location && dd_hamlet_party(&s, party)) {
                int member = 0;
                for (unsigned i = 0; i < 4; i++)
                    if (party[i] == id) member = 1;
                if (member != (location == 1)) return;
            }
        }
    }
    g_hamlet_location(roster, entry, location);
}

static void hamlet_trinket_pickup(unsigned kind, void *ui, void *arg2, void *arg3, void *data) {
    if (!g_trinket_pickup[kind]) return;
    supply_drag_pickup(ui);
    memset(&g_trinket_drag, 0, sizeof g_trinket_drag);
    if (!g_game_apply_depth && hamlet_local_active()) {
        dd_hamlet s;
        uint64_t inventory = 0;
        uint32_t owner = 0, slot = 0, type = 0, id = 0, q = 0, normal = 0;
        if (g_hamlet_open_model(&s) && safe_read((uintptr_t)ui + 0x28, &inventory, 8) &&
            dd_hamlet_inventory_owner(&s, inventory, &owner) && safe_read((uintptr_t)ui + 0xb8, &slot, 4) &&
            dd_hamlet_item(&s, inventory, slot, &type, &id, &q) && q == 1 &&
            hamlet_read(&s, s.image + 0x2a6f868, &normal, 4) && type == normal) {
            g_trinket_drag.ui = (uintptr_t)ui;
            g_trinket_drag.data = (uintptr_t)data;
            g_trinket_drag.owner = owner;
            g_trinket_drag.slot = slot;
            g_trinket_drag.type = type;
            g_trinket_drag.id = id;
            g_trinket_drag.valid = 1;
        }
        if (!g_trinket_drag.valid) {
            uint64_t selected = 0;
            uint32_t selected_type = 0;
            /* Known-build normal hash("trinket"). Unsupported trinket sources must not fall back to
             * an unsynchronized native drag. Unrelated provision/equipment types retain their own path. */
            if (safe_read((uintptr_t)ui + 0x30, &selected, 8) && selected &&
                safe_read((uintptr_t)selected + 0x44, &selected_type, 4) && selected_type == 3950276909u) {
                g_trinket_drag.ui = (uintptr_t)ui;
                g_trinket_drag.data = (uintptr_t)data;
                g_trinket_drag.valid = 3;
                shop_notice("DDCoop could not verify this trinket source. Cancel the drag and send "
                            "the logs.");
            }
        }
    }
    g_trinket_pickup[kind](ui, arg2, arg3,
                           data); /* Native pickup only copies the drag item; it does not move ownership. */
}
static void __fastcall hook_trinket_pickup_hero(void *a, void *b, void *c, void *d) {
    hamlet_trinket_pickup(0, a, b, c, d);
}
static void __fastcall hook_trinket_pickup_inventory(void *a, void *b, void *c, void *d) {
    hamlet_trinket_pickup(1, a, b, c, d);
}
static void hamlet_trinket_finish(unsigned kind, void *ui, void *data) {
    if (supply_drag_finish(ui)) return;
    if (g_trinket_drag.valid && g_trinket_drag.ui == (uintptr_t)ui && hamlet_local_active()) {
        /* Our authoritative transaction already owns both endpoints. Native finish/cancel must not subtract
         * from the source or put its stale drag copy back into the inventory. Only reset the drag flags. */
        dd_hamlet s;
        uint64_t inventory = 0;
        unsigned count = 0;
        uint32_t t = 0, id = 0, q = 0;
        uint8_t visible = 1, dragging = 0;
        if (g_hamlet_open_model(&s) && dd_hamlet_inventory(&s, g_trinket_drag.owner, &inventory, &count) &&
            dd_hamlet_item(&s, inventory, g_trinket_drag.slot, &t, &id, &q))
            visible = (uint8_t)(q > 0);
        WriteProcessMemory(GetCurrentProcess(), (uint8_t *)ui + 0xc, &visible, 1, NULL);
        WriteProcessMemory(GetCurrentProcess(), (uint8_t *)ui + 0xc0, &dragging, 1, NULL);
        g_trinket_drag.valid = 2;
        return;
    }
    if (g_trinket_finish[kind]) g_trinket_finish[kind](ui, data);
}
static void __fastcall hook_trinket_finish_hero(void *a, void *b) { hamlet_trinket_finish(0, a, b); bag_drag_done(); }
static void __fastcall hook_trinket_cancel_hero(void *a, void *b) { hamlet_trinket_finish(1, a, b); bag_drag_done(); }
static void __fastcall hook_trinket_finish_inventory(void *a, void *b) { hamlet_trinket_finish(2, a, b); }
static void __fastcall hook_trinket_cancel_inventory(void *a, void *b) { hamlet_trinket_finish(3, a, b); }
static int hamlet_trinket_drop(unsigned kind, void *ui, void *position, void *data) {
    if (!g_trinket_drag.valid || g_game_apply_depth || !hamlet_local_active())
        return g_trinket_drop[kind] ? g_trinket_drop[kind](ui, position, data) : 0;
    if (g_trinket_drag.valid != 1) return 0;
    if (g_trinket_drag.data != (uintptr_t)data) return 0;
    dd_hamlet s;
    uint64_t inventory = 0;
    uint32_t owner = 0, slot = 0, type = 0, id = 0, q = 0;
    unsigned count = 0;
    if (!g_hamlet_open_model(&s) || !safe_read((uintptr_t)ui + 0x18, &inventory, 8) ||
        !dd_hamlet_inventory_owner(&s, inventory, &owner) ||
        !dd_hamlet_inventory(&s, owner, &inventory, &count))
        return 0;
    if (kind < 2) {
        if (!safe_read((uintptr_t)ui + 0xa8, &slot, 4)) return 0;
    } else {
        for (slot = 0; slot < count; slot++) {
            if (!dd_hamlet_item(&s, inventory, slot, &type, &id, &q)) return 0;
            if (!q && dd_hamlet_can_equip(&s, owner, slot, g_trinket_drag.id, UINT32_MAX)) break;
        }
        if (slot == count) return 0;
    }
    if (owner == g_trinket_drag.owner && slot == g_trinket_drag.slot) return 0;
    if (!dd_hamlet_item(&s, inventory, slot, &type, &id, &q)) return 0;
    dd_action a = {0};
    a.kind = DD_ACT_TRINKET_MOVE;
    a.u.move.src_hero = g_trinket_drag.owner;
    a.u.move.src_slot = (uint16_t)g_trinket_drag.slot;
    a.u.move.src_type = g_trinket_drag.type;
    a.u.move.src_id = g_trinket_drag.id;
    a.u.move.dst_hero = owner;
    a.u.move.dst_slot = (uint16_t)slot;
    a.u.move.dst_type = type;
    a.u.move.dst_id = id;
    hamlet_submit(&a);
    return 0; /* Finish through the native cancellation path, whose source mutation is suppressed above. */
}
static int __fastcall hook_trinket_drop_hero(void *a, void *b, void *c) {
    if (bag_drag_refused(a, 0)) return 0;
    int result = hamlet_trinket_drop(0, a, b, c);
    bag_drag_done();
    return result;
}
static int __fastcall hook_trinket_drop_inventory(void *a, void *b, void *c) {
    return hamlet_trinket_drop(1, a, b, c);
}
static int __fastcall hook_trinket_area_hero(void *a, void *b, void *c) {
    return hamlet_trinket_drop(2, a, b, c);
}
static int __fastcall hook_trinket_area_inventory(void *a, void *b, void *c) {
    return hamlet_trinket_drop(3, a, b, c);
}
static void hamlet_bulk(unsigned kind, void *capture) {
    if (!g_game_apply_depth && hamlet_local_active()) {
        shop_notice("Automatic sorting and 'unequip all' are not synchronized yet. Move "
                    "trinkets individually by dragging in this test.");
        return;
    }
    if (g_hamlet_bulk[kind]) g_hamlet_bulk[kind](capture);
}
static void __fastcall hook_trinket_sort(void *capture) { hamlet_bulk(0, capture); }
static void __fastcall hook_trinket_unequip_all(void *capture) { hamlet_bulk(1, capture); }

typedef struct {
    const char *name;
    uintptr_t replacement;
    void *original;
    size_t length;
} hamlet_hook_spec;
static const hamlet_hook_spec g_hamlet_hooks[] = {
    {"hero_toggle_combat", (uintptr_t)hook_hamlet_combat, &g_hamlet_toggle[0], 13},
    {"hero_toggle_camping", (uintptr_t)hook_hamlet_camping, &g_hamlet_toggle[1], 16},
    {"hero_commit_name", (uintptr_t)hook_hamlet_name, &g_hamlet_name, 16},
    {"hero_dismiss", (uintptr_t)hook_hamlet_dismiss, &g_hamlet_dismiss, 17},
    {"heirloom_exchange", (uintptr_t)hook_hamlet_exchange, &g_hamlet_exchange_original, 13},
    {"roster_change_location", (uintptr_t)hook_hamlet_location, &g_hamlet_location, 10},
    {"party_set_slot", (uintptr_t)hook_hamlet_party_slot, &g_hamlet_party_slot, 13},
    {"trinket_pickup_hero", (uintptr_t)hook_trinket_pickup_hero, &g_trinket_pickup[0], 15},
    {"trinket_pickup_inventory", (uintptr_t)hook_trinket_pickup_inventory, &g_trinket_pickup[1], 15},
    {"trinket_finish_hero", (uintptr_t)hook_trinket_finish_hero, &g_trinket_finish[0], 16},
    {"trinket_cancel_hero", (uintptr_t)hook_trinket_cancel_hero, &g_trinket_finish[1], 16},
    {"trinket_finish_inventory", (uintptr_t)hook_trinket_finish_inventory, &g_trinket_finish[2], 16},
    {"trinket_cancel_inventory", (uintptr_t)hook_trinket_cancel_inventory, &g_trinket_finish[3], 16},
    {"trinket_drop_hero", (uintptr_t)hook_trinket_drop_hero, &g_trinket_drop[0], 18},
    {"trinket_drop_inventory", (uintptr_t)hook_trinket_drop_inventory, &g_trinket_drop[1], 18},
    {"trinket_area_hero", (uintptr_t)hook_trinket_area_hero, &g_trinket_drop[2], 19},
    {"trinket_area_inventory", (uintptr_t)hook_trinket_area_inventory, &g_trinket_drop[3], 23},
    {"trinket_sort_button", (uintptr_t)hook_trinket_sort, &g_hamlet_bulk[0], 7},
    {"trinket_unequip_all_dialog", (uintptr_t)hook_trinket_unequip_all, &g_hamlet_bulk[1], 11},
};
static uintptr_t hamlet_address(const char *name) {
    const dd_addr *a = dd_addr_find(dd_addresses, dd_address_count, name);
    return a ? (uintptr_t)a->resolved_va : 0;
}
static int hamlet_bind_native(void) {
    g_hamlet_skills_refresh = (hamlet_skills_refresh_fn)hamlet_address("character_refresh_skills");
    g_hamlet_roster_update = (hamlet_roster_update_fn)hamlet_address("roster_list_update");
    g_recruit_event = (recruit_event_fn)hamlet_address("recruit_event_send");
    g_recruit_clock = (recruit_clock_fn)hamlet_address("event_clock");
    g_recruit_save[0] = (recruit_save_fn)hamlet_address("save_roster");
    g_recruit_save[1] = (recruit_save_fn)hamlet_address("save_progression");
    g_recruit_save[2] = (recruit_save_fn)hamlet_address("save_town");
    g_recruit_save[3] = (recruit_save_fn)hamlet_address("save_raid");
    g_hamlet_add = (hamlet_wallet_fn)hamlet_address("wallet_add");
    g_hamlet_deduct = (hamlet_wallet_fn)hamlet_address("wallet_deduct");
    g_hamlet_notify = (hamlet_notify_fn)hamlet_address("wallet_notify");
    g_hamlet_swap = (hamlet_swap_fn)hamlet_address("inventory_swap_slot");
    g_hamlet_copy = (hamlet_copy_fn)hamlet_address("item_copy");
    g_hamlet_clear = (hamlet_notify_fn)hamlet_address("item_clear");
    g_hamlet_destroy = (hamlet_notify_fn)hamlet_address("item_destroy");
    g_hamlet_actor_refresh = (hamlet_notify_fn)hamlet_address("actor_refresh_trinkets");
    g_hero_equipment = (hamlet_notify_fn)hamlet_address("hero_equipment_levels");
    g_hero_skills = (hamlet_notify_fn)hamlet_address("hero_skill_levels");
    return g_hero_equipment && g_hero_skills && g_hamlet_add && g_hamlet_deduct && g_hamlet_notify && g_hamlet_swap && g_hamlet_copy &&
           g_hamlet_clear && g_hamlet_destroy && g_hamlet_actor_refresh && g_hamlet_skills_refresh;
}
