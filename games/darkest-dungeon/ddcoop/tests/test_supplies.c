#include "dd_supplies.h"
#include <stdio.h>
#include <string.h>
static unsigned failures;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);failures++;}}while(0)
#include "provisions_observed.inc"
static int gold(const dd_provisions_snapshot *s) {
    for(unsigned i=0;i<s->currencies;i++)if(s->wallet[i].type==DD_SUPPLIES_GOLD)return s->wallet[i].quantity;
    return 0;
}
static void observed_move(unsigned from,unsigned to) {
    const dd_provisions_snapshot *a=measured+from,*b=measured+to;
    unsigned op=gold(a)>gold(b)?DD_SUPPLY_BUY:DD_SUPPLY_RETURN,slot=999;
    const dd_provisions_item *src=op==DD_SUPPLY_BUY?a->stock:a->bag;
    const dd_provisions_item *dst=op==DD_SUPPLY_BUY?b->stock:b->bag;
    unsigned n=op==DD_SUPPLY_BUY?a->stock_slots:a->bag_slots;
    for(unsigned i=0;i<n;i++)if(src[i].quantity>dst[i].quantity)slot=i;
    int price=gold(a)-gold(b);if(price<0)price=-price;
    dd_action command;
    CHECK(dd_supplies_quote(a,op,slot,(uint32_t)price,0,&command));
    CHECK(dd_supplies_before(a,&command));
    CHECK(dd_supplies_after(a,b,&command));
    dd_msg input={0},decoded;input.type=DD_MSG_TOWN_APPLY;input.seq=7;input.u.town.action=command;
    uint8_t wire[512];size_t used=0,len=dd_encode(&input,wire,sizeof wire);
    CHECK(len && dd_decode(wire,len,&decoded,&used)==DD_DECODE_OK && used==len);
    CHECK(dd_supplies_before(a,&decoded.u.town.action) && dd_supplies_after(a,b,&decoded.u.town.action));
    dd_provisions_snapshot wrong=*b;
    wrong.wallet[0].quantity++;CHECK(!dd_supplies_after(a,&wrong,&command));
    wrong=*b;wrong.wallet[1].quantity++;CHECK(!dd_supplies_after(a,&wrong,&command));
    CHECK(!dd_supplies_before(b,&command)); /* Request replay/stale before-state. */
}
int main(void) {
    for(unsigned i=1;i<19;i++)observed_move(i,i+1); /* Nine actual buys, nine actual returns. */
    observed_move(21,22);observed_move(22,23); /* Torch then food in Back observation. */
    dd_action a;
    CHECK(dd_supplies_quote(measured+23,DD_SUPPLY_CANCEL,0,150,0,&a));
    CHECK(dd_supplies_before(measured+23,&a) && dd_supplies_after(measured+23,measured+24,&a));
    dd_provisions_snapshot s=measured[24];s.bag[0]=measured[23].bag[0];s.bag_occupied=1;
    CHECK(!dd_supplies_after(measured+23,&s,&a)); /* Free firewood must also be cleared. */
    s=measured[24];s.stock[0]=measured[23].stock[0];s.stock_occupied=1;
    CHECK(!dd_supplies_after(measured+23,&s,&a));
    a.u.supplies.gold=149;CHECK(!dd_supplies_after(measured+23,measured+24,&a));
    CHECK(!dd_supplies_quote(measured+23,DD_SUPPLY_CANCEL,0,150,1,&a));
    CHECK(!dd_supplies_quote(measured+1,DD_SUPPLY_BUY,128,75,0,&a));
    CHECK(dd_supplies_quote(measured+1,DD_SUPPLY_BUY,0,75,0,&a) && a.u.supplies.id==0);
    s=measured[2];s.bag[1]=(dd_provisions_item){0};s.bag_occupied--;
    CHECK(!dd_supplies_after(measured+1,&s,&a)); /* Removed from shop/charged, never delivered. */
    s=measured[2];s.stock[0].quantity++;
    CHECK(!dd_supplies_after(measured+1,&s,&a)); /* Created a copy without reducing stock. */
    s=measured[1];s.panel_visible=s.stock_view=s.bag_view=0;
    CHECK(dd_supplies_equal(measured+1,&s));
    dd_provisions_currency temp=s.wallet[0];s.wallet[0]=s.wallet[4];s.wallet[4]=temp;
    CHECK(dd_supplies_equal(measured+1,&s));
    s.wallet[s.currencies++]=(dd_provisions_currency){1234,0};CHECK(dd_supplies_equal(measured+1,&s));
    s.bag[1]=s.bag[0];s.bag[0]=(dd_provisions_item){0};CHECK(!dd_supplies_equal(measured+1,&s));
    s=measured[1];s.stock_slots=129;uint8_t hash[32];CHECK(!dd_supplies_digest(&s,hash));
    for(unsigned i=0;i<32;i++)CHECK(!hash[i]);
    s=measured[1];s.wallet[0].quantity=0;CHECK(!dd_supplies_quote(&s,DD_SUPPLY_BUY,0,75,0,&a));
    s=measured[23];s.wallet[0].quantity=0x7fffffff;CHECK(!dd_supplies_quote(&s,DD_SUPPLY_CANCEL,0,1,0,&a));
    printf("supplies:20 observed unit moves +actual150-gold Back refund/clear, canonical pre-state, wire, partial/money failures: %s\n",failures?"FAIL":"PASS");
    return failures?1:0;
}
