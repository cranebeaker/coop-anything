"""Poll a probe on one or more game instances at a fixed rate and keep the replies as JSONL streams.

    from animcheck.collect import Collector
    with Collector.from_addresses({"host": ("127.0.0.1", 9100), "client": ("127.0.0.1", 9101)}, out_dir="out/run1") as c:
        c.run(seconds=10, hz=20)
    streams = c.streams()

Any object with `.call(method, params)` works as a bridge (the tests use a fake one). TcpBridge speaks the simplest
possible protocol, so a game-side probe can serve it with a few lines of code: one JSON object per line over TCP.
    request:  {"method": "anim.probe", "params": {...}}\\n
    reply:    <the probe object, see FORMAT.md>\\n          (or {"error": "message"}\\n)
The collector never sends input to the game. If your probe writes JSONL files itself you do not need this module:
point `cli.py analyze` at the files.
"""
from __future__ import annotations

import json
import socket
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

from .model import Stream, stream_from_probes
from .profile import Profile


class TcpBridge:
    """Line-delimited JSON request / reply over TCP (see module doc)."""

    def __init__(self, host: str, port: int, timeout_s: float = 5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout_s)
        self.sock.settimeout(timeout_s)
        self.buf = b""

    def call(self, method: str, params: Optional[dict] = None) -> Any:
        self.sock.sendall(json.dumps({"method": method, "params": params or {}}, separators=(",", ":")).encode("utf-8") + b"\n")
        while b"\n" not in self.buf:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("probe closed the connection")
            self.buf += chunk
        line, _, self.buf = self.buf.partition(b"\n")
        r = json.loads(line.decode("utf-8"))
        if isinstance(r, dict) and "error" in r and "actors" not in r:
            raise RuntimeError(str(r["error"]))
        return r

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass


class Collector:
    def __init__(self, bridges: Dict[str, Any], out_dir: Optional[Union[str, Path]] = None, clock=time.monotonic,
                 method: str = "anim.probe", stats_method: Optional[str] = "anim.stats"):
        self.bridges = bridges
        self.out_dir = Path(out_dir) if out_dir else None
        self.clock = clock
        self.method = method
        self.stats_method = stats_method
        self.probes: Dict[str, List[dict]] = {m: [] for m in bridges}
        self.errors: Dict[str, int] = {m: 0 for m in bridges}
        self.last_error: Dict[str, str] = {}
        self._files: Dict[str, Any] = {}
        if self.out_dir:
            self.out_dir.mkdir(parents=True, exist_ok=True)

    @classmethod
    def from_addresses(cls, addresses: Dict[str, Tuple[str, int]], out_dir=None, timeout_s: float = 5.0, **kw) -> "Collector":
        return cls({m: TcpBridge(h, p, timeout_s) for m, (h, p) in addresses.items()}, out_dir, **kw)

    def __enter__(self) -> "Collector":
        return self

    def __exit__(self, *a) -> None:
        self.close()

    def close(self) -> None:
        for f in self._files.values():
            f.close()
        self._files.clear()
        for b in self.bridges.values():
            close = getattr(b, "close", None)
            if close:
                try:
                    close()
                except Exception:
                    pass

    def poll_once(self, params: Optional[dict] = None) -> int:
        n = 0
        for m, b in self.bridges.items():
            t0 = self.clock()
            try:
                r = b.call(self.method, params or {})
            except Exception as e:  # keep sampling the other machines
                self.errors[m] += 1
                self.last_error[m] = repr(e)
                continue
            t1 = self.clock()
            if not isinstance(r, dict):
                self.errors[m] += 1
                continue
            r = dict(r)
            r["machine"] = m
            r["rx_ms"] = round((t0 + t1) / 2.0 * 1000.0, 3)   # midpoint of the request: the reply describes that instant
            self.probes[m].append(r)
            self._write(m, r)
            n += 1
        return n

    def _write(self, m: str, r: dict) -> None:
        if not self.out_dir:
            return
        f = self._files.get(m)
        if f is None:
            f = open(self.out_dir / f"{m}.probe.jsonl", "a", encoding="utf-8")
            self._files[m] = f
        f.write(json.dumps(r, separators=(",", ":")) + "\n")
        f.flush()

    def run(self, seconds: float, hz: float = 20.0, params: Optional[dict] = None, sleep=time.sleep) -> None:
        period = 1.0 / hz
        end = self.clock() + seconds
        nxt = self.clock()
        while self.clock() < end:
            self.poll_once(params)
            nxt += period
            d = nxt - self.clock()
            if d > 0:
                sleep(d)
            else:
                nxt = self.clock()

    def streams(self, profile: Optional[Profile] = None) -> Dict[str, Stream]:
        return {m: stream_from_probes(m, p, use_rx=True, profile=profile) for m, p in self.probes.items()}

    def costs(self) -> Dict[str, dict]:
        """The probe's own cost statistics of every machine (stats_method); {} for machines that do not answer."""
        out = {}
        if not self.stats_method:
            return out
        for m, b in self.bridges.items():
            try:
                out[m] = b.call(self.stats_method, {})
            except Exception:
                pass
        return out
