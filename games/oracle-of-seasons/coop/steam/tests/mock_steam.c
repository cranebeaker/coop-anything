/* Original bounded Steam flat-ABI mock. No Valve implementation code. */
#include "steam_abi.h"
#include "mock_control.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
struct queued_callback {int kind,size;unsigned char data[136];};
static struct {
    struct mock_options opt;struct mock_stats stat;
    struct queued_callback callbacks[8];int callbacks_used,in_callback;
    struct steam_message message;unsigned char payload[4096];int has_message,needs_accept;
    unsigned char result[32];int result_kind,result_size;
    int count,busy,send_error;char game[32],build[64],protocol[16];
} m;
static const uint64_t lobby=9001;
static void callback(int kind,const void *data,int size) {
    if(m.callbacks_used>=8 || size>136)return;
    struct queued_callback *q=&m.callbacks[m.callbacks_used++];
    q->kind=kind;q->size=size;if(size)memcpy(q->data,data,(size_t)size);
}
static void send_packet(unsigned char kind,const void *data,size_t size) {
    unsigned char b[4112];struct sockaddr_in a={0};
    if(m.stat.fd<0 || size>4096)return;
    memset(b,0,16);b[0]=kind;memcpy(b+8,&m.opt.self,8);
    if(size)memcpy(b+16,data,size);
    a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(m.opt.remote_port);
    (void)sendto(m.stat.fd,b,16+size,0,(struct sockaddr *)&a,sizeof a);
}
void mock_reset(const struct mock_options *o) {
    if(m.stat.fd>=0 && m.opt.self)close(m.stat.fd);
    memset(&m,0,sizeof m);m.opt=*o;m.stat.fd=-1;
    m.count=o->self==o->owner?1:2;
    strcpy(m.game,"oracle-seasons-coop");strcpy(m.build,"build-test");strcpy(m.protocol,"1");
    if(o->fd>=0) {
        struct sockaddr_in a;socklen_t n=sizeof a;
        if(getsockname(o->fd,(struct sockaddr *)&a,&n)==0 && a.sin_family==AF_INET &&
           a.sin_addr.s_addr==htonl(INADDR_LOOPBACK)) {
            m.stat.fd=dup(o->fd);
            if(m.stat.fd>=0)(void)fcntl(m.stat.fd,F_SETFL,O_NONBLOCK);
        }
    }
}
struct mock_stats mock_stats(void){return m.stat;}
void SteamAPI_SteamNetworkingIdentity_SetSteamID64(struct steam_identity *id,uint64_t value) {
    uint32_t type=16,size=8;memset(id,0,sizeof *id);memcpy(id->bytes,&type,4);
    memcpy(id->bytes+4,&size,4);memcpy(id->bytes+8,&value,8);
}
uint64_t SteamAPI_SteamNetworkingIdentity_GetSteamID64(const struct steam_identity *id) {
    uint32_t type,size;uint64_t value;memcpy(&type,id->bytes,4);memcpy(&size,id->bytes+4,4);
    memcpy(&value,id->bytes+8,8);return type==16 && size==8?value:0;
}
void mock_message(uint64_t peer,const void *data,int size,int channel) {
    if(m.has_message)return;
    memset(&m.message,0,sizeof m.message);m.message.size=size;m.message.channel=channel;
    m.message.data=m.payload;SteamAPI_SteamNetworkingIdentity_SetSteamID64(&m.message.peer,peer);
    if(data && size>0 && size<=4096)memcpy(m.payload,data,(size_t)size);
    m.has_message=1;
}
void mock_command(int command,uint64_t arg) {
    unsigned char b[136]={0};struct steam_identity id;
    switch(command) {
    case MOCK_INVITE:memcpy(b,&lobby,8);memcpy(b+8,&arg,8);callback(333,b,16);break;
    case MOCK_REQUEST:case MOCK_FAILED:
        SteamAPI_SteamNetworkingIdentity_SetSteamID64(&id,arg);callback(command==MOCK_REQUEST?1251:1252,&id,136);break;
    case MOCK_COUNT:m.count=(int)arg;break;
    case MOCK_OWNER:m.opt.owner=arg;break;
    case MOCK_PEER:m.opt.peer=arg;break;
    case MOCK_SEND_BUSY:m.busy=(int)arg;break;
    case MOCK_SEND_ERROR:m.send_error=(int)arg;break;
    case MOCK_BAD_CALLBACK:callback(333,b,3);break;
    default:break;
    }
}
int SteamAPI_InitFlat(char *error) {if(error)error[0]=0;return m.opt.init_error;}
void SteamAPI_Shutdown(void) {++m.stat.shutdowns;if(m.stat.fd>=0)close(m.stat.fd);m.stat.fd=-1;}
int SteamAPI_GetHSteamPipe(void){return 1;}
void SteamAPI_ManualDispatch_Init(void){}
void SteamAPI_ManualDispatch_RunFrame(int pipe) {
    unsigned char b[4112];ssize_t n;uint64_t sender;(void)pipe;
    if(m.stat.fd<0 || m.has_message)return;
    for(unsigned i=0;i<8;++i) {
        n=recv(m.stat.fd,b,sizeof b,0);if(n<16)return;memcpy(&sender,b+8,8);
        if(b[0]==1){m.count=2;m.opt.peer=sender;}
        if(b[0]==2){
            struct steam_identity id;mock_message(sender,b+16,(int)n-16,0);
            if(m.stat.accepted!=sender){
                SteamAPI_SteamNetworkingIdentity_SetSteamID64(&id,sender);
                m.needs_accept=1;callback(1251,&id,136);
            }
            return;
        }
        if(b[0]==3)m.count=1;
    }
}
bool SteamAPI_ManualDispatch_GetNextCallback(int pipe,struct steam_callback *cb) {
    (void)pipe;if(!m.callbacks_used || m.in_callback)return false;
    cb->user=1;cb->kind=m.callbacks[0].kind;cb->size=m.callbacks[0].size;
    cb->data=m.callbacks[0].data;m.in_callback=1;return true;
}
void SteamAPI_ManualDispatch_FreeLastCallback(int pipe) {
    (void)pipe;if(!m.in_callback)return;
    ++m.stat.freed_callbacks;--m.callbacks_used;
    memmove(m.callbacks,m.callbacks+1,(size_t)m.callbacks_used*sizeof m.callbacks[0]);m.in_callback=0;
}
bool SteamAPI_ManualDispatch_GetAPICallResult(int pipe,uint64_t call,void *data,int size,int kind,bool *failed) {
    (void)pipe;if(call!=17 || size!=m.result_size || kind!=m.result_kind)return false;
    *failed=m.opt.async_failure!=0;memcpy(data,m.result,(size_t)size);return true;
}
void *SteamAPI_SteamUser_v023(void){return &m;}
void *SteamAPI_SteamMatchmaking_v009(void){return &m;}
void *SteamAPI_SteamFriends_v018(void){return &m;}
void *SteamAPI_SteamNetworkingMessages_SteamAPI_v002(void){return &m;}
uint64_t SteamAPI_ISteamUser_GetSteamID(void *self){(void)self;return m.opt.self;}
bool SteamAPI_ISteamUser_BLoggedOn(void *self){(void)self;return !m.opt.logged_out;}
static uint64_t complete(int kind,int size) {
    unsigned char b[16]={0};uint64_t call=17;uint32_t length=(uint32_t)size;
    m.result_kind=kind;m.result_size=size;
    memcpy(b,&call,8);memcpy(b+8,&kind,4);memcpy(b+12,&length,4);
    if(!m.opt.no_completion)callback(703,b,16);
    return call;
}
uint64_t SteamAPI_ISteamMatchmaking_CreateLobby(void *self,int type,int max) {
    int32_t result=1;(void)self;m.stat.create_type=type;m.stat.max_members=max;
    memset(m.result,0,sizeof m.result);memcpy(m.result,&result,4);memcpy(m.result+4,&lobby,8);
    return complete(513,12);
}
uint64_t SteamAPI_ISteamMatchmaking_JoinLobby(void *self,uint64_t target) {
    uint32_t response=target==lobby?1:2;(void)self;m.count=2;
    memset(m.result,0,sizeof m.result);memcpy(m.result,&target,8);memcpy(m.result+16,&response,4);
    send_packet(1,NULL,0);return complete(504,20);
}
void SteamAPI_ISteamMatchmaking_LeaveLobby(void *self,uint64_t id){(void)self;(void)id;++m.stat.leaves;send_packet(3,NULL,0);}
bool SteamAPI_ISteamMatchmaking_SetLobbyData(void *self,uint64_t id,const char *key,const char *value) {
    (void)self;(void)id;++m.stat.writes;
    if(strcmp(key,"game")==0)snprintf(m.game,sizeof m.game,"%s",value);
    if(strcmp(key,"build")==0)snprintf(m.build,sizeof m.build,"%s",value);
    if(strcmp(key,"protocol")==0)snprintf(m.protocol,sizeof m.protocol,"%s",value);
    return true;
}
const char *SteamAPI_ISteamMatchmaking_GetLobbyData(void *self,uint64_t id,const char *key) {
    (void)self;(void)id;
    if((m.opt.bad_metadata==1 && strcmp(key,"game")==0) ||
       (m.opt.bad_metadata==2 && strcmp(key,"build")==0) ||
       (m.opt.bad_metadata==3 && strcmp(key,"protocol")==0))return "mismatch";
    return strcmp(key,"game")==0?m.game:strcmp(key,"build")==0?m.build:m.protocol;
}
uint64_t SteamAPI_ISteamMatchmaking_GetLobbyOwner(void *self,uint64_t id){(void)self;(void)id;return m.opt.owner;}
int SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(void *self,uint64_t id){(void)self;(void)id;return m.count;}
uint64_t SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(void *self,uint64_t id,int index) {
    (void)self;(void)id;if(index==0)return m.opt.owner;
    if(index==1)return m.opt.self==m.opt.owner?m.opt.peer:m.opt.self;
    return 9999;
}
int SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit(void *self,uint64_t id){(void)self;(void)id;return 2;}
bool SteamAPI_ISteamMatchmaking_SetLobbyJoinable(void *self,uint64_t id,bool joinable){(void)self;(void)id;(void)joinable;return true;}
void SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog(void *self,uint64_t id){(void)self;(void)id;++m.stat.invites;}
int SteamAPI_ISteamNetworkingMessages_SendMessageToUser(void *self,const struct steam_identity *id,const void *data,uint32_t size,int flags,int channel) {
    (void)self;(void)id;if(flags!=9 || channel!=0)return 2;
    if(m.busy)return 25;
    if(m.send_error)return 2;
    ++m.stat.sends;send_packet(2,data,size);return 1;
}
int SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel(void *self,int channel,struct steam_message **out,int max) {
    (void)self;(void)channel;if(max!=1)return -1;
    if(!m.has_message || (m.needs_accept && !m.stat.accepted))return 0;
    *out=&m.message;return 1;
}
bool SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser(void *self,const struct steam_identity *id) {
    (void)self;++m.stat.accepts;m.stat.accepted=SteamAPI_SteamNetworkingIdentity_GetSteamID64(id);return true;
}
bool SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser(void *self,const struct steam_identity *id) {
    (void)self;++m.stat.closes;m.stat.closed=SteamAPI_SteamNetworkingIdentity_GetSteamID64(id);return true;
}
void SteamAPI_SteamNetworkingMessage_t_Release(struct steam_message *msg){(void)msg;++m.stat.releases;m.has_message=0;m.needs_accept=0;}
