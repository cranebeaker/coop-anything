#include "net.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum { HEADER = 12, META = 20, HELLO_SIZE = 84, HELLO = 1, WELCOME = 5 };
static uint32_t get32(const unsigned char *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static uint64_t get64(const unsigned char *p) {
    return (uint64_t)get32(p)<<32 | get32(p+4);
}
static void put32(unsigned char *p,uint32_t v) {
    p[0]=(unsigned char)(v>>24);p[1]=(unsigned char)(v>>16);
    p[2]=(unsigned char)(v>>8);p[3]=(unsigned char)v;
}
static void put64(unsigned char *p,uint64_t v) {
    put32(p,(uint32_t)(v>>32));put32(p+4,(uint32_t)v);
}
static int pending(void) { return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR; }
static int nonblock(int fd) {
    int flags=fcntl(fd,F_GETFL,0),clo=fcntl(fd,F_GETFD,0);
    return flags<0 || clo<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0 ||
           fcntl(fd,F_SETFD,clo|FD_CLOEXEC)<0 ? -1:0;
}
static int address(struct sockaddr_in *a,const char *text,uint16_t port) {
    memset(a,0,sizeof *a);a->sin_family=AF_INET;a->sin_port=htons(port);
    if(!text || strcmp(text,"127.0.0.1")!=0)return -1;
    a->sin_addr.s_addr=htonl(INADDR_LOOPBACK);return 0;
}
static int valid(const struct net_config *c,int host) {
    return c && c->protocol && (c->capabilities & NET_CAP_TWO_PLAYERS) && (!host || c->epoch);
}
static void init(struct net_peer *p,const struct net_config *c,int fd,int host) {
    memset(p,0,sizeof *p);p->fd=fd;p->config=*c;p->host_side=host;p->in_need=HEADER;
    p->epoch=host?c->epoch:0;
}
static void header(struct net_peer *p,unsigned kind,size_t n) {
    memcpy(p->out,"OOC1",4);p->out[4]=(unsigned char)kind;
    memset(p->out+5,0,3);put32(p->out+8,(uint32_t)n);
    p->out_used=HEADER+n;p->out_sent=0;
}
static void greeting(struct net_peer *p,unsigned kind) {
    unsigned char *b=p->out+HEADER;
    header(p,kind,HELLO_SIZE);put32(b,p->config.protocol);put64(b+4,p->epoch);
    memcpy(b+12,p->config.build,32);memcpy(b+44,p->config.rom,32);
    put32(b+76,p->config.capabilities);b[80]=NET_P2;memset(b+81,0,3);
}
int net_listen(struct net_host *h,const char *text,uint16_t port) {
    struct sockaddr_in a; socklen_t n=sizeof a; int yes=1,fd;
    if(!h)return -1;
    h->fd=-1;h->guest_fd=-1;h->port=0;h->epoch=0;h->input_sequence=0;
    if(address(&a,text,port)<0)return -1;
    fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
    if(nonblock(fd)<0 || setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes)<0 ||
       bind(fd,(struct sockaddr *)&a,sizeof a)<0 || listen(fd,2)<0 ||
       getsockname(fd,(struct sockaddr *)&a,&n)<0){close(fd);return -1;}
    h->fd=fd;h->port=ntohs(a.sin_port);return 0;
}
void net_host_close(struct net_host *h) {
    if(!h)return;
    if(h->fd>=0)close(h->fd);
    h->fd=-1;h->port=0;
    /* Active peer is separately owned: close it before destroying host storage. */
}
void net_peer_close(struct net_peer *p) {
    if(!p)return;
    if(p->fd>=0) {
        if(p->owner && p->owner->guest_fd==p->fd)p->owner->guest_fd=-1;
        close(p->fd);
    }
    p->fd=-1;p->ready=0;p->terminal=NET_CLOSED;
    p->in_used=0;p->out_used=0;p->out_sent=0;
}
int net_accept(struct net_host *h,struct net_peer *p,const struct net_config *c) {
    int fd;
    if(!h || !p || h->fd<0 || !valid(c,1))return -1;
    fd=accept(h->fd,NULL,NULL);
    if(fd<0)return pending()?0:-1;
    if(h->guest_fd>=0){close(fd);return -2;}
    if(nonblock(fd)<0){close(fd);return -1;}
    if(h->epoch!=c->epoch){h->epoch=c->epoch;h->input_sequence=0;}
    init(p,c,fd,1);p->owner=h;p->rx_sequence=h->input_sequence;h->guest_fd=fd;return 1;
}
int net_connect(struct net_peer *p,const char *text,uint16_t port,const struct net_config *c) {
    struct sockaddr_in a;int fd;
    if(!p)return -1;
    memset(p,0,sizeof *p);p->fd=-1;
    if(!valid(c,0) || !port || address(&a,text,port)<0)return -1;
    fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
    if(nonblock(fd)<0){close(fd);return -1;}
    if(connect(fd,(struct sockaddr *)&a,sizeof a)<0 && errno!=EINPROGRESS){close(fd);return -1;}
    init(p,c,fd,0);greeting(p,HELLO);return 0;
}
static int fail(struct net_peer *p,struct net_event *e,enum net_kind kind) {
    net_peer_close(p);p->terminal=kind;e->kind=kind;return -1;
}
static int compatible(struct net_peer *p,const unsigned char *b) {
    return get32(b)==p->config.protocol && memcmp(b+12,p->config.build,32)==0 &&
           memcmp(b+44,p->config.rom,32)==0 && (get32(b+76)&NET_CAP_TWO_PLAYERS) &&
           b[80]==NET_P2 && !b[81] && !b[82] && !b[83];
}
static int parse(struct net_peer *p,struct net_event *e) {
    const unsigned char *b=p->in+HEADER;unsigned kind=p->in[4];
    size_t n=p->in_need-HEADER;
    if(!p->ready) {
        if(n!=HELLO_SIZE || kind!=(unsigned)(p->host_side?HELLO:WELCOME) || !compatible(p,b))
            return fail(p,e,NET_ERROR);
        if(p->host_side) {
            if(get64(b+4)!=0)return fail(p,e,NET_ERROR);
            greeting(p,WELCOME);
        } else {
            p->epoch=get64(b+4);if(!p->epoch)return fail(p,e,NET_ERROR);
        }
        p->ready=1;e->kind=NET_READY;e->player=NET_P2;e->epoch=p->epoch;return 1;
    }
    if((p->host_side && kind!=NET_INPUT) ||
       (!p->host_side && kind!=NET_VIEW && kind!=NET_EVENT) || n<META ||
       get64(b)!=p->epoch || b[16]!=(p->host_side?NET_P2:NET_P1) ||
       b[17] || b[18] || b[19] || !get64(b+8))return fail(p,e,NET_ERROR);
    if(get64(b+8)<=p->rx_sequence)return 0; /* Drop duplicates/stale messages. */
    p->rx_sequence=get64(b+8);
    if(p->owner)p->owner->input_sequence=p->rx_sequence;
    e->kind=(enum net_kind)kind;e->player=b[16];
    e->epoch=p->epoch;e->sequence=p->rx_sequence;e->length=n-META;
    memcpy(e->payload,b+META,e->length);return 1;
}
int net_pump(struct net_peer *p,struct net_event *e) {
    ssize_t n;int r;
    if(!p || !e)return -1;
    memset(e,0,sizeof *e);
    if(p->fd<0){e->kind=p->terminal==NET_ERROR?NET_ERROR:NET_CLOSED;return -1;}
    if(p->out_sent<p->out_used) {
        n=send(p->fd,p->out+p->out_sent,p->out_used-p->out_sent,MSG_NOSIGNAL);
        if(n<0 && !pending())return fail(p,e,NET_ERROR);
        if(n>0)p->out_sent+=(size_t)n;
        if(p->out_sent==p->out_used)p->out_used=p->out_sent=0;
    }
    n=recv(p->fd,p->in+p->in_used,p->in_need-p->in_used,0);
    if(n<0)return pending()?0:fail(p,e,NET_ERROR);
    if(!n)return fail(p,e,p->ready && !p->in_used?NET_CLOSED:NET_ERROR);
    p->in_used+=(size_t)n;
    if(p->in_used<p->in_need)return 0;
    if(p->in_need==HEADER) {
        unsigned kind=p->in[4];uint32_t size=get32(p->in+8);
        if(memcmp(p->in,"OOC1",4)!=0 || p->in[5] || p->in[6] || p->in[7] ||
           kind<HELLO || kind>WELCOME || size>NET_FRAME_MAX-HEADER ||
           (!p->ready && size!=HELLO_SIZE) || (p->ready && size<META))
            return fail(p,e,NET_ERROR);
        p->in_need=HEADER+size;return 0;
    }
    r=parse(p,e);
    if(p->fd>=0){p->in_used=0;p->in_need=HEADER;}
    return r;
}
int net_send(struct net_peer *p,enum net_kind kind,uint64_t seq,const void *data,size_t n) {
    unsigned char *b;
    if(!p || p->fd<0 || !p->ready || n>NET_PAYLOAD_MAX || (n && !data))return -1;
    if((p->host_side && kind!=NET_VIEW && kind!=NET_EVENT) ||
       (!p->host_side && kind!=NET_INPUT))return -1;
    if(p->out_used)return 0;
    if(!seq || seq<=p->tx_sequence)return -1;
    header(p,(unsigned)kind,META+n);b=p->out+HEADER;
    put64(b,p->epoch);put64(b+8,seq);b[16]=(unsigned char)(p->host_side?NET_P1:NET_P2);
    memset(b+17,0,3);if(n)memcpy(b+META,data,n);p->tx_sequence=seq;return 1;
}
