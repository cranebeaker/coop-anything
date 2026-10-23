"""Pool daemon: a Pool plus a 127.0.0.1 TCP control server (one JSON object per line) and a tick thread.

    python -I tools/gamepool/gamepool.py --config game.json pool up --size 2     (starts this, detached)

It writes <state>/pool/pool.json {port, pid, token} once the instances are warm. The server listens on
127.0.0.1 only and every request carries the random token from that file.
Request:  {"op": "...", "token": "...", ...}   Reply: {"ok": true, ...} or {"ok": false, "error": "...", "kind": "busy|error"}
ops: status, lease(owner,n,ttl_s,timeout_s), release(id,dirty), renew(id,ttl_s), dirty(id,instance,reason), down
"""
from __future__ import annotations

import argparse
import json
import os
import secrets
import socket
import socketserver
import sys
import threading
from pathlib import Path
from typing import List

from . import poolcore as core
from .config import Config, state_dir


def pool_dir() -> Path:
    return state_dir() / "pool"


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, pool: core.Pool, token: str):
        super().__init__(("127.0.0.1", 0), Handler)
        self.pool, self.token = pool, token
        self.stop = threading.Event()


class Handler(socketserver.StreamRequestHandler):
    def handle(self) -> None:
        for raw in self.rfile:
            try:
                req = json.loads(raw)
                rep = self.dispatch(req)
            except core.PoolBusy as e:
                rep = {"ok": False, "kind": "busy", "error": str(e)}
            except Exception as e:  # noqa: BLE001
                rep = {"ok": False, "kind": "error", "error": f"{type(e).__name__}: {e}"}
            self.wfile.write((json.dumps(rep) + "\n").encode())
            self.wfile.flush()

    def dispatch(self, r: dict) -> dict:
        srv: Server = self.server  # type: ignore[assignment]
        if r.get("token") != srv.token:
            return {"ok": False, "kind": "error", "error": "bad token"}
        p, op = srv.pool, r.get("op")
        if op == "status":
            return {"ok": True, **p.status()}
        if op == "lease":
            return {"ok": True, "lease": p.lease(str(r["owner"]), int(r.get("n", 1)), float(r.get("ttl_s", 900)),
                                                 float(r.get("timeout_s", 600)))}
        if op == "release":
            return {"ok": True, "released": p.release(r["id"], dirty_all=bool(r.get("dirty")), reason=r.get("reason", ""))}
        if op == "renew":
            return {"ok": True, "renewed": p.renew(r["id"], r.get("ttl_s"))}
        if op == "dirty":
            return {"ok": True, "marked": p.mark_dirty(r["id"], int(r["instance"]), str(r.get("reason", "")))}
        if op == "down":
            threading.Thread(target=srv.stop.set, daemon=True).start()
            return {"ok": True}
        return {"ok": False, "kind": "error", "error": f"unknown op {op!r}"}


def parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(prog="gamepool daemon", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--size", type=int, default=1, help="warm instances (at most the number of game slots)")
    ap.add_argument("--fake", action="store_true", help="in-memory fake game (tests); never launches anything")
    ap.add_argument("--recycle-after", type=int, help="recycle an instance after K leases")
    ap.add_argument("--ws-growth-mb", type=int, help="recycle when the working set exceeds boot size + this")
    ap.add_argument("--ws-max-mb", type=int, help="recycle above this working set")
    ap.add_argument("--opt", action="append", default=[], metavar="NAME=VALUE", help="low-cost profile option")
    ap.add_argument("--no-lowcost", action="store_true")
    ap.add_argument("--boot-timeout", type=float, default=0.0, help="seconds to wait for ready (default ready.timeout_s)")
    ap.add_argument("--slot-ttl", type=int)
    ap.add_argument("--tick-s", type=float)
    return ap


def run(cfg: Config, a) -> int:
    from . import backend as be, fakes
    pdir = pool_dir()
    pdir.mkdir(parents=True, exist_ok=True)
    info_file = pdir / "pool.json"
    if info_file.exists():
        try:
            old = json.loads(info_file.read_text())
            socket.create_connection(("127.0.0.1", old["port"]), timeout=1).close()
            print("pool already running on port", old["port"], file=sys.stderr)
            return 2
        except (OSError, ValueError, KeyError):
            info_file.unlink(missing_ok=True)   # stale
    pc = cfg["pool"]
    recycle_after = a.recycle_after if a.recycle_after is not None else pc["recycle_after"]
    ws_growth = a.ws_growth_mb if a.ws_growth_mb is not None else pc["ws_growth_mb"]
    ws_max = a.ws_max_mb if a.ws_max_mb is not None else pc["ws_max_mb"]
    slot_ttl = a.slot_ttl if a.slot_ttl is not None else pc["slot_ttl_s"]
    tick_s = a.tick_s if a.tick_s is not None else pc["tick_s"]
    if a.fake:
        backend, slots = fakes.FakeBackend(), fakes.FakeSlots()
    else:
        cfg.require_dirs()
        cfg.require_process_name()
        opts = dict(o.partition("=")[::2] for o in a.opt)
        backend = be.RealBackend(cfg, lowcost_on=not a.no_lowcost, options=opts, boot_timeout_s=a.boot_timeout)
        slots = be.RealSlots(ttl_s=slot_ttl)
    pool = core.Pool(backend, slots, size=a.size, recycle_after=recycle_after, ws_growth_mb=ws_growth,
                     ws_max_mb=ws_max, slot_ttl_s=slot_ttl, state_path=pdir / "state.json")
    try:
        ids = pool.up()
    except (core.PoolError, RuntimeError) as e:
        print("pool up failed:", e, file=sys.stderr)
        pool.down()
        return 3
    srv = Server(pool, secrets.token_hex(8))
    threading.Thread(target=srv.serve_forever, daemon=True).start()

    def ticker() -> None:
        while not srv.stop.is_set():
            try:
                pool.tick()
            except Exception as e:  # noqa: BLE001 - the loop must survive a bad tick
                print("tick error:", type(e).__name__, e, file=sys.stderr)
            srv.stop.wait(tick_s)
    threading.Thread(target=ticker, daemon=True).start()
    info_file.write_text(json.dumps({"port": srv.server_address[1], "pid": os.getpid(), "token": srv.token,
                                     "instances": ids, "fake": a.fake}))
    print(f"pool ready: instances {ids} port {srv.server_address[1]}", flush=True)
    try:
        srv.stop.wait()
    except KeyboardInterrupt:
        pass
    info_file.unlink(missing_ok=True)
    srv.shutdown()
    pool.down()
    print("pool down", flush=True)
    return 0


def main(cfg: Config, argv: List[str]) -> int:
    return run(cfg, parser().parse_args(argv))
