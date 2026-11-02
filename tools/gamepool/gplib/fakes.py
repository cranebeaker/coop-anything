"""In-memory stand-ins for the pool's OS layer. Used by the tests and by `pool up --fake` (a pool that starts
nothing and opens no pipes, handy for trying the lease commands)."""
from __future__ import annotations

from typing import Dict, List


class FakeSlots:
    def __init__(self, available=(0, 1)):
        self.available, self.held, self.pids, self.renewed = list(available), [], [], 0

    def acquire(self, n: int) -> List[int]:
        self.held = self.available[:n]
        return list(self.held)

    def renew(self, n: int) -> None:
        self.renewed += 1

    def register_pid(self, pid: int) -> None:
        self.pids.append(pid)

    def release(self) -> None:
        self.held = []


class FakeBackend:
    """Instances become 'processes' in a dict. Tests inject deaths and RAM growth. `foreign` holds game
    processes that belong to someone else (the owner's own game, another agent): they must never be killed."""

    def __init__(self):
        self.procs: Dict[int, int] = {}      # pid -> working set MB
        self.foreign = {900001, 900002}
        self.mine: set = set()
        self.pid_of: Dict[int, int] = {}
        self.next_pid = 1000
        self.starts: List[int] = []
        self.killed: List[int] = []
        self.unlocked: List[int] = []
        self.prepared: List[int] = []
        self.fail_start = set()              # instance ids whose start fails
        self.unresponsive = set()            # instance ids whose game does not answer the probe
        self.boot_ws = 1200

    def prepare(self, i: int) -> None:
        self.prepared.append(i)

    def start(self, i: int) -> int:
        self.starts.append(i)
        self.next_pid += 1
        self.procs[self.next_pid] = self.boot_ws
        self.mine.add(self.next_pid)
        self.pid_of[i] = self.next_pid
        return self.next_pid

    def alive(self, pid: int) -> bool:
        return pid in self.procs or pid in self.foreign

    def ping(self, i: int) -> bool:
        return i not in self.unresponsive and self.pid_of.get(i, 0) in self.procs

    def wait_ready(self, i: int, pid: int) -> bool:
        return i not in self.fail_start and pid in self.procs

    def stats(self, pid: int) -> Dict:
        return {"ws_mb": self.procs.get(pid, 0)}

    def kill(self, pid: int) -> None:
        if pid in self.mine:                 # a real backend refuses pids it did not start
            self.procs.pop(pid, None)
            self.killed.append(pid)

    def unlock(self, i: int) -> None:
        self.unlocked.append(i)

    # ---- test levers
    def crash(self, i: int) -> None:
        self.procs.pop(self.pid_of[i], None)

    def grow(self, i: int, ws_mb: int) -> None:
        self.procs[self.pid_of[i]] = ws_mb

    def all_game_pids(self) -> List[int]:
        return list(self.procs) + sorted(self.foreign)

    def kill_any(self, pid: int) -> None:    # only a broken (twin) pool would call this
        self.procs.pop(pid, None)
        self.foreign.discard(pid)
        self.killed.append(pid)
