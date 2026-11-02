"""Tests for the chaos box: model statistics (virtual time) and real UDP round trips on loopback.

Run through run_tests.py (which also runs the mutation twins). Direct: python -I tools/chaos/test_chaos.py
"""
import json
import socket
import statistics
import subprocess
import struct
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from impair import Impairment, load_profiles, make_params, Params  # noqa: E402
from relay import Relay, RpcServer, ChaosClient, upstream_bind  # noqa: E402
import client as cc  # noqa: E402

PROFILES = load_profiles()
NAMES = ["lan", "good_wifi", "bad_wifi", "mobile_tether", "transatlantic", "hell"]


def run_model(params, n=20000, spacing_ms=10.0, size=100, label="c2s"):
    imp = Impairment(params, label)
    out = []
    for i in range(n):
        out.append(imp.decide(i * spacing_ms, size))
    return out


class ModelTests(unittest.TestCase):
    def test_profile_list(self):
        self.assertEqual(sorted(PROFILES), sorted(NAMES))
        for p in PROFILES.values():
            self.assertTrue(p.seed)

    def test_deterministic_per_seed(self):
        a = [(v.delays_ms, v.reason) for v in run_model(PROFILES["bad_wifi"], 2000)]
        b = [(v.delays_ms, v.reason) for v in run_model(PROFILES["bad_wifi"], 2000)]
        self.assertEqual(a, b)
        other = make_params({"profile": "bad_wifi", "seed": 77})
        c = [(v.delays_ms, v.reason) for v in run_model(other, 2000)]
        self.assertNotEqual(a, c)

    def test_directions_use_independent_streams(self):
        a = [v.reason for v in run_model(PROFILES["bad_wifi"], 2000, label="c2s")]
        b = [v.reason for v in run_model(PROFILES["bad_wifi"], 2000, label="s2c")]
        self.assertNotEqual(a, b)

    def test_loss_matches_expected_per_profile(self):
        for name in NAMES:
            p = PROFILES[name]
            vs = run_model(p, 40000)
            loss = sum(v.dropped for v in vs) / len(vs)
            exp = p.expected_loss()
            tol = 0.004 + 0.2 * exp
            self.assertAlmostEqual(loss, exp, delta=tol, msg=f"{name}: loss {loss:.4f} vs {exp:.4f}")

    def test_latency_matches_per_profile(self):
        for name in NAMES:
            p = PROFILES[name]
            d = [v.delays_ms[0] for v in run_model(p, 20000) if not v.dropped]
            exp = (1 - p.reorder) * p.latency_ms
            self.assertAlmostEqual(statistics.mean(d), exp, delta=1.0 + 0.05 * exp, msg=name)
            if p.reorder == 0 and p.bandwidth_bps == 0:
                self.assertGreaterEqual(min(d), max(0, p.latency_ms - p.jitter_ms) - 1e-9, name)
                self.assertLessEqual(max(d), p.latency_ms + p.jitter_ms + 1e-9, name)

    def test_burst_loss_is_clustered(self):
        p = PROFILES["bad_wifi"]
        vs = run_model(p, 60000)
        runs, cur = [], 0
        for v in vs:
            if v.dropped:
                cur += 1
            elif cur:
                runs.append(cur); cur = 0
        independent_mean_run = 1 / (1 - p.expected_loss())
        self.assertGreater(statistics.mean(runs), independent_mean_run * 1.2)

    def test_reorder_and_duplicate(self):
        p = make_params({"latency_ms": 50, "jitter_ms": 0, "reorder": 0.1, "duplicate": 0.1})
        vs = run_model(p, 20000)
        arrivals = sorted((i * 10.0 + d, i) for i, v in enumerate(vs) for d in v.delays_ms)
        inversions = sum(1 for a, b in zip(arrivals, arrivals[1:]) if b[1] < a[1])
        self.assertGreater(inversions / len(vs), 0.05)
        dups = sum(len(v.delays_ms) > 1 for v in vs) / len(vs)
        self.assertAlmostEqual(dups, 0.1, delta=0.01)
        clean = make_params({"latency_ms": 50, "jitter_ms": 0})
        vs = run_model(clean, 2000)
        arrivals = [i * 10.0 + v.delays_ms[0] for i, v in enumerate(vs)]
        self.assertEqual(arrivals, sorted(arrivals))
        self.assertTrue(all(len(v.delays_ms) == 1 for v in vs))

    def test_bandwidth_cap(self):
        p = make_params({"bandwidth_bps": 800000, "buffer_ms": 100})
        vs = run_model(p, 5000, spacing_ms=1.0, size=1000)   # offered 8 Mbps
        delivered = sum(1000 * 8 for v in vs if not v.dropped)
        bps = delivered / (5000 / 1000.0)
        self.assertAlmostEqual(bps, 800000, delta=80000)
        self.assertTrue(any(v.reason == "bandwidth" for v in vs))
        self.assertLessEqual(max(v.delays_ms[0] for v in vs if not v.dropped), 100 + 10 + 1)

    def test_partition_is_one_way_and_reversible(self):
        c, s = Impairment(make_params("lan"), "c2s"), Impairment(make_params("lan"), "s2c")
        c.partition(True)
        self.assertTrue(all(c.decide(i, 50).reason == "partition" for i in range(50)))
        self.assertFalse(any(s.decide(i, 50).dropped for i in range(50)))
        c.partition(False)
        self.assertFalse(c.decide(60, 50).dropped)

    def test_timed_disconnect(self):
        imp = Impairment(make_params("lan"), "c2s")
        imp.disconnect_until(1000)
        self.assertEqual(imp.decide(500, 50).reason, "disconnect")
        self.assertFalse(imp.decide(1000, 50).dropped)


class EchoServer:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.settimeout(0.1)
        self.port = self.sock.getsockname()[1]
        self.peers = set()
        self.received = 0
        self.stop = False
        self.t = threading.Thread(target=self._run, daemon=True)
        self.t.start()

    def _run(self):
        while not self.stop:
            try:
                d, a = self.sock.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError:
                continue
            self.peers.add(a)
            self.received += 1
            self.sock.sendto(d, a)

    def close(self):
        self.stop = True
        self.t.join(2)
        self.sock.close()


def probe(port, n, per_batch=5, gap=0.012, drain=2.0, size=64):
    """Send n numbered packets to the relay, return (sent_times, {seq: rtt_ms}) for echoes that arrive."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    s.settimeout(0.01)
    sent, got, dup = {}, {}, 0
    def pump(until):
        nonlocal dup
        while time.monotonic() < until:
            try:
                d, _ = s.recvfrom(65535)
            except (socket.timeout, ConnectionResetError):
                continue
            q = struct.unpack("!I", d[:4])[0]
            if q in got:
                dup += 1
            else:
                got[q] = (time.monotonic() - sent[q]) * 1000.0
    i = 0
    while i < n:
        for _ in range(per_batch):
            if i >= n:
                break
            sent[i] = time.monotonic()
            s.sendto(struct.pack("!I", i) + b"\0" * (size - 4), ("127.0.0.1", port))
            i += 1
        pump(time.monotonic() + gap)
    pump(time.monotonic() + drain)
    s.close()
    return sent, got, dup


class UdpTests(unittest.TestCase):
    def setUp(self):
        self.echo = EchoServer()
        self.tmp = tempfile.TemporaryDirectory()
        self.logp = Path(self.tmp.name) / "p.jsonl"
        self.relay = None

    def tearDown(self):
        if self.relay:
            self.relay.close()
        self.echo.close()
        self.tmp.cleanup()

    def mk(self, profile, **kw):
        self.relay = Relay(("127.0.0.1", self.echo.port), 0, profile, log_path=kw.pop("log", None), **kw).start()
        return self.relay

    def test_profiles_over_real_udp(self):
        for name in NAMES:
            p = PROFILES[name]
            if self.relay:
                self.relay.close()
            r = self.mk(name)
            n = 1200
            drain = 2.5 if p.latency_ms > 100 else 1.5
            sent, got, _ = probe(r.port, n, drain=drain)
            e2e_loss = 1 - len(got) / n
            exp_loss = 1 - (1 - p.expected_loss()) ** 2
            tol = 0.012 + 0.35 * exp_loss
            self.assertAlmostEqual(e2e_loss, exp_loss, delta=tol, msg=f"{name} loss {e2e_loss:.3f} vs {exp_loss:.3f}")
            rtt = statistics.mean(got.values())
            exp_rtt = 2 * (1 - p.reorder) * p.latency_ms
            self.assertAlmostEqual(rtt, exp_rtt, delta=14 + 0.12 * exp_rtt, msg=f"{name} rtt {rtt:.1f} vs {exp_rtt:.1f}")

    def test_midrun_changes_one_way_partition_and_disconnect(self):
        r = self.mk("lan")
        rpc = RpcServer(r).start()
        cl = ChaosClient(rpc.port)
        try:
            sent, got, _ = probe(r.port, 100, drain=0.3)
            self.assertGreaterEqual(len(got), 98)
            # client to server blocked: server sees nothing new
            before = self.echo.received
            cl.call("partition", dir="c2s", on=True)
            sent, got, _ = probe(r.port, 50, drain=0.3)
            self.assertEqual(len(got), 0)
            self.assertEqual(self.echo.received, before)
            cl.call("partition", dir="c2s", on=False)
            # server to client blocked: server receives, no echo comes back
            before = self.echo.received
            cl.call("partition", dir="s2c", on=True)
            sent, got, _ = probe(r.port, 50, drain=0.3)
            self.assertEqual(len(got), 0)
            self.assertGreaterEqual(self.echo.received - before, 48)
            cl.call("partition", dir="s2c", on=False)
            # timed disconnect then reconnect
            cl.call("disconnect", duration_ms=600)
            _, got, _ = probe(r.port, 20, drain=0.1)
            self.assertEqual(len(got), 0)
            time.sleep(0.6)
            _, got, _ = probe(r.port, 50, drain=0.3)
            self.assertGreaterEqual(len(got), 48)
            # live profile switch
            cl.call("set_profile", profile="transatlantic")
            _, got, _ = probe(r.port, 40, drain=0.6)
            self.assertGreater(statistics.mean(got.values()), 150)
            cl.call("set_params", params={"loss": 1.0, "burst_enter": 0.0})
            _, got, _ = probe(r.port, 40, drain=0.4)
            self.assertEqual(len(got), 0)
            st = cl.call("stats")
            self.assertGreater(st["c2s"]["dropped"], 100)
            with self.assertRaises(RuntimeError):
                cl.call("set_profile", profile="nope")
        finally:
            cl.close()
            rpc.shutdown()
            rpc.server_close()

    def test_packet_log(self):
        r = self.mk("hell", log=str(self.logp), seed=5)
        sent, got, _ = probe(r.port, 200, drain=1.5)
        r.set_profile("lan")
        r.close(); self.relay = None
        lines = [json.loads(x) for x in self.logp.read_text().splitlines()]
        hdr = lines[0]
        self.assertEqual((hdr["format"], hdr["version"], hdr["profile"], hdr["seed"]), ("coopchaos-packetlog", 1, "hell", 5))
        pk = [x for x in lines[1:] if "verdict" in x]
        c2s = [x for x in pk if x["dir"] == "c2s"]
        self.assertEqual(len(c2s), 200)
        self.assertEqual([x["n"] for x in c2s], list(range(200)))
        self.assertEqual(c2s[7]["head"][:8], struct.pack("!I", 7).hex())
        self.assertTrue(any(x["verdict"] == "drop" and x["reason"] for x in c2s))
        self.assertTrue(any(x.get("event") == "profile" for x in lines))
        for x in pk:
            self.assertEqual(x["verdict"] == "fwd", bool(x["copies"]))

    def test_two_clients_are_distinct_peers(self):
        r = self.mk("lan")
        probe(r.port, 10, drain=0.2)
        probe(r.port, 10, drain=0.2)
        self.assertEqual(len(self.echo.peers), 2)


class ClientModuleTests(unittest.TestCase):
    def setUp(self):
        self.echo = EchoServer()
        self.tmp = tempfile.TemporaryDirectory()
        self.logp = str(Path(self.tmp.name) / "c.jsonl")
        self.relay = Relay(("127.0.0.1", self.echo.port), 0, "lan", log_path=self.logp).start()
        self.rpc = RpcServer(self.relay).start()

    def tearDown(self):
        self.rpc.shutdown(); self.rpc.server_close()
        self.relay.close(); self.echo.close(); self.tmp.cleanup()

    def test_set_profile_stats_and_addr_forms(self):
        self.assertTrue(cc.set_profile(self.rpc.port, "transatlantic", seed=9))
        self.assertEqual(self.relay.profile_name, "transatlantic")
        self.assertEqual(self.relay.dirs["c2s"].p.seed, 9)
        self.assertTrue(cc.set_profile(f"127.0.0.1:{self.rpc.port}", "lan", dir="s2c"))
        self.assertEqual(self.relay.dirs["s2c"].p.latency_ms, PROFILES["lan"].latency_ms)
        self.assertEqual(self.relay.dirs["c2s"].p.latency_ms, PROFILES["transatlantic"].latency_ms)
        info = Path(self.tmp.name) / "info.json"
        info.write_text(json.dumps({"rpc": self.rpc.port}))
        cc.set_profile(str(info), "lan")
        self.assertEqual(self.relay.profile_name, "lan")
        probe(self.relay.port, 20, drain=0.3)
        st = cc.stats(self.rpc.port)
        self.assertEqual(st["c2s"]["seen"], 20)
        with self.assertRaises(RuntimeError):
            cc.set_profile(self.rpc.port, "nope")

    def test_partition_direction_and_self_heal(self):
        cc.partition(self.rpc.port, "c2s")
        self.assertTrue(self.relay.dirs["c2s"].partitioned)
        self.assertFalse(self.relay.dirs["s2c"].partitioned)
        cc.partition(self.rpc.port, "c2s", on=False)
        self.assertFalse(self.relay.dirs["c2s"].partitioned)
        cc.partition(self.rpc.port, "s2c", seconds=0.3)
        self.assertTrue(self.relay.dirs["s2c"].partitioned)
        _, got, _ = probe(self.relay.port, 10, drain=0.1)
        self.assertEqual(len(got), 0)
        time.sleep(0.4)
        self.assertFalse(self.relay.dirs["s2c"].partitioned)
        _, got, _ = probe(self.relay.port, 20, drain=0.3)
        self.assertGreaterEqual(len(got), 19)

    def test_disconnect_and_log_path(self):
        cc.disconnect(self.rpc.port, 0.5)
        _, got, _ = probe(self.relay.port, 10, drain=0.1)
        self.assertEqual(len(got), 0)
        time.sleep(0.5)
        _, got, _ = probe(self.relay.port, 20, drain=0.3)
        self.assertGreaterEqual(len(got), 19)
        self.assertEqual(cc.packet_log_path(self.rpc.port), self.logp)


class CliTests(unittest.TestCase):
    def test_upstream_bind_follows_target(self):
        self.assertEqual(upstream_bind("127.0.0.1"), "127.0.0.1")
        self.assertEqual(upstream_bind("localhost"), "127.0.0.1")
        self.assertEqual(upstream_bind("192.168.1.20"), "0.0.0.0")

    def test_profiles_command_lists_every_profile(self):
        p = subprocess.run([sys.executable, "-I", str(Path(__file__).resolve().parent / "chaos.py"), "profiles"],
                           capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr)
        for n in NAMES:
            self.assertIn(n, p.stdout)

    def test_relay_command_starts_serves_rpc_and_shuts_down(self):
        echo = EchoServer()
        with tempfile.TemporaryDirectory() as td:
            info = Path(td) / "info.json"
            proc = subprocess.Popen([sys.executable, "-I", str(Path(__file__).resolve().parent / "chaos.py"), "relay",
                                     "--target", f"127.0.0.1:{echo.port}", "--profile", "lan", "--info-file", str(info),
                                     "--seconds", "20"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                for _ in range(100):
                    if info.exists() and info.read_text():
                        break
                    time.sleep(0.1)
                port = json.loads(info.read_text())
                c = ChaosClient(port["rpc"])
                self.assertEqual(c.call("status")["profile"], "lan")
                c.call("shutdown")
                c.close()
                self.assertEqual(proc.wait(timeout=10), 0)
            finally:
                if proc.poll() is None:
                    proc.kill()
                proc.stdout.close(); proc.stderr.close()
                echo.close()


if __name__ == "__main__":
    unittest.main(verbosity=1)
