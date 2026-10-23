#include "hotreload/control_pipe.h"

#include <chrono>
#include <cstdio>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace hotreload {

// ---- JSON helpers ------------------------------------------------------------------------------------------------
namespace {
const size_t npos = std::string::npos;

size_t SkipWs(const std::string& s, size_t i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
    return i;
}
// s[i] == '"'. Returns the index after the closing quote, or npos.
size_t SkipString(const std::string& s, size_t i) {
    ++i;
    while (i < s.size()) {
        if (s[i] == '\\') i += 2;
        else if (s[i] == '"') return i + 1;
        else ++i;
    }
    return npos;
}
// Returns the index after the value that starts at i, or npos.
size_t SkipValue(const std::string& s, size_t i) {
    i = SkipWs(s, i);
    if (i >= s.size()) return npos;
    const char c = s[i];
    if (c == '"') return SkipString(s, i);
    if (c == '{' || c == '[') {
        int depth = 0;
        while (i < s.size()) {
            const char d = s[i];
            if (d == '"') {
                i = SkipString(s, i);
                if (i == npos) return npos;
                continue;
            }
            if (d == '{' || d == '[') ++depth;
            else if (d == '}' || d == ']') {
                if (--depth == 0) return i + 1;
            }
            ++i;
        }
        return npos;
    }
    const size_t start = i;
    while (i < s.size() && std::strchr(",}] \t\r\n", s[i]) == nullptr) ++i;
    return i == start ? npos : i;
}
void AppendUtf8(std::string& o, unsigned cp) {
    if (cp < 0x80) o += static_cast<char>(cp);
    else if (cp < 0x800) { o += static_cast<char>(0xC0 | (cp >> 6)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
    else { o += static_cast<char>(0xE0 | (cp >> 12)); o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
}
// raw = a JSON string literal including its quotes.
bool Unescape(const std::string& raw, std::string& out) {
    out.clear();
    if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"') return false;
    for (size_t i = 1; i + 1 < raw.size(); ++i) {
        char c = raw[i];
        if (c != '\\') { out += c; continue; }
        if (++i + 1 > raw.size() - 1) return false;
        switch (raw[i]) {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
                if (i + 4 >= raw.size() - 1) return false;
                unsigned cp = static_cast<unsigned>(std::strtoul(raw.substr(i + 1, 4).c_str(), nullptr, 16));
                AppendUtf8(out, cp);
                i += 4;
                break;
            }
            default: out += raw[i]; break;  // \" \\ \/
        }
    }
    return true;
}
}  // namespace

std::string JsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += static_cast<char>(c);
        }
    }
    return o;
}

bool JsonFindValue(const std::string& obj, const std::string& key, std::string& raw) {
    size_t i = SkipWs(obj, 0);
    if (i >= obj.size() || obj[i] != '{') return false;
    ++i;
    for (;;) {
        i = SkipWs(obj, i);
        if (i >= obj.size() || obj[i] == '}') return false;
        if (obj[i] != '"') return false;
        const size_t kend = SkipString(obj, i);
        if (kend == npos) return false;
        std::string k;
        Unescape(obj.substr(i, kend - i), k);
        i = SkipWs(obj, kend);
        if (i >= obj.size() || obj[i] != ':') return false;
        i = SkipWs(obj, i + 1);
        const size_t vend = SkipValue(obj, i);
        if (vend == npos) return false;
        if (k == key) {
            raw = obj.substr(i, vend - i);
            return true;
        }
        i = SkipWs(obj, vend);
        if (i < obj.size() && obj[i] == ',') ++i;
        else return false;
    }
}

bool JsonGetString(const std::string& obj, const std::string& key, std::string& out) {
    std::string raw;
    return JsonFindValue(obj, key, raw) && Unescape(raw, out);
}

bool JsonGetInt(const std::string& obj, const std::string& key, long long& out) {
    std::string raw;
    if (!JsonFindValue(obj, key, raw) || raw.empty()) return false;
    char* end = nullptr;
    const long long v = std::strtoll(raw.c_str(), &end, 10);
    if (end == raw.c_str()) return false;
    out = v;
    return true;
}

// ---- server ------------------------------------------------------------------------------------------------------
ControlServer::ControlServer(std::string pipeName, Handler handler) : handler_(std::move(handler)) {
    path_ = pipeName.rfind("\\\\", 0) == 0 ? pipeName : "\\\\.\\pipe\\" + pipeName;
}

ControlServer::~ControlServer() { Stop(); }

bool ControlServer::Start(std::string& err) {
    if (thread_.joinable()) return true;
    HANDLE pipe = CreateNamedPipeA(path_.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 1 << 16, 1 << 16, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        err = "cannot create control pipe " + path_ + " (error " + std::to_string(GetLastError()) + "); is the name already in use?";
        return false;
    }
    pipe_ = pipe;
    stopEv_ = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    stopping_ = false;
    thread_ = std::thread([this] { Run(); });
    return true;
}

void ControlServer::Stop() {
    if (!thread_.joinable()) return;
    {
        std::lock_guard<std::mutex> g(mu_);
        stopping_ = true;
    }
    SetEvent(static_cast<HANDLE>(stopEv_));
    cv_.notify_all();
    thread_.join();
    CloseHandle(static_cast<HANDLE>(pipe_));
    CloseHandle(static_cast<HANDLE>(stopEv_));
    pipe_ = stopEv_ = nullptr;
}

int ControlServer::Pump() {
    int n = 0;
    for (;;) {
        std::shared_ptr<Req> r;
        {
            std::lock_guard<std::mutex> g(mu_);
            if (queue_.empty()) break;
            r = queue_.front();
            queue_.pop_front();
            if (r->abandoned) continue;
            r->running = true;
        }
        std::string out;
        int code = -32000;
        bool ok = false;
        try {
            ok = handler_(r->method, r->params, out, code);
        } catch (...) {
            ok = false;
            out = "exception in method";
            code = -32000;
        }
        {
            std::lock_guard<std::mutex> g(mu_);
            r->ok = ok;
            r->out = out;
            r->code = code;
            r->done = true;
        }
        cv_.notify_all();
        ++n;
    }
    return n;
}

std::string ControlServer::Handle(const std::string& line) {
    std::string id = "null", method, params = "{}", raw;
    if (JsonFindValue(line, "id", raw)) id = raw;
    if (JsonFindValue(line, "params", raw) && !raw.empty() && raw[0] == '{') params = raw;
    auto error = [&](int code, const std::string& msg) {
        return "{\"id\":" + id + ",\"error\":{\"code\":" + std::to_string(code) + ",\"message\":\"" + JsonEscape(msg) + "\"}}";
    };
    if (!JsonGetString(line, "method", method) || method.empty()) return error(-32600, "request has no method");
    if (method == "ping") return "{\"id\":" + id + ",\"result\":{\"pid\":" + std::to_string(GetCurrentProcessId()) + "}}";

    long long timeoutMs = 30000;
    JsonGetInt(params, "timeoutMs", timeoutMs);
    if (timeoutMs < 1000) timeoutMs = 1000;
    if (timeoutMs > 600000) timeoutMs = 600000;

    auto r = std::make_shared<Req>();
    r->method = method;
    r->params = params;
    std::unique_lock<std::mutex> lk(mu_);
    if (stopping_) return error(-32000, "control server is stopping");
    queue_.push_back(r);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    cv_.wait_until(lk, deadline, [&] { return r->done || stopping_; });
    if (!r->done) {
        if (r->running) cv_.wait(lk, [&] { return r->done; });   // it started: let it finish, it may be a reload
        else {
            r->abandoned = true;
            return error(-32001, "the main loop did not run the request within " + std::to_string(timeoutMs) + " ms (is the game paused or stuck?)");
        }
    }
    if (r->ok) return "{\"id\":" + id + ",\"result\":" + (r->out.empty() ? std::string("null") : r->out) + "}";
    return error(r->code, r->out);
}

bool ControlServer::WriteAll(void* pipe, void* ev, void* stop, const std::string& data) {
    size_t off = 0;
    while (off < data.size()) {
        OVERLAPPED ov{};
        ov.hEvent = static_cast<HANDLE>(ev);
        ResetEvent(ov.hEvent);
        DWORD n = 0;
        BOOL ok = WriteFile(static_cast<HANDLE>(pipe), data.data() + off, static_cast<DWORD>(data.size() - off), &n, &ov);
        if (!ok) {
            if (GetLastError() != ERROR_IO_PENDING) return false;
            HANDLE h[2] = {ov.hEvent, static_cast<HANDLE>(stop)};
            if (WaitForMultipleObjects(2, h, FALSE, INFINITE) != WAIT_OBJECT_0) {
                CancelIoEx(static_cast<HANDLE>(pipe), &ov);
                GetOverlappedResult(static_cast<HANDLE>(pipe), &ov, &n, TRUE);
                return false;
            }
            if (!GetOverlappedResult(static_cast<HANDLE>(pipe), &ov, &n, FALSE)) return false;
        }
        off += n;
    }
    return true;
}

void ControlServer::Serve(void* pipe, void* ev, void* stop) {
    std::string buf;
    char chunk[4096];
    for (;;) {
        size_t nl;
        while ((nl = buf.find('\n')) != npos) {
            std::string line = buf.substr(0, nl);
            buf.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            std::string reply = Handle(line);
            reply += '\n';
            if (!WriteAll(pipe, ev, stop, reply)) return;
        }
        OVERLAPPED ov{};
        ov.hEvent = static_cast<HANDLE>(ev);
        ResetEvent(ov.hEvent);
        DWORD n = 0;
        BOOL ok = ReadFile(static_cast<HANDLE>(pipe), chunk, sizeof chunk, &n, &ov);
        if (!ok) {
            if (GetLastError() != ERROR_IO_PENDING) return;   // the client went away
            HANDLE h[2] = {ov.hEvent, static_cast<HANDLE>(stop)};
            if (WaitForMultipleObjects(2, h, FALSE, INFINITE) != WAIT_OBJECT_0) {
                CancelIoEx(static_cast<HANDLE>(pipe), &ov);
                GetOverlappedResult(static_cast<HANDLE>(pipe), &ov, &n, TRUE);
                return;
            }
            if (!GetOverlappedResult(static_cast<HANDLE>(pipe), &ov, &n, FALSE)) return;
        }
        if (n == 0) return;
        buf.append(chunk, n);
        if (buf.size() > (1u << 20)) return;   // a line that long is not a request
    }
}

void ControlServer::Run() {
    HANDLE pipe = static_cast<HANDLE>(pipe_);
    HANDLE stop = static_cast<HANDLE>(stopEv_);
    HANDLE ev = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    while (WaitForSingleObject(stop, 0) != WAIT_OBJECT_0) {
        OVERLAPPED ov{};
        ov.hEvent = ev;
        ResetEvent(ev);
        BOOL ok = ConnectNamedPipe(pipe, &ov);
        const DWORD err = ok ? 0 : GetLastError();
        if (!ok && err == ERROR_IO_PENDING) {
            HANDLE h[2] = {ev, stop};
            if (WaitForMultipleObjects(2, h, FALSE, INFINITE) != WAIT_OBJECT_0) {
                CancelIoEx(pipe, &ov);
                DWORD n;
                GetOverlappedResult(pipe, &ov, &n, TRUE);
                break;
            }
        } else if (!ok && err != ERROR_PIPE_CONNECTED) {
            Sleep(20);
            continue;
        }
        Serve(pipe, ev, stop);
        DisconnectNamedPipe(pipe);
    }
    CloseHandle(ev);
}

}  // namespace hotreload
