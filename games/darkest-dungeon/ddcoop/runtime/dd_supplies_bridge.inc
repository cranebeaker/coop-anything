/* Normal, genuine ProvisionSelect only. All calls run on the game thread; no peer pointers.
 *
 * One shop, shared: the first player to open it fills stock and bag on both copies, whatever the other is looking at. Buying, returning and the refund
 * are model only (buy 6960c0 uses the stock and bag it is given, return 7ebcf0 the item, refund 6e0250 the campaign), so the other copy needs no panel for them.
 * Opening later shows what is there, because its panel then does not fill again. Back closes the shop for both: everything is refunded on both. */
typedef void(__fastcall *supply_buy_fn)(void *,int);
typedef void(__fastcall *supply_click_fn)(void *,const uint32_t *);
typedef int(__fastcall *supply_return_fn)(void *,int);
typedef void(__fastcall *supply_void_fn)(void);
typedef int(__fastcall *supply_price_fn)(void *,void *);
static supply_buy_fn g_supply_buy;
static supply_click_fn g_supply_click;
static supply_return_fn g_supply_return;
static supply_void_fn g_supply_cancel;
static supply_price_fn g_supply_price[4];
typedef void(__fastcall *supply_back_fn)(void *);
static supply_back_fn g_supply_back;
typedef struct {uint64_t campaign,panel,stock,bag;} supply_model;
static uint32_t g_supply_request;
static unsigned g_supply_request_op;
static int g_supply_cancel_local,g_supply_wait;
/* Stock and bag exist without this copy's panel: the panel's own opening must not fill them again. */
typedef void(__fastcall *supply_fill_fn)(void *,void *);
static supply_fill_fn g_supply_fill[2];
static int g_supply_bg;
/* The shop's mission holds from the first open until the shared Back or the departure. */
static int g_supply_quest_known;
static uint32_t g_supply_quest_index,g_supply_quest_key;
/* The guest's clicks, one request at a time: each order is bound to the state before it, so a second click sent at once would always be refused.
 * A refusal after the host's own click is answered by quoting again on the new state. */
typedef struct {unsigned op,tries;uint32_t type,id;} supply_click;
#define DD_SUPPLY_CLICKS 16u
#define DD_SUPPLY_TRIES 3u
#define DD_SUPPLY_STUCK_MS 15000u
#define DD_SUPPLY_CLOSE_MS 3000u
static supply_click g_supply_clicks[DD_SUPPLY_CLICKS],g_supply_sent;
static unsigned g_supply_click_count;
static int g_supply_back_again;
static unsigned g_supply_wait_flags; /* Last logged panel state of a waiting order, plus one. */
static uint64_t g_supply_wait_campaign,g_supply_wait_since,g_supply_close_at;
static dd_action g_supply_last_cancel;
static uint8_t g_supply_last_after[32];
static int g_supply_last_valid;
static int supply_open(supply_model *m,dd_provisions_snapshot *s) {
    const char *stage=NULL;
    if(dd_provisions_capture_ex(shop_read,NULL,image_base(),s,&stage)!=DD_PROVISIONS_OK)return 0;
    uint64_t town=0;
    if(!safe_read(image_base()+0x118e138,&m->campaign,8) || !m->campaign ||
       !safe_read(image_base()+0x118e360,&town,8) || !town)return 0;
    m->panel=town+0x3770;m->stock=m->campaign+0x1618;m->bag=m->campaign+0x15b0;return 1;
}
static int (*g_supply_open)(supply_model *,dd_provisions_snapshot *)=supply_open;
static int supply_active(void) {
    return g_cfg.role!=DD_ROLE_OFF && InterlockedCompareExchange(&g_campaign_active,0,0) &&
        InterlockedCompareExchange(&g_session_ready,0,0) && !g_town.needs_resync;
}
static int supply_message(const dd_msg *m) {
    return m->u.town.action.kind==DD_ACT_SUPPLIES && dd_action_valid(&m->u.town.action) &&
        ((g_cfg.role==DD_ROLE_HOST && m->type==DD_MSG_TOWN_REQUEST) ||
         (g_cfg.role==DD_ROLE_GUEST && m->type==DD_MSG_TOWN_APPLY &&
          g_town.applied_seq!=UINT32_MAX && m->seq==g_town.applied_seq+1));
}
static int supply_head_cancel(void) {
    dd_msg m;return queue_peek(&g_incoming,&m) && supply_message(&m) && m.u.town.action.u.supplies.op==DD_SUPPLY_CANCEL;
}
static int g_supply_own_click;
static int provisions_busy(void) {
    if(g_supply_own_click)return 0;
    dd_msg m;return g_supply_request || g_supply_click_count || g_supply_cancel_local || g_supply_wait ||
        (queue_peek(&g_incoming,&m) && supply_message(&m));
}
/* This player's next purchase takes its turn behind other provisions business (it used to be lost). What else stops shared actions, still does. */
static int supply_input_blocked(void) {
    g_supply_own_click=1;
    int blocked=runtime_shared_blocked();
    g_supply_own_click=0;
    return blocked;
}
static void provisions_leave_town(void) {
    g_supply_bg=g_supply_quest_known=g_supply_back_again=0;g_supply_click_count=0;
}
static void provisions_reset(void) {
    g_supply_request=g_supply_request_op=0;g_supply_cancel_local=g_supply_wait=g_supply_last_valid=0;
    g_supply_wait_flags=0;g_supply_wait_campaign=g_supply_wait_since=g_supply_close_at=0;
    provisions_leave_town();
}
static int supply_fail(const char *why,int drift) {
    dd_log("Supplies refused: %s",why);if(drift)g_town.needs_resync=1;return 0;
}
static int supply_definition(uint32_t type,uint32_t id,uint64_t *out) {
    uint64_t lib=0,span[2];*out=0;
    if(!safe_read(image_base()+0x118e0f8,&lib,8) || !lib || !safe_read((uintptr_t)lib,span,sizeof span) ||
       span[1]<span[0] || (span[1]-span[0])%8 || (span[1]-span[0])/8>16384)return 0;
    for(uint64_t p=span[0];p<span[1];p+=8) {
        uint64_t def=0;uint32_t t=0,k=0;
        if(!safe_read((uintptr_t)p,&def,8) || !def || !safe_read((uintptr_t)def+0x40,&t,4) ||
           !safe_read((uintptr_t)def+0x84,&k,4))return 0;
        if(t==type && k==id){*out=def;return 1;}
    }
    return 0;
}
static int (*g_supply_definition)(uint32_t,uint32_t,uint64_t *)=supply_definition;
static int supply_prices(const supply_model *m,unsigned op,uint32_t type,uint32_t id,uint32_t *gold,uint32_t *shards) {
    uint64_t def=0;int a,b;
    if(op==DD_SUPPLY_CANCEL){a=g_supply_price[2]((void *)(uintptr_t)m->campaign,(void *)(uintptr_t)m->bag);
        b=g_supply_price[3]((void *)(uintptr_t)m->campaign,(void *)(uintptr_t)m->bag);}
    else {if(!g_supply_definition(type,id,&def))return 0;
        a=g_supply_price[0]((void *)(uintptr_t)m->campaign,(void *)(uintptr_t)def);
        b=g_supply_price[1]((void *)(uintptr_t)m->campaign,(void *)(uintptr_t)def);}
    if(a<0 || b<0 || b)return 0; /* First kit normal gold-only supplies; never drop a shard cost. */
    *gold=(uint32_t)a;*shards=(uint32_t)b;return 1;
}
static int supply_compact(void) {
    uint32_t mode=0;uint8_t enabled=0;
    if(!safe_read(image_base()+0x118d514,&mode,4) || !safe_read(image_base()+0x118525a,&enabled,1))return -1;
    return mode==1 || (!mode && enabled);
}
static int (*g_supply_compact)(void)=supply_compact;
static int supply_slot(const dd_provisions_item *items,unsigned count,unsigned index,int compact,unsigned *physical) {
    for(unsigned i=0,visible=0;i<count;i++)if(items[i].quantity) {
        if((compact?visible:i)==index){*physical=i;return 1;}visible++;
    }
    return 0;
}
static int supply_quote(const supply_model *m,const dd_provisions_snapshot *s,unsigned op,unsigned slot,dd_action *a) {
    uint32_t type=0,id=0,gold=0,shards=0;
    if(op!=DD_SUPPLY_CANCEL) {
        unsigned n=op==DD_SUPPLY_BUY?s->stock_slots:s->bag_slots;
        const dd_provisions_item *p=op==DD_SUPPLY_BUY?s->stock:s->bag;
        if(slot>=n || !p[slot].quantity)return 0;type=p[slot].type;id=p[slot].id;
    }
    return supply_prices(m,op,type,id,&gold,&shards) && dd_supplies_quote(s,op,slot,gold,shards,a);
}
static int supply_duplicate_cancel(const dd_provisions_snapshot *s,const dd_action *a) {
    uint8_t now[32];return a->u.supplies.op==DD_SUPPLY_CANCEL && g_supply_last_valid &&
        a->u.supplies.gold==g_supply_last_cancel.u.supplies.gold && /* Field by field: padding differs after decode. */
        a->u.supplies.shards==g_supply_last_cancel.u.supplies.shards &&
        !memcmp(a->u.supplies.before,g_supply_last_cancel.u.supplies.before,32) &&
        dd_supplies_digest(s,now) && !memcmp(now,g_supply_last_after,32);
}
/* The whole state, for when two copies turn out to differ: the check only says that they do. */
static void supply_log_state(const char *why,const dd_provisions_snapshot *s) {
    char line[900];size_t at=0;
    at+=(size_t)snprintf(line+at,sizeof line-at,"Supplies %s: bag",why);
    for(unsigned i=0;i<s->bag_slots && at<sizeof line-40;i++)if(s->bag[i].quantity)
        at+=(size_t)snprintf(line+at,sizeof line-at," %u:%dx%08x",i,(int)s->bag[i].quantity,s->bag[i].id?s->bag[i].id:s->bag[i].type);
    at+=(size_t)snprintf(line+at,sizeof line-at," stock");
    for(unsigned i=0;i<s->stock_slots && at<sizeof line-40;i++)if(s->stock[i].quantity)
        at+=(size_t)snprintf(line+at,sizeof line-at," %u:%d",i,(int)s->stock[i].quantity);
    for(unsigned i=0;i<s->currencies && at<sizeof line-40;i++)
        at+=(size_t)snprintf(line+at,sizeof line-at," money%u:%d",i,(int)s->wallet[i].quantity);
    dd_log("%s",line);
}
/* The game's own lookup, as in the panel's opening (694df0): the mission list at +1230, or the one at +12c0 when the index equals +1220. */
static int supply_quest_at(uint64_t campaign,int32_t index,uint64_t *quest) {
    uint64_t begin=0,end=0;int32_t single=0;
    *quest=0;
    if(!safe_read((uintptr_t)campaign+0x1230,&begin,8) || !safe_read((uintptr_t)campaign+0x1238,&end,8) || end<begin || (end-begin)%0x2f0)return 0;
    if(index>=0 && (uint64_t)index<(end-begin)/0x2f0)*quest=begin+(uint64_t)index*0x2f0;
    else if(safe_read((uintptr_t)campaign+0x1220,&single,4) && index==single)*quest=campaign+0x12c0;
    return *quest!=0;
}
static int supply_quest_read(uint64_t campaign,uint64_t *quest) {
    int32_t index=(int32_t)g_supply_quest_index;
    if(!g_supply_quest_known && !safe_read((uintptr_t)campaign+0x1248,&index,4))return 0;
    return supply_quest_at(campaign,index,quest);
}
static int (*g_supply_quest)(uint64_t,uint64_t *)=supply_quest_read;
/* What the panel's opening does to the model (694ec6, 694eda), without the panel. Stock in place means this was done. */
static int supply_bg_begin(void) {
    supply_model m;dd_provisions_snapshot s;uint64_t quest=0;
    if(g_supply_bg)return 1;
    if(!g_supply_fill[0] || !g_supply_fill[1] || !g_supply_open(&m,&s))return 0;
    if(!s.stock_occupied) {
        if(!g_supply_quest(m.campaign,&quest))return 0;
        g_game_apply_depth++;
        g_supply_fill[0]((void *)(uintptr_t)m.bag,(void *)(uintptr_t)quest);
        g_supply_fill[1]((void *)(uintptr_t)m.bag,(void *)(uintptr_t)quest);
        g_game_apply_depth--;
        dd_log("Supplies: shop filled without the panel");
    }
    g_supply_bg=1;return 1;
}
static void __fastcall hook_supply_fill_free(void *bag,void *quest) {
    if(!g_supply_bg && g_supply_fill[0])g_supply_fill[0](bag,quest);
}
static void __fastcall hook_supply_fill_stock(void *bag,void *quest) {
    if(!g_supply_bg && g_supply_fill[1])g_supply_fill[1](bag,quest);
}
static int provisions_apply(const dd_action *a) {
    supply_model m;dd_provisions_snapshot before,after;uint32_t gold=0,shards=0;
    if(!g_supply_open(&m,&before))return supply_fail("model unreadable",0);
    if(supply_duplicate_cancel(&before,a))return 1;
    unsigned op=a->u.supplies.op;
    if(op==DD_SUPPLY_CANCEL ? (before.panel_visible || before.stock_view || before.bag_view) :
       (!g_supply_bg && (!before.stock_view || !before.bag_view)))return supply_fail("UI not ready",0);
    if(!dd_supplies_before(&before,a) || !supply_prices(&m,op,a->u.supplies.type,a->u.supplies.id,&gold,&shards) ||
       gold!=a->u.supplies.gold || shards!=a->u.supplies.shards) {
        dd_log("Supplies order op=%u slot=%u type=%08x id=%08x gold=%u",op,a->u.supplies.slot,a->u.supplies.type,a->u.supplies.id,a->u.supplies.gold);
        supply_log_state("differ, here",&before);
        return supply_fail("stale state/identity/native quote",0);
    }
    int accepted=1;
    if(op==DD_SUPPLY_BUY) {
        int compact=g_supply_compact();if(compact<0)return 0;
        unsigned raw=a->u.supplies.slot;
        if(compact){raw=0;for(unsigned i=0;i<a->u.supplies.slot;i++)raw+=before.stock[i].quantity!=0;}
        uint64_t capture[3]={m.panel,m.stock,m.bag};g_supply_buy(capture,(int)raw);
    } else if(op==DD_SUPPLY_RETURN) {
        uint64_t begin=0;
        if(!safe_read((uintptr_t)m.bag+8,&begin,8) || !begin)return 0;
        __declspec(align(16)) uint8_t temp[0x5a8];memset(temp,0,sizeof temp);
        g_hamlet_copy(temp,(void *)(uintptr_t)(begin+(uint64_t)a->u.supplies.slot*0x5a8));
        accepted=g_supply_return(temp,1)!=0;
        if(accepted)g_hamlet_swap((void *)(uintptr_t)m.bag,a->u.supplies.slot,temp);
        g_hamlet_destroy(temp);
    } else g_supply_cancel();
    supply_model current;
    if(!g_supply_open(&current,&after) || current.campaign!=m.campaign)return supply_fail("after model/campaign",1);
    if(!accepted || !dd_supplies_after(&before,&after,a)) {
        if(dd_supplies_equal(&before,&after))return supply_fail("game declined without shared effects",0);
        return supply_fail("unexpected/partial native result",1);
    }
    if(op==DD_SUPPLY_CANCEL){g_supply_last_cancel=*a;g_supply_last_valid=dd_supplies_digest(&after,g_supply_last_after);g_supply_bg=g_supply_quest_known=0;}
    dd_log("Supplies result op=%u slot=%u type=%08x id=%08x gold=%u stock=%u bag=%u",op,a->u.supplies.slot,
           a->u.supplies.type,a->u.supplies.id,a->u.supplies.gold,after.stock_occupied,after.bag_occupied);
    return 1;
}
static void supply_dispatch(const dd_action *a,unsigned tries) {
    if(g_cfg.role==DD_ROLE_GUEST) {
        g_supply_request=dd_town_guest_local(&g_town,a);g_supply_request_op=a->u.supplies.op;
        g_supply_sent.op=a->u.supplies.op;g_supply_sent.tries=tries;g_supply_sent.type=a->u.supplies.type;g_supply_sent.id=a->u.supplies.id;
    } else if(!dd_town_host_local(&g_town,a))shop_notice("DDCoop kon deze voorraadactie niet uitvoeren. Controleer het geld en de voorraad; stuur de logs bij een fout.");
}
/* An unanswered request or a waiting order: this click follows it. */
static int supply_click_later(unsigned op,uint32_t type,uint32_t id) {
    dd_msg m;
    if(g_cfg.role!=DD_ROLE_GUEST)return 0;
    if(!g_supply_request && !g_supply_click_count && !(queue_peek(&g_incoming,&m) && supply_message(&m)))return 0;
    if(g_supply_click_count<DD_SUPPLY_CLICKS) {
        supply_click *c=&g_supply_clicks[g_supply_click_count++];
        c->op=op;c->tries=0;c->type=type;c->id=id;
    } else dd_log("Supplies: too many clicks waiting, one dropped");
    return 1;
}
static void supply_send_clicks(void) {
    dd_msg head;
    if(!g_supply_click_count || g_supply_request || (queue_peek(&g_incoming,&head) && supply_message(&head)))return;
    supply_click c=g_supply_clicks[0];
    memmove(g_supply_clicks,g_supply_clicks+1,(--g_supply_click_count)*sizeof *g_supply_clicks);
    supply_model m;dd_provisions_snapshot s;dd_action a;unsigned slot=0;
    if(!g_supply_open(&m,&s)){g_supply_click_count=0;dd_log("Supplies: waiting clicks dropped, model unreadable");return;}
    if(c.op!=DD_SUPPLY_CANCEL) {
        unsigned n=c.op==DD_SUPPLY_BUY?s.stock_slots:s.bag_slots,found=0;
        const dd_provisions_item *p=c.op==DD_SUPPLY_BUY?s.stock:s.bag;
        for(unsigned i=0;i<n && !found;i++)if(p[i].quantity && p[i].type==c.type && p[i].id==c.id){slot=i;found=1;}
        if(!found){dd_log("Supplies: waiting click dropped, item %08x/%08x is gone",c.type,c.id);return;}
    }
    if(!supply_quote(&m,&s,c.op,slot,&a)){dd_log("Supplies: waiting click dropped, no quote for op=%u",c.op);return;}
    supply_dispatch(&a,c.tries);
}
static void __fastcall hook_supply_buy(void *capture,int index) {
    if(runtime_waiting_for_peer())return;
    if(g_game_apply_depth || !supply_active()){if(!runtime_shared_blocked() && g_supply_buy)g_supply_buy(capture,index);return;}
    if(supply_input_blocked())return;
    supply_model m;dd_provisions_snapshot s;uint64_t pointers[3];unsigned slot=0;int compact=g_supply_compact();
    if(index<0 || compact<0 || !g_supply_open(&m,&s) || !safe_read((uintptr_t)capture,pointers,sizeof pointers) ||
       pointers[0]!=m.panel || pointers[1]!=m.stock || pointers[2]!=m.bag ||
       !supply_slot(s.stock,s.stock_slots,(unsigned)index,compact,&slot)){supply_fail("buy input not recognised",0);return;}
    dd_action a;if(!supply_quote(&m,&s,DD_SUPPLY_BUY,slot,&a)){supply_fail("buy quote",0);return;}
    if(!supply_click_later(DD_SUPPLY_BUY,a.u.supplies.type,a.u.supplies.id))supply_dispatch(&a,0);
}
static void __fastcall hook_supply_return(void *capture,const uint32_t *index) {
    if(runtime_waiting_for_peer())return;
    if(g_game_apply_depth || !supply_active()){if(!runtime_shared_blocked() && g_supply_click)g_supply_click(capture,index);return;}
    if(supply_input_blocked())return;
    supply_model m;dd_provisions_snapshot s;uint64_t pointers[2];uint32_t visible=0;unsigned slot=0;
    if(!g_supply_open(&m,&s) || !safe_read((uintptr_t)capture,pointers,sizeof pointers) ||
       pointers[0]!=image_base()+0xebdf38 || pointers[1]!=m.bag || !safe_read((uintptr_t)index,&visible,4) ||
       !supply_slot(s.bag,s.bag_slots,visible,1,&slot)){supply_fail("return input not recognised",0);return;}
    dd_action a;if(!supply_quote(&m,&s,DD_SUPPLY_RETURN,slot,&a)){supply_fail("return quote",0);return;}
    if(!supply_click_later(DD_SUPPLY_RETURN,a.u.supplies.type,a.u.supplies.id))supply_dispatch(&a,0);
}
/* The refund has two callers: the Back button (6e85e0) and the switch back to town (6debf0, itself with two callers).
 * Which code asked for it tells why a player returned to the Hamlet without clicking. */
static void supply_log_caller(void) {
    void *frames[8];
    uintptr_t rva[6] = {0}, base = image_base();
    USHORT count = RtlCaptureStackBackTrace(0, 8, frames, NULL);
    for (USHORT i = 1, n = 0; i < count && n < 6; i++)
        if ((uintptr_t)frames[i] >= base && (uintptr_t)frames[i] - base < 0x2000000) rva[n++] = (uintptr_t)frames[i] - base;
    dd_log("Supplies refund called from %llx %llx %llx %llx %llx %llx", (unsigned long long)rva[0], (unsigned long long)rva[1],
           (unsigned long long)rva[2], (unsigned long long)rva[3], (unsigned long long)rva[4], (unsigned long long)rva[5]);
}
static void __fastcall hook_supply_cancel(void) {
    if(g_game_apply_depth || !supply_active()){if(g_supply_cancel)g_supply_cancel();return;}
    supply_log_caller();
    supply_model m;dd_provisions_snapshot s;
    if(!g_supply_open(&m,&s)){if(g_supply_cancel)g_supply_cancel();return;}
    if(!s.stock_occupied && !s.bag_occupied){g_supply_cancel();g_supply_bg=g_supply_quest_known=0;return;}
    if(supply_head_cancel()) { /* The other copy already ordered Back; let the UI hide, then apply the FIFO head. */
        dd_log("Supplies native Back seen with a shared Back waiting");
        g_supply_close_at=GetTickCount64()+DD_SUPPLY_CLOSE_MS;
        return;
    }
    if(g_supply_request || provisions_busy()) { /* Only a forced close: the button waits in hook_supply_back. */
        supply_fail("Back while a purchase/return was still pending",1);
        shop_notice("Er liep nog een voorraadactie tijdens Terug. Sluit beide spellen en start opnieuw; stuur de logs mee.");return;
    }
    dd_action a;
    if(!supply_quote(&m,&s,DD_SUPPLY_CANCEL,0,&a)) {
        supply_fail("cancel native quote",1);return;
    }
    g_supply_cancel_local=1;g_supply_wait_campaign=m.campaign;
    /* The engine completes its genuine hide first. Do not refund while callbacks own old widgets. */
}
/* Is a refund still on its way here? Opening the shop then would fill what is about to be emptied. */
static int supply_cancel_pending(void) {
    if(g_supply_cancel_local || g_supply_back_again || supply_head_cancel() || (g_supply_request && g_supply_request_op==DD_SUPPLY_CANCEL))return 1;
    for(unsigned i=0;i<g_supply_click_count;i++)if(g_supply_clicks[i].op==DD_SUPPLY_CANCEL)return 1;
    return 0;
}
/* The Back button's own callback (6e85e0). With a purchase or return still on its way, Back follows by itself once that is done. */
static void __fastcall hook_supply_back(void *capture) {
    dd_msg m;
    if(!g_game_apply_depth && supply_active() && supply_cancel_pending()) {
        dd_log("Supplies Back ignored: the refund is on its way");
        return;
    }
    if(!g_game_apply_depth && supply_active() && (g_supply_request || g_supply_click_count ||
       (queue_peek(&g_incoming,&m) && supply_message(&m) && m.u.town.action.u.supplies.op!=DD_SUPPLY_CANCEL))) {
        dd_log("Supplies Back follows a pending purchase/return");
        g_supply_back_again=1;
        return;
    }
    if(g_supply_back)g_supply_back(capture);
}
static const char g_supply_stuck[] = "Een voorraadactie van de andere speler kan hier nog niet worden verwerkt. Sluit het scherm dat nu openstaat; blijft dit staan, stuur dan de logs mee.";
static void supply_wait_over(void) {
    if(g_supply_wait)shop_notice_clear(g_supply_stuck);
    g_supply_wait=0;g_supply_wait_flags=0;g_supply_wait_since=0;
}
/* Back on this copy, because the other player's Back closed the shop. The hook on the refund sees the order at the head of the queue and lets the panel hide first. */
static void supply_close_panel(const supply_model *m) {
    uint64_t now=GetTickCount64();
    uint64_t functor[2]={0,m->panel-0x3770};
    if(g_supply_cancel_local || now<g_supply_close_at || !g_supply_back)return;
    g_supply_close_at=now+DD_SUPPLY_CLOSE_MS;
    dd_log("Supplies: the other player's Back closes this panel");
    g_supply_back(functor);
}
static int provisions_gate(const dd_msg *message) {
    if(!supply_message(message))return 0;
    supply_model m;dd_provisions_snapshot s;
    const dd_action *a=&message->u.town.action;
    if(!g_supply_open(&m,&s))return 0;
    if(g_supply_wait && m.campaign!=g_supply_wait_campaign){supply_fail("campaign changed while waiting",1);return 0;}
    unsigned op=a->u.supplies.op;
    int views=s.stock_view && s.bag_view,any_view=s.stock_view || s.bag_view,can_run;
    if(op==DD_SUPPLY_CANCEL)can_run=!s.panel_visible && !any_view;
    /* The panel's flag flickers while it is open (7 Nov 02:59:36, 1.6 s), and the views last until it has gone: with the views up and the model empty, it is leaving. */
    else can_run=g_supply_bg || (views && (s.stock_occupied || s.bag_occupied)) || (!any_view && supply_bg_begin());
    if(can_run){supply_wait_over();return 0;}
    /* A stale or repeated Back must not close a panel: the apply answers it. */
    if(op==DD_SUPPLY_CANCEL && (supply_duplicate_cancel(&s,a) || !dd_supplies_before(&s,a)))return 0;
    if(op==DD_SUPPLY_CANCEL && s.panel_visible)supply_close_panel(&m);
    unsigned flags=1u+s.panel_visible+2u*s.stock_view+4u*s.bag_view;
    if(flags!=g_supply_wait_flags) {
        g_supply_wait_flags=flags;
        dd_log("Supplies order waits op=%u panel=%u stock_view=%u bag_view=%u",op,s.panel_visible,s.stock_view,s.bag_view);
    }
    uint64_t now=GetTickCount64();
    if(!g_supply_wait_since)g_supply_wait_since=now;
    if(!g_supply_wait){g_supply_wait=1;g_supply_wait_campaign=m.campaign;}
    if(now-g_supply_wait_since>=DD_SUPPLY_STUCK_MS)shop_notice(g_supply_stuck);
    return 1;
}
static void provisions_poll(void) {
    if(g_town.needs_resync)return;
    supply_send_clicks();
    supply_model m;dd_provisions_snapshot s;dd_msg head;
    if(g_supply_back_again && !g_supply_request && !g_supply_click_count &&
       !(queue_peek(&g_incoming,&head) && supply_message(&head) && head.u.town.action.u.supplies.op!=DD_SUPPLY_CANCEL)) {
        g_supply_back_again=0;
        if(g_supply_open(&m,&s) && s.panel_visible && g_supply_back) {
            uint64_t functor[2]={0,m.panel-0x3770};
            g_supply_back(functor);
        }
    }
    if(!g_supply_cancel_local)return;
    if(!g_supply_open(&m,&s) || m.campaign!=g_supply_wait_campaign){supply_fail("local Back lost campaign",1);return;}
    if(s.panel_visible || s.stock_view || s.bag_view)return;
    g_supply_cancel_local=0;
    if(supply_head_cancel())return; /* The other copy's Back arrived during the hide: that order refunds us. */
    if(!s.stock_occupied && !s.bag_occupied){g_supply_bg=g_supply_quest_known=0;return;}
    /* Quoted now: the other player may have bought something during the hide. */
    dd_action a;
    if(!supply_quote(&m,&s,DD_SUPPLY_CANCEL,0,&a)){supply_fail("local Back has no quote",1);return;}
    supply_dispatch(&a,0);
}
static void provisions_request_done(uint32_t request,int rejected) {
    if(!request || g_supply_request!=request)return;
    g_supply_request=g_supply_request_op=0;
    if(!rejected)return;
    supply_model m;dd_provisions_snapshot s;
    if(g_supply_open(&m,&s))supply_log_state("refused by the host, here",&s);
    /* The host had acted first; its order is here by now, so a new quote fits. */
    if(g_supply_sent.tries<DD_SUPPLY_TRIES) {
        if(g_supply_click_count==DD_SUPPLY_CLICKS)g_supply_click_count--;
        memmove(g_supply_clicks+1,g_supply_clicks,g_supply_click_count*sizeof *g_supply_clicks);
        g_supply_clicks[0]=g_supply_sent;g_supply_clicks[0].tries++;g_supply_click_count++;
        return;
    }
    shop_notice("De host heeft deze voorraadactie geweigerd. Controleer of jullie voorraad en missie gelijk zijn; stuur de logs bij een fout.");
}
static const hamlet_hook_spec g_supply_hooks[]={
    {"provisions_buy",(uintptr_t)hook_supply_buy,&g_supply_buy,16},
    {"provision_return_click",(uintptr_t)hook_supply_return,&g_supply_click,18},
    {"provision_cancel",(uintptr_t)hook_supply_cancel,&g_supply_cancel,15},
    {"provision_back",(uintptr_t)hook_supply_back,&g_supply_back,13},
    {"supply_fill_free",(uintptr_t)hook_supply_fill_free,&g_supply_fill[0],18},
    {"supply_fill_stock",(uintptr_t)hook_supply_fill_stock,&g_supply_fill[1],16},
};
static int supplies_bind_native(void) {
    static const struct {const char *name;uint32_t rva;uint8_t code[48];} pins[]={
#include "provisions_native_pins.inc"
    };
    for(unsigned i=0;i<4;i++) {
        uint8_t code[48];uintptr_t at=image_base()+pins[i].rva;
        if(!safe_read(at,code,sizeof code) || memcmp(code,pins[i].code,sizeof code))return 0;
        g_supply_price[i]=(supply_price_fn)at;
    }
    g_supply_return=(supply_return_fn)hamlet_address("provisions_return");return g_supply_return!=NULL;
}
