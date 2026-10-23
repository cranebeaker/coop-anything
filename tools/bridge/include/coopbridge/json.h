// coopbridge/json.h: a small JSON value, parser and writer. Header only, C++17, no dependencies.
//
// Enough for the bridge wire format and for adapters that build a state dump. It is not a general
// purpose library: numbers are doubles (integers up to 2^53 are exact and are written without a
// fraction), objects keep insertion order, and nesting is limited to 128 levels.
//
// The writer always produces valid UTF-8: bytes that are not valid UTF-8 (game text is often in a
// legacy code page) are written as U+FFFD, so a client can decode every reply.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace coopbridge {
namespace json {

class Value {
public:
    enum Type { Null, Bool, Number, String, Array, Object };

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : type_(Bool), bool_(b) {}
    template <class T, typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, int>::type = 0>
    Value(T v) : type_(Number), num_(static_cast<double>(v)) {}
    Value(const char* s) : type_(String), str_(s ? s : "") {}
    Value(std::string s) : type_(String), str_(std::move(s)) {}

    static Value object() { Value v; v.type_ = Object; return v; }
    static Value array() { Value v; v.type_ = Array; return v; }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Null; }
    bool isBool() const { return type_ == Bool; }
    bool isNumber() const { return type_ == Number; }
    bool isString() const { return type_ == String; }
    bool isArray() const { return type_ == Array; }
    bool isObject() const { return type_ == Object; }

    bool asBool(bool def = false) const { return type_ == Bool ? bool_ : def; }
    double asNumber(double def = 0) const { return type_ == Number ? num_ : (type_ == Bool ? (bool_ ? 1.0 : 0.0) : def); }
    int64_t asInt(int64_t def = 0) const { return type_ == Number ? static_cast<int64_t>(num_) : def; }
    std::string asString(const std::string& def = std::string()) const { return type_ == String ? str_ : def; }
    const std::string& str() const { return str_; }

    // Object access. get() never fails: a missing key (or a non-object) gives a shared null value.
    const Value* find(const std::string& key) const {
        if (type_ != Object) return nullptr;
        for (const auto& kv : members_)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }
    const Value& get(const std::string& key) const {
        static const Value kNull;
        const Value* v = find(key);
        return v ? *v : kNull;
    }
    Value& set(const std::string& key, Value v) {
        if (type_ != Object) { *this = object(); }
        for (auto& kv : members_)
            if (kv.first == key) { kv.second = std::move(v); return *this; }
        members_.emplace_back(key, std::move(v));
        return *this;
    }
    Value& push(Value v) {
        if (type_ != Array) { *this = array(); }
        items_.push_back(std::move(v));
        return *this;
    }
    const std::vector<Value>& items() const { return items_; }
    const std::vector<std::pair<std::string, Value>>& members() const { return members_; }

    std::string dump() const {
        std::string out;
        write(out);
        return out;
    }

private:
    static void writeString(std::string& out, const std::string& s) {
        static const char* hex = "0123456789abcdef";
        out.push_back('"');
        const size_t n = s.size();
        for (size_t i = 0; i < n;) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            if (c < 0x80) {
                switch (c) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    default:
                        if (c < 0x20) { out += "\\u00"; out.push_back(hex[c >> 4]); out.push_back(hex[c & 15]); }
                        else out.push_back(static_cast<char>(c));
                }
                ++i;
                continue;
            }
            const size_t len = utf8Length(s, i);
            if (len == 0) { out += "\xEF\xBF\xBD"; ++i; }
            else { out.append(s, i, len); i += len; }
        }
        out.push_back('"');
    }

    // Length of the valid UTF-8 sequence that starts at s[i] (a lead byte >= 0x80), or 0 when it is not valid.
    static size_t utf8Length(const std::string& s, size_t i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = 0;
        if (c >= 0xC2 && c <= 0xDF) len = 2;
        else if (c >= 0xE0 && c <= 0xEF) len = 3;
        else if (c >= 0xF0 && c <= 0xF4) len = 4;
        else return 0;
        if (i + len > s.size()) return 0;
        for (size_t k = 1; k < len; ++k)
            if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return 0;
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        if (c == 0xE0 && c1 < 0xA0) return 0;   // overlong
        if (c == 0xED && c1 > 0x9F) return 0;   // surrogate
        if (c == 0xF0 && c1 < 0x90) return 0;   // overlong
        if (c == 0xF4 && c1 > 0x8F) return 0;   // above U+10FFFF
        return len;
    }

    void write(std::string& out) const {
        switch (type_) {
            case Null: out += "null"; break;
            case Bool: out += bool_ ? "true" : "false"; break;
            case Number: {
                if (!std::isfinite(num_)) { out += "null"; break; }
                char buf[40];
                if (num_ == std::floor(num_) && std::fabs(num_) < 9007199254740992.0) {
                    std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(num_));
                } else {
                    std::snprintf(buf, sizeof buf, "%.15g", num_);
                    if (std::strtod(buf, nullptr) != num_) std::snprintf(buf, sizeof buf, "%.17g", num_);
                }
                out += buf;
                break;
            }
            case String: writeString(out, str_); break;
            case Array: {
                out.push_back('[');
                bool first = true;
                for (const auto& v : items_) {
                    if (!first) out.push_back(',');
                    first = false;
                    v.write(out);
                }
                out.push_back(']');
                break;
            }
            case Object: {
                out.push_back('{');
                bool first = true;
                for (const auto& kv : members_) {
                    if (!first) out.push_back(',');
                    first = false;
                    writeString(out, kv.first);
                    out.push_back(':');
                    kv.second.write(out);
                }
                out.push_back('}');
                break;
            }
        }
    }

    Type type_ = Null;
    bool bool_ = false;
    double num_ = 0;
    std::string str_;
    std::vector<Value> items_;
    std::vector<std::pair<std::string, Value>> members_;
};

namespace detail {

class Parser {
public:
    Parser(const std::string& s, std::string& err) : s_(s), err_(err) {}

    bool run(Value& out) {
        skipWs();
        if (!value(out, 0)) return false;
        skipWs();
        if (pos_ != s_.size()) return fail("unexpected text after the value");
        return true;
    }

private:
    bool fail(const std::string& why) {
        if (err_.empty()) err_ = why + " at offset " + std::to_string(pos_);
        return false;
    }
    void skipWs() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
    }
    bool literal(const char* word) {
        size_t n = 0;
        while (word[n]) ++n;
        if (s_.compare(pos_, n, word) != 0) return false;
        pos_ += n;
        return true;
    }
    static int hexVal(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
    bool hex4(unsigned& out) {
        if (pos_ + 4 > s_.size()) return fail("short \\u escape");
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            const int h = hexVal(s_[pos_ + i]);
            if (h < 0) return fail("bad \\u escape");
            v = (v << 4) | static_cast<unsigned>(h);
        }
        pos_ += 4;
        out = v;
        return true;
    }
    static void putUtf8(std::string& o, unsigned cp) {
        if (cp < 0x80) o.push_back(static_cast<char>(cp));
        else if (cp < 0x800) { o.push_back(static_cast<char>(0xC0 | (cp >> 6))); o.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else if (cp < 0x10000) {
            o.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            o.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            o.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            o.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            o.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            o.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            o.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    bool string(std::string& out) {
        ++pos_;  // opening quote
        while (pos_ < s_.size()) {
            const char c = s_[pos_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return fail("control character in string");
            if (c != '\\') { out.push_back(c); continue; }
            if (pos_ >= s_.size()) break;
            const char e = s_[pos_++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        unsigned lo = 0;
                        if (pos_ + 1 < s_.size() && s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            if (!hex4(lo)) return false;
                            if (lo < 0xDC00 || lo > 0xDFFF) return fail("bad surrogate pair");
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else return fail("lone surrogate");
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) return fail("lone surrogate");
                    putUtf8(out, cp);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }
    bool value(Value& out, int depth) {
        if (depth > 128) return fail("nesting too deep");
        if (pos_ >= s_.size()) return fail("unexpected end");
        const char c = s_[pos_];
        if (c == '{') {
            ++pos_;
            out = Value::object();
            skipWs();
            if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return true; }
            for (;;) {
                skipWs();
                if (pos_ >= s_.size() || s_[pos_] != '"') return fail("expected a string key");
                std::string key;
                if (!string(key)) return false;
                skipWs();
                if (pos_ >= s_.size() || s_[pos_] != ':') return fail("expected ':'");
                ++pos_;
                skipWs();
                Value v;
                if (!value(v, depth + 1)) return false;
                out.set(key, std::move(v));
                skipWs();
                if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
                if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            ++pos_;
            out = Value::array();
            skipWs();
            if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return true; }
            for (;;) {
                skipWs();
                Value v;
                if (!value(v, depth + 1)) return false;
                out.push(std::move(v));
                skipWs();
                if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
                if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') {
            std::string str;
            if (!string(str)) return false;
            out = Value(std::move(str));
            return true;
        }
        if (c == 't') { if (!literal("true")) return fail("bad literal"); out = Value(true); return true; }
        if (c == 'f') { if (!literal("false")) return fail("bad literal"); out = Value(false); return true; }
        if (c == 'n') { if (!literal("null")) return fail("bad literal"); out = Value(); return true; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            size_t end = pos_ + 1;
            while (end < s_.size() && ((s_[end] >= '0' && s_[end] <= '9') || s_[end] == '+' || s_[end] == '-' ||
                                       s_[end] == '.' || s_[end] == 'e' || s_[end] == 'E'))
                ++end;
            const std::string text = s_.substr(pos_, end - pos_);
            const size_t digit = (text[0] == '-') ? 1 : 0;
            if (digit >= text.size() || text[digit] < '0' || text[digit] > '9') return fail("bad number");
            char* e = nullptr;
            const double d = std::strtod(text.c_str(), &e);
            if (e != text.c_str() + text.size()) return fail("bad number");
            pos_ = end;
            out = Value(d);
            return true;
        }
        return fail("unexpected character");
    }

    const std::string& s_;
    std::string& err_;
    size_t pos_ = 0;
};

}  // namespace detail

// Parses `text` into `out`. Returns false and sets `err` when the text is not one JSON value.
inline bool parse(const std::string& text, Value& out, std::string& err) {
    err.clear();
    detail::Parser p(text, err);
    return p.run(out);
}

}  // namespace json
}  // namespace coopbridge
