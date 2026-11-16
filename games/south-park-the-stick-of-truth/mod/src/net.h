// Minimal transport for co-op: one UDP socket, snapshots at a fixed rate, no dependencies.
//
// First release per SHAPE.md is a direct address inside a VPN: the host listens, the guest
// dials, both send their own state and keep the latest state they heard from the peer. There is
// no reliability layer yet because a snapshot that arrives late is worthless anyway; ordering
// is handled by a sequence number and a "newer wins" rule. Anything that must not be lost
// (spawn, join, level change) will need its own reliable kind and is not in this file.
#pragma once

#include <cstdint>
#include <string>

namespace coop {

struct PlayerState {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float yaw = 0.0f;
    uint32_t animation = 0;
    uint32_t body_id = 0;
};

class Net
{
public:
    Net();
    ~Net();

    // One socket for both roles: the host binds `port`; the guest binds any port and sends to
    // `peer_host : peer_port`. Calling Listen twice is harmless.
    bool Listen(uint16_t port);
    bool Connect(const std::string& peer_host, uint16_t peer_port);
    void Stop();

    // Sends this machine's state to the peer, at most `kSendHz` times a second.
    void Publish(const PlayerState& state, uint32_t now_ms);

    // Reads everything that arrived; keeps the newest peer state by sequence number.
    // Returns true when fresh peer state was received.
    bool Poll(uint32_t now_ms);

    bool HasPeer() const { return has_peer_; }
    PlayerState Peer() const { return peer_; }
    uint32_t PeerAgeMs(uint32_t now_ms) const;

    const std::string& LastError() const { return last_error_; }
    uint32_t SentCount() const { return sent_; }
    uint32_t ReceivedCount() const { return received_; }

    static constexpr int kSendHz = 20;

private:
    void SetError(const char* where);

    uintptr_t socket_ = ~static_cast<uintptr_t>(0);   // INVALID_SOCKET without <winsock2.h> here
    bool listening_ = false;
    bool connected_ = false;
    bool has_peer_ = false;
    uint32_t sequence_ = 0;
    uint32_t sequence_peer_ = 0;
    uint32_t last_send_ms_ = 0;
    uint32_t peer_received_ms_ = 0;
    uint32_t sent_ = 0;
    uint32_t received_ = 0;
    PlayerState peer_;
    std::string peer_host_;
    uint16_t peer_port_ = 0;
    std::string last_error_;
};

}  // namespace coop

