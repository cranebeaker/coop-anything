#include "steam.h"
#include "mock_control.h"
#include <arpa/inet.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL steam:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *library="./tests/libmocksteam.so";
static struct {
    void *handle;void (*reset)(const struct mock_options *);void (*command)(int,uint64_t);
    void (*message)(uint64_t,const void *,int,int);struct mock_stats (*stats)(void);
    const char *(*lobby_data)(const char *);struct mock_delivery (*delivery)(void);
} mock;
static void controls(void) {
    mock.handle=dlopen(library,RTLD_NOW|RTLD_LOCAL);CHECK(mock.handle);
#define LOAD(field,name) do{void *p=dlsym(mock.handle,name);CHECK(p);memcpy(&mock.field,&p,sizeof p);}while(0)
    LOAD(reset,"mock_reset");LOAD(command,"mock_command");LOAD(message,"mock_message");LOAD(stats,"mock_stats");
    LOAD(lobby_data,"mock_lobby_data");LOAD(delivery,"mock_delivery");
#undef LOAD
}
static struct mock_options options(int host) {
    struct mock_options o={0};o.self=host?1001:1002;o.owner=1001;o.peer=host?1002:1001;o.fd=-1;return o;
}
static struct sa_config config(void) {
    struct sa_config c={"oracle-seasons-coop","build-test","1",1000};return c;
}
static int event(struct sa *s,struct sa_event *e,uint64_t now) {
    for(unsigned i=0;i<32;++i){int r=sa_poll(s,now,e);if(r)return r;}
    CHECK(0);return -1;
}
static void open_context(struct sa *s,int host) {
    struct mock_options o=options(host);struct sa_config c=config();mock.reset(&o);
    CHECK(sa_open(s,library,&c,100)==0);
}
static void guest_ready(struct sa *s) {
    struct sa_event e;open_context(s,0);CHECK(sa_join(s,9001,100)==0);
    CHECK(event(s,&e,101)==1 && e.type==SA_PARTNER_READY && e.player==2 && e.peer==1001);
}
/* Catches publishing a wrong key or a default/wrong configured value. */
static void host_publishes_configured_metadata(void) {
    struct sa s;struct sa_event e;struct mock_options o=options(1);
    struct sa_config c={"seasons-lobby-qa","revision-7f9-oracle","coop-v7",1000};
    mock.reset(&o);
    CHECK(strcmp(mock.lobby_data("game"),"")==0);
    CHECK(strcmp(mock.lobby_data("build"),"")==0);
    CHECK(strcmp(mock.lobby_data("protocol"),"")==0);
    CHECK(sa_open(&s,library,&c,100)==0 && sa_host(&s,100)==0);
    CHECK(event(&s,&e,101)==1 && e.type==SA_LOBBY_READY && e.lobby==9001);
    CHECK(strcmp(mock.lobby_data("game"),"seasons-lobby-qa")==0);
    CHECK(strcmp(mock.lobby_data("build"),"revision-7f9-oracle")==0);
    CHECK(strcmp(mock.lobby_data("protocol"),"coop-v7")==0);
    sa_close(&s);
    puts("PASS empty host lobby publishes exact nondefault game/build/protocol values");
}
/* Catches wrong-recipient sends and overwriting the occupied copied slot. */
static void busy_send_delivers_original_copy_once(void) {
    struct sa s;struct sa_event e;struct mock_delivery delivered;
    unsigned char caller[]={0x00,0x81,0x2a,0xff,0x19,0x00,0x7e};
    guest_ready(&s);mock.command(MOCK_SEND_BUSY,1);
    CHECK(sa_send(&s,caller,sizeof caller,102)==1);
    memset(caller,0x55,sizeof caller);
    CHECK(sa_poll(&s,103,&e)==0 && e.type==SA_NONE);
    CHECK(mock.delivery().count==0);
    CHECK(sa_send(&s,"replace",7,104)==0);
    CHECK(sa_poll(&s,105,&e)==0 && mock.delivery().count==0);
    mock.command(MOCK_SEND_BUSY,0);
    CHECK(sa_poll(&s,106,&e)==0 && e.type==SA_NONE);
    delivered=mock.delivery();
    CHECK(delivered.count==1 && delivered.recipient==1001 && delivered.size==7);
    CHECK(memcmp(delivered.data,"\x00\x81\x2a\xff\x19\x00\x7e",7)==0);
    CHECK(sa_poll(&s,107,&e)==0 && sa_poll(&s,108,&e)==0);
    CHECK(mock.delivery().count==1);
    sa_close(&s);
    puts("PASS busy send delivers original copied binary bytes to peer1001 exactly once");
}
/* Catches removal of only the send deadline, with receive silence unexpired. */
static void send_deadline_expires_while_peer_is_live(void) {
    struct sa s;struct sa_event e;guest_ready(&s);
    mock.command(MOCK_SEND_BUSY,1);
    CHECK(sa_send(&s,"pending",7,102)==1);
    mock.message(1001,"alive",5,0);
    CHECK(sa_poll(&s,1100,&e)==1 && e.type==SA_MESSAGE && e.peer==1001);
    CHECK(e.size==5 && memcmp(e.data,"alive",5)==0);
    CHECK(mock.stats().releases==1 && mock.delivery().count==0);
    CHECK(sa_poll(&s,1101,&e)==0 && e.type==SA_NONE);
    CHECK(mock.stats().leaves==0 && mock.stats().shutdowns==0);
    CHECK(sa_poll(&s,1102,&e)==-1 && e.type==SA_FAILURE);
    CHECK(mock.delivery().count==0 && mock.stats().closes==1);
    CHECK(mock.stats().closed==1001 && mock.stats().leaves==1 && mock.stats().shutdowns==1 && mock.stats().fd==-1);
    CHECK(sa_poll(&s,1103,&e)==-1 && e.type==SA_FAILURE);
    sa_close(&s);
    CHECK(mock.stats().closes==1 && mock.stats().leaves==1 && mock.stats().shutdowns==1);
    puts("PASS busy send fails at1102 after peer alive at1100 with exactly-once cleanup");
}
static void lobby_and_invite(void) {
    struct sa s;struct sa_event e;struct sa_config c=config();
    CHECK(sa_open(&s,"./tests/absent-library.so",&c,100)==-1);
    open_context(&s,1);
    mock.command(MOCK_REQUEST,9999);CHECK(sa_poll(&s,100,&e)==0);
    CHECK(mock.stats().accepts==0 && mock.stats().closed==9999);
    CHECK(sa_host(&s,100)==0);CHECK(sa_host(&s,100)==-1);
    CHECK(event(&s,&e,101)==1 && e.type==SA_LOBBY_READY && e.player==1 && e.lobby==9001);
    CHECK(mock.stats().create_type==1 && mock.stats().max_members==2 && mock.stats().writes==3);
    CHECK(sa_invite(&s)==0 && mock.stats().invites==1);
    mock.command(MOCK_COUNT,2);
    CHECK(event(&s,&e,102)==1 && e.type==SA_PARTNER_READY && e.peer==1002 && e.player==1);
    sa_close(&s);CHECK(mock.stats().leaves==1 && mock.stats().shutdowns==1 && mock.stats().closes==2);
    open_context(&s,0);mock.command(MOCK_INVITE,1001);
    CHECK(event(&s,&e,101)==1 && e.type==SA_JOIN_REQUEST && e.lobby==9001 && e.peer==1001);
    CHECK(sa_join(&s,e.lobby,102)==0);
    CHECK(event(&s,&e,103)==1 && e.type==SA_PARTNER_READY && e.player==2 && e.peer==1001);
    CHECK(sa_invite(&s)==-1);sa_close(&s);
    open_context(&s,1);CHECK(sa_host(&s,100)==0 && event(&s,&e,101)==1);
    mock.command(MOCK_COUNT,2);CHECK(event(&s,&e,102)==1 && e.peer==1002);
    mock.command(MOCK_PEER,9999);CHECK(sa_poll(&s,103,&e)==-1 && e.type==SA_FAILURE);
    puts("PASS friends-only max2 host, metadata, explicit invite/join callback, stable P1/P2, cleanup");
}
static void rejected_metadata_and_failures(void) {
    struct sa s;struct sa_event e;struct mock_options o;struct sa_config c=config();
    for(int i=1;i<=3;++i) {
        o=options(0);o.bad_metadata=i;mock.reset(&o);
        CHECK(sa_open(&s,library,&c,100)==0 && sa_join(&s,9001,100)==0);
        CHECK(event(&s,&e,101)==-1 && e.type==SA_FAILURE);
        CHECK(mock.stats().leaves==1 && mock.stats().shutdowns==1);
    }
    o=options(1);o.init_error=2;mock.reset(&o);CHECK(sa_open(&s,library,&c,100)==-1);
    CHECK(mock.stats().shutdowns==0);
    o=options(1);o.logged_out=1;mock.reset(&o);CHECK(sa_open(&s,library,&c,100)==-1);
    CHECK(mock.stats().shutdowns==1);
    o=options(1);o.async_failure=1;mock.reset(&o);CHECK(sa_open(&s,library,&c,100)==0);
    CHECK(sa_host(&s,100)==0 && event(&s,&e,101)==-1 && e.type==SA_FAILURE);
    o=options(1);o.no_completion=1;mock.reset(&o);CHECK(sa_open(&s,library,&c,100)==0);
    CHECK(sa_host(&s,100)==0 && sa_poll(&s,1101,&e)==-1 && e.type==SA_FAILURE);
    open_context(&s,0);mock.command(MOCK_BAD_CALLBACK,0);
    CHECK(event(&s,&e,101)==-1 && mock.stats().freed_callbacks==1);
    puts("PASS metadata mismatch, initialization/account unavailable, async failure, timeout, malformed callback");
}
static void membership_and_messages(void) {
    struct sa s;struct sa_event e;guest_ready(&s);
    mock.command(MOCK_REQUEST,9999);CHECK(sa_poll(&s,102,&e)==0);
    CHECK(mock.stats().accepts==0 && mock.stats().closed==9999);
    mock.command(MOCK_REQUEST,1001);CHECK(sa_poll(&s,103,&e)==0);
    CHECK(mock.stats().accepts==1 && mock.stats().accepted==1001);
    mock.message(9999,"intruder",8,0);CHECK(sa_poll(&s,104,&e)==0 && mock.stats().releases==1);
    mock.message(1001,"host-state",10,0);
    CHECK(event(&s,&e,105)==1 && e.type==SA_MESSAGE && e.peer==1001 && e.player==2);
    CHECK(e.size==10 && memcmp(e.data,"host-state",10)==0 && mock.stats().releases==2);
    CHECK(sa_send(&s,"input",5,106)==1 && sa_send(&s,"overwrite",9,106)==0);
    mock.command(MOCK_SEND_BUSY,1);CHECK(sa_poll(&s,107,&e)==0 && mock.stats().sends==0);
    mock.command(MOCK_SEND_BUSY,0);CHECK(sa_poll(&s,108,&e)==0 && mock.stats().sends==1);
    CHECK(sa_send(&s,NULL,1,109)==-1 && sa_send(&s,"x",SA_MAX_PAYLOAD+1,109)==-1);
    mock.command(MOCK_OWNER,9999);CHECK(sa_poll(&s,110,&e)==-1 && e.type==SA_FAILURE);
    CHECK(mock.stats().shutdowns==1 && mock.stats().leaves==1);
    for(unsigned i=0;i<5;++i) {
        guest_ready(&s);
        if(i==0)mock.command(MOCK_COUNT,3);
        if(i==1)mock.command(MOCK_COUNT,1);
        if(i==2)mock.command(MOCK_FAILED,1001);
        if(i==3)mock.message(1001,"x",4097,0);
        if(i==4)mock.message(1001,"x",1,1);
        CHECK(event(&s,&e,102)==-1 && e.type==SA_FAILURE);
        if(i>=3)CHECK(mock.stats().releases==1);
    }
    guest_ready(&s);CHECK(sa_poll(&s,1102,&e)==-1 && e.type==SA_FAILURE);
    guest_ready(&s);CHECK(sa_send(&s,"pending",7,102)==1);mock.command(MOCK_SEND_BUSY,1);
    CHECK(sa_poll(&s,1103,&e)==-1 && e.type==SA_FAILURE);
    guest_ready(&s);CHECK(sa_send(&s,"fail",4,102)==1);mock.command(MOCK_SEND_ERROR,1);
    CHECK(sa_poll(&s,103,&e)==-1 && e.type==SA_FAILURE);
    puts("PASS joined-partner/session/message filtering, bounded queue, backpressure, membership/failure/timeouts");
}
static void foreign_traffic_does_not_extend_partner_deadline(void) {
    struct sa s;struct sa_event e;guest_ready(&s); /* Joined at101, timeout1000. */
    mock.message(9999,"before",6,0);
    CHECK(sa_poll(&s,1100,&e)==0 && e.type==SA_NONE);
    CHECK(mock.stats().releases==1 && mock.stats().closed==9999);
    CHECK(mock.stats().leaves==0 && mock.stats().shutdowns==0);
    mock.message(9999,"after",5,0);
    CHECK(sa_poll(&s,1102,&e)==-1 && e.type==SA_FAILURE);
    CHECK(mock.stats().releases==2 && mock.stats().closes==3);
    CHECK(mock.stats().leaves==1 && mock.stats().shutdowns==1 && mock.stats().fd==-1);
    CHECK(sa_poll(&s,2102,&e)==-1 && e.type==SA_FAILURE);
    CHECK(mock.stats().releases==2 && mock.stats().leaves==1 && mock.stats().shutdowns==1);
    puts("PASS foreign traffic drops before deadline and cannot bypass partner silence cleanup");
}
static uint64_t now_ms(void) {struct timespec t;CHECK(clock_gettime(CLOCK_MONOTONIC,&t)==0);return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static void tick(void){struct timespec t={0,1000000};nanosleep(&t,NULL);}
static int socket_port(uint16_t *port) {
    int fd=socket(AF_INET,SOCK_DGRAM,0);struct sockaddr_in a={0};socklen_t size=sizeof a;CHECK(fd>=0);
    a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    CHECK(bind(fd,(struct sockaddr *)&a,sizeof a)==0 && getsockname(fd,(struct sockaddr *)&a,&size)==0);
    *port=ntohs(a.sin_port);return fd;
}
static void two_process(void) {
    uint16_t host_port,guest_port;int host_fd=socket_port(&host_port),guest_fd=socket_port(&guest_port),sync[2];
    struct mock_options o=options(1);struct sa_config c=config();struct sa s;struct sa_event e;
    CHECK(pipe(sync)==0);pid_t child=fork();CHECK(child>=0);
    if(!child) {
        char start;close(sync[1]);close(host_fd);CHECK(read(sync[0],&start,1)==1);close(sync[0]);
        o=options(0);o.fd=guest_fd;o.remote_port=host_port;mock.reset(&o);close(guest_fd);
        CHECK(sa_open(&s,library,&c,now_ms())==0 && sa_join(&s,9001,now_ms())==0);
        unsigned char input[SA_MAX_PAYLOAD];memset(input,'G',sizeof input);
        int sent=0,received=0;
        while(!received) {
            int r=sa_poll(&s,now_ms(),&e);CHECK(r>=0);
            if(r && e.type==SA_PARTNER_READY){CHECK(e.peer==1001 && e.player==2);CHECK(sa_send(&s,input,sizeof input,now_ms())==1);sent=1;}
            if(r && e.type==SA_MESSAGE){
                CHECK(e.peer==1001 && e.size==SA_MAX_PAYLOAD);
                for(size_t i=0;i<e.size;++i)CHECK(e.data[i]=='V');
                received=1;
            }
            tick();
        }
        CHECK(sent && mock.stats().accepts>=1);sa_close(&s);CHECK(mock.stats().fd==-1);_exit(0);
    }
    close(sync[0]);close(guest_fd);o.fd=host_fd;o.remote_port=guest_port;mock.reset(&o);close(host_fd);
    CHECK(sa_open(&s,library,&c,now_ms())==0 && sa_host(&s,now_ms())==0);
    unsigned char view[SA_MAX_PAYLOAD];memset(view,'V',sizeof view);
    int started=0,got_input=0,disconnected=0;
    while(!disconnected) {
        int r=sa_poll(&s,now_ms(),&e);
        if(r<0){CHECK(got_input && e.type==SA_FAILURE);disconnected=1;break;}
        if(r && e.type==SA_LOBBY_READY){CHECK(write(sync[1],"x",1)==1);close(sync[1]);started=1;}
        if(r && e.type==SA_MESSAGE) {
            CHECK(e.peer==1002 && e.size==SA_MAX_PAYLOAD);
            for(size_t i=0;i<e.size;++i)CHECK(e.data[i]=='G');
            got_input=1;CHECK(sa_send(&s,view,sizeof view,now_ms())==1);
        }
        tick();
    }
    CHECK(started && got_input && mock.stats().fd==-1 && mock.stats().releases==1);
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    /* Exact UDP ports are available again after mock shutdown. */
    int fd=socket(AF_INET,SOCK_DGRAM,0);struct sockaddr_in a={0};CHECK(fd>=0);
    a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(host_port);
    CHECK(bind(fd,(struct sockaddr *)&a,sizeof a)==0);close(fd);
    fd=socket(AF_INET,SOCK_DGRAM,0);CHECK(fd>=0);a.sin_port=htons(guest_port);
    CHECK(bind(fd,(struct sockaddr *)&a,sizeof a)==0);close(fd);
    puts("PASS two mock processes: real loopback max-payload input/view, session accept, peer leave, exact port cleanup");
}
int main(int argc,char **argv) {
    alarm(20);controls();
    if(argc==2) {
        if(strcmp(argv[1],"--oracle-metadata")==0)host_publishes_configured_metadata();
        else if(strcmp(argv[1],"--oracle-send")==0)busy_send_delivers_original_copy_once();
        else if(strcmp(argv[1],"--oracle-deadline")==0)send_deadline_expires_while_peer_is_live();
        else CHECK(0);
    } else {
        CHECK(argc==1);
        host_publishes_configured_metadata();busy_send_delivers_original_copy_once();send_deadline_expires_while_peer_is_live();
        lobby_and_invite();rejected_metadata_and_failures();membership_and_messages();foreign_traffic_does_not_extend_partner_deadline();two_process();
    }
    dlclose(mock.handle);puts("PASS Steam helper mock suite (no real Steam or Internet proof)");return 0;
}
