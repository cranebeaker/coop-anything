"""UDP relay with live-adjustable impairment, plus a JSON-RPC control port and a packet log.

Game clients send to the relay's listen port; the relay forwards to the target (the host) through one upstream
socket per client address (like a NAT), so the host sees distinct peers. Replies come back the same way.
Directions: "c2s" (client to server) and "s2c" (server to client). Each has its own Impairment.
"""
import heapq
import json
import socket
import socketserver
import threading
import time

from impair import Impairment, make_params, load_profiles, flip_bit

SIO_UDP_CONNRESET = 0x9800000C


def _timer_res(ms):
    # Windows timers default to ~15.6 ms ticks, which would add that much to every delayed packet.
    try:
        import ctypes
        ctypes.windll.winmm.timeBeginPeriod(ms)
        return True
    except (AttributeError, OSError, ImportError):
        return False


def _no_connreset(sock):
    # Windows reports ICMP port-unreachable as a recvfrom error; turn that off.
    try:
        sock.ioctl(SIO_UDP_CONNRESET, False)
    except (AttributeError, OSError, ValueError):
        pass


def upstream_bind(target_host):
    """Local address for the per-client upstream sockets: loopback when the target is on this PC,
    otherwise any interface, so the relay can also sit in front of a server on another machine."""
    return "127.0.0.1" if target_host.startswith("127.") or target_host in ("localhost", "::1") else "0.0.0.0"


class PacketLog:
    def __init__(self, path, profile, seed):
        self.f = None
        self.lock = threading.Lock()
        if path:
            self.f = open(path, "w", encoding="utf-8", buffering=1)
            self._w({"format": "coopchaos-packetlog", "version": 1, "profile": profile, "seed": seed,
                     "start_wall": time.time()})

    def _w(self, obj):
        if self.f:
            with self.lock:
                self.f.write(json.dumps(obj, separators=(",", ":")) + "\n")

    def packet(self, **kw):
        self._w(kw)

    def event(self, t, name, **kw):
        self._w({"t": round(t, 3), "event": name, **kw})

    def close(self):
        if self.f:
            with self.lock:
                self.f.close()
                self.f = None


class Relay:
    def __init__(self, target, listen_port=0, profile="lan", seed=None, log_path=None, bind="127.0.0.1"):
        self.target = target
        self.t0 = time.monotonic()
        self.profile_name = profile if isinstance(profile, str) else "custom"
        params = make_params(profile)
        if seed is not None:
            params.seed = seed
        self.dirs = {d: Impairment(make_params(params), d) for d in ("c2s", "s2c")}
        self.log_path = str(log_path) if log_path else None
        self.log = PacketLog(log_path, self.profile_name, params.seed)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        _no_connreset(self.sock)
        self.sock.bind((bind, listen_port))
        self.sock.settimeout(0.1)
        self.port = self.sock.getsockname()[1]
        self.clients = {}                      # client addr -> upstream socket
        self.stats = {d: self._blank() for d in self.dirs}
        self.n = {d: 0 for d in self.dirs}
        self.lock = threading.RLock()
        self.heap = []
        self.cv = threading.Condition()
        self.seq = 0
        self.stop_ev = threading.Event()
        self.shutdown_requested = threading.Event()
        self.threads = [threading.Thread(target=self._listen, daemon=True),
                        threading.Thread(target=self._sched, daemon=True)]
        self.reconnect_timer = None

    @staticmethod
    def _blank():
        return {"seen": 0, "forwarded": 0, "dropped": 0, "duplicated": 0, "bytes_in": 0, "bytes_out": 0,
                "reasons": {}}

    def now_ms(self):
        return (time.monotonic() - self.t0) * 1000.0

    def start(self):
        self._res = _timer_res(1)
        for t in self.threads:
            t.start()
        return self

    def close(self):
        self.stop_ev.set()
        with self.cv:
            self.cv.notify_all()
        for t in self.threads:
            t.join(timeout=2)
        with self.lock:
            for s in list(self.clients.values()):
                s.close()
        self.sock.close()
        self.log.close()
        if getattr(self, '_res', False):
            import ctypes
            ctypes.windll.winmm.timeEndPeriod(1)

    # ---- datapath ----
    def _listen(self):
        while not self.stop_ev.is_set():
            try:
                data, addr = self.sock.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError:
                if self.stop_ev.is_set():
                    return
                continue
            with self.lock:
                if addr not in self.clients:
                    up = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    _no_connreset(up)
                    up.bind((upstream_bind(self.target[0]), 0))
                    up.settimeout(0.1)
                    self.clients[addr] = up
                    threading.Thread(target=self._upstream, args=(addr, up), daemon=True).start()
                up = self.clients[addr]
            self._handle("c2s", addr, data, up, self.target)

    def _upstream(self, client, up):
        while not self.stop_ev.is_set():
            try:
                data, _ = up.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError:
                if self.stop_ev.is_set():
                    return
                continue
            self._handle("s2c", client, data, self.sock, client)

    def _handle(self, d, client, data, out_sock, dest):
        t = self.now_ms()
        with self.lock:
            imp = self.dirs[d]
            v = imp.decide(t, len(data))
            st = self.stats[d]
            n = self.n[d]
            self.n[d] += 1
            st["seen"] += 1
            st["bytes_in"] += len(data)
            if v.dropped:
                st["dropped"] += 1
                st["reasons"][v.reason] = st["reasons"].get(v.reason, 0) + 1
            else:
                st["forwarded"] += 1
                st["duplicated"] += len(v.delays_ms) - 1
                st["bytes_out"] += len(data) * len(v.delays_ms)
            payload = flip_bit(data, imp.rng) if v.corrupt else data
        self.log.packet(t=round(t, 3), dir=d, peer=f"{client[0]}:{client[1]}", n=n, len=len(data),
                        head=data[:16].hex(), verdict="drop" if v.dropped else "fwd",
                        reason=v.reason or None, copies=[round(x, 3) for x in v.delays_ms], corrupt=v.corrupt)
        for delay in v.delays_ms:
            self._schedule(t + delay, out_sock, dest, payload)

    def _schedule(self, due_ms, sock, dest, data):
        with self.cv:
            self.seq += 1
            heapq.heappush(self.heap, (due_ms, self.seq, sock, dest, data))
            self.cv.notify()

    def _sched(self):
        while not self.stop_ev.is_set():
            with self.cv:
                while not self.heap and not self.stop_ev.is_set():
                    self.cv.wait(0.1)
                if self.stop_ev.is_set():
                    return
                due = self.heap[0][0]
                wait = (due - self.now_ms()) / 1000.0
                if wait > 0.002:
                    self.cv.wait(min(wait - 0.001, 0.1))
                    continue
                if wait > 0:
                    pass            # under 2 ms: fall through and send now (sub-ms error is fine)
                _, _, sock, dest, data = heapq.heappop(self.heap)
            try:
                sock.sendto(data, dest)
            except OSError:
                pass

    # ---- control ----
    def _dirs(self, which):
        if which in (None, "both"):
            return list(self.dirs)
        if which not in self.dirs:
            raise ValueError(f"dir must be c2s, s2c or both, got {which!r}")
        return [which]

    def set_profile(self, profile, seed=None, dir=None):
        params = make_params(profile)
        if seed is not None:
            params.seed = seed
        with self.lock:
            for d in self._dirs(dir):
                self.dirs[d].set_params(make_params(params), reseed=True)
            if dir in (None, "both"):
                self.profile_name = profile if isinstance(profile, str) else "custom"
        self.log.event(self.now_ms(), "profile", profile=profile if isinstance(profile, str) else "custom",
                       dir=dir or "both", seed=params.seed)

    def set_params(self, overrides, dir=None):
        with self.lock:
            for d in self._dirs(dir):
                cur = self.dirs[d].p
                merged = make_params({**cur.__dict__, **overrides})
                self.dirs[d].set_params(merged)
        self.log.event(self.now_ms(), "params", dir=dir or "both", overrides=overrides)

    def partition(self, dir="both", on=True, duration_ms=None):
        with self.lock:
            for d in self._dirs(dir):
                self.dirs[d].partition(on)
        self.log.event(self.now_ms(), "partition", dir=dir, on=bool(on))
        if on and duration_ms:
            t = threading.Timer(duration_ms / 1000.0, lambda: None if self.stop_ev.is_set() else self.partition(dir, False))
            t.daemon = True
            t.start()

    def disconnect(self, duration_ms):
        t = self.now_ms()
        with self.lock:
            for imp in self.dirs.values():
                imp.disconnect_until(t + duration_ms)
        self.log.event(t, "disconnect", duration_ms=duration_ms)
        if self.reconnect_timer:
            self.reconnect_timer.cancel()
        self.reconnect_timer = threading.Timer(duration_ms / 1000.0, self._log_reconnect)
        self.reconnect_timer.daemon = True
        self.reconnect_timer.start()

    def _log_reconnect(self):
        if not self.stop_ev.is_set():
            self.log.event(self.now_ms(), "reconnect")

    def reconnect(self):
        with self.lock:
            for imp in self.dirs.values():
                imp.reconnect()
        self.log.event(self.now_ms(), "reconnect")

    def get_stats(self):
        with self.lock:
            return json.loads(json.dumps(self.stats))

    def reset_stats(self):
        with self.lock:
            self.stats = {d: self._blank() for d in self.dirs}

    def status(self):
        with self.lock:
            return {"listen_port": self.port, "target": list(self.target), "profile": self.profile_name,
                    "clients": len(self.clients),
                    "dirs": {d: {"params": i.p.__dict__.copy(), "partitioned": i.partitioned,
                                 "down": self.now_ms() < i.down_until_ms} for d, i in self.dirs.items()}}


# ---- JSON-RPC (newline-delimited JSON-RPC 2.0 over TCP) ----
class _Handler(socketserver.StreamRequestHandler):
    def handle(self):
        relay = self.server.relay
        for line in self.rfile:
            line = line.strip()
            if not line:
                continue
            rid = None
            try:
                req = json.loads(line)
                rid = req.get("id")
                m, p = req["method"], req.get("params") or {}
                if m == "status":
                    res = relay.status()
                elif m == "stats":
                    res = relay.get_stats()
                elif m == "reset_stats":
                    relay.reset_stats(); res = True
                elif m == "profiles":
                    res = {k: v.__dict__ for k, v in load_profiles().items()}
                elif m == "log_path":
                    res = relay.log_path
                elif m == "set_profile":
                    relay.set_profile(p.get("profile", p.get("name")), p.get("seed"), p.get("dir")); res = True
                elif m == "set_params":
                    relay.set_params(p.get("params", {}), p.get("dir")); res = True
                elif m == "partition":
                    relay.partition(p.get("dir", "both"), p.get("on", True), p.get("duration_ms")); res = True
                elif m == "disconnect":
                    relay.disconnect(float(p["duration_ms"])); res = True
                elif m == "reconnect":
                    relay.reconnect(); res = True
                elif m == "shutdown":
                    relay.shutdown_requested.set(); res = True
                else:
                    raise ValueError(f"unknown method {m!r}")
                out = {"jsonrpc": "2.0", "id": rid, "result": res}
            except Exception as e:  # report to caller, keep serving
                out = {"jsonrpc": "2.0", "id": rid, "error": {"code": -32000, "message": f"{type(e).__name__}: {e}"}}
            self.wfile.write((json.dumps(out) + "\n").encode())
            self.wfile.flush()


class RpcServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, relay, port=0, bind="127.0.0.1"):
        super().__init__((bind, port), _Handler)
        self.relay = relay
        self.port = self.server_address[1]

    def start(self):
        threading.Thread(target=self.serve_forever, daemon=True).start()
        return self


class ChaosClient:
    """Tiny JSON-RPC client for tests and scripts."""
    def __init__(self, port, host="127.0.0.1"):
        self.s = socket.create_connection((host, port), timeout=5)
        self.f = self.s.makefile("rwb")
        self.i = 0

    def call(self, method, **params):
        self.i += 1
        self.f.write((json.dumps({"jsonrpc": "2.0", "id": self.i, "method": method, "params": params}) + "\n").encode())
        self.f.flush()
        resp = json.loads(self.f.readline())
        if "error" in resp:
            raise RuntimeError(resp["error"]["message"])
        return resp["result"]

    def close(self):
        self.s.close()
