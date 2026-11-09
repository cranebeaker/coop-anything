#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif

#include "dd_saves.h"
#include "dd_addr.h"
#include "dd_config.h"
#include "dd_log.h"
#include "dd_net.h"
#include "dd_runtime.h"
#include "dd_town.h"
#include "dd_shop.h"
#include "dd_recruit.h"
#include "dd_hamlet.h"
#include "dd_supplies.h"
#include "dd_overlay.h"
static unsigned g_game_apply_depth;
static volatile LONG g_peer_sync_stop;
static int g_sync_stop_sent;
static void runtime_sync_stop(uint32_t reason);
static int bag_repair_busy(void);
static int hamlet_apply(const dd_action *a);
static void hamlet_reset(void);
static void hamlet_refresh_schedule(uint32_t id);
static void hamlet_upgrade_refresh(uint32_t id);
static void hamlet_refresh_poll(void);
static void hamlet_party_show_poll(void);
static int hamlet_dismiss_busy(void);
static int hamlet_party_busy(void);
static void hamlet_request_done(uint32_t request,int rejected);
static int provisions_busy(void);
static int provisions_apply(const dd_action *);
static void provisions_reset(void);
static int provisions_gate(const dd_msg *);
static void provisions_request_done(uint32_t,int);
static void provisions_poll(void);
static int departure_apply(const dd_action *);
static int embark_apply(const dd_action *);
static int walk_travel_apply(const dd_action *);
static int walk_use_apply(const dd_action *);
static void departure_reset(void);
static void departure_poll(void);

#define DD_ACTIVITY_SLOT_OFFSET 0x90
#define DD_HOST_REFUSED_NOTICE 3
#define DD_ACTIVITY_SLOTS 3
#define DD_RESULT_BYTES 0x28
#define DD_QUEUE_CAP 64
#define DD_HELLO_TIMEOUT_MS 8000
#define DD_TOWN_PORT_DEFAULT 27360

typedef uintptr_t (__fastcall *assign_hero_fn)(void *activity, void *result, uint32_t hero_id);
typedef uintptr_t (__fastcall *slot_widget_ctor_fn)(void *widget, void *activity, int slot_index);
typedef int (__fastcall *buy_upgrade_fn)(const void *request);
typedef void (__fastcall *wagon_buy_fn)(void *context, int visible_index);
typedef int (__fastcall *trinket_price_fn)(int base_price, uint32_t modifier);
typedef void (__fastcall *wagon_sell_fn)(const void *request);
typedef struct {uint64_t inventory;int32_t gold;uint32_t slot;} sale_request;
_Static_assert(sizeof(sale_request)==16,"normal sale callback capture layout");
typedef void (__fastcall *recruit_fn)(void *capture,void *hero,uint32_t index);
typedef void (__fastcall *recruit_panel_fn)(void *panel);
typedef uintptr_t (__fastcall *roster_add_fn)(void *roster,const void *entry);
typedef unsigned (__fastcall *roster_size_fn)(void);
typedef int (__fastcall *roster_drop_fn)(void *target,void *position,void *drag);

typedef struct {
    void *target;
    uintptr_t replacement;
    void *trampoline;
    void *relay;
    uint8_t original[64];
    size_t patch_len;
} dd_hook;

typedef struct {
    CRITICAL_SECTION lock;
    dd_msg items[DD_QUEUE_CAP];
    unsigned head, count;
} dd_msg_queue;

static dd_config g_cfg;
static uint8_t g_build_id[8];
static uintptr_t g_assign_va, g_widget_ctor_va, g_buy_upgrade_va, g_wagon_va;
static assign_hero_fn g_original_assign;
static slot_widget_ctor_fn g_original_widget_ctor;
static buy_upgrade_fn g_original_buy_upgrade;
static wagon_buy_fn g_original_wagon;
static trinket_price_fn g_trinket_price;
static uintptr_t g_sell_va;
static wagon_sell_fn g_original_sell;
static uint32_t g_sell_request;
static uintptr_t g_recruit_va,g_recruit_show_va,g_recruit_hide_va;
static recruit_fn g_original_recruit;
static recruit_panel_fn g_original_recruit_show,g_original_recruit_hide;
typedef void (__fastcall *recruit_event_fn)(void *bus,const void *event);
typedef uint64_t (__fastcall *recruit_clock_fn)(void);
typedef void (__fastcall *recruit_save_fn)(void *object,int flag);
static recruit_event_fn g_recruit_event;
static recruit_clock_fn g_recruit_clock;
static recruit_save_fn g_recruit_save[4];
static uintptr_t g_roster_drop_va;
static roster_drop_fn g_original_roster_drop;
static roster_add_fn g_roster_add;
static roster_size_fn g_roster_count,g_roster_capacity;
static uint64_t g_recruit_panel,g_recruit_ready_at;
static uint32_t g_recruit_request;
static int g_recruit_waiting;
static volatile LONG g_queue_failed;
static void *g_activity[DD_ACTIVITY_COUNT_];
static dd_msg_queue g_incoming, g_outgoing;
static dd_town g_town;
static volatile LONG g_initialized, g_started, g_campaign_active, g_session_ready, g_hooks_installed;
static volatile LONG g_host_refused_count; /* Guest: failed handshakes in a row. */
static volatile LONG g_session_seen, g_session_lost;
static volatile LONG64 g_transport_started_at; /* GetTickCount64 when the network thread started; 0 before */
static volatile LONG g_peer_load_slot; /* The menu slot the other copy waits to load at its save screen (DD_MSG_LOAD); 0 = not seen */
static void runtime_transport_start(void);
static int g_session_loss_notified;
static volatile LONG g_unknown_activity_logged;
static HANDLE g_network_thread;
static int g_shop_status=-1, g_shop_before_gold;
static char g_shop_notice[512];
static const char *g_shop_refusal;
static uint32_t g_shop_request;

static int queue_init(dd_msg_queue *q) {
    memset(q, 0, sizeof *q);
    InitializeCriticalSection(&q->lock);
    return 1;
}

static void queue_clear(dd_msg_queue *q) {
    EnterCriticalSection(&q->lock);
    q->head = q->count = 0;
    LeaveCriticalSection(&q->lock);
}
/* A restored raid can be ready on the host while this copy's 2-second slot
 * watcher still says inactive. Preserve its initial table and ordered tail,
 * atomically with network enqueue. Nothing is applied before the slot gate. */
static void incoming_clear_inactive(void) {
    dd_msg_queue *q = &g_incoming;
    EnterCriticalSection(&q->lock);
    const dd_msg *m = q->count ? &q->items[q->head] : NULL;
    if (!(g_cfg.role == DD_ROLE_GUEST && InterlockedCompareExchange(&g_session_ready, 0, 0) &&
          m && m->type == DD_MSG_TOWN_APPLY && m->u.town.action.kind == DD_ACT_OWNERS &&
          dd_action_valid(&m->u.town.action))) q->head = q->count = 0;
    LeaveCriticalSection(&q->lock);
}

static int queue_push(dd_msg_queue *q, const dd_msg *m) {
    int ok = 0;
    EnterCriticalSection(&q->lock);
    if (q->count < DD_QUEUE_CAP) {
        unsigned at = (q->head + q->count) % DD_QUEUE_CAP;
        q->items[at] = *m;
        q->count++;
        ok = 1;
    }
    LeaveCriticalSection(&q->lock);
    return ok;
}

static int queue_pop(dd_msg_queue *q, dd_msg *m) {
    int ok = 0;
    EnterCriticalSection(&q->lock);
    if (q->count) {
        *m = q->items[q->head];
        q->head = (q->head + 1) % DD_QUEUE_CAP;
        q->count--;
        ok = 1;
    }
    LeaveCriticalSection(&q->lock);
    return ok;
}

static int queue_peek(dd_msg_queue *q,dd_msg *m) {
    int ok=0;
    EnterCriticalSection(&q->lock);
    if(q->count){*m=q->items[q->head];ok=1;}
    LeaveCriticalSection(&q->lock);
    return ok;
}

static int safe_read(uintptr_t addr, void *dst, size_t len) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), (const void *)addr, dst, len, &got) && got == len;
}

static uintptr_t image_base(void) { return (uintptr_t)GetModuleHandleA(NULL); }

static int shop_read(void *unused,uint64_t address,void *out,size_t len) {
    (void)unused;return safe_read((uintptr_t)address,out,len);
}

static int shop_open(dd_shop *s) {
    uintptr_t campaign = 0;
    safe_read(image_base() + 0x118e138, &campaign, sizeof campaign);
    dd_shop_result r = dd_shop_open(s, shop_read, NULL, image_base(), campaign);
    if (g_shop_status != (int)r) {
        dd_log("Nomad model: %s", dd_shop_result_name(r));
        g_shop_status = (int)r;
    }
    return r == DD_SHOP_READY;
}

static void shop_notice(const char *message) {
    if (strcmp(g_shop_notice, message) == 0) return;
    dd_log("local notice: %s",message);
    snprintf(g_shop_notice, sizeof g_shop_notice, "%s", message);
    /* A Windows popup would stop the game thread and take the player out of the game. */
    if (!dd_overlay_notice(message)) MessageBoxA(NULL, message, "DDCoop", MB_OK | MB_ICONINFORMATION);
}
static void shop_notice_clear(const char *message) {
    if (!strcmp(g_shop_notice, message)) g_shop_notice[0] = 0;
    dd_overlay_notice_clear(message);
}

typedef struct {
    int32_t gold, owned, stock;
} shop_balance;
static int shop_balance_read(const dd_shop *s, const dd_action *a, shop_balance *b) {
    if (!dd_shop_gold(s, &b->gold)) {
        dd_log("Nomad balance read failed: wallet");
        return 0;
    }
    if (!dd_shop_owned(s, a->u.items.type, a->u.items.id, &b->owned)) {
        uint64_t begin = 0, end = 0;
        safe_read((uintptr_t)s->estate + 8, &begin, 8);
        safe_read((uintptr_t)s->estate + 0x10, &end, 8);
        dd_log("Nomad balance read failed: estate inventory span=%llu limit=%u slots",
               (unsigned long long)(end >= begin ? end - begin : UINT64_MAX), DD_SHOP_MAX_STORAGE);
        return 0;
    }
    if (!dd_shop_quantity(s, a->u.items.slot, &b->stock)) {
        dd_log("Nomad balance read failed: stock slot=%u", a->u.items.slot);
        return 0;
    }
    return 1;
}

static int shop_refuse(const dd_action *a, const char *why) {
    g_shop_refusal = why;
    dd_log("Nomad order refused: %s slot=%u type=%08x item=%08x price=%u", why, a->u.items.slot, a->u.items.type,
           a->u.items.id, a->u.items.cost);
    return 0;
}

/* A disconnected stream cannot resume ordered authority without a fresh host snapshot. The worker
 * only latches flags; all model/UI state and notices stay on the game thread. */
static const char g_wait_for_peer[] = "Wacht tot beide spellen verbonden zijn: deze wijziging is niet uitgevoerd, "
                                      "anders lopen jullie uit de pas. Staat hieronder geen melding meer, probeer het dan opnieuw.";
/* A real player action with no session yet. It would change only this copy, without a trace. The save load itself
 * and everything else the game does on its own keep running. A solo test build has no second player to wait for. */
static int runtime_waiting_for_peer(void) {
#ifdef DD_SOLO_MAP_SEED
    return 0;
#else
    if (g_game_apply_depth || g_cfg.role == DD_ROLE_OFF || !InterlockedCompareExchange(&g_campaign_active, 0, 0) ||
        InterlockedCompareExchange(&g_session_ready, 0, 0))
        return 0;
    if (!strcmp(g_shop_notice, g_wait_for_peer)) g_shop_notice[0] = 0; /* Say it again on every refused action. */
    dd_log("action refused: no session yet (role=%s)", dd_role_name(g_cfg.role));
    shop_notice(g_wait_for_peer);
    return 1;
#endif
}
static int runtime_shared_blocked(void) {
    if (g_game_apply_depth || g_cfg.role == DD_ROLE_OFF ||
        !InterlockedCompareExchange(&g_campaign_active, 0, 0)) return 0;
    int lost = InterlockedCompareExchange(&g_session_lost, 0, 0) ||
               (InterlockedCompareExchange(&g_session_seen, 0, 0) &&
                !InterlockedCompareExchange(&g_session_ready, 0, 0));
    if (!lost && !g_town.needs_resync) return hamlet_dismiss_busy() || hamlet_party_busy() || provisions_busy() || bag_repair_busy();
    if (lost) {
        g_town.needs_resync = 1;
        if (!g_session_loss_notified) {
            g_session_loss_notified = 1;
            dd_log("shared actions stopped after live session loss; restart both games for a fresh snapshot");
            shop_notice("De verbinding met de andere speler is verbroken. Gedeelde acties zijn gestopt. "
                        "Sluit beide spellen en start een nieuwe co-op sessie via de launcher.");
        }
    }
    return 1;
}

static void shop_refusal_notice(const dd_action *a) {
    if (g_town.needs_resync) {
        shop_notice("De trinketaankoop kon niet volledig worden gecontroleerd. Sluit beide spellen en start de co-op "
                    "sessie opnieuw. Stuur de logs mee.");
    } else if (g_shop_refusal && strcmp(g_shop_refusal, "insufficient gold") == 0) {
        char message[180];
        snprintf(message, sizeof message, "Onvoldoende goud voor deze trinket: nodig %u, beschikbaar %d.",
                 a->u.items.cost, g_shop_before_gold);
        shop_notice(message);
    } else if (g_shop_refusal && strcmp(g_shop_refusal, "game declined") == 0) {
        shop_notice("De game heeft deze aankoop geweigerd. Controleer of er plaats is in de trinketinventaris.");
    } else {
        shop_notice("DDCoop kon deze aankoop niet afronden. Controleer de winkelvoorraad. Stuur de logs mee als de "
                    "aankoop blijft mislukken.");
    }
}

static int shop_quote(const dd_shop *s, const dd_shop_item *item, uint32_t *price) {
    uint32_t modifier = 0;
    if (item->quantity != 1 || !g_trinket_price ||
        !safe_read((uintptr_t)s->image + 0x2a7b404, &modifier, sizeof modifier))
        return 0;
    int amount = g_trinket_price(item->base_price, modifier);
    if (amount < 0) return 0;
    *price = (uint32_t)amount;
    return 1;
}

/* The game's normal captureless affordability callback (0x1406944f0) reads no UI object.
 * Recreate only its std::function object/context on the stack, using validated persistent inventories.
 * Game code performs ALL mutations, payment, save notifications and inventory-change events. */
static int shop_apply(const dd_action *a) {
    dd_shop s;
    dd_shop_item item;
    shop_balance before, after;
    uint32_t price = 0;
    g_shop_refusal = NULL;
    if (!g_original_wagon || !shop_open(&s)) return shop_refuse(a, "model unavailable");
    if (!dd_shop_slot(&s, a->u.items.slot, &item)) return shop_refuse(a, "stock or definition unavailable");
    if (item.type != a->u.items.type || item.id != a->u.items.id) return shop_refuse(a, "item identity changed");
    if (!shop_quote(&s, &item, &price)) return shop_refuse(a, "price or quantity invalid");
    if (price != a->u.items.cost) {
        dd_log("Nomad current price=%u requested=%u", price, a->u.items.cost);
        return shop_refuse(a, "quoted price changed");
    }
    if (!shop_balance_read(&s, a, &before)) return shop_refuse(a, "balance unreadable");
    g_shop_before_gold = before.gold;
    if (before.gold < (int32_t)price) {
        dd_log("Nomad gold available=%d required=%u", before.gold, price);
        return shop_refuse(a, "insufficient gold");
    }
    if (before.owned == INT32_MAX) return shop_refuse(a, "item quantity overflow");
    uint64_t callback = s.image + 0xebda18;
    uint64_t context[10] = {0};
    context[0] = s.stock;
    context[1] = s.estate;
    context[9] = (uintptr_t)&callback;
    dd_log("Nomad pre-purchase slot=%u visible=%u item=%08x price=%u gold=%d owned=%d stock=%d",
           item.physical,item.visible,item.id,price,before.gold,before.owned,before.stock);
    g_original_wagon(context, item.visible);
    if (!shop_balance_read(&s, a, &after)) {
        g_town.needs_resync = 1;
        return shop_refuse(a, "post-purchase state unreadable; resync required");
    }
    dd_log("Nomad transaction slot=%u type=%08x item=%08x price=%u gold=%d->%d owned=%d->%d stock=%d->%d",
           item.physical, item.type, item.id, price, before.gold, after.gold, before.owned, after.owned, before.stock,
           after.stock);
    if (after.gold == before.gold - (int32_t)price && after.owned == before.owned + 1 && after.stock == 0) return 1;
    if (after.gold != before.gold || after.owned != before.owned || after.stock != before.stock) {
        g_town.needs_resync = 1;
        dd_log("Nomad transaction was not complete; stop shop actions and rejoin");
        return shop_refuse(a, "partial transaction; resync required");
    }
    return shop_refuse(a, "game declined");
}

/* Normal sale capture: {Inventory*, quotedGold, physicalSlot}. Hook after the normal confirmation,
 * before any model mutation. Only the capture is reconstructed; no synthetic UI or inventory. */
static int (*g_sale_open_model)(dd_shop *s)=shop_open;
static int sale_quote(const dd_shop *s,const dd_shop_item *item,uint32_t *amount) {
    uint32_t modifier=0;
    if(!g_trinket_price || item->quantity!=1 || item->base_sale<=0 ||
       !s->read(s->ctx,s->image+0x2a7dca4,&modifier,4))return 0;
    int value=g_trinket_price(item->base_sale,modifier);
    if(value<=0)return 0;
    *amount=(uint32_t)value;return 1;
}
static int sale_apply(const dd_action *a) {
    dd_shop s;dd_shop_item item;uint32_t amount=0;
    int32_t gold_before=0,gold_after=0,owned_before=0,owned_after=0,quantity_after=0;
    if(!g_original_sell || !g_sale_open_model(&s) || !dd_shop_estate_slot(&s,a->u.items.slot,&item) ||
       item.type!=a->u.items.type || item.id!=a->u.items.id || !sale_quote(&s,&item,&amount) || amount!=a->u.items.cost) {
        dd_log("Trinket sale refused: source identity/quantity or quote changed slot=%u item=%08x quote=%u",
               a->u.items.slot,a->u.items.id,a->u.items.cost);return 0;
    }
    if(!dd_shop_gold(&s,&gold_before) || !dd_shop_owned(&s,item.type,item.id,&owned_before) ||
       owned_before<1 || amount>(uint32_t)(INT32_MAX-gold_before)) {
        dd_log("Trinket sale refused: wallet/inventory unreadable or gold overflow");return 0;
    }
    sale_request request={s.estate,(int32_t)amount,item.physical};
    dd_log("Trinket pre-sale slot=%u item=%08x proceeds=%u gold=%d owned=%d",item.physical,item.id,amount,gold_before,owned_before);
    g_original_sell(&request);
    if(!dd_shop_gold(&s,&gold_after) || !dd_shop_owned(&s,item.type,item.id,&owned_after) ||
       !dd_shop_estate_quantity(&s,item.physical,&quantity_after)) {
        g_town.needs_resync=1;dd_log("Trinket sale post-state unreadable; rejoin required");return 0;
    }
    dd_log("Trinket sale result slot=%u item=%08x proceeds=%u gold=%d->%d owned=%d->%d source=1->%d",
           item.physical,item.id,amount,gold_before,gold_after,owned_before,owned_after,quantity_after);
    if(gold_after==gold_before+(int32_t)amount && owned_after==owned_before-1 && quantity_after==0)return 1;
    if(gold_after!=gold_before || owned_after!=owned_before || quantity_after!=1) {
        g_town.needs_resync=1;dd_log("Trinket sale partial result; rejoin required");
    }
    return 0;
}
static void __fastcall hook_wagon_sell(const void *capture) {
    if(!g_original_sell)return;
    if(!InterlockedCompareExchange(&g_campaign_active,0,0) || g_cfg.role==DD_ROLE_OFF) {
        g_original_sell(capture);return;
    }
    if (runtime_shared_blocked()) return;
    if(!InterlockedCompareExchange(&g_session_ready,0,0) || g_town.needs_resync) {
        shop_notice("Wacht met verkopen tot beide spellen verbonden zijn. Bij een synchronisatiefout: start beide spellen opnieuw.");return;
    }
    dd_shop s;dd_shop_item item;sale_request request={0};uint32_t amount=0;
    if(!safe_read((uintptr_t)capture,&request,sizeof request) || !g_sale_open_model(&s) || request.inventory!=s.estate ||
       request.slot>=DD_SHOP_MAX_STORAGE || !dd_shop_estate_slot(&s,(uint16_t)request.slot,&item) ||
       !sale_quote(&s,&item,&amount) || request.gold!=(int32_t)amount) {
        dd_log("Trinket sale held: invalid source or quote; only normal estate storage is supported");
        shop_notice("DDCoop kon deze verkoop niet controleren. Verkoop één gewone trinket vanuit de gedeelde voorraad. Stuur de logs mee als dit blijft gebeuren.");return;
    }
    dd_action a={0};a.kind=DD_ACT_WAGON_SELL;a.u.items.slot=item.physical;a.u.items.count=1;
    a.u.items.type=item.type;a.u.items.id=item.id;a.u.items.cost=amount;
    if(g_cfg.role==DD_ROLE_GUEST) {
        g_sell_request=dd_town_guest_local(&g_town,&a);
        dd_log("guest sent trinket sale before mutation request=%u slot=%u item=%08x proceeds=%u",g_sell_request,item.physical,item.id,amount);
    } else if(!dd_town_host_local(&g_town,&a)) {
        shop_notice(g_town.needs_resync?"De verkoop kon niet volledig worden gecontroleerd. Sluit beide spellen en start de co-op sessie opnieuw. Stuur de logs mee.":
                    "De verkoop is geweigerd. Controleer of de trinket nog in de voorraad staat.");
    }
}

static void __fastcall hook_wagon_buy(void *context, int visible_index) {
    if (!g_original_wagon) return;
    if (!InterlockedCompareExchange(&g_campaign_active, 0, 0) || g_cfg.role == DD_ROLE_OFF) {
        g_original_wagon(context, visible_index);
        return;
    }
    if (runtime_shared_blocked()) return;
    if (!InterlockedCompareExchange(&g_session_ready, 0, 0) || g_town.needs_resync) {
        dd_log("Nomad local purchase held: session unavailable or resync required");
        shop_notice("Wacht tot beide spelers verbonden zijn. Als de verbinding al werkte: sluit beide spellen en start "
                    "de co-op sessie opnieuw.");
        return;
    }
    dd_shop s;
    dd_shop_item item;
    uint32_t price = 0;
    uint64_t source = 0, destination = 0;
    if (visible_index < 0 || visible_index >= DD_SHOP_MAX_STOCK || !shop_open(&s) ||
        !safe_read((uintptr_t)context, &source, 8) || !safe_read((uintptr_t)context + 8, &destination, 8) ||
        source != s.stock || destination != s.estate || !dd_shop_visible(&s, (uint16_t)visible_index, &item) ||
        !shop_quote(&s, &item, &price)) {
        dd_log("Nomad local purchase held: stock/context validation failed");
        shop_notice("DDCoop kan deze winkel nog niet veilig synchroniseren. De aankoop is niet uitgevoerd. Sluit het "
                    "spel en stuur de logs mee.");
        return;
    }
    dd_action a = {0};
    a.kind = DD_ACT_WAGON_BUY;
    a.u.items.slot = item.physical;
    a.u.items.count = 1;
    a.u.items.type = item.type;
    a.u.items.id = item.id;
    a.u.items.cost = price;
    if (g_cfg.role == DD_ROLE_GUEST) {
        uint32_t request = dd_town_guest_local(&g_town, &a);
        g_shop_request=request;
        dd_log("guest sent Nomad order request=%u slot=%u type=%08x item=%08x price=%u", request, item.physical,
               item.type, item.id, price);
    } else if (!dd_town_host_local(&g_town, &a)) {
        dd_log("host Nomad purchase refused by local game or order validation");
        shop_refusal_notice(&a);
    }
}

static int recruit_open(dd_recruit *s) {
    uint64_t campaign=0;
    return safe_read(image_base()+0x118e138,&campaign,8) &&
           dd_recruit_open(s,shop_read,NULL,image_base(),campaign);
}
static int (*g_recruit_open_model)(dd_recruit *s)=recruit_open;
/* A panel that has only just opened gets a moment for its rows. Without a panel there is nothing to wait for. */
static int recruit_context_settled(void) {
    return !g_recruit_panel || GetTickCount64()>=g_recruit_ready_at;
}
/* Kept as a predicate so the main-thread FIFO can be exercised without a running game. */
static int (*g_recruit_can_apply)(void)=recruit_context_settled;
static int recruit_bus_read(uint64_t *bus) {
    return safe_read(image_base()+0x118e028,bus,8) && *bus;
}
static int (*g_recruit_bus)(uint64_t *)=recruit_bus_read;
/* What the Stagecoach's callback 6c3c10 does once the hero is in the roster, for a copy without that screen: the HeroRecruited event (6c54d0), the candidate
 * taken, the count of recruited heroes (+1708, total_recruited_stage_coach_heroes), and the game's four writers of persist.roster, .progression, .town and .raid (which
 * write nothing with -nosave). Left out is only what belongs to the screen: its row, its sound, and closing it when it has no rows left. The candidate's rows are built when it opens. */
static int recruit_background_ready(const dd_recruit *s) {
    uint64_t bus=0;int32_t total=0;
    return g_recruit_event && g_recruit_clock && g_recruit_save[0] && g_recruit_save[1] && g_recruit_save[2] && g_recruit_save[3] &&
           g_recruit_bus(&bus) && safe_read((uintptr_t)s->campaign+0x1708,&total,4);
}
static void recruit_background(const dd_recruit *s,const dd_recruit_choice *c) {
    uint64_t bus=0,event[3];int32_t total=0;uint8_t taken=0;
    g_recruit_bus(&bus);
    event[0]=image_base()+0xe79ae8;event[1]=g_recruit_clock();event[2]=c->id;
    g_recruit_event((void *)(uintptr_t)bus,event);
    memcpy((void *)(uintptr_t)(c->entry+0x15a8),&taken,1);
    if(safe_read((uintptr_t)s->campaign+0x1708,&total,4)) {
        total++;
        memcpy((void *)(uintptr_t)(s->campaign+0x1708),&total,4);
    }
    g_recruit_save[0]((void *)(uintptr_t)s->campaign,0);
    g_recruit_save[1]((void *)(uintptr_t)(s->campaign+0x1680),0);
    g_recruit_save[2]((void *)(uintptr_t)(s->campaign+0x1138),0);
    g_recruit_save[3]((void *)(image_base()+0x2a6cf70),0);
}

static int recruit_apply(const dd_action *a) {
    dd_recruit s;dd_recruit_choice before,after;unsigned roster_before=0,roster_after=0;
    int screen=g_recruit_panel!=0;
    if(!g_original_recruit || !g_recruit_open_model(&s) ||
       !(screen?dd_recruit_resolve(&s,g_recruit_panel,a->u.hero.hero_id,&before):dd_recruit_candidate(&s,a->u.hero.hero_id,&before)) ||
       !before.available || (!screen && !recruit_background_ready(&s))) {
        dd_log("Stagecoach refused: candidate/display mismatch hero=%u screen=%d",a->u.hero.hero_id,screen);return 0;
    }
    if(screen && !before.enabled) {
        dd_log("Stagecoach refused: game disabled recruit row (roster full or candidate unavailable) hero=%u",before.id);return 0;
    }
    int had=dd_recruit_roster_has(&s,before.id,&roster_before);
    uint32_t special_limit=0;
    if(had!=0 || !g_roster_add || !g_roster_count || !g_roster_capacity ||
       !safe_read((uintptr_t)before.entry+0x144c,&special_limit,4) || special_limit) {
        dd_log("Stagecoach refused: existing/unreadable hero or unsupported roster-limit candidate hero=%u present=%d limit=%u",before.id,had,special_limit);return 0;
    }
    unsigned used=g_roster_count(),capacity=g_roster_capacity();
    if(capacity>64 || !capacity || used>=capacity) {
        dd_log("Stagecoach refused: roster full/invalid used=%u capacity=%u",used,capacity);return 0;
    }
    dd_log("Stagecoach pre-recruit hero=%u screen=%d row=%u available=%d roster=%u present=%d",before.id,screen,before.index,before.available,roster_before,had);
    /* Only the callback's one-pointer capture is reconstructed. The panel, row, controller and Hero
     * are genuine live game objects; their model must match this campaign's normal Stagecoach. */
    uint64_t capture=g_recruit_panel;
    if(!g_roster_add((void *)(uintptr_t)s.campaign,(void *)(uintptr_t)before.entry))return 0;
    if(screen) {
        dd_recruit_choice live;
        if(g_recruit_panel!=capture || !dd_recruit_resolve(&s,capture,before.id,&live) ||
           live.hero!=before.hero || live.index!=before.index) {
            g_town.needs_resync=1;
            dd_log("Stagecoach display changed during roster addition; rejoin required");return 0;
        }
        g_original_recruit(&capture,(void *)(uintptr_t)before.hero,before.index);
    } else recruit_background(&s,&before);
    int present=dd_recruit_roster_has(&s,before.id,&roster_after);
    int readable=dd_recruit_candidate(&s,before.id,&after);
    dd_log("Stagecoach result hero=%u available=%d->%d roster=%u->%u present=%d",before.id,before.available,
           readable?after.available:-1,roster_before,roster_after,present);
    if(readable && !after.available && present==1 && roster_after==roster_before+1)return 1;
    if(!readable || present<0 || !after.available || roster_before!=roster_after) {
        g_town.needs_resync=1;dd_log("Stagecoach partial/unreadable result; rejoin required");
    }
    return 0;
}
static void __fastcall hook_recruit_show(void *panel) {
    g_original_recruit_show(panel);
    if(!InterlockedCompareExchange(&g_campaign_active,0,0) || g_cfg.role==DD_ROLE_OFF)return;
    dd_recruit s;
    if(g_recruit_open_model(&s) && dd_recruit_panel_ready(&s,(uintptr_t)panel)) {
        g_recruit_panel=(uintptr_t)panel;g_recruit_ready_at=GetTickCount64()+150;
        dd_log("Stagecoach normal panel opened; genuine recruit context ready");
    }
}
static void __fastcall hook_recruit_hide(void *panel) {
    if(g_recruit_panel==(uintptr_t)panel) {
        g_recruit_panel=0;g_recruit_ready_at=0;
        dd_log("Stagecoach panel closed");
    }
    g_original_recruit_hide(panel);
}
static int __fastcall hook_roster_drop(void *target,void *position,void *drag) {
    if(!g_original_roster_drop)return 0;
    if(!InterlockedCompareExchange(&g_campaign_active,0,0) || g_cfg.role==DD_ROLE_OFF)
        return g_original_roster_drop(target,position,drag);
    if (runtime_shared_blocked()) return 0;
    uint64_t entry=0;uint32_t id=0;dd_recruit s;dd_recruit_choice choice;
    int candidate=safe_read((uintptr_t)drag+8,&entry,8) && entry && safe_read((uintptr_t)entry+0x137c,&id,4) &&
        g_recruit_open_model(&s) && dd_recruit_candidate(&s,id,&choice) && choice.available && choice.entry==entry;
    if(!candidate)return g_original_roster_drop(target,position,drag); /* ordinary roster reordering */
    uint32_t special_limit=0;
    if(!safe_read((uintptr_t)entry+0x144c,&special_limit,4) || special_limit) {
        shop_notice("Deze kandidaat gebruikt een speciale rosterlimiet. Die wordt in deze testkit nog niet ondersteund.");return 0;
    }
    if(!InterlockedCompareExchange(&g_session_ready,0,0) || g_town.needs_resync) {
        shop_notice("Wacht met recruiten tot beide spellen verbonden zijn. Bij een synchronisatiefout: start beide spellen opnieuw.");return 0;
    }
    if(g_cfg.role==DD_ROLE_HOST)return g_original_roster_drop(target,position,drag);
    if(!dd_recruit_resolve(&s,g_recruit_panel,id,&choice) || !choice.enabled) {
        shop_notice("Open de Stagecoach en kies een beschikbare kandidaat.");return 0;
    }
    dd_action a={0};a.kind=DD_ACT_RECRUIT;a.u.hero.hero_id=id;
    g_recruit_request=dd_town_guest_local(&g_town,&a);
    dd_log("guest sent Stagecoach request before local roster mutation request=%u hero=%u",g_recruit_request,id);
    return 0;
}
static void __fastcall hook_recruit(void *capture,void *hero,uint32_t index) {
    if(!g_original_recruit)return;
    if(!InterlockedCompareExchange(&g_campaign_active,0,0) || g_cfg.role==DD_ROLE_OFF) {
        g_original_recruit(capture,hero,index);return;
    }
    if (runtime_shared_blocked()) return;
    if(g_cfg.role==DD_ROLE_GUEST) {
        dd_log("guest recruit completion held; only host orders may mutate Stagecoach");return;
    }
    uint64_t panel=0;uint32_t id=0;dd_recruit s;dd_recruit_choice choice;
    if(!safe_read((uintptr_t)capture,&panel,8) || panel!=g_recruit_panel ||
       !safe_read((uintptr_t)hero+0x137c,&id,4) || !g_recruit_open_model(&s) ||
       !dd_recruit_resolve(&s,panel,id,&choice) || choice.hero!=(uintptr_t)hero || choice.index!=index || !choice.enabled) {
        dd_log("Stagecoach local recruit refused: context/candidate/row invalid or disabled");
        g_town.needs_resync=1;
        shop_notice("De recruit kon niet volledig worden gecontroleerd. Sluit beide spellen en start de co-op sessie opnieuw. Stuur de logs mee.");return;
    }
    g_original_recruit(capture,hero,index);
    dd_recruit_choice after;unsigned count=0;
    if(InterlockedCompareExchange(&g_session_ready,0,0) && dd_recruit_candidate(&s,id,&after) &&
       !after.available && dd_recruit_roster_has(&s,id,&count)==1) {
        dd_action a={0};a.kind=DD_ACT_RECRUIT;a.u.hero.hero_id=id;
        dd_town_host_observed(&g_town,&a);
        dd_log("host committed Stagecoach recruit hero=%u available=1->0 roster=%u",id,count);
    } else {
        g_town.needs_resync=1;dd_log("host Stagecoach completion did not match live roster; rejoin required");
        shop_notice("De recruit kon niet volledig worden gecontroleerd. Sluit beide spellen en start de co-op sessie opnieuw. Stuur de logs mee.");
    }
}

static int image_range(uintptr_t p, size_t n) {
    uintptr_t base = image_base();
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)(base + (uint32_t)dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    uintptr_t end = base + nt->OptionalHeader.SizeOfImage;
    return p >= base && p <= end && n <= end - p;
}

/* Resolve the MSVC RTTI name for a game object. The build/address gate is checked before this runs. */
static int activity_kind(void *activity) {
    uintptr_t vt = 0, col = 0, td = 0;
    uintptr_t base = image_base();
    uint32_t self_rva = 0, type_rva = 0;
    char name[128];
    if (!activity || !safe_read((uintptr_t)activity, &vt, sizeof vt) || vt < base + 8 || !image_range(vt - 8, 8)) return -1;
    if (!safe_read(vt - 8, &col, sizeof col) || !image_range(col, 24)) return -1;
    if (!safe_read(col + 12, &type_rva, sizeof type_rva) || !safe_read(col + 20, &self_rva, sizeof self_rva)) return -1;
    if (col < base + self_rva) return -1;
    uintptr_t image = col - self_rva;
    if (image != base || (uintptr_t)type_rva > UINTPTR_MAX - image) return -1;
    td = image + type_rva;
    if (!image_range(td, 16)) return -1;
    memset(name, 0, sizeof name);
    for (size_t i = 0; i < sizeof name - 1; i++) {
        if (!safe_read(td + 16 + i, &name[i], 1)) return -1;
        if (!name[i]) break;
    }
    if (strstr(name, "Bar@Activity@Building")) return DD_ACTIVITY_BAR;
    if (strstr(name, "Gambling@Activity@Building")) return DD_ACTIVITY_GAMBLING;
    if (strstr(name, "Brothel@Activity@Building")) return DD_ACTIVITY_BROTHEL;
    if (strstr(name, "Meditation@Activity@Building")) return DD_ACTIVITY_MEDITATION;
    if (strstr(name, "Prayer@Activity@Building")) return DD_ACTIVITY_PRAYER;
    if (strstr(name, "Flagellation@Activity@Building")) return DD_ACTIVITY_FLAGELLATION;
    if (strstr(name, "QuirkTreatment@Activity@Building")) return DD_ACTIVITY_QUIRK_TREATMENT;
    if (strstr(name, "DiseaseTreatment@Activity@Building")) return DD_ACTIVITY_DISEASE_TREATMENT;
    return -1;
}

static int (*g_activity_kind)(void *) = activity_kind;
/* A building's activity from the town model, so a placement needs no building screen on this copy. The town is
 * campaign+1138 (591540 saves it); its buildings are a std::map at +c0 (saved as "buildings": node +0 left, +10 right,
 * +19 nil, the Building* at +28); each building keeps its activities in a vector at +c0 (5453d0 saves them as
 * "activities"). NULL when not found, or when two activities of this kind exist and it is not clear which. */
static void *activity_in_town(uint64_t town, int kind) {
    uint64_t head = 0, root = 0, stack[64], found = 0;
    unsigned top = 0, seen = 0, ambiguous = 0;
    if (!town || !safe_read((uintptr_t)town + 0xc0, &head, 8) || !head || !safe_read((uintptr_t)head + 8, &root, 8)) return NULL;
    stack[top++] = root;
    while (top) {
        uint64_t node = stack[--top], building = 0, begin = 0, end = 0, left = 0, right = 0;
        uint8_t nil = 1;
        if (!node || ++seen > 64 || !safe_read((uintptr_t)node + 0x19, &nil, 1)) return NULL;
        if (nil) continue;
        if (!safe_read((uintptr_t)node + 0x28, &building, 8) || !building || !safe_read((uintptr_t)node, &left, 8) ||
            !safe_read((uintptr_t)node + 0x10, &right, 8))
            return NULL;
        if (safe_read((uintptr_t)building + 0xc0, &begin, 8) && safe_read((uintptr_t)building + 0xc8, &end, 8) && end >= begin &&
            (end - begin) % 8 == 0 && (end - begin) / 8 <= 16)
            for (uint64_t p = begin; p < end; p += 8) {
                uint64_t activity = 0;
                if (!safe_read((uintptr_t)p, &activity, 8) || !activity || g_activity_kind((void *)(uintptr_t)activity) != kind) continue;
                if (found && found != activity) ambiguous = 1;
                found = activity;
            }
        if (top + 2 > 64) return NULL;
        stack[top++] = left;
        stack[top++] = right;
    }
    return ambiguous ? NULL : (void *)(uintptr_t)found;
}
static uint64_t town_object(void) {
    uint64_t campaign = 0;
    return safe_read(image_base() + 0x118e138, &campaign, 8) && campaign ? campaign + 0x1138 : 0;
}
static uint64_t (*g_town_object)(void) = town_object;

static void emit_absolute_jump(uint8_t *at, const void *destination) {
    /* RIP-indirect jump preserves every register, including bool results in AL. */
    const uint8_t prefix[6]={0xff,0x25,0,0,0,0};
    uint64_t address=(uintptr_t)destination;
    memcpy(at,prefix,sizeof prefix);memcpy(at+6,&address,sizeof address);
}

static void *alloc_near(void *target, size_t bytes) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t gran = si.dwAllocationGranularity;
    uintptr_t center = (uintptr_t)target / gran * gran;
    const uintptr_t limit = 0x70000000u; /* leave margin inside the signed rel32 range */
    for (uintptr_t distance = 0; distance < limit; distance += gran) {
        uintptr_t candidates[2] = { center + distance, center >= distance ? center - distance : 0 };
        for (int i = 0; i < 2; i++) {
            uintptr_t p = candidates[i];
            if (!p || (p > (uintptr_t)target ? p - (uintptr_t)target : (uintptr_t)target - p) >= limit) continue;
            void *mem = VirtualAlloc((void *)p, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (mem) return mem;
        }
    }
    return NULL;
}

/* These copy lengths end on instruction boundaries before any RIP-relative instruction in the
 * supported-build prologues (assign=18, widget ctor=15, upgrade=16, wagon=20,
 * recruit=19, recruit show=20, recruit hide=18, roster drop=16, sell=13 bytes). */
static int hook_prepare(dd_hook *h, void *target, uintptr_t replacement, size_t n) {
    if (n < 5 || n > sizeof h->original || !image_range((uintptr_t)target, n)) return 0;
    memset(h, 0, sizeof *h);
    h->target = target;
    h->replacement = replacement;
    h->patch_len = n;
    if (!safe_read((uintptr_t)target, h->original, n)) return 0;
    h->trampoline = alloc_near(target, n + 28);
    if (!h->trampoline) return 0;
    memcpy(h->trampoline, h->original, n);
    emit_absolute_jump((uint8_t *)h->trampoline + n, (uint8_t *)target + n);
    h->relay=(uint8_t *)h->trampoline+n+14;
    emit_absolute_jump(h->relay,(void *)replacement);
    FlushInstructionCache(GetCurrentProcess(), h->trampoline, n + 28);
    return 1;
}

static int hook_apply(dd_hook *h) {
    intptr_t distance=(intptr_t)h->relay-((intptr_t)h->target+5);
    if(distance<INT32_MIN || distance>INT32_MAX)return 0;
    DWORD old = 0;
    if (!VirtualProtect(h->target, h->patch_len, PAGE_EXECUTE_READWRITE, &old)) return 0;
    uint8_t *p = (uint8_t *)h->target;
    int32_t relative=(int32_t)distance;
    p[0]=0xe9;memcpy(p+1,&relative,4);
    if (h->patch_len > 5) memset(p + 5, 0x90, h->patch_len - 5);
    FlushInstructionCache(GetCurrentProcess(), h->target, h->patch_len);
    DWORD ignored = 0;
    VirtualProtect(h->target, h->patch_len, old, &ignored);
    return 1;
}

static void hook_restore(dd_hook *h) {
    DWORD old = 0;
    if (h->target && VirtualProtect(h->target, h->patch_len, PAGE_EXECUTE_READWRITE, &old)) {
        memcpy(h->target, h->original, h->patch_len);
        FlushInstructionCache(GetCurrentProcess(), h->target, h->patch_len);
        DWORD ignored = 0;
        VirtualProtect(h->target, h->patch_len, old, &ignored);
    }
}

static void outgoing_send(void *unused, int to, const dd_msg *m) {
    (void)unused; (void)to;
    if (!queue_push(&g_outgoing, m)) {
        InterlockedExchange(&g_queue_failed,1);dd_log("town outbound queue full; rejoin required");
    }
}
static void runtime_stop_shared(uint32_t reason, int notify) {
    g_town.needs_resync = 1;
    dd_overlay_turn(DD_OVERLAY_TURN_NONE);
    dd_overlay_session(DD_OVERLAY_REFUSED);
    dd_log("Sync stop: reason=%u role=%s peer=%d", reason, dd_role_name(g_cfg.role), !notify);
    if (!g_sync_stop_sent) {
        g_sync_stop_sent = 1;
        if (notify) { dd_msg m = {0}; m.type = DD_MSG_SYNC_STOP; m.u.load.slot = reason; outgoing_send(NULL, 0, &m); }
        shop_notice("De spellen lopen niet meer gelijk. Gedeelde acties zijn bij beide spelers gestopt. Sluit beide spellen en start een nieuwe sessie; stuur de logs mee.");
    }
}
static void runtime_sync_stop(uint32_t reason) { runtime_stop_shared(reason, 1); }

static void log_town_action(const char *phase, const dd_action *a, uint32_t request_id, uint32_t sequence) {
    if (!a) return;
    if (a->kind == DD_ACT_ASSIGN_HERO) {
        dd_log("%s (request=%u seq=%u activity=%u slot=%u hero=%u)", phase, request_id, sequence,
               a->u.assign.activity, a->u.assign.slot, a->u.assign.hero_id);
    } else if (a->kind == DD_ACT_BUY_UPGRADE) {
        dd_log("%s (request=%u seq=%u tree=%08x code=%c hero=%u)", phase, request_id, sequence,
               a->u.upgrade.tree, a->u.upgrade.code, a->u.upgrade.hero_id);
    } else if(a->kind==DD_ACT_RECRUIT) {
        dd_log("%s (request=%u seq=%u Stagecoach hero=%u)",phase,request_id,sequence,a->u.hero.hero_id);
    } else if(a->kind==DD_ACT_DISMISS) {
        dd_log("%s (request=%u seq=%u dismiss hero=%u)",phase,request_id,sequence,a->u.hero.hero_id);
    } else if(a->kind==DD_ACT_WAGON_SELL) {
        dd_log("%s (request=%u seq=%u Trinket sale slot=%u type=%08x item=%08x proceeds=%u)",phase,request_id,sequence,
               a->u.items.slot,a->u.items.type,a->u.items.id,a->u.items.cost);
    } else if(a->kind==DD_ACT_WAGON_BUY) {
        dd_log("%s (request=%u seq=%u Nomad slot=%u type=%08x item=%08x price=%u)",phase,request_id,sequence,
               a->u.items.slot,a->u.items.type,a->u.items.id,a->u.items.cost);
    } else {
        dd_log("%s (request=%u seq=%u action=%u)", phase, request_id, sequence, a->kind);
    }
}

static int battle_apply(const dd_action *a);
static int item_apply(const dd_action *a);
static int hero_owners_apply(const dd_action *a);
static int prepare_apply(const dd_action *a);
static int runtime_apply_inner(void *unused, const dd_action *a) {
    (void)unused;
    if (!a || !dd_action_valid(a)) return 0;
    if(a->kind==DD_ACT_OWNERS)return hero_owners_apply(a);
    if(a->kind==DD_ACT_PREPARE)return prepare_apply(a);
    if(a->kind==DD_ACT_EMBARK)return embark_apply(a);
    if(a->kind==DD_ACT_SUPPLIES)return provisions_apply(a);
    if(a->kind==DD_ACT_DEPARTURE)return departure_apply(a);
    if(a->kind==DD_ACT_TRAVEL)return walk_travel_apply(a);
    if(a->kind==DD_ACT_USE)return walk_use_apply(a);
    if(a->kind==DD_ACT_BATTLE)return battle_apply(a);
    if(a->kind==DD_ACT_ITEM)return item_apply(a);
    if(a->kind==DD_ACT_RECRUIT)return recruit_apply(a);
    if(a->kind==DD_ACT_WAGON_BUY)return shop_apply(a);
    if(a->kind==DD_ACT_WAGON_SELL)return sale_apply(a);
    if(a->kind==DD_ACT_DISMISS || a->kind==DD_ACT_HEIRLOOM_EXCHANGE || a->kind>=DD_ACT_RENAME)return hamlet_apply(a);
    if (a->kind == DD_ACT_BUY_UPGRADE) {
        if (!g_original_buy_upgrade) return 0;
        uint8_t request[12] = {0};
        memcpy(request, &a->u.upgrade.tree, sizeof a->u.upgrade.tree);
        request[4] = a->u.upgrade.code;
        memcpy(request + 8, &a->u.upgrade.hero_id, sizeof a->u.upgrade.hero_id);
        int accepted = g_original_buy_upgrade(request) != 0;
        dd_log("upgrade native result tree=%08x code=%c hero=%u accepted=%d",
               a->u.upgrade.tree, a->u.upgrade.code, a->u.upgrade.hero_id, accepted);
        if (accepted) {
            hamlet_upgrade_refresh(a->u.upgrade.hero_id);
            hamlet_refresh_schedule(a->u.upgrade.hero_id);
        }
        return accepted;
    }
    if (a->kind != DD_ACT_ASSIGN_HERO || !g_original_assign) return 0;
    unsigned kind = a->u.assign.activity;
    if (kind >= DD_ACTIVITY_COUNT_ || a->u.assign.slot >= DD_ACTIVITY_SLOTS) return 0;
    /* The model first: the building's screen may never have been open on this copy. */
    void *activity = activity_in_town(g_town_object(), (int)kind);
    if (!activity) activity = g_activity[kind];
    else if (activity != g_activity[kind]) dd_log("hero placement: activity %u taken from the town model", kind);
    if (!activity || g_activity_kind(activity) != (int)kind) return 0;
    int slot = (int)a->u.assign.slot;
    uint8_t result[DD_RESULT_BYTES];
    memset(result, 0, sizeof result);
    if (!WriteProcessMemory(GetCurrentProcess(), (uint8_t *)activity + DD_ACTIVITY_SLOT_OFFSET, &slot, sizeof slot, NULL)) return 0;
    (void)g_original_assign(activity, result, a->u.assign.hero_id);
    int accepted = result[0] != 0;
    if (!accepted) dd_log("local game refused hero placement (role=%s activity=%u slot=%u hero=%u)",
                          dd_role_name(g_cfg.role), kind, a->u.assign.slot, a->u.assign.hero_id);
    return accepted != 0;
}

static int runtime_apply(void *unused,const dd_action *a) {
    g_game_apply_depth++;
    int result=runtime_apply_inner(unused,a);
    g_game_apply_depth--;return result;
}

static uintptr_t __fastcall hook_assign_hero(void *activity, void *result, uint32_t hero_id) {
    if (!g_original_assign) return 0;
    if (runtime_shared_blocked() || runtime_waiting_for_peer()) return 0;
    if (g_cfg.role == DD_ROLE_GUEST && InterlockedCompareExchange(&g_campaign_active, 0, 0)) {
        int kind = activity_kind(activity), slot = -1;
        if (kind < 0 || kind >= DD_ACTIVITY_COUNT_ ||
            !safe_read((uintptr_t)activity + DD_ACTIVITY_SLOT_OFFSET, &slot, sizeof slot) ||
            slot < 0 || slot >= DD_ACTIVITY_SLOTS || !InterlockedCompareExchange(&g_session_ready, 0, 0)) {
            dd_log("guest hero placement held: no valid shared activity context or session");
            return 0;
        }
        dd_action a;
        memset(&a, 0, sizeof a);
        a.kind = DD_ACT_ASSIGN_HERO;
        a.u.assign.activity = (uint8_t)kind;
        a.u.assign.slot = (uint8_t)slot;
        a.u.assign.hero_id = hero_id;
        uint32_t request_id = dd_town_guest_local(&g_town, &a);
        log_town_action("guest sent hero placement request to host", &a, request_id, 0);
        return 0;
    }
    int kind = -1, slot = -1;
    int observe = InterlockedCompareExchange(&g_session_ready, 0, 0) &&
                  InterlockedCompareExchange(&g_campaign_active, 0, 0) &&
                  g_cfg.role == DD_ROLE_HOST && result;
    if (observe && (!safe_read((uintptr_t)activity + DD_ACTIVITY_SLOT_OFFSET, &slot, sizeof slot) ||
                    (kind = activity_kind(activity)) < 0 || kind >= DD_ACTIVITY_COUNT_ ||
                    slot < 0 || slot >= DD_ACTIVITY_SLOTS)) observe = 0;
    uintptr_t ret = g_original_assign(activity, result, hero_id);
    uint8_t accepted = 0;
    if (!observe || !safe_read((uintptr_t)result, &accepted, sizeof accepted) || !accepted) return ret;
    dd_action a;
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ASSIGN_HERO;
    a.u.assign.activity = (uint8_t)kind;
    a.u.assign.slot = (uint8_t)slot;
    a.u.assign.hero_id = hero_id;
    dd_town_host_observed(&g_town, &a);
    dd_log("host committed hero placement (activity=%d slot=%d hero=%u)", kind, slot, hero_id);
    return ret;
}

static int __fastcall hook_buy_upgrade(const void *request) {
    if (!g_original_buy_upgrade) return 0;
    if (runtime_shared_blocked() || runtime_waiting_for_peer()) return 0;
    uint8_t bytes[12];
    if (!safe_read((uintptr_t)request, bytes, sizeof bytes)) return 0;
    dd_action a;
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_BUY_UPGRADE;
    memcpy(&a.u.upgrade.tree, bytes, sizeof a.u.upgrade.tree);
    a.u.upgrade.code = bytes[4];
    memcpy(&a.u.upgrade.hero_id, bytes + 8, sizeof a.u.upgrade.hero_id);
    if (!dd_action_valid(&a)) return 0;
    if (g_cfg.role == DD_ROLE_GUEST && InterlockedCompareExchange(&g_campaign_active, 0, 0)) {
        if (!InterlockedCompareExchange(&g_session_ready, 0, 0)) {
            dd_log("guest upgrade held: no host session");
            return 0;
        }
        uint32_t request_id = dd_town_guest_local(&g_town, &a);
        log_town_action("guest sent upgrade request to host", &a, request_id, 0);
        return 0;
    }
    g_game_apply_depth++;
    int accepted = g_original_buy_upgrade(request);
    g_game_apply_depth--;
    if (accepted && g_cfg.role == DD_ROLE_HOST &&
        InterlockedCompareExchange(&g_session_ready, 0, 0) &&
        InterlockedCompareExchange(&g_campaign_active, 0, 0)) {
        dd_town_host_observed(&g_town, &a);
        hamlet_upgrade_refresh(a.u.upgrade.hero_id);
        hamlet_refresh_schedule(a.u.upgrade.hero_id);
        log_town_action("host committed upgrade", &a, 0, g_town.applied_seq);
    }
    return accepted;
}

static uintptr_t __fastcall hook_slot_widget_ctor(void *widget, void *activity, int slot_index) {
    uintptr_t ret = g_original_widget_ctor ? g_original_widget_ctor(widget, activity, slot_index) : 0;
    if (InterlockedCompareExchange(&g_campaign_active, 0, 0) && slot_index >= 0 && slot_index < DD_ACTIVITY_SLOTS) {
        int kind = activity_kind(activity);
        if (kind >= 0 && kind < DD_ACTIVITY_COUNT_) {
            g_activity[kind] = activity;
            dd_log("activity panel ready for town sync (activity=%d)", kind);
        } else if (InterlockedCompareExchange(&g_unknown_activity_logged, 1, 0) == 0) {
            dd_log("an activity panel class was not mapped for placement sync");
        }
    }
    return ret;
}

static int bag_drag_refused(void *ui, int on_shop);
static void supply_drag_pickup(void *ui);
static int supply_drag_finish(void *ui);
static void bag_drag_done(void);
#include "dd_hamlet_bridge.inc"
#include "dd_supplies_bridge.inc"
#include "dd_departure_bridge.inc"
#include "dd_frontend_bridge.inc"
#include "dd_walk_bridge.inc"

static int install_hooks(void) {
    dd_hook h[80] = {0};
    unsigned prepared = 0, applied = 0;
    unsigned hamlet_count=(unsigned)(sizeof g_hamlet_hooks/sizeof *g_hamlet_hooks);
    unsigned supply_count=(unsigned)(sizeof g_supply_hooks/sizeof *g_supply_hooks);
    unsigned departure_count=(unsigned)(sizeof g_departure_hooks/sizeof *g_departure_hooks);
    unsigned extra=hamlet_count+supply_count+departure_count+(unsigned)(sizeof g_walk_hooks/sizeof *g_walk_hooks),total=9+extra;
#define RUNTIME_HOOK_SPEC(i) ((i)<hamlet_count?&g_hamlet_hooks[i]:(i)<hamlet_count+supply_count?\
    &g_supply_hooks[(i)-hamlet_count]:(i)<hamlet_count+supply_count+departure_count?\
    &g_departure_hooks[(i)-hamlet_count-supply_count]:&g_walk_hooks[(i)-hamlet_count-supply_count-departure_count])
    if(total>sizeof h/sizeof *h || !departure_start_ready() || !hamlet_bind_native() || !supplies_bind_native())return 0;
    uintptr_t targets[] = {g_assign_va, g_widget_ctor_va, g_buy_upgrade_va, g_wagon_va,g_recruit_va,g_recruit_show_va,g_recruit_hide_va,g_roster_drop_va,g_sell_va};
    uintptr_t hooks[] = {(uintptr_t)hook_assign_hero, (uintptr_t)hook_slot_widget_ctor, (uintptr_t)hook_buy_upgrade,
                         (uintptr_t)hook_wagon_buy,(uintptr_t)hook_recruit,(uintptr_t)hook_recruit_show,(uintptr_t)hook_recruit_hide,(uintptr_t)hook_roster_drop,(uintptr_t)hook_wagon_sell};
    size_t sizes[] = {18, 15, 16, 20,19,20,18,16,13};
    for (; prepared < 9; prepared++)
        if (!hook_prepare(&h[prepared], (void *)targets[prepared], hooks[prepared], sizes[prepared])) goto fail;
    for(;prepared<total;prepared++) {
        const hamlet_hook_spec *spec=RUNTIME_HOOK_SPEC(prepared-9);uintptr_t target=hamlet_address(spec->name);
        for(unsigned prior=0;prior<prepared;prior++)if((uintptr_t)h[prior].target==target) {
            dd_log("duplicate hook target refused: %s",spec->name);goto fail;
        }
        if(!target || target == (uintptr_t)g_raid_start_hook.target ||
           !hook_prepare(&h[prepared],(void *)target,spec->replacement,spec->length))goto fail;
    }
    g_original_assign = (assign_hero_fn)h[0].trampoline;
    g_original_widget_ctor = (slot_widget_ctor_fn)h[1].trampoline;
    g_original_buy_upgrade = (buy_upgrade_fn)h[2].trampoline;
    g_original_wagon = (wagon_buy_fn)h[3].trampoline;
    g_original_recruit=(recruit_fn)h[4].trampoline;
    g_original_recruit_show=(recruit_panel_fn)h[5].trampoline;
    g_original_recruit_hide=(recruit_panel_fn)h[6].trampoline;
    g_original_roster_drop=(roster_drop_fn)h[7].trampoline;
    g_original_sell=(wagon_sell_fn)h[8].trampoline;
    for(unsigned i=0;i<extra;i++)memcpy(RUNTIME_HOOK_SPEC(i)->original,&h[9+i].trampoline,sizeof(void *));
    for (; applied < total; applied++)
        if (!hook_apply(&h[applied])) goto fail;
    if (!embark_bind()) goto fail;
    if (!quest_return_bind()) { embark_unbind(); goto fail; }
    dd_log("town hooks installed: placement, upgrades, Nomad buy/sell, Stagecoach, Hamlet commands, supplies and departure (%u hooks)",total);
    departure_bind_rand_s();
    g_town_depart=(town_depart_fn)hamlet_address("town_depart");
    g_party_select=(party_select_fn)hamlet_address("party_select");
    g_item_clear=(item_clear_fn)hamlet_address("item_clear");
    return 1;
fail:
    while (applied)
        hook_restore(&h[--applied]);
    while (prepared)
        VirtualFree(h[--prepared].trampoline, 0, MEM_RELEASE);
    g_original_assign = NULL;
    g_original_widget_ctor = NULL;
    g_original_buy_upgrade = NULL;
    g_original_wagon = NULL;
    g_original_recruit=NULL;g_original_recruit_show=NULL;g_original_recruit_hide=NULL;
    g_original_roster_drop=NULL;
    g_original_sell=NULL;
    for(unsigned i=0;i<extra;i++)memset(RUNTIME_HOOK_SPEC(i)->original,0,sizeof(void *));
    return 0;
#undef RUNTIME_HOOK_SPEC
}

static void hello_message(dd_msg *m, const char *name) {
    memset(m, 0, sizeof *m);
    m->type = DD_MSG_HELLO;
    m->u.hello.protocol = DD_PROTO_VERSION;
    memcpy(m->u.hello.mod_version, "0.5.0-test56", sizeof "0.5.0-test56");
    memcpy(m->u.hello.build, g_build_id, sizeof g_build_id);
    size_t n = strlen(name);
    if (n >= sizeof m->u.hello.name) n = sizeof m->u.hello.name - 1;
    memcpy(m->u.hello.name, name, n);
    m->u.hello.name[n] = 0;
}

static int receive_hello(dd_conn *c, dd_msg *hello) {
    ULONGLONG until = GetTickCount64() + DD_HELLO_TIMEOUT_MS;
    while (GetTickCount64() < until) {
        dd_poll_result p = dd_conn_poll(c, 250, hello);
        if (p == DD_POLL_CLOSED) return 0;
        if (p == DD_POLL_TIMEOUT) continue;
        return hello->type == DD_MSG_HELLO && hello->u.hello.protocol == DD_PROTO_VERSION &&
               memcmp(hello->u.hello.build, g_build_id, sizeof g_build_id) == 0;
    }
    return 0;
}

static int exchange_hello(dd_conn *c, int host) {
    dd_msg local, remote;
    hello_message(&local, host ? "Host" : "Guest");
    if (!host) {
        if (!dd_conn_send(c, &local)) return 0;
        return receive_hello(c, &remote);
    }
    if (!receive_hello(c, &remote)) return 0;
    return dd_conn_send(c, &local);
}

static int session_loop(dd_conn *c, int host) {
    InterlockedExchange(&g_host_refused_count, 0);
    InterlockedExchange(&g_session_ready, 1);
    InterlockedExchange(&g_session_seen, 1);
    dd_log("direct TCP session ready (role=%s)", dd_role_name(g_cfg.role));
    for (;;) {
        dd_msg m;
        while (queue_pop(&g_outgoing, &m))
            if (!dd_conn_send(c, &m)) return 0;
        dd_poll_result p = dd_conn_poll(c, 20, &m);
        if (p == DD_POLL_CLOSED) return 0;
        if (p != DD_POLL_MSG) continue;
        if (m.type == DD_MSG_SYNC_STOP) { InterlockedExchange(&g_peer_sync_stop, (LONG)m.u.load.slot); continue; }
        if (!host && m.type == DD_MSG_WALK) {
            walk_receive(&m);
            continue;
        }
        if (host && m.type == DD_MSG_WALK_INTENT) {
            walk_intent_receive(&m);
            continue;
        }
        if (m.type == DD_MSG_LOAD) {
            if (InterlockedExchange(&g_peer_load_slot, (LONG)m.u.load.slot) != (LONG)m.u.load.slot)
                dd_log("Start screen: the other copy waits at its save screen for slot %u", m.u.load.slot);
            continue;
        }
        if ((host && m.type == DD_MSG_TOWN_REQUEST) ||
            (!host && (m.type == DD_MSG_TOWN_APPLY || m.type == DD_MSG_TOWN_REJECT))) {
            if (!queue_push(&g_incoming, &m)) {
                InterlockedExchange(&g_queue_failed,1);dd_log("town inbound queue full; rejoin required");
            }
        }
    }
}

static DWORD WINAPI network_main(void *unused) {
    (void)unused;
    if (!dd_net_startup()) { dd_log("network startup failed"); return 0; }
    if (g_cfg.role == DD_ROLE_HOST) {
        /* DDCoop's launcher relays this stream over Steam on the same PC. Never expose a game port. */
        dd_socket listener = dd_net_listen(g_cfg.port ? g_cfg.port : DD_TOWN_PORT_DEFAULT, 0);
        if (listener == DD_NO_SOCKET) { dd_log("could not listen on configured TCP port"); return 0; }
        dd_log("waiting for one relayed guest on loopback port %d", g_cfg.port ? g_cfg.port : DD_TOWN_PORT_DEFAULT);
        for (;;) {
            dd_socket s = dd_net_accept(listener, 1000);
            if (s == DD_NO_SOCKET) continue;
            dd_conn c;
            if (!dd_conn_init(&c, s)) { dd_net_close(s); continue; }
            if (dd_conn_handshake_host(&c, g_cfg.password, DD_HELLO_TIMEOUT_MS) && exchange_hello(&c, 1)) {
                if (!session_loop(&c, 1)) dd_log("guest disconnected");
                InterlockedExchange(&g_session_lost, 1);
            } else dd_log("guest rejected or protocol/build mismatch");
            InterlockedExchange(&g_session_ready, 0);
            InterlockedExchange(&g_peer_load_slot, 0);
            queue_clear(&g_incoming);
            queue_clear(&g_outgoing);
            dd_conn_free(&c);
            if (InterlockedCompareExchange(&g_session_lost, 0, 0)) { dd_net_close(listener); return 0; }
        }
    }
    if (g_cfg.role == DD_ROLE_GUEST) {
        for (;;) {
            dd_socket s = dd_net_connect(g_cfg.host, g_cfg.port ? g_cfg.port : DD_TOWN_PORT_DEFAULT, 2000);
            if (s == DD_NO_SOCKET) { Sleep(1000); continue; }
            dd_conn c;
            if (!dd_conn_init(&c, s)) { dd_net_close(s); Sleep(1000); continue; }
            if (dd_conn_handshake_guest(&c, g_cfg.password, DD_HELLO_TIMEOUT_MS) && exchange_hello(&c, 0)) {
                if (!session_loop(&c, 0)) dd_log("host disconnected");
                InterlockedExchange(&g_session_lost, 1);
            } else {
                dd_log("host rejected or protocol/build mismatch");
                InterlockedIncrement(&g_host_refused_count);
            }
            InterlockedExchange(&g_session_ready, 0);
            InterlockedExchange(&g_peer_load_slot, 0);
            queue_clear(&g_incoming);
            dd_conn_free(&c);
            if (InterlockedCompareExchange(&g_session_lost, 0, 0)) return 0;
            Sleep(1000);
        }
    }
    return 0;
}

int dd_runtime_init(const dd_config *cfg, const uint8_t build_id[8], uint64_t assign_hero_va,
                    uint64_t slot_widget_ctor_va, uint64_t buy_upgrade_va,
                    uint64_t wagon_buy_va, uint64_t trinket_price_va,
                    uint64_t recruit_va,uint64_t recruit_show_va,uint64_t recruit_hide_va,
                    uint64_t roster_add_va,uint64_t roster_count_va,uint64_t roster_capacity_va,uint64_t roster_drop_va,uint64_t sell_va) {
    if (!cfg || !build_id || !assign_hero_va || !slot_widget_ctor_va || !buy_upgrade_va || !wagon_buy_va || !trinket_price_va ||
        !recruit_va || !recruit_show_va || !recruit_hide_va || !roster_add_va || !roster_count_va || !roster_capacity_va || !roster_drop_va || !sell_va) return 0;
    if (InterlockedCompareExchange(&g_initialized, 1, 0)) return 1;
    frontend_bind();
    g_cfg = *cfg;
    memcpy(g_build_id, build_id, sizeof g_build_id);
    g_assign_va = (uintptr_t)assign_hero_va;
    g_widget_ctor_va = (uintptr_t)slot_widget_ctor_va;
    g_buy_upgrade_va = (uintptr_t)buy_upgrade_va;
    g_wagon_va=(uintptr_t)wagon_buy_va;g_trinket_price=(trinket_price_fn)(uintptr_t)trinket_price_va;
    g_recruit_va=(uintptr_t)recruit_va;g_recruit_show_va=(uintptr_t)recruit_show_va;g_recruit_hide_va=(uintptr_t)recruit_hide_va;
    g_roster_add=(roster_add_fn)(uintptr_t)roster_add_va;g_roster_count=(roster_size_fn)(uintptr_t)roster_count_va;
    g_roster_capacity=(roster_size_fn)(uintptr_t)roster_capacity_va;g_roster_drop_va=(uintptr_t)roster_drop_va;
    g_sell_va=(uintptr_t)sell_va;
    queue_init(&g_incoming);
    queue_init(&g_outgoing);
    dd_town_init(&g_town, NULL, runtime_apply, NULL, outgoing_send);
    return 1;
}

void dd_runtime_campaign_active(int active) {
    InterlockedExchange(&g_campaign_active, active != 0);
    if (!active) {
        incoming_clear_inactive();
        queue_clear(&g_outgoing);
        return;
    }
    if (g_cfg.role == DD_ROLE_OFF || !g_cfg.password[0]) return;
    if (InterlockedCompareExchange(&g_started, 0, 0) == 0) dd_log("co-op campaign active; starting direct TCP transport");
    runtime_transport_start();
}
/* The connection between the two games. Since protocol 22 it starts at the start screen already (frontend_poll), so that both
 * copies can load the campaign together; otherwise when the co-op campaign is active. Once per game. */
static void runtime_transport_start(void) {
    if (g_cfg.role == DD_ROLE_OFF || !g_cfg.password[0]) return;
    if (InterlockedCompareExchange(&g_started, 1, 0) == 0) {
        g_network_thread = CreateThread(NULL, 0, network_main, NULL, 0, NULL);
        if (!g_network_thread) {
            InterlockedExchange(&g_started, 0);
            dd_log("could not start network thread");
            return;
        }
        CloseHandle(g_network_thread);
        InterlockedExchange64(&g_transport_started_at, (LONG64)GetTickCount64());
        dd_log("direct TCP transport started (role=%s)", dd_role_name(g_cfg.role));
    }
}

/* A guest that never reached a host. Nothing listened in the owner's run of 7 Nov 17:40: the host's game had been started alone,
 * for a new campaign, so it had no session, and the guest's window sat in town for minutes without a word. */
static const char g_host_absent[] = "Nog geen verbinding met de host. Draait het spel van de host via Host in DDCoop, met deze campagne geladen? "
                                    "Een spel dat alleen is gestart om een nieuwe campagne te maken, deelt niets: de host sluit het en klikt op Host.";
#define DD_HOST_ABSENT_MS 20000u
static const char g_host_silent[] = "Het spel van de host antwoordt niet. Heeft de host de co-op campagne al geladen? Zo ja: de host sluit het "
                                    "spel en klikt opnieuw op Host, daarna join je opnieuw.";
/* What the F9 panel says, from the runtime itself: the launcher only knows its own connection. */
static void runtime_publish_state(void) {
    int state = DD_OVERLAY_WAITING;
    if (InterlockedCompareExchange(&g_session_ready, 0, 0) && !g_town.needs_resync) state = DD_OVERLAY_CONNECTED;
    else if (g_town.needs_resync || InterlockedCompareExchange(&g_session_lost, 0, 0) ||
             InterlockedCompareExchange(&g_session_seen, 0, 0)) state = DD_OVERLAY_LOST;
    else if (InterlockedCompareExchange(&g_host_refused_count, 0, 0) >= DD_HOST_REFUSED_NOTICE) state = DD_OVERLAY_REFUSED;
    dd_overlay_session(state);
    if (state == DD_OVERLAY_CONNECTED) {
        shop_notice_clear(g_wait_for_peer);
        shop_notice_clear(g_host_silent);
        shop_notice_clear(g_host_absent);
    }
    if (state == DD_OVERLAY_REFUSED) shop_notice(g_host_silent);
    else if (state == DD_OVERLAY_WAITING && g_cfg.role == DD_ROLE_GUEST && InterlockedCompareExchange(&g_campaign_active, 0, 0)) {
        uint64_t started = (uint64_t)InterlockedCompareExchange64(&g_transport_started_at, 0, 0);
        if (started && GetTickCount64() - started >= DD_HOST_ABSENT_MS) shop_notice(g_host_absent);
    }
}
/* The launcher's game alone, for a new campaign (role off, DDCOOP_NEW_SLOT): it has no session and shares nothing, also not
 * once the tutorial is over. In the owner's run of 7 Nov 17:37-17:42 the tutorial was played out, the guest's window was started
 * while this game was still running, and two recruits stayed on this side. So once the town is on screen (its display there, no
 * raid, for a few seconds: the tutorial ends in town), say what to do. Once per game. */
static const char g_alone_done[] = "Je bent in het dorp: de tutorial is klaar. Dit spel is alleen gestart om de campagne te maken en deelt niets "
                                   "met je vriend. Sluit het spel en klik in DDCoop op Host om samen te spelen.";
#define DD_ALONE_TOWN_MS 3000u
static uint64_t g_alone_town_since;
static int g_alone_told;
static void alone_town_poll(void) {
    dd_hamlet s;
    walk_view v;
    uint64_t panel = 0, now;
    if (g_alone_told || g_cfg.role != DD_ROLE_OFF || !g_fe_new_slot || !InterlockedCompareExchange(&g_campaign_active, 0, 0)) return;
    if (g_walk_view(&v) || !g_hamlet_open_model(&s) || dd_hamlet_panel(&s, 0xec00a0, &panel) != 1) {
        g_alone_town_since = 0;
        return;
    }
    now = GetTickCount64();
    if (!g_alone_town_since) {
        g_alone_town_since = now;
        return;
    }
    if (now - g_alone_town_since < DD_ALONE_TOWN_MS) return;
    g_alone_told = 1;
    dd_log("New campaign: in town in the game started alone; told the player to close it and click Host");
    shop_notice(g_alone_done);
}
static void runtime_poll_once(void);
/* The game can pump events from inside what an order runs (its message dialog, 23601b), and that comes back here. A nested pump took the next order while the first had not finished:
 * "sequence failed", and the session stopped (owner's run of 6 Nov 18:10:35). Orders wait until the one running is done. */
static int g_poll_running;
void dd_runtime_poll(void) {
    if (g_poll_running) return;
    g_poll_running = 1;
    runtime_poll_once();
    g_poll_running = 0;
}
static void runtime_poll_once(void) {
    if (!InterlockedCompareExchange(&g_initialized, 0, 0)) return;
    LONG peer_stop = InterlockedExchange(&g_peer_sync_stop, 0);
    if (peer_stop) runtime_stop_shared((uint32_t)peer_stop, 0);
    frontend_poll();
    runtime_publish_state();
    alone_town_poll();
    if (!InterlockedCompareExchange(&g_campaign_active, 0, 0)) {
        g_shop_status = -1;
        g_shop_notice[0] = 0;
        g_shop_request = 0;
        g_sell_request = 0;
        hamlet_reset();
        provisions_reset();
        departure_reset();
        walk_reset();
        g_recruit_panel=0;g_recruit_waiting=0;g_recruit_request=0;
        memset(g_activity, 0, sizeof g_activity);
        incoming_clear_inactive();
        return;
    }
    if (g_cfg.role == DD_ROLE_OFF || !g_cfg.password[0]) return;
    if (InterlockedCompareExchange(&g_hooks_installed, 1, 0) == 0) {
        if (!install_hooks()) {
            InterlockedExchange(&g_hooks_installed, -1);
            dd_log("town placement hooks could not be installed; sync remains off");
            return;
        }
    } else if (InterlockedCompareExchange(&g_hooks_installed, 0, 0) < 0) return;
    hamlet_refresh_poll();
    hamlet_party_show_poll();
    departure_poll();
    (void)runtime_shared_blocked(); /* Pending dismissal blocks input, but this pump must finish it. */
    if (!InterlockedCompareExchange(&g_session_ready, 0, 0)) return;
    if(InterlockedExchange(&g_queue_failed,0)) {
        g_town.needs_resync=1;
        shop_notice("De gedeelde actiewachtrij is vol. Sluit beide spellen en start de co-op sessie opnieuw.");
    }
    if(g_town.needs_resync)return;
    if(hamlet_dismiss_poll())return;
    provisions_poll();
    embark_poll();
    if(g_town.needs_resync)return;
    walk_poll();
    dd_msg m;
    while (queue_peek(&g_incoming, &m)) {
        if (prepare_gate(&m)) break;
        if (hero_owners_gate(&m)) break;
        if (walk_gate(&m)) break;
        if (battle_gate(&m)) break;
        if (item_gate(&m)) break;
        if ((m.type == DD_MSG_TOWN_REQUEST || m.type == DD_MSG_TOWN_APPLY) && hamlet_dismiss_gate(&m)) break;
        if ((m.type == DD_MSG_TOWN_REQUEST || m.type == DD_MSG_TOWN_APPLY) && provisions_gate(&m)) break;
        if((m.type==DD_MSG_TOWN_REQUEST || m.type==DD_MSG_TOWN_APPLY) &&
           m.u.town.action.kind==DD_ACT_RECRUIT && !g_recruit_can_apply()) {
            if(!g_recruit_waiting) {
                g_recruit_waiting=1;
                dd_log("Stagecoach action waits a moment for the panel that just opened (role=%s hero=%u)",dd_role_name(g_cfg.role),m.u.town.action.u.hero.hero_id);
            }
            break;
        }
        queue_pop(&g_incoming,&m);g_recruit_waiting=0;
        if (g_cfg.role == DD_ROLE_HOST && m.type == DD_MSG_TOWN_REQUEST) {
            uint32_t accepted_before = g_town.accepted;
            uint32_t rejected_before = g_town.rejected;
            log_town_action("host received guest town request", &m.u.town.action, m.u.town.request_id, 0);
            dd_town_host_request(&g_town, 0, &m);
            if (g_town.accepted != accepted_before)
                log_town_action("host accepted guest town request", &m.u.town.action, m.u.town.request_id,
                                g_town.applied_seq);
            else if (g_town.rejected != rejected_before)
                log_town_action("host rejected guest town request", &m.u.town.action, m.u.town.request_id, 0);
            if(g_town.needs_resync) {
                shop_notice("De gedeelde actie kon niet volledig worden gecontroleerd. Sluit beide spellen en start de co-op sessie opnieuw. Stuur de logs mee.");
                break;
            }
        } else if (g_cfg.role == DD_ROLE_GUEST && m.type == DD_MSG_TOWN_APPLY) {
            if (!dd_town_guest_apply(&g_town, &m)) {
                runtime_sync_stop(1);
                log_town_action("guest failed to apply host town action", &m.u.town.action, m.u.town.request_id,
                                m.seq);
                dd_log("town apply sequence or local game check failed; this copy needs a resync");
                if(m.u.town.action.kind==DD_ACT_WAGON_BUY)
                    shop_notice("De trinketaankoop kon hier niet gelijk worden toegepast. Sluit beide spellen en start de co-op sessie opnieuw. Stuur daarna de logs mee.");
                else if(m.u.town.action.kind==DD_ACT_RECRUIT)
                    shop_notice("De recruit kon hier niet gelijk worden toegepast. Sluit beide spellen en start de co-op sessie opnieuw. Stuur de logs mee.");
                else if(m.u.town.action.kind==DD_ACT_WAGON_SELL)
                    shop_notice("De verkoop kon hier niet gelijk worden toegepast. Sluit beide spellen en start de co-op sessie opnieuw. Stuur de logs mee.");
                else
                    shop_notice("Een gedeelde wijziging kon hier niet gelijk worden toegepast. Gedeelde acties zijn gestopt. "
                                "Sluit beide spellen en start een nieuwe co-op sessie. Stuur de logs mee.");
                break;
            }
            log_town_action("guest applied host town action", &m.u.town.action, m.u.town.request_id, m.seq);
            hamlet_request_done(m.u.town.request_id,0);
            hunger_request_done(m.u.town.request_id);
            quest_return_request_done(m.u.town.request_id);
            provisions_request_done(m.u.town.request_id,0);
            if (m.u.town.action.kind == DD_ACT_WAGON_BUY && m.u.town.request_id == g_shop_request) g_shop_request = 0;
            if(m.u.town.action.kind==DD_ACT_RECRUIT && m.u.town.request_id==g_recruit_request)g_recruit_request=0;
            if(m.u.town.action.kind==DD_ACT_WAGON_SELL && m.u.town.request_id==g_sell_request)g_sell_request=0;
        } else if (g_cfg.role == DD_ROLE_GUEST && m.type == DD_MSG_TOWN_REJECT) {
            dd_log("host rejected town request %u (reason=%u)", m.u.reject.request_id, m.u.reject.reason);
            hamlet_request_done(m.u.reject.request_id,1);
            hunger_request_done(m.u.reject.request_id);
            quest_return_request_done(m.u.reject.request_id);
            embark_request_done(m.u.reject.request_id);
            loot_request_done(m.u.reject.request_id);
            provisions_request_done(m.u.reject.request_id,1);
            if (g_shop_request && m.u.reject.request_id == g_shop_request) {
                g_shop_request = 0;
                shop_notice("De host heeft deze trinketaankoop geweigerd. Controleer het goud en de winkelvoorraad; "
                            "probeer daarna opnieuw. Stuur de logs mee als dit blijft gebeuren.");
            }
            if(g_recruit_request && m.u.reject.request_id==g_recruit_request) {
                g_recruit_request=0;
                shop_notice("De host kon deze held niet recruiten. Controleer of de kandidaat nog beschikbaar is en het roster plaats heeft.");
            }
            if(g_sell_request && m.u.reject.request_id==g_sell_request) {
                g_sell_request=0;
                shop_notice("De host heeft deze verkoop geweigerd. Controleer of de trinket nog beschikbaar is en de verkoopprijs klopt.");
            }
        }
    }
}

int dd_runtime_session_ready(void) { return InterlockedCompareExchange(&g_session_ready, 0, 0) != 0; }
int dd_runtime_input_stopped(void) {
    return g_cfg.role != DD_ROLE_OFF && InterlockedCompareExchange(&g_campaign_active, 0, 0) && g_town.needs_resync;
}
