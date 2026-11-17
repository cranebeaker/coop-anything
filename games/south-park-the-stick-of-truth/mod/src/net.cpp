#include "net.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdio>
#include <cstring>

namespace coop {
namespace {

constexpr uint32_t kMagic = 0x50534331;   // "PSC1"
constexpr uint32_t kHelloMagic = 0x50534330;  // "PSC0": {magic, version, build_id, seq}
constexpr int kHelloBytes = 16;
constexpr int kPacketBytes = 8 + sizeof(float) * 4 + sizeof(uint32_t) * 2;

struct Packet {
    uint32_t magic;
    uint32_t sequence;
    float x;
    float y;
    float z;
    float yaw;
    uint32_t animation;
    uint32_t body_id;
};

bool sockets_ready = false;

bool ensure_sockets()
{
    if (sockets_ready) {
        return true;
    }
    WSADATA data = {};
    sockets_ready = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    return sockets_ready;
}

}  // namespace

Net::Net() = default;

Net::~Net()
{
    Stop();
}

void Net::SetError(const char* where)
{
    char line[160];
    std::snprintf(line, sizeof(line), "%s failed, WSA error %d", where, WSAGetLastError());
    last_error_ = line;
}

bool Net::Listen(uint16_t port)
{
    if (!ensure_sockets()) {
        last_error_ = "WSAStartup failed";
        return false;
    }
    if (socket_ != ~static_cast<uintptr_t>(0)) {
        return true;
    }
    SOCKET handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (handle == INVALID_SOCKET) {
        SetError("socket");
        return false;
    }
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (::bind(handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        SetError("bind");
        ::closesocket(handle);
        return false;
    }
    u_long nonblocking = 1;
    ::ioctlsocket(handle, FIONBIO, &nonblocking);
    socket_ = static_cast<uintptr_t>(handle);
    listening_ = true;
    return true;
}

bool Net::Connect(const std::string& peer_host, uint16_t peer_port)
{
    if (!Listen(0)) {
        return false;
    }
    peer_host_ = peer_host;
    peer_port_ = peer_port;
    connected_ = true;
    return true;
}

void Net::Stop()
{
    if (socket_ != ~static_cast<uintptr_t>(0)) {
        ::closesocket(static_cast<SOCKET>(socket_));
        socket_ = ~static_cast<uintptr_t>(0);
    }
    listening_ = connected_ = has_peer_ = false;
}

void Net::Publish(const PlayerState& state, uint32_t now_ms)
{
    if (!connected_ || socket_ == ~static_cast<uintptr_t>(0) || peer_port_ == 0) {
        return;
    }
    const uint32_t interval = 1000 / kSendHz;
    if (last_send_ms_ != 0 && now_ms - last_send_ms_ < interval) {
        return;
    }
    last_send_ms_ = now_ms;
    sockaddr_in peer = {};
    peer.sin_family = AF_INET;
    peer.sin_port = htons(peer_port_);
    ::inet_pton(AF_INET, peer_host_.c_str(), &peer.sin_addr);
    // 'PSC0' hello: before the first snapshot and every 20th send, so a peer that starts
    // late still hears the identity quickly.
    if (!hello_sent_ || (sequence_ % 20) == 0) {
        unsigned char hello[kHelloBytes] = {};
        uint32_t value = kHelloMagic;
        std::memcpy(hello, &value, 4);
        value = version_;
        std::memcpy(hello + 4, &value, 4);
        value = build_id_;
        std::memcpy(hello + 8, &value, 4);
        value = sequence_;
        std::memcpy(hello + 12, &value, 4);
        if (::sendto(static_cast<SOCKET>(socket_), reinterpret_cast<const char*>(hello),
                     kHelloBytes, 0, reinterpret_cast<sockaddr*>(&peer), sizeof(peer)) > 0) {
            hello_sent_ = true;
        }
    }
    Packet packet = {};
    packet.magic = kMagic;
    packet.sequence = ++sequence_;
    packet.x = state.x;
    packet.y = state.y;
    packet.z = state.z;
    packet.yaw = state.yaw;
    packet.animation = state.animation;
    packet.body_id = state.body_id;
    if (::sendto(static_cast<SOCKET>(socket_), reinterpret_cast<const char*>(&packet),
                 kPacketBytes, 0, reinterpret_cast<sockaddr*>(&peer), sizeof(peer)) > 0) {
        ++sent_;
    }
}

bool Net::Poll(uint32_t now_ms)
{
    if (socket_ == ~static_cast<uintptr_t>(0)) {
        return false;
    }
    bool fresh = false;
    for (;;) {
        unsigned char buf[64] = {};
        sockaddr_in from = {};
        int from_length = sizeof(from);
        const int got = ::recvfrom(static_cast<SOCKET>(socket_), reinterpret_cast<char*>(&buf[0]),
                                   sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from),
                                   &from_length);
        if (got <= 0) {
            break;
        }
        uint32_t magic = 0;
        if (got < 4) {
            continue;
        }
        std::memcpy(&magic, buf, 4);
        if (got < kHelloBytes || (magic != kMagic && magic != kHelloMagic)) {
            continue;
        }
        // The guest learns where the host is from the first valid packet it hears.
        if (!connected_ && from_length >= static_cast<int>(sizeof(sockaddr_in))) {
            char host[64] = {};
            ::inet_ntop(AF_INET, &from.sin_addr, host, sizeof(host));
            peer_host_ = host;
            peer_port_ = ntohs(from.sin_port);
            connected_ = true;
        }
        if (magic == kHelloMagic) {
            if (got >= kHelloBytes) {
                std::memcpy(&peer_version_, buf + 4, 4);
                std::memcpy(&peer_build_, buf + 8, 4);
                peer_hello_ = true;
            }
            continue;
        }
        if (got < kPacketBytes) {
            continue;
        }
        Packet packet = {};
        std::memcpy(&packet, buf, sizeof(packet) <= static_cast<size_t>(got)
                                   ? sizeof(packet) : static_cast<size_t>(got));
        ++received_;
        if (!has_peer_ || packet.sequence > sequence_peer_) {
            sequence_peer_ = packet.sequence;
            peer_.x = packet.x;
            peer_.y = packet.y;
            peer_.z = packet.z;
            peer_.yaw = packet.yaw;
            peer_.animation = packet.animation;
            peer_.body_id = packet.body_id;
            peer_received_ms_ = now_ms;
            has_peer_ = true;
            fresh = true;
        }
    }
    return fresh;
}

uint32_t Net::PeerAgeMs(uint32_t now_ms) const
{
    if (!has_peer_) {
        return 0xFFFFFFFFu;
    }
    return now_ms - peer_received_ms_;
}

}  // namespace coop

