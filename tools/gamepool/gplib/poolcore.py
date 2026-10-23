"""Warm game pool: the state machine. No game, socket or process code lives here. A Backend and a Slots
object (backend.py) do the OS work, so the whole policy is testable with in-memory fakes.

Why a warm pool: starting a big game takes minutes. A test that needs a running game waits for that every
time. The pool keeps N copies running (minimized, cheap settings) and leases them to tests.

Rules
  * N warm instances. Instance id == game slot index, so ports and pipe names never collide with other agents.
  * A lease hands out whole instances; nobody else gets them until release or ttl expiry.
  * An instance is recycled (killed and started again) after K leases, when its RAM grew too much, when the
    lessee marked it dirty, when its lease expired, or when the health check finds it dead.
  * A dead instance is never handed to a lessee.
  * Only pids this pool launched (and recorded) are ever killed.
"""
from __future__ import annotations

import json
import threading
import time
from pathlib import Path
from typing import Callable, Dict, List, Optional


class PoolError(Exception):
    pass


class PoolBusy(PoolError):
    """No free warm instance within the timeout."""


class Inst:
    def __init__(self, i: int):
        self.id = i
        self.pid = 0
        self.state = "down"          # down | starting | warm | leased
        self.tests = 0               # leases completed since the last (re)start
        self.gen = 0                 # +1 on every (re)start; a lessee compares it to detect a restart
        self.base_ws_mb = 0
        self.lease: Optional[str] = None
        self.fails = 0               # consecutive failed starts
        self.last_error = ""
        self.recycled_for = ""

    def view(self) -> Dict:
        return {"id": self.id, "pid": self.pid, "state": self.state, "tests": self.tests, "gen": self.gen,
                "base_ws_mb": self.base_ws_mb, "lease": self.lease, "fails": self.fails,
                "last_error": self.last_error, "recycled_for": self.recycled_for}


class Pool:
    def __init__(self, backend, slots, size: int = 1, *, recycle_after: int = 25, ws_growth_mb: int = 1200,
                 ws_max_mb: int = 3300, slot_ttl_s: int = 3600, clock: Callable[[], float] = time.monotonic,
                 sync: bool = False, state_path: Optional[Path] = None, max_start_fails: int = 3):
        self.b, self.slots = backend, slots
        self.size = max(1, int(size))
        self.recycle_after, self.ws_growth_mb, self.ws_max_mb = recycle_after, ws_growth_mb, ws_max_mb
        self.slot_ttl_s, self.clock, self.sync = slot_ttl_s, clock, sync
        self.state_path, self.max_start_fails = state_path, max_start_fails
        self.cv = threading.Condition()
        self.insts: Dict[int, Inst] = {}
        self.leases: Dict[str, Dict] = {}
        self._next = 0
        self._slot_renewed = clock()
        self.stats = {"leases": 0, "recycles": 0, "restarts": 0, "expired": 0}

    # ------------------------------------------------------------ persistence (pids we launched, for reaping)
    def _save(self) -> None:
        if self.state_path:
            self.state_path.parent.mkdir(parents=True, exist_ok=True)
            self.state_path.write_text(json.dumps({"pids": {str(i.id): i.pid for i in self.insts.values() if i.pid},
                                                   "stats": self.stats}))

    def reap_previous(self) -> List[int]:
        """Kill pids a previous pool run recorded (if the backend still sees them as ours). Never anything else."""
        killed: List[int] = []
        if self.state_path and self.state_path.exists():
            try:
                old = json.loads(self.state_path.read_text()).get("pids", {})
            except ValueError:
                old = {}
            for pid in old.values():
                if self.b.alive(int(pid)):
                    self.b.kill(int(pid))
                    killed.append(int(pid))
            self.state_path.unlink()
        return killed

    # ------------------------------------------------------------ start / stop
    def up(self) -> List[int]:
        self.reap_previous()
        ids = self.slots.acquire(self.size)
        if len(ids) < self.size:
            raise PoolError(f"wanted {self.size} game slots, got {ids}")
        with self.cv:
            for i in ids:
                self.insts[i] = Inst(i)
        for i in ids:                     # strictly one after the other
            self._start(self.insts[i], "up")
        bad = [i for i in ids if self.insts[i].state != "warm"]
        if bad:
            raise PoolError("instances failed to start: " + "; ".join(f"{i}: {self.insts[i].last_error}" for i in bad))
        return ids

    def _start(self, inst: Inst, reason: str) -> bool:
        """Launch inst and wait until it is ready. Caller must not hold self.cv."""
        with self.cv:
            inst.state = "starting"
            inst.recycled_for = reason
        try:
            self.b.prepare(inst.id)
            pid = self.b.start(inst.id)
            with self.cv:
                inst.pid = pid
                self._save()
            self.slots.register_pid(pid)
            if not self.b.wait_ready(inst.id, pid):
                raise PoolError("the game did not become ready before the boot timeout")
            ws = self.b.stats(pid).get("ws_mb", 0)
        except Exception as e:  # noqa: BLE001 - any launch problem is a failed start
            if inst.pid:
                self.b.kill(inst.pid)
            with self.cv:
                inst.pid, inst.state = 0, "down"
                inst.fails += 1
                inst.last_error = f"{type(e).__name__}: {e}"
                self._save()
                self.cv.notify_all()
            return False
        with self.cv:
            inst.base_ws_mb, inst.tests, inst.fails, inst.last_error = ws, 0, 0, ""
            inst.gen += 1
            inst.state = "leased" if inst.lease else "warm"
            self._save()
            self.cv.notify_all()
        return True

    def _kill_inst(self, inst: Inst) -> None:
        if inst.pid:
            self.b.kill(inst.pid)
        with self.cv:
            inst.pid = 0
            self._save()

    def _recycle(self, inst: Inst, reason: str) -> None:
        self.stats["recycles"] += 1
        self._kill_inst(inst)
        run = lambda: self._start(inst, reason)  # noqa: E731
        if self.sync:
            run()
        else:
            with self.cv:
                inst.state = "starting"
            threading.Thread(target=run, daemon=True).start()

    def down(self) -> List[int]:
        killed = []
        with self.cv:
            insts = list(self.insts.values())
        for inst in insts:
            if inst.pid:
                killed.append(inst.pid)
            self._kill_inst(inst)
            inst.state = "down"
        with self.cv:
            self.leases.clear()
            self.cv.notify_all()
        self.slots.release()
        if self.state_path:
            self.state_path.unlink(missing_ok=True)
        return killed

    # ------------------------------------------------------------ leasing
    def _pick_free(self, n: int) -> List[Inst]:
        """The n lowest-numbered warm (idle, ready) instances, or fewer if not enough are free."""
        free = sorted((i for i in self.insts.values() if i.state == "warm"), key=lambda i: i.id)
        return free[:n]

    def lease(self, owner: str, n: int = 1, ttl_s: float = 900, timeout_s: float = 600) -> Dict:
        if n < 1 or n > self.size:
            raise PoolError(f"lease of {n} instance(s) impossible with a pool of {self.size}")
        deadline = time.monotonic() + timeout_s
        with self.cv:
            while True:
                if self.insts and all(i.state == "down" and i.fails >= self.max_start_fails for i in self.insts.values()):
                    raise PoolError("all instances failed to start: " + "; ".join(i.last_error for i in self.insts.values()))
                picked = self._pick_free(n)
                if len(picked) >= n:
                    self._next += 1
                    lid = f"L{self._next}"
                    for i in picked:
                        i.state, i.lease = "leased", lid
                    self.leases[lid] = {"id": lid, "owner": owner, "instances": [i.id for i in picked],
                                        "gens": {str(i.id): i.gen for i in picked},
                                        "ttl_s": ttl_s, "expires": self.clock() + ttl_s, "dirty": {}}
                    self.stats["leases"] += 1
                    break
                left = deadline - time.monotonic()
                if left <= 0:
                    raise PoolBusy(f"no {n} warm instance(s) free for {owner} within {timeout_s}s: " + json.dumps(
                        [i.view() for i in self.insts.values()]))
                self.cv.wait(min(left, 1.0))
            out = dict(self.leases[lid])
        # health check outside the lock: a dead warm instance is restarted before the lessee sees it
        for i in picked:
            if not (self.b.alive(i.pid) and self.b.ping(i.id)):
                self.stats["restarts"] += 1
                self._kill_inst(i)
                if not self._start(i, "dead at lease"):
                    self.release(lid, dirty_all=True, reason="start failed")
                    raise PoolError(f"instance {i.id} would not start: {i.last_error}")
        with self.cv:
            out["gens"] = {str(i.id): i.gen for i in picked}
            self.leases[lid]["gens"] = dict(out["gens"])
        return out

    def renew(self, lid: str, ttl_s: Optional[float] = None) -> bool:
        with self.cv:
            l = self.leases.get(lid)
            if not l:
                return False
            l["expires"] = self.clock() + (ttl_s or l["ttl_s"])
            return True

    def mark_dirty(self, lid: str, inst_id: int, reason: str) -> bool:
        with self.cv:
            l = self.leases.get(lid)
            if not l or inst_id not in l["instances"]:
                return False
            l["dirty"][str(inst_id)] = reason
            return True

    def _recycle_reason(self, inst: Inst, dirty: str = "") -> str:
        if dirty:
            return "dirty: " + dirty
        if inst.tests >= self.recycle_after:
            return f"{inst.tests} tests >= {self.recycle_after}"
        ws = self.b.stats(inst.pid).get("ws_mb", 0) if inst.pid else 0
        if ws and (ws > inst.base_ws_mb + self.ws_growth_mb or ws > self.ws_max_mb):
            return f"memory {ws} MB (base {inst.base_ws_mb} MB)"
        return ""

    def release(self, lid: str, dirty_all: bool = False, reason: str = "") -> bool:
        with self.cv:
            l = self.leases.pop(lid, None)
        if not l:
            return False
        for iid in l["instances"]:
            inst = self.insts[iid]
            self.b.unlock(iid)
            inst.tests += 1
            dirty = l["dirty"].get(str(iid), "") or (reason or "released dirty" if dirty_all else "")
            why = self._recycle_reason(inst, dirty)
            with self.cv:
                inst.lease = None
                if inst.state == "leased":     # "starting"/"down" (restart in progress or failed) stay as they are
                    inst.state = "warm"
            if why and inst.state == "warm":
                self._recycle(inst, why)
            with self.cv:
                self.cv.notify_all()
        return True

    # ------------------------------------------------------------ periodic work
    def _reap_expired(self, now: float) -> None:
        with self.cv:
            dead = [lid for lid, l in self.leases.items() if l["expires"] <= now]
        for lid in dead:
            self.stats["expired"] += 1
            self.release(lid, dirty_all=True, reason="lease expired")

    def tick(self) -> None:
        now = self.clock()
        self._reap_expired(now)
        with self.cv:
            snapshot = [i for i in self.insts.values() if i.state in ("warm", "leased")]
        for inst in snapshot:
            if not self.b.alive(inst.pid):
                self.stats["restarts"] += 1
                self._kill_inst(inst)
                self._recycle_start(inst, "died")
            elif inst.state == "warm":
                if not self.b.ping(inst.id):
                    self.stats["restarts"] += 1
                    self._kill_inst(inst)
                    self._recycle_start(inst, "game unresponsive")
                else:
                    why = self._recycle_reason(inst)
                    if why:
                        self._recycle(inst, why)
        with self.cv:   # a failed start is retried until max_start_fails
            retry = [i for i in self.insts.values() if i.state == "down" and 0 < i.fails < self.max_start_fails]
        for inst in retry:
            self._recycle_start(inst, "retry after failed start")
        if now - self._slot_renewed > self.slot_ttl_s / 3:
            self.slots.renew(self.size)
            self._slot_renewed = now

    def _recycle_start(self, inst: Inst, reason: str) -> None:
        if self.sync:
            self._start(inst, reason)
        else:
            with self.cv:
                inst.state = "starting"
            threading.Thread(target=self._start, args=(inst, reason), daemon=True).start()

    def status(self) -> Dict:
        with self.cv:
            return {"size": self.size, "recycle_after": self.recycle_after, "stats": dict(self.stats),
                    "instances": [i.view() for i in sorted(self.insts.values(), key=lambda i: i.id)],
                    "leases": [dict(l, remaining_s=round(l["expires"] - self.clock(), 1)) for l in self.leases.values()]}
