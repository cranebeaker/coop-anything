// Wire primitives for the fake world: compact, little endian, bounds checked.
//
// One symmetric Archive class serves both directions: every io(Archive&, T&) function reads when the archive is a
// reader and writes when it is a writer, so encode and decode can never drift apart. Integers are LEB128 varints
// (signed ones zigzag), floats are 4 bytes and must be finite, strings and arrays are length prefixed and checked
// against both a hard cap and the bytes remaining. After any failure the archive is poisoned: ok() is false, reads
// return zero values and writes do nothing. Replace this with your game's real serializer if you have one.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "coop/types.h"

namespace coop::fakeworld {

using Bytes = std::vector<uint8_t>;

constexpr size_t kMaxStringLen = 255;
constexpr size_t kMaxEffectsPerBatch = 1024;

uint32_t crc32(const uint8_t* data, size_t size);

class Archive {
public:
    static Archive writer();
    static Archive reader(const uint8_t* data, size_t size);

    bool reading() const { return reading_; }
    bool ok() const { return ok_; }
    void fail() { ok_ = false; }
    size_t remaining() const { return reading_ ? size_ - pos_ : 0; }
    bool atEnd() const { return reading_ && pos_ == size_; }

    void u8(uint8_t& v);
    void u16(uint16_t& v);
    void u32(uint32_t& v);
    void u64(uint64_t& v);
    void varu(uint64_t& v);
    void vari(int64_t& v);
    void varu32(uint32_t& v);
    void vari32(int32_t& v);
    void f32(float& v);
    void boolean(bool& v);
    void str(std::string& s, size_t maxLen = kMaxStringLen);
    void bytes(std::vector<uint8_t>& b, size_t maxLen);
    // Element count of a container. Reading checks n <= maxN and n <= remaining() (every element is >= 1 byte).
    bool count(size_t& n, size_t maxN);

    // Writer only: the bytes written so far (empty after a failure).
    std::vector<uint8_t> finish() const;

private:
    bool reading_ = false;
    bool ok_ = true;
    std::vector<uint8_t> out_;
    const uint8_t* in_ = nullptr;
    size_t size_ = 0, pos_ = 0;
};

}  // namespace coop::fakeworld
