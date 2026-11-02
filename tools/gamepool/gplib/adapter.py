"""Adapter: the only place where the pool talks to the game itself.

The pool needs four answers from a running copy: "are you ready?", "are you still healthy?", "something to do
once ready" (load a save, say), and "the lease is over, clean up". The default adapter answers the first two
from a probe described in the config (ready.type):

    process     the process is alive (settle_s seconds after launch)
    window      the process has a visible top-level window
    file        a file exists, path relative to the instance folder, {i} is the instance number
    log_line    a file contains a text (ready.path, ready.text)
    tcp_port    127.0.0.1:(base_port + i) accepts a connection
    named_pipe  the named pipe ready.name exists, {i} is the instance number

Games with an in-game bridge (a script plugin that answers pings, loads saves, runs test steps) write a few
lines of Python instead: set "adapter": "my_adapter.py:MyAdapter" in the config (path relative to the config
file), subclass Adapter, and override what you need. The adapter file is your own code and runs with your
rights, like any script you run.

    class MyAdapter(Adapter):
        def probe(self, i, pid):            # healthy right now?
            return my_bridge_ping(i)
        def after_ready(self, i, pid):      # runs once after each (re)start
            my_bridge_load_save(i, "base")
        def on_release(self, i):            # a lease ended; drop any game side lock
            my_bridge_unlock(i)

A game with a hot reload tool (swap new code into a running copy, no restart) adds two more hooks. `gamepool.py swap` calls
them and does nothing else game specific:

        def can_swap(self, i, pid, payload):   # None when copy i can be swapped, else the reason it cannot
            return None if my_tool_can_reload(i) else "no reload pipe"
        def swap(self, i, pid, payload):       # do the swap; raise or return {"ok": False, "detail": ...} on failure
            return my_tool_reload(i, **payload)
"""
from __future__ import annotations

import importlib.util
import time
from pathlib import Path
from typing import Callable, Dict, Optional

from . import winproc
from .config import Config, ConfigError


class Adapter:
    def __init__(self, cfg: Config):
        self.cfg = cfg
        self.ready = cfg["ready"]

    # ---- hooks a custom adapter may override
    def prepare(self, i: int) -> None:
        """Before every launch of instance i. The default removes a stale ready file (ready.type 'file'),
        so a copy that is started again is not taken for ready because of the previous run's file."""
        if self.ready["type"] == "file":
            (self.cfg.instance_dir(i) / self.cfg.fmt(self.ready["path"], i)).unlink(missing_ok=True)

    def probe(self, i: int, pid: int) -> bool:
        """True when instance i (process pid) is ready and healthy right now."""
        return probe(self.cfg, i, pid)

    def after_ready(self, i: int, pid: int) -> None:
        """After the instance became ready (every start and restart)."""

    def on_release(self, i: int) -> None:
        """A lease on instance i ended."""

    def can_swap(self, i: int, pid: int, payload: Dict[str, str]) -> Optional[str]:
        """None when copy i can have new code swapped in while it runs, else the reason it cannot. The default says it cannot,
        so `swap` refuses before touching any copy unless an adapter provides the hook."""
        return "this adapter has no swap hook (override can_swap and swap in your adapter)"

    def swap(self, i: int, pid: int, payload: Dict[str, str]) -> Optional[Dict]:
        """Swap new code into the running copy i. payload holds the --arg KEY=VALUE pairs of the command line. Raise an exception,
        or return a dict with "ok": False and a "detail", to report a failure. A returned dict is shown in the report."""
        raise NotImplementedError("this adapter has no swap hook")

    # ---- driven by the backend
    def wait_ready(self, i: int, pid: int, alive: Callable[[int], bool], timeout_s: float = 0.0) -> bool:
        limit = timeout_s or float(self.ready.get("timeout_s", 300))
        t0 = time.monotonic()
        settle = float(self.ready.get("settle_s", 0))
        while time.monotonic() - t0 < limit:
            if not alive(pid):
                return False
            if self.probe(i, pid):
                if settle and time.monotonic() - t0 < settle:
                    time.sleep(min(0.5, settle))
                    continue
                self.after_ready(i, pid)
                return True
            time.sleep(float(self.ready.get("poll_s", 1.0)))
        return False


def probe(cfg: Config, i: int, pid: int) -> bool:
    r = cfg["ready"]
    t = r["type"]
    if t == "process":
        return winproc.pid_alive(pid)
    if t == "window":
        wins = winproc.windows_of({pid})
        need = r.get("title_contains", "")
        return any(need.lower() in title.lower() for _, _, title, _ in wins) if wins else False
    if t in ("file", "log_line"):
        p = cfg.instance_dir(i) / cfg.fmt(r["path"], i)
        if not p.exists():
            return False
        if t == "file":
            return True
        try:
            return r["text"] in p.read_text(errors="replace")
        except OSError:
            return False
    if t == "tcp_port":
        return winproc.tcp_open(int(r["base_port"]) + int(i))
    if t == "named_pipe":
        return winproc.pipe_exists(cfg.fmt(r["name"], i))
    raise ConfigError(f"unknown ready.type {t!r}")


def load_adapter(cfg: Config) -> Adapter:
    spec = cfg["adapter"]
    if not spec:
        return Adapter(cfg)
    path, _, cls = spec.partition(":")
    f = Path(path)
    if not f.is_absolute():
        f = cfg.base_dir / f
    if not f.is_file() or not cls:
        raise ConfigError(f"adapter must be 'file.py:ClassName' pointing at an existing file, got {spec!r}")
    s = importlib.util.spec_from_file_location("gamepool_user_adapter", f)
    mod = importlib.util.module_from_spec(s)
    s.loader.exec_module(mod)
    ad = getattr(mod, cls)(cfg)
    if not isinstance(ad, Adapter):
        raise ConfigError(f"{cls} must subclass gplib.adapter.Adapter")
    return ad
