#include "steam.h"
#include <string.h>
enum { IDLE=1,CREATING,JOINING,IN_LOBBY };
static void identity(struct sa *s,struct steam_identity *id,uint64_t peer){s->api.set_id(id,peer);}
void sa_close(struct sa *s) {
    if(!s)return;
    if(s->initialized) {
        if(s->peer){struct steam_identity id;identity(s,&id,s->peer);s->api.close_session(s->messages,&id);}
        if(s->lobby)s->api.leave(s->match,s->lobby);
        s->api.shutdown();
    }
    steam_unload(&s->api);memset(s,0,sizeof *s);s->state=-1;
}
static int failed(struct sa *s,struct sa_event *e) {
    e->type=SA_FAILURE;e->lobby=s->lobby;e->peer=s->peer;sa_close(s);return -1;
}
static int string_ok(const char *p,size_t size){return p[0] && memchr(p,0,size)!=NULL;}
int sa_open(struct sa *s,const char *path,const struct sa_config *c,uint64_t now) {
    char error[1024];void *user;
    if(!s)return -1;
    memset(s,0,sizeof *s);
    if(!path || !c || !string_ok(c->game,sizeof c->game) || !string_ok(c->build,sizeof c->build) ||
       !string_ok(c->protocol,sizeof c->protocol) || !c->timeout_ms || c->timeout_ms>600000 ||
       now>UINT64_MAX-c->timeout_ms || steam_load(&s->api,path)<0)return -1;
    s->config=*c;s->now=now;
    if(s->api.init(error)!=0){sa_close(s);return -1;}
    s->initialized=1;user=s->api.user();s->match=s->api.match();s->friends=s->api.friends();s->messages=s->api.messages();
    if(!user || !s->match || !s->friends || !s->messages || !s->api.logged(user) || !(s->self=s->api.id(user))) {
        sa_close(s);return -1;
    }
    s->pipe=s->api.pipe();if(s->pipe<=0){sa_close(s);return -1;}
    s->api.dispatch_init();s->state=IDLE;return 0;
}
int sa_host(struct sa *s,uint64_t now) {
    if(!s || s->state!=IDLE || now<s->now || now>UINT64_MAX-s->config.timeout_ms)return -1;
    s->pending=s->api.create(s->match,1,2);if(!s->pending)return -1;
    s->host=1;s->state=CREATING;s->deadline=now+s->config.timeout_ms;s->now=now;return 0;
}
int sa_join(struct sa *s,uint64_t lobby,uint64_t now) {
    if(!s || s->state!=IDLE || !lobby || now<s->now || now>UINT64_MAX-s->config.timeout_ms)return -1;
    s->pending=s->api.join(s->match,lobby);if(!s->pending)return -1;
    s->host=0;s->target=lobby;s->state=JOINING;s->deadline=now+s->config.timeout_ms;s->now=now;return 0;
}
static bool metadata(struct sa *s) {
    const char *keys[]={"game","build","protocol"};
    const char *values[]={s->config.game,s->config.build,s->config.protocol};
    const size_t sizes[]={sizeof s->config.game,sizeof s->config.build,sizeof s->config.protocol};
    for(unsigned i=0;i<3;++i) {
        const char *value=s->api.get_data(s->match,s->lobby,keys[i]);
        if(!value || strncmp(value,values[i],sizes[i])!=0)return false;
    }
    return true;
}
/* UINT64_MAX is an invalid membership snapshot. Never adopt a new lobby owner. */
static uint64_t partner(struct sa *s) {
    int count=s->api.count(s->match,s->lobby),self_count=0;uint64_t other=0;
    if(s->api.owner(s->match,s->lobby)!=s->owner || s->api.limit(s->match,s->lobby)!=2 ||
       count<1 || count>2)return UINT64_MAX;
    for(int i=0;i<count;++i) {
        uint64_t id=s->api.member(s->match,s->lobby,i);
        if(!id)return UINT64_MAX;
        if(id==s->self)++self_count;
        else if(!other)other=id;
        else return UINT64_MAX;
    }
    if(self_count!=1 || (s->peer && other!=s->peer) || (!s->host && other!=s->owner))return UINT64_MAX;
    return other;
}
int sa_invite(struct sa *s) {
    if(!s || s->state!=IN_LOBBY || !s->host || s->peer || partner(s)!=0)return -1;
    s->api.invite(s->friends,s->lobby);return 0; /* API is void; overlay visibility unproven. */
}
int sa_send(struct sa *s,const void *data,size_t size,uint64_t now) {
    if(!s || s->state!=IN_LOBBY || !s->peer || partner(s)!=s->peer || size>SA_MAX_PAYLOAD ||
       (size && !data) || now<s->now || now>UINT64_MAX-s->config.timeout_ms)return -1;
    if(s->has_output)return 0;
    if(size)memcpy(s->output,data,size);
    s->queued=size;s->has_output=true;s->send_deadline=now+s->config.timeout_ms;return 1;
}
static int joined(struct sa *s,int kind,const unsigned char *data,struct sa_event *e) {
    if(kind==513) {
        struct steam_created c;memcpy(&c,data,sizeof c);
        if(c.result!=1 || !c.lobby)return -1;
        s->lobby=c.lobby;s->owner=s->self;
        if(s->api.owner(s->match,s->lobby)!=s->self || !s->api.joinable(s->match,s->lobby,false) ||
           !s->api.set_data(s->match,s->lobby,"game",s->config.game) ||
           !s->api.set_data(s->match,s->lobby,"build",s->config.build) ||
           !s->api.set_data(s->match,s->lobby,"protocol",s->config.protocol) ||
           !s->api.joinable(s->match,s->lobby,true))return -1;
        s->state=IN_LOBBY;if(partner(s)==UINT64_MAX)return -1;
        e->type=SA_LOBBY_READY;
    } else {
        struct steam_entered c;memcpy(&c,data,sizeof c);
        if(c.response!=1 || !c.lobby || c.lobby!=s->target)return -1;
        s->lobby=c.lobby;s->owner=s->api.owner(s->match,s->lobby);
        if(!s->owner || s->owner==s->self || !metadata(s))return -1;
        s->peer=s->owner;s->state=IN_LOBBY;
        if(partner(s)!=s->peer)return -1;
        s->last_rx=s->now;e->type=SA_PARTNER_READY;
    }
    s->pending=0;e->lobby=s->lobby;e->peer=s->peer;e->player=s->host?1:2;return 1;
}
static int callback(struct sa *s,const struct steam_callback *cb,struct sa_event *e) {
    if(cb->size<0 || cb->size>1024 || (cb->size && !cb->data))return -1;
    if(cb->kind==703) {
        struct steam_completed done;unsigned char result[32];bool error=false;
        if(cb->size!=(int)sizeof done)return -1;
        memcpy(&done,cb->data,sizeof done);
        if(!s->pending || done.call!=s->pending)return 0;
        int expected=s->state==CREATING?513:504;
        size_t size=expected==513?sizeof(struct steam_created):sizeof(struct steam_entered);
        if(done.kind!=expected || done.size!=size ||
           !s->api.result(s->pipe,done.call,result,(int)size,expected,&error) || error)return -1;
        return joined(s,expected,result,e);
    }
    if(cb->kind==333) {
        struct steam_join_request request;
        if(cb->size!=(int)sizeof request)return -1;
        memcpy(&request,cb->data,sizeof request);
        if(s->state!=IDLE || !request.lobby)return 0;
        e->type=SA_JOIN_REQUEST;e->lobby=request.lobby;e->peer=request.friend_id;return 1;
    }
    if(cb->kind==1251 || cb->kind==1252) {
        struct steam_identity id;
        if(cb->size<(int)sizeof id)return -1; /* SessionFailed info begins with identity. */
        memcpy(&id,cb->data,sizeof id);uint64_t peer=s->api.get_id(&id);
        if(cb->kind==1252)return s->peer && peer==s->peer?-1:0;
        if(s->state==IN_LOBBY && s->peer && peer==s->peer && partner(s)==s->peer) {
            if(!s->api.accept(s->messages,&id))return -1;
        } else s->api.close_session(s->messages,&id);
    }
    return 0;
}
int sa_poll(struct sa *s,uint64_t now,struct sa_event *e) {
    struct steam_callback cb;struct steam_identity id;struct steam_message *message=NULL;int r;
    if(!s || !e)return -1;
    memset(e,0,sizeof *e);
    if(!s->initialized){e->type=SA_FAILURE;return -1;}
    if(now<s->now)return failed(s,e);
    s->now=now;
    if((s->state==CREATING || s->state==JOINING) && now>=s->deadline)return failed(s,e);
    s->api.run(s->pipe);
    if(s->state==IN_LOBBY) {
        uint64_t peer=partner(s);
        if(peer==UINT64_MAX || !metadata(s))return failed(s,e);
        if(peer && !s->peer) {
            s->peer=peer;s->last_rx=now;e->type=SA_PARTNER_READY;
            e->lobby=s->lobby;e->peer=peer;e->player=s->host?1:2;return 1;
        }
    }
    for(unsigned i=0;i<8 && s->api.next(s->pipe,&cb);++i) {
        r=callback(s,&cb,e);s->api.free_callback(s->pipe);
        if(r<0)return failed(s,e);
        if(r>0)return r;
    }
    if(s->state!=IN_LOBBY || !s->peer)return 0;
    if(s->has_output) {
        if(now>=s->send_deadline)return failed(s,e);
        identity(s,&id,s->peer);
        r=s->api.send(s->messages,&id,s->output,(uint32_t)s->queued,9,0);
        if(r==1)s->has_output=false;
        else if(r!=25)return failed(s,e); /* LimitExceeded: retry bounded output slot. */
    }
    r=s->api.receive(s->messages,0,&message,1);
    if(r<0 || r>1 || (r==1 && !message))return failed(s,e);
    if(r==1) {
        uint64_t sender=s->api.get_id(&message->peer);
        if(sender!=s->peer) {
            s->api.close_session(s->messages,&message->peer);s->api.release(message);
            return now-s->last_rx>=s->config.timeout_ms?failed(s,e):0;
        }
        bool valid=message->size>=0 && message->size<=(int)SA_MAX_PAYLOAD && message->channel==0 &&
                   (!message->size || message->data);
        if(valid) {
            e->type=SA_MESSAGE;e->lobby=s->lobby;e->peer=sender;e->player=s->host?1:2;
            e->size=(size_t)message->size;if(e->size)memcpy(e->data,message->data,e->size);
        }
        s->api.release(message);if(!valid)return failed(s,e);
        s->last_rx=now;return 1;
    }
    if(now-s->last_rx>=s->config.timeout_ms)return failed(s,e);
    return 0;
}
