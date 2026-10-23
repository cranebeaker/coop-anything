// How a mod uses the recorder. Build target: cofr_example. Run: cofr_example [output folder]
// It plays a tiny fake session and writes one .cofr file at the end, the way a mod does when it detects a desync.
#include <cstdio>
#include <cstring>
#include "cofr.h"

namespace {

// Your protocol decoder. It runs when the recorder dumps, on every datagram that was not cut.
// This fake protocol is: byte 0 = 0xC0, byte 1 = message type, bytes 2..9 = seq.
bool decodeMyProtocol(const uint8_t* d, size_t n, cofr::NetMeta& m) {
    if (n < 10 || d[0] != 0xC0) return false;   // not my protocol
    m.msgType = d[1];
    std::memcpy(&m.seqLo, d + 2, 8);            // fine on little-endian machines
    m.seqHi = m.seqLo;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    // 1. Configure and create one recorder per machine.
    cofr::Config cfg;
    cfg.slot = 1;                         // this machine's number; the host is usually 0
    cfg.role = cofr::kRoleClient;
    cfg.label = "client-1";               // shows up in reports and file names
    cfg.dir = argc > 1 ? argv[1] : "flight";
    cfg.ringBytes = 16u << 20;            // the newest 16 MB of records are kept
    cfg.decoder = decodeMyProtocol;       // optional
    cfg.schemaJson = R"({"game":"mygame","digest_categories":{"0":"quests","1":"npcs"},"message_types":{"4":"batch","5":"ack"}})";
    cofr::Recorder recorder(cfg);
    recorder.mapPeer(0, 0);               // on this machine transport peer 0 is the host, machine slot 0

    // Hooks only need the tiny interface, so a release build can pass a null observer instead.
    cofr::IObserver* obs = &recorder;

    // 2. Your hooks call it. Here, a fake world of key -> value hash that changes when the host sends a batch.
    cofr::BucketedState world;            // keeps the digest up to date one entry at a time
    for (uint64_t seq = 1; seq <= 3; ++seq) {
        obs->setFrame(static_cast<uint32_t>(seq * 60));

        uint8_t batch[10] = {0xC0, 4};
        std::memcpy(batch + 2, &seq, 8);
        obs->netIn(0, batch, sizeof batch);                       // a datagram arrived from peer 0

        world.set(1000 + seq, static_cast<uint32_t>(seq * 10));   // the batch changed one entry of the world
        cofr::AppliedEvent applied;                               // and the effect was applied to the local world
        applied.seq = seq; applied.index = 0; applied.epoch = 1;
        applied.hash = cofr::hash64(&seq, sizeof seq);            // any hash of the effect that is equal on every machine
        obs->applied(applied);

        uint8_t ack[10] = {0xC0, 5};
        std::memcpy(ack + 2, &seq, 8);
        obs->netOut(0, ack, sizeof ack);                          // an ack went out
    }

    // 3. A digest at a quiet point (every few seconds is plenty).
    obs->digest(world.digest(0, 1, 3));

    // 4. Your own check found a desync: write the per-key detail, then dump. onDesync is rate limited.
    recorder.digestDetail(0, 1, 3, world.entries());
    for (const auto& e : world.entries()) recorder.entryText(e.first, e.second, "quest " + std::to_string(e.first) + " = " + std::to_string(e.second));
    std::string path = recorder.onDesync("quests digest differs from the host at seq 3");
    if (path.empty()) {
        std::printf("no file written (is %s writable?)\n", cfg.dir.c_str());
        return 1;
    }
    std::printf("wrote %s\n", path.c_str());

    // 5. Read it back, as a test would.
    cofr::Trace t;
    std::string err;
    if (!cofr::readTrace(path, t, &err)) { std::printf("cannot read it back: %s\n", err.c_str()); return 1; }
    std::printf("%zu records, slot %d\n", t.records.size(), static_cast<int>(t.header.slot));
    return t.records.size() > 10 ? 0 : 1;
}
