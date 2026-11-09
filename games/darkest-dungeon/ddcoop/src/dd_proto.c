#include "dd_proto.h"
#include "dd_text.h"

#include <string.h>

/* --- byte writer / reader with bounds checks --- */

typedef struct { uint8_t *p; size_t cap, n; int bad; } wr;
typedef struct { const uint8_t *p; size_t len, n; int bad; } rd;

static void w_u8(wr *w, uint8_t v) { if (w->n + 1 > w->cap) { w->bad = 1; return; } w->p[w->n++] = v; }
static void w_u16(wr *w, uint16_t v) { w_u8(w, (uint8_t)v); w_u8(w, (uint8_t)(v >> 8)); }
static void w_u32(wr *w, uint32_t v) { w_u16(w, (uint16_t)v); w_u16(w, (uint16_t)(v >> 16)); }
static void w_u64(wr *w, uint64_t v) { w_u32(w, (uint32_t)v); w_u32(w, (uint32_t)(v >> 32)); }
static void w_bytes(wr *w, const void *src, size_t len) {
    if (w->n + len > w->cap) { w->bad = 1; return; }
    memcpy(w->p + w->n, src, len);
    w->n += len;
}

static uint8_t r_u8(rd *r) { if (r->n + 1 > r->len) { r->bad = 1; return 0; } return r->p[r->n++]; }
static uint16_t r_u16(rd *r) { uint16_t lo = r_u8(r); return (uint16_t)(lo | (uint16_t)r_u8(r) << 8); }
static uint32_t r_u32(rd *r) { uint32_t lo = r_u16(r); return lo | (uint32_t)r_u16(r) << 16; }
static uint64_t r_u64(rd *r) { uint64_t lo = r_u32(r); return lo | (uint64_t)r_u32(r) << 32; }
static void r_bytes(rd *r, void *dst, size_t len) {
    if (r->n + len > r->len) { r->bad = 1; memset(dst, 0, len); return; }
    memcpy(dst, r->p + r->n, len);
    r->n += len;
}
/* Fixed-size text field. A field without a terminator marks the message as invalid: refused, not repaired. */
static void r_text(rd *r, char *dst, size_t len, int *invalid) {
    r_bytes(r, dst, len);
    if (memchr(dst, 0, len) == NULL) {
        *invalid = 1;
        dst[len - 1] = 0; /* keep the buffer a valid string even though the message is refused */
    }
}

/* --- actions --- */

static size_t action_body_size(uint8_t kind) {
    switch (kind) {
    case DD_ACT_ASSIGN_HERO: return 1 + 1 + 1 + 4;
    case DD_ACT_BUY_UPGRADE: return 1 + 4 + 1 + 4;
    case DD_ACT_RECRUIT:
    case DD_ACT_DISMISS: return 1 + 4;
    case DD_ACT_PROVISION_BUY:
    case DD_ACT_PROVISION_RETURN: return 1 + 2 + 2;
    case DD_ACT_WAGON_SELL:
    case DD_ACT_WAGON_BUY: return 1 + 2 + 2 + 4 + 4 + 4;
    case DD_ACT_HEIRLOOM_EXCHANGE: return 1 + 4 * 4;
    case DD_ACT_RENAME: return 1+4+64*2;
    case DD_ACT_SKILL: return 1+4+4+4;
    case DD_ACT_TRINKET_MOVE: return 1+4*6+2*2;
    case DD_ACT_PARTY: return 1+4*8+1;
    case DD_ACT_SUPPLIES: return 1+1+2+4*4+32;
    case DD_ACT_DEPARTURE: return 1+4*10+1;
    case DD_ACT_TRAVEL: return 1+4*24+3;
    case DD_ACT_USE: return 1+4*7+3;
    case DD_ACT_BATTLE: return 1+4*4+6;
    case DD_ACT_ITEM: return 1+4*9+5;
    case DD_ACT_OWNERS: return 1+4*5+1;
    case DD_ACT_EMBARK:
    case DD_ACT_PREPARE: return 1+4*2;
    default: return 0;
    }
}

static int party_ids_valid(const uint32_t ids[4]) {
    for(unsigned i=0;i<4;i++)if(ids[i]) {
        if(ids[i]==UINT32_MAX)return 0;
        for(unsigned j=0;j<i;j++)if(ids[j]==ids[i])return 0;
    }
    return 1;
}
int dd_action_valid(const dd_action *a) {
    switch (a->kind) {
    case DD_ACT_ASSIGN_HERO:
        return a->u.assign.activity < DD_ACTIVITY_COUNT_ && a->u.assign.slot < 3 && a->u.assign.hero_id != 0;
    case DD_ACT_BUY_UPGRADE: {
        uint8_t c = a->u.upgrade.code; /* level letters/digits seen: '0'.., 'a'.. */
        return a->u.upgrade.tree != 0 && ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'));
    }
    case DD_ACT_RECRUIT:
    case DD_ACT_DISMISS: return a->u.hero.hero_id != 0;
    case DD_ACT_PROVISION_BUY:
    case DD_ACT_PROVISION_RETURN: return a->u.items.count >= 1 && a->u.items.count <= 999;
    case DD_ACT_WAGON_SELL: return a->u.items.slot<9999 && a->u.items.count==1 && a->u.items.type && a->u.items.id && a->u.items.cost>0 && a->u.items.cost<=0x7fffffffu;
    case DD_ACT_WAGON_BUY: return a->u.items.slot<128 && a->u.items.count==1 && a->u.items.type && a->u.items.id && a->u.items.cost<=0x7fffffffu;
    case DD_ACT_HEIRLOOM_EXCHANGE:
        return a->u.exchange.src_amount > 0 && a->u.exchange.dst_amount > 0 &&
               a->u.exchange.src_type != a->u.exchange.dst_type;
    case DD_ACT_RENAME: return a->u.rename.hero_id && dd_utf8_name_valid(a->u.rename.before,64) &&
        dd_utf8_name_valid(a->u.rename.after,64) && strcmp(a->u.rename.before,a->u.rename.after)!=0;
    case DD_ACT_SKILL: return a->u.skill.hero_id && a->u.skill.index<32 && a->u.skill.camping<=1 &&
        a->u.skill.before<=1 && a->u.skill.after<=1 && a->u.skill.before!=a->u.skill.after &&
        ((a->u.skill.selected_mask>>a->u.skill.index)&1u)==a->u.skill.before;
    case DD_ACT_TRINKET_MOVE: return a->u.move.src_slot<DD_TRINKET_SLOTS(a->u.move.src_hero) &&
        a->u.move.dst_slot<DD_TRINKET_SLOTS(a->u.move.dst_hero) && a->u.move.src_type && a->u.move.src_id &&
        ((a->u.move.dst_type==0)==(a->u.move.dst_id==0)) &&
        (a->u.move.src_hero!=a->u.move.dst_hero || a->u.move.src_slot!=a->u.move.dst_slot);
    case DD_ACT_PARTY: return party_ids_valid(a->u.party.before) && party_ids_valid(a->u.party.after) && a->u.party.show<=4 &&
        (a->u.party.show ? memcmp(a->u.party.before,a->u.party.after,sizeof a->u.party.before)==0 && a->u.party.after[a->u.party.show-1]!=0 :
                           memcmp(a->u.party.before,a->u.party.after,sizeof a->u.party.before)!=0);
    case DD_ACT_SUPPLIES: {
        unsigned bits=0;for(unsigned i=0;i<32;i++)bits|=a->u.supplies.before[i];
        if(!bits || a->u.supplies.gold>0x7fffffffu || a->u.supplies.shards>0x7fffffffu)return 0;
        if(a->u.supplies.op==DD_SUPPLY_CANCEL)return !a->u.supplies.slot && !a->u.supplies.type && !a->u.supplies.id;
        return (a->u.supplies.op==DD_SUPPLY_BUY || a->u.supplies.op==DD_SUPPLY_RETURN) &&
            a->u.supplies.type && a->u.supplies.slot<(a->u.supplies.op==DD_SUPPLY_BUY?128:64);
    }
    case DD_ACT_DEPARTURE: {
        unsigned ids=0;for(unsigned i=0;i<4;i++)ids|=a->u.departure.heroes[i];
        return a->u.departure.seed && a->u.departure.seed<=0x7fffffffu &&
            a->u.departure.rules && a->u.departure.rules<=0x7fffffffu && a->u.departure.owners<16 && ids;
    }
    case DD_ACT_TRAVEL: return a->u.travel.dest[1]<4096 && a->u.travel.has_from<=1 && a->u.travel.flip<=1 && a->u.travel.extra<=1 &&
        (a->u.travel.has_from || !a->u.travel.from_area);
    case DD_ACT_USE: return a->u.use.prop<4096 && a->u.use.option<16 && a->u.use.hero<4 && a->u.use.flag<=1 && a->u.use.door<=4 &&
        (a->u.use.pos & 0x7f800000u) != 0x7f800000u;
    case DD_ACT_BATTLE: return a->u.battle.op<=DD_BATTLE_OP_MOVE && (a->u.battle.state==0x1d || a->u.battle.state==0x1e) && a->u.battle.hero<4 && a->u.battle.flag<=1 &&
        a->u.battle.side<=1 && a->u.battle.place<16 && a->u.battle.value>=-1 && a->u.battle.value<16;
    case DD_ACT_EMBARK:
    case DD_ACT_PREPARE: return a->u.prepare.index<4096 && a->u.prepare.key;
    case DD_ACT_OWNERS: return a->u.owners.mask<16 && party_ids_valid(a->u.owners.heroes) &&
        (a->u.owners.heroes[0] || a->u.owners.heroes[1] || a->u.owners.heroes[2] || a->u.owners.heroes[3]);
    case DD_ACT_ITEM: return a->u.item.op<=DD_ITEM_OP_QUEST_RETURN && (a->u.item.hero<4 || a->u.item.hero==255) && a->u.item.inv<=1 && a->u.item.flag<=1 &&
        (a->u.item.op!=DD_ITEM_OP_QUEST_RETURN || (!a->u.item.state && a->u.item.hero==255 && !a->u.item.flag)) &&
        (a->u.item.op!=DD_ITEM_OP_HUNGER || (a->u.item.round && !a->u.item.state && a->u.item.hero==255 && a->u.item.type && a->u.item.amount<=0x42800000u)) &&
        (!a->u.item.state || a->u.item.state==0x1d || a->u.item.state==0x1e) && a->u.item.slot>=0 && a->u.item.slot<4096 &&
        (a->u.item.pos & 0x7f800000u) != 0x7f800000u && (a->u.item.amount & 0x7f800000u) != 0x7f800000u;
    default: return 0;
    }
}

static void w_action(wr *w, const dd_action *a) {
    w_u8(w, a->kind);
    switch (a->kind) {
    case DD_ACT_EMBARK:
    case DD_ACT_PREPARE: w_u32(w,a->u.prepare.index);w_u32(w,a->u.prepare.key);break;
    case DD_ACT_OWNERS:
        for(unsigned i=0;i<4;i++)w_u32(w,a->u.owners.heroes[i]);
        w_u32(w,a->u.owners.revision);w_u8(w,a->u.owners.mask);break;
    case DD_ACT_ASSIGN_HERO: w_u8(w, a->u.assign.activity); w_u8(w, a->u.assign.slot); w_u32(w, a->u.assign.hero_id); break;
    case DD_ACT_BUY_UPGRADE: w_u32(w, a->u.upgrade.tree); w_u8(w, a->u.upgrade.code); w_u32(w, a->u.upgrade.hero_id); break;
    case DD_ACT_RECRUIT:
    case DD_ACT_DISMISS: w_u32(w, a->u.hero.hero_id); break;
    case DD_ACT_PROVISION_BUY:
    case DD_ACT_PROVISION_RETURN: w_u16(w, a->u.items.slot); w_u16(w, a->u.items.count); break;
    case DD_ACT_WAGON_SELL:
    case DD_ACT_WAGON_BUY:
        w_u16(w,a->u.items.slot);w_u16(w,a->u.items.count);w_u32(w,a->u.items.type);w_u32(w,a->u.items.id);w_u32(w,a->u.items.cost);break;
    case DD_ACT_HEIRLOOM_EXCHANGE:
        w_u32(w, (uint32_t)a->u.exchange.src_type); w_u32(w, (uint32_t)a->u.exchange.src_amount);
        w_u32(w, (uint32_t)a->u.exchange.dst_type); w_u32(w, (uint32_t)a->u.exchange.dst_amount);
        break;
    case DD_ACT_RENAME:
        w_u32(w,a->u.rename.hero_id);w_bytes(w,a->u.rename.before,64);w_bytes(w,a->u.rename.after,64);break;
    case DD_ACT_SKILL:
        w_u32(w,a->u.skill.hero_id);w_u8(w,a->u.skill.index);w_u8(w,a->u.skill.camping);w_u8(w,a->u.skill.before);w_u8(w,a->u.skill.after);w_u32(w,a->u.skill.selected_mask);break;
    case DD_ACT_TRINKET_MOVE:
        w_u32(w,a->u.move.src_hero);w_u32(w,a->u.move.dst_hero);w_u16(w,a->u.move.src_slot);w_u16(w,a->u.move.dst_slot);
        w_u32(w,a->u.move.src_type);w_u32(w,a->u.move.src_id);w_u32(w,a->u.move.dst_type);w_u32(w,a->u.move.dst_id);break;
    case DD_ACT_PARTY:
        for(unsigned i=0;i<4;i++)w_u32(w,a->u.party.before[i]);
        for(unsigned i=0;i<4;i++)w_u32(w,a->u.party.after[i]);w_u8(w,a->u.party.show);break;
    case DD_ACT_SUPPLIES:
        w_u8(w,a->u.supplies.op);w_u16(w,a->u.supplies.slot);w_u32(w,a->u.supplies.type);
        w_u32(w,a->u.supplies.id);w_u32(w,a->u.supplies.gold);w_u32(w,a->u.supplies.shards);
        w_bytes(w,a->u.supplies.before,32);break;
    case DD_ACT_DEPARTURE:
        w_u32(w,a->u.departure.seed);w_u32(w,a->u.departure.rules);w_u32(w,a->u.departure.quest);w_u32(w,a->u.departure.quest_index);
        for(unsigned i=0;i<4;i++)w_u32(w,a->u.departure.heroes[i]);
        w_u32(w,a->u.departure.supplies);w_u32(w,a->u.departure.revision);w_u8(w,a->u.departure.owners);break;
    case DD_ACT_TRAVEL:
        for(unsigned i=0;i<4;i++)w_u32(w,a->u.travel.dest[i]);
        w_u32(w,a->u.travel.from_area);w_u32(w,a->u.travel.rules);w_u32(w,a->u.travel.torch);w_u32(w,a->u.travel.bag);
        for(unsigned i=0;i<4;i++){w_u32(w,a->u.travel.ids[i]);w_u32(w,a->u.travel.hp[i]);w_u32(w,a->u.travel.stress[i]);w_u32(w,a->u.travel.trinkets[i]);}
        w_u8(w,a->u.travel.has_from);w_u8(w,a->u.travel.flip);w_u8(w,a->u.travel.extra);break;
    case DD_ACT_USE:
        w_u32(w,a->u.use.area);w_u32(w,a->u.use.prop);w_u32(w,a->u.use.prop_kind);w_u32(w,a->u.use.pos);w_u32(w,a->u.use.option);w_u32(w,a->u.use.item_type);w_u32(w,a->u.use.item_id);
        w_u8(w,a->u.use.hero);w_u8(w,a->u.use.flag);w_u8(w,a->u.use.door);break;
    case DD_ACT_BATTLE:
        w_u32(w,a->u.battle.round);w_u32(w,a->u.battle.turn);w_u32(w,a->u.battle.rules);w_u32(w,(uint32_t)a->u.battle.value);
        w_u8(w,a->u.battle.op);w_u8(w,a->u.battle.state);w_u8(w,a->u.battle.hero);w_u8(w,a->u.battle.flag);w_u8(w,a->u.battle.side);w_u8(w,a->u.battle.place);break;
    case DD_ACT_ITEM:
        w_u32(w,a->u.item.area);w_u32(w,a->u.item.pos);w_u32(w,a->u.item.round);w_u32(w,a->u.item.turn);w_u32(w,a->u.item.rules);w_u32(w,a->u.item.amount);w_u32(w,a->u.item.type);w_u32(w,a->u.item.id);w_u32(w,(uint32_t)a->u.item.slot);
        w_u8(w,a->u.item.op);w_u8(w,a->u.item.state);w_u8(w,a->u.item.hero);w_u8(w,a->u.item.inv);w_u8(w,a->u.item.flag);break;
    default: w->bad = 1;
    }
}

static void r_action(rd *r, dd_action *a) {
    memset(a, 0, sizeof *a);
    a->kind = r_u8(r);
    switch (a->kind) {
    case DD_ACT_EMBARK:
    case DD_ACT_PREPARE: a->u.prepare.index=r_u32(r);a->u.prepare.key=r_u32(r);break;
    case DD_ACT_OWNERS:
        for(unsigned i=0;i<4;i++)a->u.owners.heroes[i]=r_u32(r);
        a->u.owners.revision=r_u32(r);a->u.owners.mask=r_u8(r);break;
    case DD_ACT_ASSIGN_HERO: a->u.assign.activity = r_u8(r); a->u.assign.slot = r_u8(r); a->u.assign.hero_id = r_u32(r); break;
    case DD_ACT_BUY_UPGRADE: a->u.upgrade.tree = r_u32(r); a->u.upgrade.code = r_u8(r); a->u.upgrade.hero_id = r_u32(r); break;
    case DD_ACT_RECRUIT:
    case DD_ACT_DISMISS: a->u.hero.hero_id = r_u32(r); break;
    case DD_ACT_PROVISION_BUY:
    case DD_ACT_PROVISION_RETURN: a->u.items.slot = r_u16(r); a->u.items.count = r_u16(r); break;
    case DD_ACT_WAGON_SELL:
    case DD_ACT_WAGON_BUY:
        a->u.items.slot=r_u16(r);a->u.items.count=r_u16(r);a->u.items.type=r_u32(r);a->u.items.id=r_u32(r);a->u.items.cost=r_u32(r);break;
    case DD_ACT_HEIRLOOM_EXCHANGE:
        a->u.exchange.src_type = (int32_t)r_u32(r); a->u.exchange.src_amount = (int32_t)r_u32(r);
        a->u.exchange.dst_type = (int32_t)r_u32(r); a->u.exchange.dst_amount = (int32_t)r_u32(r);
        break;
    case DD_ACT_RENAME:
        a->u.rename.hero_id=r_u32(r);r_bytes(r,a->u.rename.before,64);r_bytes(r,a->u.rename.after,64);break;
    case DD_ACT_SKILL:
        a->u.skill.hero_id=r_u32(r);a->u.skill.index=r_u8(r);a->u.skill.camping=r_u8(r);a->u.skill.before=r_u8(r);a->u.skill.after=r_u8(r);a->u.skill.selected_mask=r_u32(r);break;
    case DD_ACT_TRINKET_MOVE:
        a->u.move.src_hero=r_u32(r);a->u.move.dst_hero=r_u32(r);a->u.move.src_slot=r_u16(r);a->u.move.dst_slot=r_u16(r);
        a->u.move.src_type=r_u32(r);a->u.move.src_id=r_u32(r);a->u.move.dst_type=r_u32(r);a->u.move.dst_id=r_u32(r);break;
    case DD_ACT_PARTY:
        for(unsigned i=0;i<4;i++)a->u.party.before[i]=r_u32(r);
        for(unsigned i=0;i<4;i++)a->u.party.after[i]=r_u32(r);a->u.party.show=r_u8(r);break;
    case DD_ACT_SUPPLIES:
        a->u.supplies.op=r_u8(r);a->u.supplies.slot=r_u16(r);a->u.supplies.type=r_u32(r);
        a->u.supplies.id=r_u32(r);a->u.supplies.gold=r_u32(r);a->u.supplies.shards=r_u32(r);
        r_bytes(r,a->u.supplies.before,32);break;
    case DD_ACT_DEPARTURE:
        a->u.departure.seed=r_u32(r);a->u.departure.rules=r_u32(r);a->u.departure.quest=r_u32(r);a->u.departure.quest_index=r_u32(r);
        for(unsigned i=0;i<4;i++)a->u.departure.heroes[i]=r_u32(r);
        a->u.departure.supplies=r_u32(r);a->u.departure.revision=r_u32(r);a->u.departure.owners=r_u8(r);break;
    case DD_ACT_TRAVEL:
        for(unsigned i=0;i<4;i++)a->u.travel.dest[i]=r_u32(r);
        a->u.travel.from_area=r_u32(r);a->u.travel.rules=r_u32(r);a->u.travel.torch=r_u32(r);a->u.travel.bag=r_u32(r);
        for(unsigned i=0;i<4;i++){a->u.travel.ids[i]=r_u32(r);a->u.travel.hp[i]=r_u32(r);a->u.travel.stress[i]=r_u32(r);a->u.travel.trinkets[i]=r_u32(r);}
        a->u.travel.has_from=r_u8(r);a->u.travel.flip=r_u8(r);a->u.travel.extra=r_u8(r);break;
    case DD_ACT_USE:
        a->u.use.area=r_u32(r);a->u.use.prop=r_u32(r);a->u.use.prop_kind=r_u32(r);a->u.use.pos=r_u32(r);a->u.use.option=r_u32(r);a->u.use.item_type=r_u32(r);a->u.use.item_id=r_u32(r);
        a->u.use.hero=r_u8(r);a->u.use.flag=r_u8(r);a->u.use.door=r_u8(r);break;
    case DD_ACT_BATTLE:
        a->u.battle.round=r_u32(r);a->u.battle.turn=r_u32(r);a->u.battle.rules=r_u32(r);a->u.battle.value=(int32_t)r_u32(r);
        a->u.battle.op=r_u8(r);a->u.battle.state=r_u8(r);a->u.battle.hero=r_u8(r);a->u.battle.flag=r_u8(r);a->u.battle.side=r_u8(r);a->u.battle.place=r_u8(r);break;
    case DD_ACT_ITEM:
        a->u.item.area=r_u32(r);a->u.item.pos=r_u32(r);a->u.item.round=r_u32(r);a->u.item.turn=r_u32(r);a->u.item.rules=r_u32(r);a->u.item.amount=r_u32(r);a->u.item.type=r_u32(r);a->u.item.id=r_u32(r);a->u.item.slot=(int32_t)r_u32(r);
        a->u.item.op=r_u8(r);a->u.item.state=r_u8(r);a->u.item.hero=r_u8(r);a->u.item.inv=r_u8(r);a->u.item.flag=r_u8(r);break;
    default: r->bad = 1;
    }
}

/* --- messages --- */

static size_t body_size(const dd_msg *m) {
    switch (m->type) {
    case DD_MSG_HELLO: return 4 + 16 + 8 + 32;
    case DD_MSG_TOWN_REQUEST:
    case DD_MSG_TOWN_APPLY: return 4 + action_body_size(m->u.town.action.kind);
    case DD_MSG_TOWN_REJECT: return 4 + 1;
    case DD_MSG_PING:
    case DD_MSG_PONG: return 8;
    case DD_MSG_WALK: return 8;
    case DD_MSG_WALK_INTENT: return 4;
    case DD_MSG_LOAD: return 4;
    default: return 0;
    }
}

size_t dd_encode(const dd_msg *m, uint8_t *buf, size_t cap) {
    size_t body = body_size(m);
    if (body == 0 || body > DD_PROTO_MAX_BODY) return 0;
    if ((m->type == DD_MSG_TOWN_REQUEST || m->type == DD_MSG_TOWN_APPLY) && !dd_action_valid(&m->u.town.action)) return 0;
    wr w = {buf, cap, 0, 0};
    w_u32(&w, DD_PROTO_MAGIC);
    w_u16(&w, DD_PROTO_VERSION);
    w_u16(&w, m->type);
    w_u32(&w, m->seq);
    w_u32(&w, (uint32_t)body);
    switch (m->type) {
    case DD_MSG_HELLO:
        w_u32(&w, m->u.hello.protocol);
        w_bytes(&w, m->u.hello.mod_version, sizeof m->u.hello.mod_version);
        w_bytes(&w, m->u.hello.build, sizeof m->u.hello.build);
        w_bytes(&w, m->u.hello.name, sizeof m->u.hello.name);
        break;
    case DD_MSG_TOWN_REQUEST:
    case DD_MSG_TOWN_APPLY:
        w_u32(&w, m->u.town.request_id);
        w_action(&w, &m->u.town.action);
        break;
    case DD_MSG_TOWN_REJECT:
        w_u32(&w, m->u.reject.request_id);
        w_u8(&w, m->u.reject.reason);
        break;
    case DD_MSG_WALK:
        w_u32(&w, m->u.walk.area);
        w_u32(&w, m->u.walk.pos);
        break;
    case DD_MSG_WALK_INTENT:
        w_u32(&w, (uint32_t)m->u.intent.direction);
        break;
    case DD_MSG_LOAD:
        w_u32(&w, m->u.load.slot);
        break;
    default:
        w_u64(&w, m->u.ping.time_ms);
    }
    return w.bad ? 0 : w.n;
}

dd_decode_result dd_decode(const uint8_t *buf, size_t len, dd_msg *m, size_t *used) {
    memset(m, 0, sizeof *m);
    if (len < DD_PROTO_HEADER) return DD_DECODE_SHORT;
    rd h = {buf, DD_PROTO_HEADER, 0, 0};
    if (r_u32(&h) != DD_PROTO_MAGIC) return DD_DECODE_BAD_MAGIC;
    if (r_u16(&h) != DD_PROTO_VERSION) return DD_DECODE_BAD_VERSION;
    m->type = r_u16(&h);
    m->seq = r_u32(&h);
    uint32_t body = r_u32(&h);
    if (m->type < DD_MSG_HELLO || m->type > DD_MSG_LOAD) return DD_DECODE_BAD_TYPE;
    if (body == 0 || body > DD_PROTO_MAX_BODY) return DD_DECODE_BAD_LENGTH;
    if (len < DD_PROTO_HEADER + (size_t)body) return DD_DECODE_SHORT;

    rd r = {buf + DD_PROTO_HEADER, body, 0, 0};
    switch (m->type) {
    case DD_MSG_HELLO: {
        int unterminated = 0;
        m->u.hello.protocol = r_u32(&r);
        r_text(&r, m->u.hello.mod_version, sizeof m->u.hello.mod_version, &unterminated);
        r_bytes(&r, m->u.hello.build, sizeof m->u.hello.build);
        r_text(&r, m->u.hello.name, sizeof m->u.hello.name, &unterminated);
        if (!r.bad && unterminated) return DD_DECODE_BAD_VALUE;
        break;
    }
    case DD_MSG_TOWN_REQUEST:
    case DD_MSG_TOWN_APPLY:
        m->u.town.request_id = r_u32(&r);
        r_action(&r, &m->u.town.action);
        if (!r.bad && !dd_action_valid(&m->u.town.action)) return DD_DECODE_BAD_VALUE;
        break;
    case DD_MSG_TOWN_REJECT:
        m->u.reject.request_id = r_u32(&r);
        m->u.reject.reason = r_u8(&r);
        if (!r.bad && (m->u.reject.reason < DD_REJECT_NOT_ALLOWED || m->u.reject.reason > DD_REJECT_INVALID))
            return DD_DECODE_BAD_VALUE;
        break;
    case DD_MSG_WALK:
        m->u.walk.area = r_u32(&r);
        m->u.walk.pos = r_u32(&r);
        if (!r.bad && (m->u.walk.pos & 0x7f800000u) == 0x7f800000u) return DD_DECODE_BAD_VALUE; /* Not a number or infinite. */
        break;
    case DD_MSG_WALK_INTENT:
        m->u.intent.direction = (int32_t)r_u32(&r);
        if (!r.bad && (m->u.intent.direction < -1 || m->u.intent.direction > 1)) return DD_DECODE_BAD_VALUE;
        break;
    case DD_MSG_LOAD:
        m->u.load.slot = r_u32(&r);
        if (!r.bad && (m->u.load.slot < 1 || m->u.load.slot > 9)) return DD_DECODE_BAD_VALUE;
        break;
    default:
        m->u.ping.time_ms = r_u64(&r);
    }
    /* The body must be exactly what this type needs: nothing missing, nothing extra. */
    if (r.bad || r.n != body || body != body_size(m)) return DD_DECODE_BAD_LENGTH;
    *used = DD_PROTO_HEADER + body;
    return DD_DECODE_OK;
}

const char *dd_decode_result_name(dd_decode_result r) {
    switch (r) {
    case DD_DECODE_OK: return "ok";
    case DD_DECODE_SHORT: return "short";
    case DD_DECODE_BAD_MAGIC: return "bad magic";
    case DD_DECODE_BAD_VERSION: return "bad version";
    case DD_DECODE_BAD_TYPE: return "bad type";
    case DD_DECODE_BAD_LENGTH: return "bad length";
    default: return "bad value";
    }
}
