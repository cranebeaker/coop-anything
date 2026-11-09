#include "dd_supplies.h"
#include "dd_crypto.h"
#include <limits.h>
#include <string.h>

static int inventory_valid(const dd_provisions_item *items, unsigned slots, unsigned occupied) {
    unsigned count=0;
    for(unsigned i=0;i<slots;i++) {
        const dd_provisions_item *p=items+i;
        if(p->quantity<0 || p->quantity>1000000 || (p->quantity && !p->type) ||
           (!p->quantity && (p->type || p->id)))return 0;
        count+=p->quantity!=0;
    }
    return count==occupied;
}
static int valid(const dd_provisions_snapshot *s) {
    if(!s || s->stock_slots>DD_PROVISIONS_STOCK_MAX || s->bag_slots>DD_PROVISIONS_BAG_MAX ||
       s->currencies>DD_PROVISIONS_CURRENCY_MAX ||
       !inventory_valid(s->stock,s->stock_slots,s->stock_occupied) ||
       !inventory_valid(s->bag,s->bag_slots,s->bag_occupied))return 0;
    for(unsigned i=0;i<s->currencies;i++) {
        if(!s->wallet[i].type || s->wallet[i].quantity<0)return 0;
        for(unsigned j=0;j<i;j++)if(s->wallet[i].type==s->wallet[j].type)return 0;
    }
    return 1;
}
static void u32(uint8_t **at,uint32_t value) {
    for(unsigned i=0;i<4;i++)*(*at)++=(uint8_t)(value>>(i*8));
}
int dd_supplies_digest(const dd_provisions_snapshot *s,uint8_t out[32]) {
    static const uint8_t domain[]="DDCoop supplies state v1";
    if(!out)return 0;
    memset(out,0,32);
    if(!valid(s))return 0;
    uint8_t bytes[16+12*(DD_PROVISIONS_STOCK_MAX+DD_PROVISIONS_BAG_MAX)+8*DD_PROVISIONS_CURRENCY_MAX];
    uint8_t *at=bytes;
    u32(&at,s->context_1838);u32(&at,s->stock_slots);u32(&at,s->bag_slots);
    for(unsigned kind=0;kind<2;kind++) {
        unsigned n=kind?s->bag_slots:s->stock_slots;
        const dd_provisions_item *items=kind?s->bag:s->stock;
        for(unsigned i=0;i<n;i++) {u32(&at,items[i].type);u32(&at,items[i].id);u32(&at,(uint32_t)items[i].quantity);}
    }
    dd_provisions_currency sorted[DD_PROVISIONS_CURRENCY_MAX];unsigned count=0;
    for(unsigned i=0;i<s->currencies;i++)if(s->wallet[i].quantity)sorted[count++]=s->wallet[i];
    for(unsigned i=1;i<count;i++)for(unsigned j=i;j && sorted[j].type<sorted[j-1].type;j--) {
        dd_provisions_currency swap=sorted[j];sorted[j]=sorted[j-1];sorted[j-1]=swap;
    }
    u32(&at,count);
    for(unsigned i=0;i<count;i++){u32(&at,sorted[i].type);u32(&at,(uint32_t)sorted[i].quantity);}
    return dd_hmac_sha256(domain,sizeof domain-1,bytes,(size_t)(at-bytes),out);
}
int dd_supplies_equal(const dd_provisions_snapshot *a,const dd_provisions_snapshot *b) {
    uint8_t first[32],second[32];
    return dd_supplies_digest(a,first) && dd_supplies_digest(b,second) && !memcmp(first,second,32);
}
static int64_t currency(const dd_provisions_snapshot *s,uint32_t type) {
    for(unsigned i=0;i<s->currencies;i++)if(s->wallet[i].type==type)return s->wallet[i].quantity;
    return 0;
}
int dd_supplies_before(const dd_provisions_snapshot *s,const dd_action *a) {
    uint8_t hash[32];
    if(!a || a->kind!=DD_ACT_SUPPLIES || !dd_action_valid(a) || a->u.supplies.shards ||
       !dd_supplies_digest(s,hash) || memcmp(hash,a->u.supplies.before,32))return 0;
    int64_t gold=currency(s,DD_SUPPLIES_GOLD),price=a->u.supplies.gold;
    if(a->u.supplies.op==DD_SUPPLY_BUY) {if(gold<price)return 0;}
    else if(gold+price>INT32_MAX)return 0;
    if(a->u.supplies.op==DD_SUPPLY_CANCEL)return 1;
    unsigned slots=a->u.supplies.op==DD_SUPPLY_BUY?s->stock_slots:s->bag_slots;
    const dd_provisions_item *items=a->u.supplies.op==DD_SUPPLY_BUY?s->stock:s->bag;
    unsigned i=a->u.supplies.slot;
    return i<slots && items[i].quantity>0 && items[i].type==a->u.supplies.type && items[i].id==a->u.supplies.id;
}
int dd_supplies_quote(const dd_provisions_snapshot *s,unsigned op,unsigned slot,uint32_t gold,uint32_t shards,
                      dd_action *out) {
    if(!out)return 0;
    memset(out,0,sizeof *out);
    if(op<DD_SUPPLY_BUY || op>DD_SUPPLY_CANCEL || shards || gold>INT32_MAX || !valid(s))return 0;
    dd_action a={0};a.kind=DD_ACT_SUPPLIES;a.u.supplies.op=(uint8_t)op;
    a.u.supplies.gold=gold;
    if(op!=DD_SUPPLY_CANCEL) {
        unsigned n=op==DD_SUPPLY_BUY?s->stock_slots:s->bag_slots;
        const dd_provisions_item *p=op==DD_SUPPLY_BUY?s->stock:s->bag;
        if(slot>=n || !p[slot].quantity)return 0;
        a.u.supplies.slot=(uint16_t)slot;a.u.supplies.type=p[slot].type;a.u.supplies.id=p[slot].id;
    } else if(slot)return 0;
    if(!dd_supplies_digest(s,a.u.supplies.before) || !dd_supplies_before(s,&a))return 0;
    *out=a;return 1;
}
static int64_t quantity(const dd_provisions_item *p,unsigned n,uint32_t type,uint32_t id) {
    int64_t total=0;for(unsigned i=0;i<n;i++)if(p[i].type==type && p[i].id==id)total+=p[i].quantity;
    return total;
}
static int inventory_delta(const dd_provisions_item *a,const dd_provisions_item *b,unsigned n,
                           const dd_action *command,int change) {
    if(quantity(b,n,command->u.supplies.type,command->u.supplies.id)!=
       quantity(a,n,command->u.supplies.type,command->u.supplies.id)+change)return 0;
    /* Check both key sets so an unexpected newly introduced item cannot be missed. */
    for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<n;i++) {
        const dd_provisions_item *p=(pass?b:a)+i;if(!p->quantity)continue;
        int64_t old=quantity(a,n,p->type,p->id),now=quantity(b,n,p->type,p->id);
        int delta=p->type==command->u.supplies.type && p->id==command->u.supplies.id?change:0;
        if(now!=old+delta)return 0;
    }
    return 1;
}
int dd_supplies_after(const dd_provisions_snapshot *before,const dd_provisions_snapshot *after,
                      const dd_action *a) {
    if(!dd_supplies_before(before,a) || !valid(after) || before->stock_slots!=after->stock_slots ||
       before->bag_slots!=after->bag_slots || before->context_1838!=after->context_1838)return 0;
    int sign=a->u.supplies.op==DD_SUPPLY_BUY?-1:1;
    if(a->u.supplies.op==DD_SUPPLY_CANCEL) {
        if(after->stock_occupied || after->bag_occupied)return 0;
    } else if(!inventory_delta(before->stock,after->stock,before->stock_slots,a,sign) ||
              !inventory_delta(before->bag,after->bag,before->bag_slots,a,-sign))return 0;
    for(unsigned pass=0;pass<2;pass++) {
        const dd_provisions_snapshot *s=pass?after:before;
        for(unsigned i=0;i<s->currencies;i++) {
            uint32_t type=s->wallet[i].type;
            int64_t expected=currency(before,type)+(type==DD_SUPPLIES_GOLD?(int64_t)sign*a->u.supplies.gold:0);
            if(currency(after,type)!=expected)return 0;
        }
    }
    return currency(after,DD_SUPPLIES_GOLD)==currency(before,DD_SUPPLIES_GOLD)+(int64_t)sign*a->u.supplies.gold;
}
