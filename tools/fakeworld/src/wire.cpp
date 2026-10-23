#include "fakeworld/wire.h"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace coop::fakeworld {

uint32_t crc32(const uint8_t* data, size_t size) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

Archive Archive::writer() { return Archive(); }

Archive Archive::reader(const uint8_t* data, size_t size) {
    Archive a;
    a.reading_ = true;
    a.in_ = data;
    a.size_ = size;
    return a;
}

std::vector<uint8_t> Archive::finish() const {
    if (!ok_) return {};
    return out_;
}

void Archive::u8(uint8_t& v) {
    if (!ok_) { if (reading_) v = 0; return; }
    if (reading_) {
        if (pos_ + 1 > size_) { ok_ = false; v = 0; return; }
        v = in_[pos_++];
    } else {
        out_.push_back(v);
    }
}

namespace {
template <class T>
void fixedLE(T& v, bool reading, bool& ok, const uint8_t* in, size_t size, size_t& pos, std::vector<uint8_t>& out) {
    if (!ok) { if (reading) v = 0; return; }
    if (reading) {
        if (size - pos < sizeof(T)) { ok = false; v = 0; return; }
        T r = 0;
        for (size_t i = 0; i < sizeof(T); ++i) r = static_cast<T>(r | static_cast<T>(static_cast<T>(in[pos + i]) << (8 * i)));
        pos += sizeof(T);
        v = r;
    } else {
        for (size_t i = 0; i < sizeof(T); ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
}
}  // namespace

void Archive::u16(uint16_t& v) { fixedLE(v, reading_, ok_, in_, size_, pos_, out_); }
void Archive::u32(uint32_t& v) { fixedLE(v, reading_, ok_, in_, size_, pos_, out_); }
void Archive::u64(uint64_t& v) { fixedLE(v, reading_, ok_, in_, size_, pos_, out_); }

void Archive::varu(uint64_t& v) {
    if (!ok_) { if (reading_) v = 0; return; }
    if (reading_) {
        uint64_t r = 0;
        for (int i = 0; i < 10; ++i) {
            if (pos_ >= size_) { ok_ = false; v = 0; return; }
            uint8_t b = in_[pos_++];
            if (i == 9 && b > 1) { ok_ = false; v = 0; return; }  // overflow past 64 bits
            r |= static_cast<uint64_t>(b & 0x7F) << (7 * i);
            if (!(b & 0x80)) {
                if (b == 0 && i > 0) { ok_ = false; v = 0; return; }  // non-canonical (overlong) encoding
                v = r;
                return;
            }
        }
        ok_ = false; v = 0;
    } else {
        uint64_t x = v;
        while (x >= 0x80) { out_.push_back(static_cast<uint8_t>(x | 0x80)); x >>= 7; }
        out_.push_back(static_cast<uint8_t>(x));
    }
}

void Archive::vari(int64_t& v) {
    if (reading_) {
        uint64_t z = 0;
        varu(z);
        v = static_cast<int64_t>(z >> 1) ^ -static_cast<int64_t>(z & 1);
    } else {
        uint64_t z = (static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63);
        varu(z);
    }
}

void Archive::varu32(uint32_t& v) {
    uint64_t t = v;
    varu(t);
    if (reading_) {
        if (t > 0xFFFFFFFFull) { ok_ = false; v = 0; } else v = static_cast<uint32_t>(t);
    }
}

void Archive::vari32(int32_t& v) {
    int64_t t = v;
    vari(t);
    if (reading_) {
        if (t < INT32_MIN || t > INT32_MAX) { ok_ = false; v = 0; } else v = static_cast<int32_t>(t);
    }
}

void Archive::f32(float& v) {
    if (!ok_) { if (reading_) v = 0; return; }
    uint32_t bits = 0;
    if (!reading_) std::memcpy(&bits, &v, 4);
    u32(bits);
    if (reading_) {
        std::memcpy(&v, &bits, 4);
        if (!std::isfinite(v)) { ok_ = false; v = 0; }
    } else if (!std::isfinite(v)) {
        ok_ = false;
    }
}

void Archive::boolean(bool& v) {
    uint8_t b = v ? 1 : 0;
    u8(b);
    if (reading_) {
        if (b > 1) { ok_ = false; v = false; } else v = b == 1;
    }
}

bool Archive::count(size_t& n, size_t maxN) {
    uint64_t t = n;
    varu(t);
    if (!ok_) { n = 0; return false; }
    if (t > maxN || (reading_ && t > remaining())) { ok_ = false; n = 0; return false; }
    n = static_cast<size_t>(t);
    return true;
}

void Archive::str(std::string& s, size_t maxLen) {
    size_t n = s.size();
    if (!count(n, maxLen)) { s.clear(); return; }
    if (reading_) {
        s.assign(reinterpret_cast<const char*>(in_ + pos_), n);
        pos_ += n;
    } else {
        out_.insert(out_.end(), s.begin(), s.end());
    }
}

void Archive::bytes(std::vector<uint8_t>& b, size_t maxLen) {
    size_t n = b.size();
    if (!count(n, maxLen)) { b.clear(); return; }
    if (reading_) {
        b.assign(in_ + pos_, in_ + pos_ + n);
        pos_ += n;
    } else {
        out_.insert(out_.end(), b.begin(), b.end());
    }
}

}  // namespace coop::fakeworld
