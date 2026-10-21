// Scenario generator and JSON (de)serialization.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "coopfuzz/fuzz.h"

namespace coopfuzz {

static const char* kOpNames[] = {"move", "hit", "pickup", "drop", "spawn", "activate", "join", "leave", "save", "load", "link", "stale"};

const char* opName(OpKind k) { return kOpNames[static_cast<int>(k)]; }
bool opFromName(const std::string& s, OpKind& out) {
    for (int i = 0; i < static_cast<int>(OpKind::Count); ++i)
        if (s == kOpNames[i]) { out = static_cast<OpKind>(i); return true; }
    return false;
}

Scenario generate(uint64_t seed, int machines, uint32_t durationMs) {
    Rng r(seed * 0x9E3779B97F4A7C15ull + 77);
    Scenario s;
    s.seed = seed;
    s.machines = std::max(2, std::min(4, machines));
    s.durationMs = durationMs;
    s.link.minLatencyMs = r.range(0, 60);
    s.link.maxLatencyMs = s.link.minLatencyMs + r.range(0, 200);
    s.link.loss = r.chance(0.3) ? 0.0 : r.uni() * 0.35;
    s.link.duplicate = r.chance(0.4) ? 0.0 : r.uni() * 0.25;
    s.link.corrupt = r.chance(0.6) ? 0.0 : r.uni() * 0.03;

    auto add = [&](uint32_t t, OpKind k, int m, int a = 0, int b = 0, int c = 0, int d = 0) {
        Op o; o.t = t; o.kind = k; o.m = m; o.a = a; o.b = b; o.c = c; o.d = d;
        s.ops.push_back(o);
    };
    // Encounters first so there are actors to fight over.
    int early = static_cast<int>(r.range(2, 4));
    for (int i = 0; i < early; ++i) add(r.range(50, 1500), OpKind::Spawn, static_cast<int>(r.range(0, static_cast<uint32_t>(s.machines - 1))), static_cast<int>(r.range(0, 4)));

    // Rate: about 25 ops per virtual second spread over the weights.
    uint32_t count = std::max<uint32_t>(10, durationMs / 40);
    struct W { OpKind k; int w; };
    static const W weights[] = {{OpKind::Move, 30}, {OpKind::Hit, 13}, {OpKind::Pickup, 10}, {OpKind::Drop, 4}, {OpKind::Spawn, 6},
                                {OpKind::Activate, 6}, {OpKind::Join, 5}, {OpKind::Leave, 5}, {OpKind::Save, 1}, {OpKind::Load, 1},
                                {OpKind::SetLink, 3}, {OpKind::Stale, 8}};
    int total = 0;
    for (auto& w : weights) total += w.w;
    const uint32_t lastMachine = static_cast<uint32_t>(s.machines - 1);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t t = r.range(0, durationMs > 0 ? durationMs - 1 : 0);
        int pick = static_cast<int>(r.range(0, static_cast<uint32_t>(total - 1)));
        OpKind k = OpKind::Move;
        for (auto& w : weights) { if (pick < w.w) { k = w.k; break; } pick -= w.w; }
        int m = static_cast<int>(r.range(0, lastMachine));
        auto ri = [&](uint32_t lo, uint32_t hi) { return static_cast<int>(r.range(lo, hi)); };
        switch (k) {
            case OpKind::Move: { int x = ri(0, 1000), y = ri(0, 1000), c = r.chance(0.15) ? ri(0, 2) : 0; add(t, k, m, x, y, c); break; }
            case OpKind::Hit: { int a = ri(0, 15), b = ri(5, 60); add(t, k, m, a, b); break; }
            case OpKind::Pickup: add(t, k, m, ri(0, 15)); break;
            case OpKind::Drop: { int a = ri(0, 7), b = ri(0, 1000), c = ri(0, 1000), d = ri(0, 2); add(t, k, m, a, b, c, d); break; }
            case OpKind::Spawn: add(t, k, m, ri(0, 4)); break;
            case OpKind::Activate: add(t, k, m, ri(0, 3)); break;
            case OpKind::Join: case OpKind::Leave: add(t, k, 1 + ri(0, static_cast<uint32_t>(s.machines - 2))); break;
            case OpKind::Save: case OpKind::Load: add(t, k, 0); break;
            case OpKind::SetLink: { int a = ri(0, 80), b = ri(80, 400), c = ri(0, 450), d = ri(0, 300); add(t, k, 0, a, b, c, d); break; }
            case OpKind::Stale: { int a = ri(0, lastMachine), b = ri(0, 31), c = ri(0, 1); add(t, k, m, a, b, c); break; }
            default: break;
        }
    }
    std::stable_sort(s.ops.begin(), s.ops.end(), [](const Op& a, const Op& b) { return a.t < b.t; });
    return s;
}

// ---------------------------------------------------------------- JSON
std::string jsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += static_cast<char>(c); }
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c < 32) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += static_cast<char>(c);
    }
    return o;
}

std::string toJson(const Scenario& s) {
    std::string o;
    char buf[256];
    std::snprintf(buf, sizeof buf, "{\n \"format\": \"coopfuzz-1\",\n \"seed\": %llu,\n \"machines\": %d,\n \"durationMs\": %u,\n",
                  static_cast<unsigned long long>(s.seed), s.machines, s.durationMs);
    o += buf;
    std::snprintf(buf, sizeof buf, " \"link\": {\"minLatencyMs\": %u, \"maxLatencyMs\": %u, \"loss\": %.6f, \"duplicate\": %.6f, \"corrupt\": %.6f},\n",
                  s.link.minLatencyMs, s.link.maxLatencyMs, s.link.loss, s.link.duplicate, s.link.corrupt);
    o += buf;
    if (!s.profile.empty()) o += " \"profile\": \"" + jsonEscape(s.profile) + "\",\n";
    o += " \"target\": \"" + jsonEscape(s.target) + "\",\n";
    if (!s.expect.empty()) o += " \"expect\": \"" + jsonEscape(s.expect) + "\",\n";
    o += " \"mutations\": [";
    for (size_t i = 0; i < s.bugs.size(); ++i) { if (i) o += ", "; o += "\"" + jsonEscape(s.bugs[i]) + "\""; }
    o += "],\n \"ops\": [\n";
    for (size_t i = 0; i < s.ops.size(); ++i) {
        const Op& p = s.ops[i];
        std::snprintf(buf, sizeof buf, "  {\"t\": %u, \"op\": \"%s\", \"m\": %d, \"a\": %d, \"b\": %d, \"c\": %d, \"d\": %d}%s\n",
                      p.t, opName(p.kind), p.m, p.a, p.b, p.c, p.d, i + 1 < s.ops.size() ? "," : "");
        o += buf;
    }
    o += " ]\n}\n";
    return o;
}

namespace {
struct J {
    enum T { Null, Num, Str, Arr, Obj } t = Null;
    double n = 0;
    std::string s;
    std::vector<J> a;
    std::vector<std::pair<std::string, J>> o;
    const J* get(const char* k) const { for (auto& kv : o) if (kv.first == k) return &kv.second; return nullptr; }
};
struct Parser {
    const std::string& s;
    size_t i = 0;
    std::string err;
    explicit Parser(const std::string& text) : s(text) {}
    void ws() { while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; }
    bool fail(const char* m) { if (err.empty()) err = std::string(m) + " at " + std::to_string(i); return false; }
    bool value(J& v) {
        ws();
        if (i >= s.size()) return fail("eof");
        char c = s[i];
        if (c == '{') {
            v.t = J::Obj; ++i; ws();
            if (i < s.size() && s[i] == '}') { ++i; return true; }
            for (;;) {
                ws(); J k; if (!value(k) || k.t != J::Str) return fail("key");
                ws(); if (i >= s.size() || s[i] != ':') return fail("colon");
                ++i;
                J x; if (!value(x)) return false;
                v.o.emplace_back(k.s, std::move(x));
                ws(); if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == '}') { ++i; return true; }
                return fail("object");
            }
        }
        if (c == '[') {
            v.t = J::Arr; ++i; ws();
            if (i < s.size() && s[i] == ']') { ++i; return true; }
            for (;;) {
                J x; if (!value(x)) return false;
                v.a.push_back(std::move(x));
                ws(); if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == ']') { ++i; return true; }
                return fail("array");
            }
        }
        if (c == '"') {
            v.t = J::Str; ++i;
            while (i < s.size() && s[i] != '"') {
                if (s[i] == '\\' && i + 1 < s.size()) {
                    ++i;
                    char e = s[i++];
                    v.s += e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e;
                } else v.s += s[i++];
            }
            if (i >= s.size()) return fail("string");
            ++i; return true;
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            char* e = nullptr; v.t = J::Num; v.n = std::strtod(s.c_str() + i, &e);
            if (e == s.c_str() + i) return fail("number");
            i = static_cast<size_t>(e - s.c_str()); return true;
        }
        if (s.compare(i, 4, "true") == 0) { v.t = J::Num; v.n = 1; i += 4; return true; }
        if (s.compare(i, 5, "false") == 0) { v.t = J::Num; v.n = 0; i += 5; return true; }
        if (s.compare(i, 4, "null") == 0) { i += 4; return true; }
        return fail("value");
    }
};
}  // namespace

bool fromJson(const std::string& text, Scenario& out, std::string* err) {
    Parser p(text);
    J root;
    if (!p.value(root) || root.t != J::Obj) { if (err) *err = "bad json: " + p.err; return false; }
    const J* f = root.get("format");
    if (!f || f->s != "coopfuzz-1") { if (err) *err = "format is not coopfuzz-1"; return false; }
    Scenario s;
    auto num = [&](const J& o, const char* k, double dflt) { const J* v = o.get(k); return v && v->t == J::Num ? v->n : dflt; };
    s.seed = static_cast<uint64_t>(num(root, "seed", 1));
    s.machines = static_cast<int>(num(root, "machines", 3));
    s.durationMs = static_cast<uint32_t>(num(root, "durationMs", 20000));
    if (const J* l = root.get("link")) {
        s.link.minLatencyMs = static_cast<uint32_t>(num(*l, "minLatencyMs", 0));
        s.link.maxLatencyMs = static_cast<uint32_t>(num(*l, "maxLatencyMs", 0));
        s.link.loss = num(*l, "loss", 0); s.link.duplicate = num(*l, "duplicate", 0); s.link.corrupt = num(*l, "corrupt", 0);
    }
    if (const J* pf = root.get("profile")) s.profile = pf->s;
    if (const J* tg = root.get("target")) s.target = tg->s;
    if (const J* ex = root.get("expect")) s.expect = ex->s;
    if (const J* m = root.get("mutations")) for (auto& x : m->a) s.bugs.push_back(x.s);
    const J* ops = root.get("ops");
    if (!ops || ops->t != J::Arr) { if (err) *err = "no ops array"; return false; }
    for (auto& oj : ops->a) {
        Op op;
        const J* n = oj.get("op");
        if (!n || !opFromName(n->s, op.kind)) { if (err) *err = "unknown op"; return false; }
        op.t = static_cast<uint32_t>(num(oj, "t", 0));
        op.m = static_cast<int>(num(oj, "m", 0)); op.a = static_cast<int>(num(oj, "a", 0)); op.b = static_cast<int>(num(oj, "b", 0));
        op.c = static_cast<int>(num(oj, "c", 0)); op.d = static_cast<int>(num(oj, "d", 0));
        s.ops.push_back(op);
    }
    s.machines = std::max(2, std::min(4, s.machines));
    std::stable_sort(s.ops.begin(), s.ops.end(), [](const Op& a, const Op& b) { return a.t < b.t; });
    out = std::move(s);
    return true;
}

std::string fnvHex(const std::string& str) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : str) { h ^= c; h *= 1099511628211ull; }
    char b[20];
    std::snprintf(b, sizeof b, "%08llx", static_cast<unsigned long long>(h & 0xFFFFFFFFull));
    return b;
}

}  // namespace coopfuzz
