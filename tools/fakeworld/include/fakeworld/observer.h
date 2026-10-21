// Hook for a flight recorder and for test oracles. Every callback has a no-op default body.
#pragma once
#include <cstdint>
#include "fakeworld/effect.h"
#include "fakeworld/messages.h"
#include "fakeworld/types.h"
#include "coop/ownership.h"

namespace coop::fakeworld {

struct IEngineObserver {
    virtual ~IEngineObserver() = default;
    // Application-level message (an encoded Envelope) leaving / arriving at machine `self`. channel: 0 reliable, 1 unreliable.
    virtual void onSend(PlayerId /*self*/, uint64_t /*nowMs*/, PeerId /*to*/, uint8_t /*channel*/, const Bytes& /*envelope*/) {}
    virtual void onRecv(PlayerId /*self*/, uint64_t /*nowMs*/, PeerId /*from*/, uint8_t /*channel*/, const Bytes& /*envelope*/) {}
    // A message was dropped before it reached the application (bad crc, stale session, undecodable).
    virtual void onDrop(PlayerId /*self*/, uint64_t /*nowMs*/, PeerId /*from*/, const char* /*reason*/) {}
    // An ownership message was applied to this machine's view (host: also when it emits it).
    virtual void onOwnership(PlayerId /*self*/, uint64_t /*nowMs*/, const OwnMsg& /*m*/, ApplyResult /*r*/) {}
    // An actor update was judged by the epoch/owner check.
    virtual void onActorUpdate(PlayerId /*self*/, uint64_t /*nowMs*/, NetId /*id*/, PlayerId /*from*/, OwnerEpoch /*epoch*/, UpdateVerdict /*v*/) {}
    // One story effect was applied to the local world (host: at commit; clients: on receipt).
    virtual void onEffectApplied(PlayerId /*self*/, uint64_t /*nowMs*/, uint64_t /*txnSeq*/, const Effect& /*e*/) {}
    // Periodic state digest (monotonic time, per category).
    virtual void onDigest(PlayerId /*self*/, uint64_t /*nowMs*/, Category /*cat*/, uint64_t /*hash*/) {}
    // A local player/host action entered the machine (the replayable input stream).
    virtual void onAction(PlayerId /*self*/, uint64_t /*nowMs*/, const Action& /*i*/) {}
    // A digest mismatch was repaired (host side): which keys differed.
    virtual void onRepair(PlayerId /*self*/, uint64_t /*nowMs*/, PlayerId /*target*/, Category /*cat*/, size_t /*keys*/) {}
};

}  // namespace coop::fakeworld
