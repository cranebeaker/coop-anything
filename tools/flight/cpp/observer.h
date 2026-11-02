// Observer interface of the co-op flight recorder.
//
// This is the only header your game hooks need. It has no dependencies besides the standard library.
// Your hooks report what happened to an IObserver*; the recorder (cofr.h) is one implementation of it,
// and a null pointer or the do-nothing base class is another, so a release build can drop the recorder.
//
// Format: tools/flight/FORMAT.md (co-op flight record, .cofr, version 1).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace cofr {

// ---- constants shared with the file format ----

enum RecType : uint8_t {
    kMeta = 1, kNetOut = 2, kNetIn = 3, kTxnCaptured = 4, kTxnApplied = 5, kOwnership = 6, kDigest = 7, kMarker = 8,
    kSchema = 9,
    kFirstCustom = 32,   // 32..255: game-defined record types, described by the schema
};
enum RecFlag : uint8_t {
    kFlagTruncated = 1,   // Net: the datagram was cut at maxRawBytes
    kFlagDerived = 2,     // Digest: built from a received message, not from the local state
    kFlagDetail = 4,      // Digest: carries only per-key (key, value hash) entries
    kFlagBucketed = 8,    // Digest: the shards are key buckets (bucket = key mod number of shards)
};
enum DumpReason : uint8_t { kDumpManual = 0, kDumpDesync = 1, kDumpCrash = 2, kDumpShutdown = 3, kDumpTest = 4 };
enum Role : uint8_t { kRoleHost = 0, kRoleClient = 1, kRoleSim = 2 };
enum OwnershipOp : uint8_t { kOwnGrant = 0, kOwnTransfer = 1, kOwnRelease = 2, kOwnRejectStale = 3, kOwnRejectEcho = 4, kOwnRejectNonOwner = 5 };
enum MarkerKind : uint16_t { kMarkNote = 0, kMarkDesync = 1, kMarkJoin = 2, kMarkLeave = 3, kMarkTestStep = 4, kMarkSave = 5, kMarkLoad = 6, kMarkRebase = 7 };
enum ApplyFlag : uint8_t { kApplySnapshot = 1, kApplyRepair = 2, kApplyBaseline = 4, kApplyCatchUp = 8 };
constexpr uint8_t kNoSlot = 0xFF;

// ---- events ----

// Digest: a hash of a part of the shared state ("category" is a number your schema gives a name to).
struct DigestEvent {
    uint8_t category = 0;
    uint32_t epoch = 0;
    uint64_t seq = 0;
    uint64_t hash = 0;
    uint32_t itemCount = 0;
    bool bucketed = false;                               // shards are key buckets (see bucketedDigest)
    std::vector<uint32_t> shards;                        // optional
    std::vector<std::pair<uint64_t, uint32_t>> keys;     // optional per-key detail (at most kMaxDigestKeys are kept per record)
};

// What a game-side decoder can tell the recorder about one datagram (filled at dump time, never on the hot path).
struct NetMeta {
    uint8_t msgType = 0;
    uint32_t session = 0, epoch = 0;
    uint64_t seqLo = 0, seqHi = 0;       // range of transaction numbers the datagram carries
    bool hasDigest = false;              // the datagram carries a digest of the sender's state: the recorder adds a "derived" Digest record
    DigestEvent digest;
};

// A change set the local machine decided on (kind 0, a host commit) or asked for (kind 1, a client intent).
struct CapturedEvent {
    uint8_t kind = 0;
    uint32_t intentId = 0;
    uint64_t seq = 0;            // 0 for an intent that has no number yet
    uint32_t effectCount = 0;
    uint64_t hash = 0;           // hash over the effect hashes, computed by the game
    uint8_t cause = 0;
    uint32_t epoch = 0;
};

// One effect applied to the local world. (seq, index) names the effect; its hash must be equal on every machine.
struct AppliedEvent {
    uint64_t seq = 0;
    uint32_t epoch = 0;
    uint8_t origin = 0;
    uint32_t index = 0;
    uint8_t flags = 0;           // ApplyFlag bits; snapshot, repair and baseline applies restate state instead of replaying a transaction
    uint8_t cause = 0;
    uint8_t effectTag = 0;       // which kind of effect (game-defined)
    uint64_t hash = 0;
};

struct OwnershipEvent {
    uint8_t op = kOwnGrant;
    uint64_t actor = 0;
    uint32_t epoch = 0;
    uint8_t fromOwner = kNoSlot, toOwner = kNoSlot;
    uint8_t reason = 0;
    uint64_t txnId = 0;
};

// ---- the observer ----

// Every method does nothing by default, so `IObserver` itself is the null observer.
class IObserver {
public:
    virtual ~IObserver() = default;
    virtual void setFrame(uint32_t /*frame*/) {}
    virtual void netOut(uint32_t /*to*/, const uint8_t* /*data*/, size_t /*size*/, uint8_t /*channel*/ = 0) {}
    virtual void netIn(uint32_t /*from*/, const uint8_t* /*data*/, size_t /*size*/) {}
    virtual void captured(const CapturedEvent&) {}
    virtual void applied(const AppliedEvent&) {}
    virtual void ownership(const OwnershipEvent&) {}
    virtual void digest(const DigestEvent&, uint8_t /*flags*/ = 0) {}
    virtual void marker(uint16_t /*kind*/, const std::string& /*text*/) {}
    // A game-defined record. `type` is 32..255 and must be described in the schema for the analyzer to decode it.
    virtual void custom(uint8_t /*type*/, const uint8_t* /*payload*/, size_t /*size*/) {}
};

}  // namespace cofr
