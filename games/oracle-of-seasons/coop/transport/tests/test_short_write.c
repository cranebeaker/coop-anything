/* Test-only syscall boundary control: real TCP, no production fault hooks. */
#include "net.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL short-write:%d: %s\n",__LINE__,#x);exit(1);} } while(0)
ssize_t __real_send(int,const void *,size_t,int);
static unsigned calls,short_writes,interrupts,would_blocks;
ssize_t __wrap_send(int fd,const void *data,size_t length,int flags) {
    ++calls;
    if(calls%7==0){++interrupts;errno=EINTR;return -1;}
    if(calls%5==0){++would_blocks;errno=EAGAIN;return -1;}
    if(length>7){length=7;++short_writes;}
    return __real_send(fd,data,length,flags);
}
int main(void) {
    struct net_config c={0};struct net_host h;struct net_peer p,g;struct net_event e;
    unsigned char payload[NET_PAYLOAD_MAX];int a=0,b=0,got_input=0,got_view=0,r;unsigned ticks=0;
    struct timespec tick={0,1000000};
    alarm(10);c.protocol=1;c.capabilities=NET_CAP_TWO_PLAYERS;c.epoch=123;
    memset(c.build,0x42,32);memset(c.rom,0x52,32);memset(payload,0xab,sizeof payload);
    CHECK(net_listen(&h,"127.0.0.1",0)==0);
    CHECK(net_connect(&g,"127.0.0.1",h.port,&c)==0);
    do{r=net_accept(&h,&p,&c);}while(!r);CHECK(r==1);
    while(!got_view) {
        CHECK(++ticks<5000);
        r=net_pump(&p,&e);CHECK(r>=0);
        if(r) {
            if(e.kind==NET_READY)a=1;
            else {
                CHECK(e.kind==NET_INPUT && e.sequence==1 && e.player==NET_P2);
                CHECK(e.length==sizeof payload && memcmp(e.payload,payload,sizeof payload)==0);
                CHECK(!got_input);got_input=1;
                CHECK(net_send(&p,NET_VIEW,2,payload,sizeof payload)==1);
            }
        }
        r=net_pump(&g,&e);CHECK(r>=0);
        if(r) {
            if(e.kind==NET_READY){b=1;CHECK(net_send(&g,NET_INPUT,1,payload,sizeof payload)==1);}
            else {
                CHECK(e.kind==NET_VIEW && e.sequence==2 && e.player==NET_P1);
                CHECK(e.length==sizeof payload && memcmp(e.payload,payload,sizeof payload)==0);
                got_view=1;
            }
        }
        nanosleep(&tick,NULL);
    }
    CHECK(a && b && got_input && short_writes && interrupts && would_blocks);
    net_peer_close(&g);net_peer_close(&p);net_host_close(&h);
    puts("PASS real TCP max payload both directions with forced short sends/EINTR/EAGAIN");
    return 0;
}
