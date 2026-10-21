// A small control channel: a Windows named pipe that speaks one JSON object per line.
//
//   request : {"id":1,"method":"dev.status","params":{}}\n
//   reply   : {"id":1,"result":{...}}\n          or          {"id":1,"error":{"code":-32601,"message":"..."}}\n
//
// One client at a time. The pipe is served by its own thread, but every method except `ping` is run on YOUR thread: your
// main loop calls Pump() once per frame, so methods can touch game state without locks and a reload can happen between
// frames. `ping` is answered on the pipe thread ({"pid":<process id>}), so a client can tell "game is up but its main loop
// is stuck" from "no game".
//
// Error codes: -32601 unknown method, -32000 the method failed, -32001 the main loop did not run the request in time.
// Params may carry "timeoutMs" (default 30000): how long the pipe thread waits for the main loop.
//
// The static library `hotreload_control` has no other dependency, so a host can use it on its own.
#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace hotreload {

// ---- tiny JSON helpers (flat lookups in an object; enough for the control protocol) ----
std::string JsonEscape(const std::string& s);
// The raw text of the value of a top-level key of the JSON object `obj`. False if obj is not an object or has no such key.
bool JsonFindValue(const std::string& obj, const std::string& key, std::string& raw);
bool JsonGetString(const std::string& obj, const std::string& key, std::string& out);   // unescaped
bool JsonGetInt(const std::string& obj, const std::string& key, long long& out);

class ControlServer {
public:
    // Runs on the thread that calls Pump(). Return true with JSON in `out`, or false with a message in `out` (and a code,
    // default -32000; set -32601 for an unknown method).
    using Handler = std::function<bool(const std::string& method, const std::string& params, std::string& out, int& code)>;

    // pipeName: a short name ("mygame-1") or a full \\.\pipe\name path.
    ControlServer(std::string pipeName, Handler handler);
    ~ControlServer();
    ControlServer(const ControlServer&) = delete;
    ControlServer& operator=(const ControlServer&) = delete;

    // Creates the pipe and starts the serving thread. False (with the reason) if the name is taken or cannot be created.
    bool Start(std::string& err);
    void Stop();
    // Main thread: run the queued requests. Returns how many ran.
    int Pump();
    const std::string& path() const { return path_; }

private:
    struct Req {
        std::string method, params;
        bool running = false, abandoned = false, done = false, ok = false;
        std::string out;
        int code = -32000;
    };
    void Run();
    void Serve(void* pipe, void* ev, void* stop);
    bool WriteAll(void* pipe, void* ev, void* stop, const std::string& data);
    std::string Handle(const std::string& line);

    std::string path_;
    Handler handler_;
    void* pipe_ = nullptr;
    void* stopEv_ = nullptr;
    std::thread thread_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::shared_ptr<Req>> queue_;
    bool stopping_ = false;
};

}  // namespace hotreload
