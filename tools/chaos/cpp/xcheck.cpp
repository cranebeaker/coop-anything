// Cross-check driver: runs an op script through the C++ impairment mirror and prints one result line per op.
// tools/chaos/xcheck.py generates the script, runs the Python model on it, and compares both outputs.
// Ops: new <profile> <seed> <label> | set <key> <value> | d <nowMs> <size> | part <0|1> | down <untilMs> | up
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include "impair.h"

using namespace coopchaos;

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: chaos_xcheck <script>\n"); return 2; }
    std::ifstream in(argv[1]);
    std::string line;
    Params params;
    std::string label;
    Impairment* imp = nullptr;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string op;
        ss >> op;
        if (op == "new") {
            std::string name; uint64_t seed;
            ss >> name >> seed >> label;
            params = profile(name);
            params.seed = seed;
            delete imp;
            imp = new Impairment(params, label);
            std::printf("ok\n");
        } else if (op == "set") {
            std::string k; double v;
            ss >> k >> v;
            if (k == "latency_ms") params.latencyMs = v;
            else if (k == "jitter_ms") params.jitterMs = v;
            else if (k == "loss") params.loss = v;
            else if (k == "burst_enter") params.burstEnter = v;
            else if (k == "burst_exit") params.burstExit = v;
            else if (k == "burst_loss") params.burstLoss = v;
            else if (k == "reorder") params.reorder = v;
            else if (k == "duplicate") params.duplicate = v;
            else if (k == "corrupt") params.corrupt = v;
            else if (k == "bandwidth_bps") params.bandwidthBps = static_cast<uint64_t>(v);
            else if (k == "buffer_ms") params.bufferMs = v;
            else { std::fprintf(stderr, "bad key %s\n", k.c_str()); return 2; }
            imp->setParams(params, false);
            std::printf("ok\n");
        } else if (op == "d") {
            double now; uint32_t size;
            ss >> now >> size;
            Verdict v = imp->decide(now, size);
            std::printf("D %s %d %zu", v.dropped() ? reasonName(v.reason) : "-", v.corrupt ? 1 : 0, v.delaysMs.size());
            for (double d : v.delaysMs) std::printf(" %.17g", d);
            if (v.corrupt) {
                std::vector<uint8_t> buf(size > 64 ? 64 : size, 0);
                imp->flipBit(buf);
                std::printf(" flip");
                for (uint8_t b : buf) std::printf(" %02x", b);
            }
            std::printf("\n");
        } else if (op == "part") { int on; ss >> on; imp->partition(on != 0); std::printf("ok\n"); }
        else if (op == "down") { double u; ss >> u; imp->disconnectUntil(u); std::printf("ok\n"); }
        else if (op == "up") { imp->reconnect(); std::printf("ok\n"); }
        else if (!op.empty()) { std::fprintf(stderr, "bad op %s\n", op.c_str()); return 2; }
    }
    delete imp;
    return 0;
}
