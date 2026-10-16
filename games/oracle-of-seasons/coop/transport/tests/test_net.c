#include "net.h"
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
static const uint64_t epoch = UINT64_C(0x1122334455667788);
static struct net_config config(void) {
    struct net_config c = {0};
    c.protocol=1; memset(c.build,0x42,32); memset(c.rom,0x52,32);
    c.capabilities=NET_CAP_TWO_PLAYERS; c.epoch=epoch; return c;
}
static void pause_tick(void) { struct timespec t={0,1000000}; nanosleep(&t,NULL); }
static void accept_peer(struct net_host *h, struct net_peer *p, struct net_config *c) {
    int r; do { r=net_accept(h,p,c); if(!r)pause_tick(); } while(!r); CHECK(r==1);
}
static int next(struct net_peer *p,struct net_event *e) {
    int r; do {r=net_pump(p,e); if(!r)pause_tick();} while(!r); return r;
}
static void child_ok(pid_t pid) {
    int status; CHECK(waitpid(pid,&status,0)==pid); CHECK(WIFEXITED(status)); CHECK(WEXITSTATUS(status)==0);
}
static void cleaned(struct net_host *h,struct net_peer *p) {
    uint16_t port=h->port;
    net_peer_close(p); net_host_close(h);
    CHECK(p->fd==-1 && h->fd==-1);
    /* Rebind the exact released ephemeral port: no leaked listener. */
    CHECK(net_listen(h,"127.0.0.1",port)==0); net_host_close(h);
}
static void roundtrip(void) {
    struct net_host h; struct net_peer p; struct net_config c=config(); struct net_event e;
    CHECK(net_listen(&h,"127.0.0.1",0)==0); CHECK(h.port!=0);
    pid_t pid=fork(); CHECK(pid>=0);
    if(pid==0) {
        struct net_peer guest; close(h.fd);
        CHECK(net_connect(&guest,"127.0.0.1",h.port,&c)==0);
        CHECK(next(&guest,&e)==1 && e.kind==NET_READY && e.player==NET_P2 && e.epoch==epoch);
        CHECK(net_send(&guest,NET_INPUT,1,"right",5)==1);
        CHECK(net_send(&guest,NET_INPUT,2,"busy",4)==0);
        CHECK(next(&guest,&e)==1 && e.kind==NET_VIEW && e.player==NET_P1 && e.sequence==10);
        CHECK(e.length==17 && memcmp(e.payload,"opaque-host-state",17)==0);
        CHECK(net_send(&guest,NET_VIEW,3,"x",1)==-1);
        net_peer_close(&guest); _exit(0);
    }
    accept_peer(&h,&p,&c);
    CHECK(next(&p,&e)==1 && e.kind==NET_READY && e.player==NET_P2);
    CHECK(next(&p,&e)==1 && e.kind==NET_INPUT && e.sequence==1 && e.player==NET_P2);
    CHECK(e.length==5 && memcmp(e.payload,"right",5)==0);
    CHECK(net_send(&p,NET_VIEW,10,"opaque-host-state",17)==1);
    CHECK(next(&p,&e)==-1 && e.kind==NET_CLOSED);
    child_ok(pid); CHECK(h.guest_fd==-1); cleaned(&h,&p);
    puts("PASS process input/state roundtrip, busy slot, graceful disconnect, port cleanup");
}
/* Raw fixtures independently encode the documented wire protocol. */
static void be32(unsigned char *p,uint32_t v) {p[0]=(unsigned char)(v>>24);p[1]=(unsigned char)(v>>16);p[2]=(unsigned char)(v>>8);p[3]=(unsigned char)v;}
static void be64(unsigned char *p,uint64_t v) { be32(p,(uint32_t)(v>>32));be32(p+4,(uint32_t)v); }
static void raw_write(int fd,const unsigned char *p,size_t n,int fragmented) {
    while(n) { ssize_t r=send(fd,p,fragmented?1:n,MSG_NOSIGNAL); if(r<0 && (errno==EAGAIN || errno==EWOULDBLOCK)){pause_tick();continue;} CHECK(r>0);p+=r;n-=(size_t)r; if(fragmented)pause_tick(); }
}
static size_t frame(unsigned char *b,unsigned kind,uint64_t seq,unsigned player) {
    memcpy(b,"OOC1",4);b[4]=(unsigned char)kind;memset(b+5,0,3);be32(b+8,21);
    be64(b+12,epoch);be64(b+20,seq);b[28]=(unsigned char)player;memset(b+29,0,3);b[32]='x';return 33;
}
static void raw_hello(int fd,struct net_config *c,int fragmented) {
    unsigned char b[96]={0}; memcpy(b,"OOC1",4);b[4]=1;be32(b+8,84);
    be32(b+12,c->protocol);be64(b+16,0);memcpy(b+24,c->build,32);memcpy(b+56,c->rom,32);
    be32(b+88,c->capabilities);b[92]=NET_P2;
    raw_write(fd,b,sizeof b,fragmented);
}
static int raw_connect(uint16_t port) {
    int fd=socket(AF_INET,SOCK_STREAM,0);struct sockaddr_in a={0};CHECK(fd>=0);
    a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    CHECK(connect(fd,(struct sockaddr *)&a,sizeof a)==0);return fd;
}
static void mismatch(unsigned which) {
    struct net_host h;struct net_peer p;struct net_config c=config();struct net_event e;
    CHECK(net_listen(&h,"127.0.0.1",0)==0);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid) {
        int fd;unsigned char b;close(h.fd);
        if(which==0)c.protocol=2;
        if(which==1)c.rom[3]^=1;
        if(which==2)c.build[4]^=1;
        if(which==3)c.capabilities=0;
        fd=raw_connect(h.port);raw_hello(fd,&c,0);
        CHECK(recv(fd,&b,1,0)==0);close(fd);_exit(0);
    }
    accept_peer(&h,&p,&c);CHECK(next(&p,&e)==-1 && e.kind==NET_ERROR);
    child_ok(pid);cleaned(&h,&p);
}
static void malformed(unsigned which) {
    struct net_host h;struct net_peer p;struct net_config c=config();struct net_event e;
    CHECK(net_listen(&h,"127.0.0.1",0)==0);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid) {
        unsigned char b[100];size_t n;int fd;close(h.fd);fd=raw_connect(h.port);
        if(which==0){raw_write(fd,(const unsigned char *)"OOC",3,0);shutdown(fd,SHUT_WR);}
        else {
            raw_hello(fd,&c,0);CHECK(recv(fd,b,96,MSG_WAITALL)==96);
            n=frame(b,2,1,2);
            if(which==1)be32(b+8,NET_PAYLOAD_MAX+21);
            if(which==2)b[4]=99;
            if(which==3)n=20;
            if(which==4)b[28]=NET_P1;
            if(which==5)b[12]^=1;
            if(which==6)be64(b+20,0);
            if(which==7)b[5]=1;
            if(which==8)b[0]='X';
            raw_write(fd,b,n,0);shutdown(fd,SHUT_WR);
        }
        while(recv(fd,b,sizeof b,0)>0){}close(fd);_exit(0);
    }
    accept_peer(&h,&p,&c);
    if(which!=0)CHECK(next(&p,&e)==1 && e.kind==NET_READY);
    CHECK(next(&p,&e)==-1 && e.kind==NET_ERROR);
    child_ok(pid);cleaned(&h,&p);
}
static void fragmented_and_replay(void) {
    struct net_host h;struct net_peer p;struct net_config c=config();struct net_event e;
    CHECK(net_listen(&h,"127.0.0.1",0)==0);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid) {
        int fd;unsigned char b[100];size_t n;close(h.fd);fd=raw_connect(h.port);raw_hello(fd,&c,1);
        CHECK(recv(fd,b,96,MSG_WAITALL)==96);
        n=frame(b,2,7,2);raw_write(fd,b,n,1);
        n=frame(b,2,7,2);raw_write(fd,b,n,0);
        n=frame(b,2,6,2);raw_write(fd,b,n,0);
        n=frame(b,2,8,2);raw_write(fd,b,n,0);close(fd);_exit(0);
    }
    accept_peer(&h,&p,&c);CHECK(next(&p,&e)==1 && e.kind==NET_READY);
    CHECK(next(&p,&e)==1 && e.kind==NET_INPUT && e.sequence==7);
    CHECK(next(&p,&e)==1 && e.kind==NET_INPUT && e.sequence==8);
    CHECK(next(&p,&e)==-1 && e.kind==NET_CLOSED);
    child_ok(pid);cleaned(&h,&p);
    puts("PASS byte-fragmented handshake/input and duplicate/stale suppression");
}
enum guest_case {
    WELCOME_PROTOCOL, WELCOME_ROM, WELCOME_BUILD, WELCOME_CAPABILITY,
    WELCOME_SLOT, WELCOME_EPOCH, WELCOME_RESERVED, WELCOME_KIND,
    HOST_INPUT, HOST_SLOT, HOST_EPOCH, HOST_ZERO_SEQUENCE, HOST_RESERVED,
    HOST_UNKNOWN_KIND, HOST_FLAGS, HOST_OVERSIZE, HOST_TRUNCATED, HOST_MAGIC,
    HOST_STREAM
};
/* A raw host encodes WELCOME independently of net.c and never uses net_accept. */
static void welcome(unsigned char *b) {
    memset(b,0,96);memcpy(b,"OOC1",4);b[4]=5;be32(b+8,84);
    be32(b+12,1);be64(b+16,epoch);memset(b+24,0x42,32);
    memset(b+56,0x52,32);be32(b+88,1);b[92]=2;
}
static void raw_host_fixture(struct net_host *h,enum guest_case which) {
    unsigned char b[100],hello[96];int fd;
    do {fd=accept(h->fd,NULL,NULL);if(fd<0){CHECK(errno==EAGAIN || errno==EWOULDBLOCK);pause_tick();}}while(fd<0);
    close(h->fd);CHECK(recv(fd,hello,96,MSG_WAITALL)==96);
    CHECK(memcmp(hello,"OOC1",4)==0 && hello[4]==1);
    welcome(b);
    switch(which) {
    case WELCOME_PROTOCOL:be32(b+12,2);break;
    case WELCOME_ROM:b[56]^=1;break;
    case WELCOME_BUILD:b[24]^=1;break;
    case WELCOME_CAPABILITY:be32(b+88,0);break;
    case WELCOME_SLOT:b[92]=1;break;
    case WELCOME_EPOCH:be64(b+16,0);break;
    case WELCOME_RESERVED:b[93]=1;break;
    case WELCOME_KIND:b[4]=1;break;
    default:break;
    }
    raw_write(fd,b,96,0);
    if(which==HOST_STREAM) {
        /* Expected surviving events are literal fixtures, not helper output. */
        static const unsigned kinds[]={3,4,4,4,3,3,3,4};
        static const unsigned sequences[]={7,7,6,8,8,5,9,10};
        for(unsigned i=0;i<8;++i) {
            size_t n=frame(b,kinds[i],sequences[i],1);b[32]=(unsigned char)('A'+i);
            raw_write(fd,b,n,0);
        }
    } else if(which>=HOST_INPUT) {
        size_t n=frame(b,3,1,1);
        switch(which) {
        case HOST_INPUT:b[4]=2;break; /* Valid P1 metadata isolates direction. */
        case HOST_SLOT:b[28]=2;break;
        case HOST_EPOCH:b[12]^=1;break;
        case HOST_ZERO_SEQUENCE:be64(b+20,0);break;
        case HOST_RESERVED:b[29]=1;break;
        case HOST_UNKNOWN_KIND:b[4]=99;break;
        case HOST_FLAGS:b[6]=1;break;
        case HOST_OVERSIZE:be32(b+8,NET_PAYLOAD_MAX+21);break;
        case HOST_TRUNCATED:n=20;break;
        case HOST_MAGIC:b[0]='X';break;
        default:CHECK(0);
        }
        raw_write(fd,b,n,0);
    }
    close(fd);_exit(0);
}
static void guest_validation(enum guest_case which) {
    struct net_host h;struct net_peer guest;struct net_config c=config();struct net_event e;
    CHECK(net_listen(&h,"127.0.0.1",0)==0);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid)raw_host_fixture(&h,which);
    CHECK(net_connect(&guest,"127.0.0.1",h.port,&c)==0);
    if(which>=HOST_INPUT) {
        CHECK(next(&guest,&e)==1 && e.kind==NET_READY && e.player==NET_P2 && e.epoch==epoch);
    }
    if(which==HOST_STREAM) {
        static const enum net_kind kinds[]={NET_VIEW,NET_EVENT,NET_VIEW,NET_EVENT};
        static const uint64_t sequences[]={7,8,9,10};
        static const unsigned char payloads[]={'A','D','G','H'};
        for(unsigned i=0;i<4;++i) {
            CHECK(next(&guest,&e)==1 && e.kind==kinds[i] && e.sequence==sequences[i]);
            CHECK(e.player==NET_P1 && e.epoch==epoch && e.length==1 && e.payload[0]==payloads[i]);
        }
        CHECK(next(&guest,&e)==-1 && e.kind==NET_CLOSED);
    } else {
        CHECK(next(&guest,&e)==-1 && e.kind==NET_ERROR);
    }
    child_ok(pid);cleaned(&h,&guest);
}
static void pair(struct net_host *h,struct net_peer *host,struct net_peer *guest) {
    struct net_config c=config();struct net_event e;int a=0,b=0,r;
    CHECK(net_listen(h,"127.0.0.1",0)==0);CHECK(net_connect(guest,"127.0.0.1",h->port,&c)==0);
    accept_peer(h,host,&c);
    while(!a || !b) {
        r=net_pump(host,&e);CHECK(r>=0);if(r){CHECK(e.kind==NET_READY);a=1;}
        r=net_pump(guest,&e);CHECK(r>=0);if(r){CHECK(e.kind==NET_READY);b=1;}
        pause_tick();
    }
    /* Ensure queued welcome has actually flushed. */
    CHECK(net_pump(host,&e)==0);
}
static void second_guest(void) {
    struct net_host h;struct net_peer p,g,extra;struct net_config c=config();struct net_event e;int r,fd;
    pair(&h,&p,&g);fd=raw_connect(h.port);
    do {r=net_accept(&h,&extra,&c);}while(!r);
    CHECK(r==-2);{unsigned char b;CHECK(recv(fd,&b,1,0)==0);}close(fd);
    CHECK(net_send(&g,NET_INPUT,1,"ok",2)==1);
    CHECK(net_pump(&g,&e)==0);CHECK(next(&p,&e)==1 && e.kind==NET_INPUT);
    net_peer_close(&g);net_peer_close(&p);
    CHECK(net_connect(&g,"127.0.0.1",h.port,&c)==0);accept_peer(&h,&p,&c);
    {
        int a=0,b=0;
        while(!a || !b) {
            r=net_pump(&p,&e);CHECK(r>=0);if(r){CHECK(e.kind==NET_READY && e.player==NET_P2);a=1;}
            r=net_pump(&g,&e);CHECK(r>=0);if(r){CHECK(e.kind==NET_READY && e.player==NET_P2);b=1;}
            pause_tick();
        }
    }
    CHECK(net_send(&g,NET_INPUT,1,"replayed",8)==1);
    CHECK(net_pump(&g,&e)==0);
    CHECK(net_send(&g,NET_INPUT,2,"new",3)==1);
    CHECK(net_pump(&g,&e)==0);
    CHECK(next(&p,&e)==1 && e.kind==NET_INPUT && e.sequence==2);
    CHECK(e.length==3 && memcmp(e.payload,"new",3)==0);
    net_peer_close(&g);cleaned(&h,&p);
    puts("PASS second guest refusal, retained original, stable P2 rejoin/replay floor");
}
static void bounded_stream(void) {
    struct net_host h;struct net_peer p,g;struct net_event e;
    unsigned char payload[NET_PAYLOAD_MAX];unsigned sent=0,received=0;int snd=1024,blocked=0,r;
    pair(&h,&p,&g);CHECK(setsockopt(p.fd,SOL_SOCKET,SO_SNDBUF,&snd,sizeof snd)==0);
    memset(payload,0x7a,sizeof payload);
    CHECK(net_send(&p,NET_VIEW,1,payload,sizeof payload+1)==-1);
    CHECK(net_send(&p,NET_INPUT,1,payload,1)==-1);
    /* Stop reading until bounded sender reports backpressure. */
    while(sent<1024) {
        r=net_send(&p,NET_VIEW,sent+1,payload,sizeof payload);
        CHECK(r>=0);if(r==0){blocked=1;break;}++sent;
        CHECK(net_pump(&p,&e)==0);
    }
    CHECK(blocked && sent>0);
    while(received<sent) {
        CHECK(net_pump(&p,&e)==0);r=net_pump(&g,&e);CHECK(r>=0);
        if(r){CHECK(e.kind==NET_VIEW && e.sequence==received+1 && e.length==sizeof payload);
            CHECK(memcmp(payload,e.payload,sizeof payload)==0);++received;}
        pause_tick();
    }
    CHECK(net_send(&p,NET_EVENT,sent+1,"event",5)==1);
    CHECK(net_send(&p,NET_EVENT,sent+1,"event",5)==0);
    CHECK(net_pump(&p,&e)==0);CHECK(next(&g,&e)==1 && e.kind==NET_EVENT);
    CHECK(e.length==5 && memcmp(e.payload,"event",5)==0);
    CHECK(net_send(&p,NET_EVENT,sent+1,"replay",6)==-1);
    net_peer_close(&g);cleaned(&h,&p);
    puts("PASS bounded max-payload stream/backpressure, event, send validation");
}
int main(void) {
    struct net_host h;struct net_peer p;struct net_config c=config();
    alarm(20);
    CHECK(net_listen(&h,"0.0.0.0",0)==-1);
    CHECK(net_connect(&p,"192.0.2.1",1,&c)==-1);
    roundtrip();for(unsigned i=0;i<4;++i)mismatch(i);
    puts("PASS protocol/ROM/build/two-player mismatch rejection");
    for(unsigned i=0;i<9;++i)malformed(i);
    puts("PASS truncated header/body, oversize, unknown kind, identity, epoch, zero sequence, flags, magic");
    fragmented_and_replay();
    for(unsigned i=0;i<=HOST_STREAM;++i)guest_validation((enum guest_case)i);
    puts("PASS raw host: guest WELCOME/metadata/direction rejection, interleaved VIEW/EVENT replay suppression");
    second_guest();bounded_stream();
    puts("PASS all transport tests (helper only; no native gameplay claim)");return 0;
}
