#!/usr/bin/env python3
"""A demo of the inspector on fake game copies. No game is started.

    python -I tools/inspect/demo.py                  # 5 refreshes, one per second
    python -I tools/inspect/demo.py --refreshes 20 --interval 0.5

It starts three fake copies on loopback (tools/bridge/fake_game.py): a host, a client, and a client that is not running. The
host and the client move and take damage. After the second refresh the client raises a message box and a sync mismatch, so
you can see the problems list fill in. The table is made by the same code as `inspect_live.py`, from examples/demo_fields.json.
"""
from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path
from typing import Any, Optional, Sequence

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "bridge"))
sys.path.insert(0, str(HERE))

import inspect_live as il  # noqa: E402
from fake_game import FakeGame  # noqa: E402


def make_copy(name: str, role: str, x: float, health: float, port: int = 0) -> FakeGame:
    g = FakeGame(name=name, vars={"health": health}, port=port, state={
        "session": {"role": role},
        "menu": {"kind": "none"},
        "player": {"pos": [x, 100.0, 8.0], "cell": 0x104C70},
        "sync": {"lastSeq": 120, "peers": [], "digests": {"ok": 0, "mismatch": 0, "repairs": 0}},
    })
    return g


def tick(game: FakeGame) -> None:
    """The game moves a little every frame."""
    if int(game.frames) % 20 == 0:
        game.state["player"]["pos"][0] += 3.0


def run(refreshes: int = 5, interval: float = 1.0, out: Any = None, fields: Optional[str] = None) -> int:
    out = out or sys.stdout
    spec = il.load_spec(fields or str(HERE / "examples" / "demo_fields.json"))
    host = make_copy("host", "host", 1000.0, 100.0)
    client = make_copy("client", "client", 1010.0, 80.0)
    host.state["sync"]["peers"] = [{"slot": 1, "acked": 120, "joined": True}]
    for g in (host, client):
        g.on_tick = tick
        g.start()
    copies = [il.CopyState("host", host.endpoint), il.CopyState("client", client.endpoint),
              il.CopyState("client2", "tcp://127.0.0.1:1")]          # nothing listens there: shown as unreachable
    insp = il.Inspector(spec, copies, connect_wait_s=0.3)
    try:
        for i in range(refreshes):
            if i == 1:                                                # something goes wrong on the client
                client.menu = "message"
                client.state["menu"]["kind"] = "message"
                client.push_event("ui.messagebox", {"text": "The save you loaded needs a newer plugin."})
                client.vars["health"] = 22.0
                client.state["sync"]["lastSeq"] = 117
                host.state["sync"]["peers"][0]["acked"] = 117
                client.state["sync"]["digests"]["mismatch"] = 1
                client.push_event("sync.desync", {"key": "quest/104C1C", "host": 20, "client": 10})
            elif i > 1:
                client.state["sync"]["lastSeq"] += 1
                host.state["sync"]["lastSeq"] += 1
            print(f"--- refresh {i + 1} of {refreshes} ---", file=out)
            print(insp.render(insp.refresh()), file=out)
            print("", file=out)
            if i + 1 < refreshes:
                out.flush()
                time.sleep(interval)
    finally:
        insp.close()
        host.stop()
        client.stop()
    return 0


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="Inspector demo on fake game copies")
    ap.add_argument("--refreshes", type=int, default=5)
    ap.add_argument("--interval", type=float, default=1.0)
    a = ap.parse_args(argv)
    return run(a.refreshes, a.interval)


if __name__ == "__main__":
    sys.exit(main())
