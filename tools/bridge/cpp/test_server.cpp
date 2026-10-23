// A tiny pretend game that embeds coopbridge/server.h. tools/bridge/test_cpp_server.py starts this program and drives it
// with the Python client. It shows the minimum an embedding mod needs: an Adapter, Options, one Pump() per frame.
//
//   coopbridge_test [--port N] [--pipe NAME] [--endpoint-file PATH] [--host H] [--budget N] [--max-pending N]
//                   [--slack-ms N] [--event-capacity N] [--max-line N] [--no-screenshot]
//
// It prints "READY {json}" when the server runs, or "FAILED <reason>" (exit 3) when it cannot start, and exits when a
// client calls test.quit.
#include <coopbridge/server.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>

using coopbridge::json::Value;

namespace {

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

class FakeGameAdapter : public coopbridge::Adapter {
public:
    coopbridge::Server* server = nullptr;
    bool screenshotOn = true;
    std::map<std::string, double> vars;
    std::map<std::string, bool> saves;
    int loadCountdown = -1;
    int saveCountdown = -1;
    std::string pendingSave;

    coopbridge::Capabilities capabilities() const override {
        coopbridge::Capabilities c;
        c.exec = c.eval = c.state = c.save = c.load = true;
        c.screenshot = screenshotOn;
        return c;
    }

    coopbridge::ExecResult Exec(const std::string& line, const std::string&) override {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        coopbridge::ExecResult r;
        if (cmd == "set") {
            std::string name;
            double v = 0;
            in >> name >> v;
            vars[name] = v;
            r.ok = true;
            r.output = "ok";
        } else if (cmd == "inc") {
            std::string name;
            in >> name;
            vars[name] += 1;
            r.ok = true;
            r.output = std::to_string(static_cast<long long>(vars[name]));
        } else if (cmd == "get") {
            std::string name;
            in >> name;
            r.ok = true;
            r.output = name + " = " + std::to_string(static_cast<long long>(vars[name]));
        } else if (cmd == "say") {
            std::string rest;
            std::getline(in, rest);
            r.ok = true;
            r.output = rest.empty() ? rest : rest.substr(1);
        } else if (cmd == "whoami") {
            r.ok = true;
            r.output = server->OnPumpThread() ? "main" : "other";
        } else if (cmd == "boom") {
            throw std::runtime_error("the adapter threw");
        } else if (cmd == "throw") {
            throw coopbridge::Error(-32050, "custom adapter error");
        } else {
            r.ok = false;
            r.output = "Script command not found: " + cmd;
        }
        return r;
    }

    Value Eval(const std::string& expr, const std::string&) override {
        char* end = nullptr;
        const double d = std::strtod(expr.c_str(), &end);
        if (end != expr.c_str() && *end == '\0') return Value(d);
        auto it = vars.find(expr);
        if (it == vars.end()) throw std::runtime_error("unknown name " + expr);
        return Value(it->second);
    }

    Value State(const std::vector<std::string>& sections) override {
        Value v = Value::object();
        Value vars_ = Value::object();
        for (const auto& kv : vars) vars_.set(kv.first, kv.second);
        if (sections.empty() || sections[0] == "vars") v.set("vars", vars_);
        if (sections.empty() || sections[0] == "menu") v.set("menu", Value::object().set("kind", "none"));
        return v;
    }

    void Load(const std::string& name) override {
        if (name == "missing") throw std::runtime_error("no such save: missing");
        loadCountdown = 10;   // frames until the game is in the world
    }

    void Save(const std::string& name, bool overwrite) override {
        if (saves.count(name) && !overwrite) throw coopbridge::Error(-32012, "save '" + name + "' already exists");
        saves[name] = true;
        pendingSave = name;
        saveCountdown = name == "slowsave" ? -1 : 5;   // "slowsave" never finishes
    }

    std::string Screenshot(const std::string& path) override { return path.empty() ? "shot.png" : path; }

    // The game's own per-frame work: finishes loads and saves a few frames after they were asked for.
    void Tick() {
        if (loadCountdown > 0 && --loadCountdown == 0) { loadCountdown = -1; server->NotifyLoaded(); }
        if (saveCountdown > 0 && --saveCountdown == 0) { saveCountdown = -1; server->NotifySaved(pendingSave + ".sav"); }
    }
};

}  // namespace

int main(int argc, char** argv) {
    coopbridge::Options opt;
    opt.game = "cppfake";
    opt.build = "test-1";
    opt.maxCallsPerPump = 8;
    opt.slackMs = 300;
    bool noShot = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--port") opt.tcpPort = std::atoi(next().c_str());
        else if (a == "--pipe") opt.pipeName = next();
        else if (a == "--endpoint-file") opt.endpointFile = next();
        else if (a == "--host") opt.tcpHost = next();
        else if (a == "--budget") opt.maxCallsPerPump = static_cast<size_t>(std::atoi(next().c_str()));
        else if (a == "--max-pending") opt.maxPending = static_cast<size_t>(std::atoi(next().c_str()));
        else if (a == "--slack-ms") opt.slackMs = std::atoi(next().c_str());
        else if (a == "--event-capacity") opt.eventCapacity = static_cast<size_t>(std::atoi(next().c_str()));
        else if (a == "--max-line") opt.maxLineBytes = static_cast<size_t>(std::atoi(next().c_str()));
        else if (a == "--no-screenshot") noShot = true;
    }

    FakeGameAdapter adapter;
    adapter.screenshotOn = !noShot;
    coopbridge::Server server;
    adapter.server = &server;

    std::atomic<bool> quit{false};
    std::atomic<uint64_t> pauseUntilMs{0};
    server.Register("test.pause_pump", [&](const Value& p) {
        pauseUntilMs = NowMs() + static_cast<uint64_t>(p.get("ms").asInt(0));
        return Value::object();
    }, false);
    server.Register("test.pump_stats", [&](const Value&) {
        const coopbridge::Stats s = server.stats();
        Value v = Value::object();
        v.set("maxDrained", s.maxDrained).set("served", s.served).set("frames", s.frames).set("rejectedBusy", s.rejectedBusy);
        return v;
    }, false);
    server.Register("test.on_main", [&](const Value&) {
        Value v = Value::object();
        v.set("onPumpThread", server.OnPumpThread());
        return v;
    });
    server.Register("test.quit", [&](const Value&) { quit = true; return Value::object(); }, false);
    server.Register("test.echo", [](const Value& p) { return p; }, false);
    server.Register("test.bytes", [](const Value&) {
        Value v = Value::object();
        v.set("text", std::string("ab\xFF" "cd"));   // not valid UTF-8: the writer must replace the byte
        return v;
    }, false);
    server.Register("test.event", [&](const Value& p) {
        server.PushEvent(p.get("type").asString("test"), p.get("data"));
        return Value::object();
    }, false);

    if (!server.Start(opt, adapter)) {
        std::cout << "FAILED " << server.lastError() << std::endl;
        return 3;
    }
    {
        Value v = Value::object();
        Value eps = Value::array();
        for (const auto& e : server.endpoints()) eps.push(e);
        v.set("endpoints", eps).set("port", server.tcpPort());
        std::cout << "READY " << v.dump() << std::endl;
    }
    server.SetReady(true);

    // The "game loop": one Pump() and one tick of game work per frame.
    while (!quit.load()) {
        if (NowMs() >= pauseUntilMs.load()) {
            server.Pump();
            adapter.Tick();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    server.Stop();
    return 0;
}
