#ifndef COOP_PROTOCOL_H
#define COOP_PROTOCOL_H

// ================= Outlast 2 co-op sync protocol (Phase 3 design) =================
// Host-authoritative model, adapted from OpenOL (Outlast 1) architecture.
//
// Authority split:
//   - Host: enemy AI, doors, items, checkpoints / level streaming, story events,
//           damage & struggle/execution sequences, enemy relay to clients.
//   - Client: local pawn physics, camera, stamina; broadcasts own state at
//           20-30 Hz (unreliable UDP) and receives host-relayed state for others.
//
// Packet framing: single UDP socket, little-endian, fixed-size structs below.
// Sequence numbers allow interpolation on the receiver side; reliable channels
// retransmit until acknowledged (WORLD_EVENT, LEVEL_BARRIER, ENEMY_STATE).

#define COOP_MAGIC          0x4F4C // 'OL'
#define COOP_PROTO_VERSION  1
#define COOP_MAX_PLAYERS    8
#define COOP_TICK_RATE      25     // Hz, player state broadcast

// Packet types
#define COOP_PKT_PLAYER_STATE  0x01 // unreliable, 20-30 Hz, client -> host -> others
#define COOP_PKT_WORLD_EVENT   0x02 // reliable, host <-> clients (doors, items, story)
#define COOP_PKT_LEVEL_BARRIER 0x03 // reliable, host -> clients + ack (streaming load)
#define COOP_PKT_ENEMY_STATE   0x04 // reliable, host -> clients (AI state, chase target)
#define COOP_PKT_JOIN          0x05 // reliable, client -> host (room join / late join)
#define COOP_PKT_HEARTBEAT     0x06 // unreliable, 1 Hz keep-alive

// Player movement / state flags (u8 flags byte)
#define COOP_FLAG_CROUCH     0x01
#define COOP_FLAG_PRONE      0x02 // crawling
#define COOP_FLAG_SLIDE      0x04
#define COOP_FLAG_SWIM       0x08
#define COOP_FLAG_DIVE       0x10
#define COOP_FLAG_SPRINT     0x20
#define COOP_FLAG_DEAD       0x40
#define COOP_FLAG_CAMCORDER  0x80 // raised (camcorder in hand)

// Extra state byte (u8 misc)
#define COOP_MISC_NIGHTVISION 0x01
#define COOP_MISC_MIC         0x02 // directional microphone extended
#define COOP_MISC_BLEEDING    0x04
#define COOP_MISC_BANDAGE     0x08
#define COOP_MISC_INTERACT    0x10 // holding use/interact

#pragma pack(push, 1)

typedef struct CoopPacketHeader {
    unsigned short magic;   // COOP_MAGIC
    unsigned char  version; // COOP_PROTO_VERSION
    unsigned char  type;    // COOP_PKT_*
    unsigned int   seq;     // monotonic per sender
} CoopPacketHeader;

typedef struct CoopPlayerState {
    CoopPacketHeader hdr;
    unsigned char  slot;    // 0-7 remote player id (host assigns on JOIN)
    unsigned char  flags;   // COOP_FLAG_*
    unsigned char  misc;    // COOP_MISC_*
    unsigned char  stamina; // 0-100
    float          pos[3];  // world position (UE3 coords)
    float          vel[3];  // velocity, for interpolation/physics prediction
    float          yaw;     // rotation, degrees
    float          pitch;   // head / camera pitch, degrees
} CoopPlayerState;

typedef struct CoopWorldEvent {
    CoopPacketHeader hdr;
    unsigned char  kind;    // door open/close/lock, item pickup, story trigger...
    unsigned char  actorId; // host-assigned stable id for the actor
    unsigned char  arg0;
    unsigned char  arg1;
    float          extra;   // event-specific (e.g., door angle, battery level)
} CoopWorldEvent;

typedef struct CoopLevelBarrier {
    CoopPacketHeader hdr;
    unsigned char  levelId;   // streaming persistent map id
    unsigned char  phase;     // 0=host requests, 1=client ready, 2=resume
    unsigned short reserved;
} CoopLevelBarrier;

typedef struct CoopEnemyState {
    CoopPacketHeader hdr;
    unsigned char  enemyId;  // host-assigned id for OLEnemyPawn
    unsigned char  state;    // idle / patrol / chase / attack / stunned
    unsigned char  targetSlot; // 0-7 = which player is being chased
    unsigned char  reserved;
} CoopEnemyState;

#pragma pack(pop)

// ================= Packet validation helpers =================

// Expected wire size for a packet type, or 0 when the type is unknown.
static __inline unsigned int coop_packet_size(unsigned char type)
{
    switch (type) {
    case COOP_PKT_PLAYER_STATE:  return (unsigned int)sizeof(CoopPlayerState);
    case COOP_PKT_WORLD_EVENT:   return (unsigned int)sizeof(CoopWorldEvent);
    case COOP_PKT_LEVEL_BARRIER: return (unsigned int)sizeof(CoopLevelBarrier);
    case COOP_PKT_ENEMY_STATE:   return (unsigned int)sizeof(CoopEnemyState);
    case COOP_PKT_JOIN:
    case COOP_PKT_HEARTBEAT:     return (unsigned int)sizeof(CoopPacketHeader);
    default:                     return 0;
    }
}

// Returns 1 when buf holds a complete, well-formed packet: at least a header,
// matching magic and version, a known type, and an exact length for that type.
static __inline int coop_packet_validate(const void* buf, int len)
{
    const CoopPacketHeader* hdr = (const CoopPacketHeader*)buf;
    unsigned int expected;
    if (!buf || len < (int)sizeof(CoopPacketHeader))
        return 0;
    if (hdr->magic != COOP_MAGIC || hdr->version != COOP_PROTO_VERSION)
        return 0;
    expected = coop_packet_size(hdr->type);
    if (expected == 0)
        return 0;
    return (unsigned int)len == expected;
}

#endif // COOP_PROTOCOL_H
