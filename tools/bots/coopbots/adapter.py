"""The action adapter: the only game specific code a co-op test needs.

One ActionAdapter object drives ONE running game instance (one player's machine). Playbooks call these methods and never
touch the game directly, so the same playbook runs on any game once its adapter exists, and on the fake co-op world in
fakecoop.py without a game.

How to write one for your game
------------------------------
1. Subclass ActionAdapter. Implement only what your game can do. Every method you leave out raises MissingAction, and
   `supports("take")` tells a playbook (and the runner) it is missing, so that playbook is reported SKIP, never PASS.
2. Implement the three engine methods first: `status()`, `snapshot()` and `capture()`. The runner uses them after every step.
3. Actions return a Result (ok, reason, data). Never raise for an ordinary refusal ("door is locked"): return
   Result(False, "door is locked"). Raise only when the game or the link is dead, so the run stops with a clear error.
   The base class turns whatever an action returns into an ActionResult (ok, reason, data, action, elapsed_s), so every
   action of every adapter has one result model. See results.py.
4. Check the postcondition inside the action: `take` is only ok when the item count really moved. A bot that reports
   success because the call returned is the main source of hollow passes. `self.act(do, pre=[...], post=[...])` runs a
   precondition, the action and a postcondition, and any `raise Fail("code", "what I saw")` becomes a failed result.
5. Ids ("refs") are opaque strings you choose: an editor id, a form id, a path, a name. The playbooks only pass them back.
6. Say why. A failed action reads "code: what the bot saw" (units left of a walk, the menu in the way, the weapon state).
   Override `blockers()` to list what stands in the way of input right now (an open menu, a message box, a holstered
   weapon); the base class then adds that text to every failure, and `act(..., free=True)` refuses to start while
   something blocks. `Why` in results.py words the common ones (stuck, too_close, too_steep, menu, message_box, holstered).

Reference semantics used by the playbooks
-----------------------------------------
Actions (all return Result)
    goto_ref(ref, dist=150)            walk or teleport to within `dist` of the object
    goto_pos(x, y, z, cell=None)       same for a coordinate
    equip(item)  aim_at(ref)  fire(n=1)
    activate(ref)                      use the object the way a player presses the use key
    open_door(ref)  close_door(ref)  unlock(ref, key=None)
    open_container(ref)  close_container(ref)
    take(item, count, from_ref)  put(item, count, to_ref)
    pickup(ref)                        grab a world item. Fails with "already taken" for the loser of a race
    drop(item, count)                  data["ref"] is the new world object
    talk(npc)  pick_topic(topic)  end_dialogue()
    use_device(ref, entry)             a terminal, lever panel or any menu object
    set_stage(quest, stage)  start_quest(quest)
    barrier_pass(quest)                this player finished its own part of a step that waits for everybody
    answer_prompt(button)              press a button of the open message box (0 = first)
    consent(kind, quest, yes)          answer the party's "ready" or "fast travel" consent prompt
    fast_travel(marker)  spawn(base, at)  save_game(name)  load_game(name)  session_leave()  session_join()
    set_timescale(x)                   the game's time multiplier (the runner calls it for --timescale and resets it to 1)
Queries (return values)
    cell()  distance_to(ref)  inventory_count(item, holder=None)  death_count(ref)  target_alive(ref)
    ref_enabled(ref)  in_dialogue()  spectating()  is_locked(ref)  door_open(ref)  device_entry_used(ref, entry)
    quest_stage(quest)  stage_done(quest, stage)  quest_running(quest)  objective_displayed(quest, n)
    objective_completed(quest, n)  owner_of(ref) -> (owner, epoch)  ref_count(base, cell="")
    consent_pending() -> bool  rebases() -> int
    timescale() -> float               optional: the speed the game really runs at, so the runner can check set_timescale
Engine
    status() -> {"seq": int, "synced": bool, "counters": {name: int}}   see core.py; optional "acks": {name: int}
    snapshot() -> {"shared": {...}, "per_player": {...}}                see core.py
    capture(directory) -> dict                                          optional: write logs for a failed step
Say-why hooks (all optional)
    blockers() -> [(code, text), ...]  what blocks input now        explain(action, result) -> str   text added to a failure
"""
import collections
import functools
import os
from typing import Any, Callable, Iterable, List, Sequence, Tuple

from .results import ActionFailed, ActionResult, Fail, Result, Why, ensure, now  # noqa: F401  (re-exported for adapters)

MUTANT = os.environ.get("COOPBOTS_MUTANT", "")   # test-only twin switch: nowhy | nopost


class MissingAction(NotImplementedError):
    """The adapter does not implement this action. The text says which one."""


def _stub(name, doc):
    def f(self, *args, **kwargs):
        raise MissingAction("this adapter does not implement %s" % name)
    f.__name__ = name
    f.__doc__ = doc
    f._not_implemented = True
    return f


ACTIONS = {
    "goto_ref": "goto_ref(ref, dist=150) -> Result",
    "goto_pos": "goto_pos(x, y, z, cell=None) -> Result",
    "equip": "equip(item) -> Result",
    "aim_at": "aim_at(ref) -> Result",
    "fire": "fire(n=1) -> Result; fails when nothing is aimed at",
    "activate": "activate(ref) -> Result",
    "open_door": "open_door(ref) -> Result; fails with a reason when locked",
    "close_door": "close_door(ref) -> Result",
    "unlock": "unlock(ref, key=None) -> Result",
    "open_container": "open_container(ref) -> Result; fails while another player has it open",
    "close_container": "close_container(ref) -> Result",
    "take": "take(item, count, from_ref) -> Result; ok only if the container lost and the player gained `count`",
    "put": "put(item, count, to_ref) -> Result",
    "pickup": "pickup(ref) -> Result; the loser of a race gets ok=False",
    "drop": "drop(item, count) -> Result with data['ref'] = the new world object",
    "talk": "talk(npc) -> Result",
    "pick_topic": "pick_topic(topic) -> Result",
    "end_dialogue": "end_dialogue() -> Result",
    "use_device": "use_device(ref, entry) -> Result",
    "set_stage": "set_stage(quest, stage) -> Result",
    "start_quest": "start_quest(quest) -> Result",
    "barrier_pass": "barrier_pass(quest) -> Result",
    "answer_prompt": "answer_prompt(button) -> Result",
    "consent": "consent(kind, quest='', yes=True) -> Result",
    "fast_travel": "fast_travel(marker) -> Result",
    "spawn": "spawn(base, at) -> Result",
    "save_game": "save_game(name) -> Result",
    "load_game": "load_game(name) -> Result",
    "session_leave": "session_leave() -> Result",
    "session_join": "session_join() -> Result",
    "set_timescale": "set_timescale(x) -> Result; the game's time multiplier, 1 = normal speed",
}
QUERIES = {
    "cell": "cell() -> str",
    "distance_to": "distance_to(ref) -> float",
    "inventory_count": "inventory_count(item, holder=None) -> int; holder None = this player",
    "death_count": "death_count(ref) -> int",
    "target_alive": "target_alive(ref) -> bool",
    "ref_enabled": "ref_enabled(ref) -> bool",
    "in_dialogue": "in_dialogue() -> bool",
    "spectating": "spectating() -> bool",
    "is_locked": "is_locked(ref) -> bool",
    "door_open": "door_open(ref) -> bool",
    "device_entry_used": "device_entry_used(ref, entry) -> bool",
    "quest_stage": "quest_stage(quest) -> int",
    "stage_done": "stage_done(quest, stage) -> bool",
    "quest_running": "quest_running(quest) -> bool",
    "objective_displayed": "objective_displayed(quest, n) -> bool",
    "objective_completed": "objective_completed(quest, n) -> bool",
    "owner_of": "owner_of(ref) -> (owner slot, epoch)",
    "ref_count": "ref_count(base, cell='') -> int; live objects of that base",
    "consent_pending": "consent_pending() -> bool",
    "rebases": "rebases() -> int; how often this client had to adopt the host's world",
    "timescale": "timescale() -> float; the speed the game runs at now (optional; lets the runner check set_timescale)",
}


ENGINE = {
    "status": "status() -> {'seq': int, 'synced': bool, 'counters': {name: int}}. seq: number of the last host update this "
              "machine applied (the host reports the last one it sent). synced: host only, False while any client is behind. "
              "counters: desync counters, see core.REPAIR_COUNTERS and core.HARD_COUNTERS. Optional 'acks': {client: last "
              "update number the host knows it has}, which lets a stuck wait say who is behind",
    "snapshot": "snapshot() -> {'shared': {...}, 'per_player': {...}}. `shared` must be equal on every machine once synced",
}


def _coerce(name: str, out: Any) -> ActionResult:
    if isinstance(out, ActionResult):
        return out
    if isinstance(out, Result):
        return ActionResult.of(out, name)
    if isinstance(out, bool):
        return ActionResult(out, "" if out else "the action returned False", {}, name)
    raise TypeError("action %s returned %r; return a Result (ok, reason, data)" % (name, out))


def _wrap_action(name: str, fn: Callable) -> Callable:
    @functools.wraps(fn)
    def action(self, *args, **kwargs):
        t0 = now()
        try:
            res = _coerce(name, fn(self, *args, **kwargs))
        except Fail as f:
            res = ActionResult(False, f.reason, dict(f.data), name)
        res.action = res.action or name
        res.elapsed_s = res.elapsed_s or (now() - t0)
        if not res.ok and not getattr(res, "_explained", False):
            res._explained = True
            self._say_why(name, res)
        self._record(res)
        if self.strict and not res.ok:
            raise ActionFailed(res)
        return res
    action._action_wrapped = True
    return action


class ActionAdapter:
    """Base class. See the module docstring."""

    name = "bot"
    strict = False                    # True: a failed action raises ActionFailed instead of returning its result

    # status() and snapshot() are stubs too (see ENGINE above); the runner uses them after every step.

    def __init_subclass__(cls, **kw):
        super().__init_subclass__(**kw)
        for n in ACTIONS:
            f = cls.__dict__.get(n)
            if callable(f) and not getattr(f, "_not_implemented", False) and not getattr(f, "_action_wrapped", False):
                setattr(cls, n, _wrap_action(n, f))

    def capture(self, directory):
        """Optional: write logs or dumps for a failed step into `directory`; return a small dict describing them."""
        return {}

    def supports(self, action):
        m = getattr(self, action, None)
        return callable(m) and not getattr(getattr(m, "__func__", m), "_not_implemented", False)

    # ---- the action result model ----
    def blockers(self) -> List[Tuple[str, str]]:
        """What stands in the way of input right now, as [(code, text), ...]: ("menu", "the inventory menu is open"),
        ("message_box", "a message box is up"), ("holstered", "the weapon is holstered"). Default: nothing is known."""
        return []

    def require_free(self, ignore: Iterable[str] = ()) -> None:
        """Fail with the first blocker (all of them in the text) unless its code is in `ignore`."""
        skip = set(ignore)
        bl = [b for b in self.blockers() if b[0] not in skip]
        if bl:
            raise Fail(bl[0][0], "; ".join(t for _, t in bl))

    def explain(self, action: str, result: ActionResult) -> str:
        """Text added to every failed result so the reason names the cause. Default: the current blockers."""
        return "; ".join(t for _, t in self.blockers())

    def _say_why(self, action: str, result: ActionResult) -> None:
        if MUTANT == "nowhy":
            return
        try:
            note = self.explain(action, result)
        except Exception:  # noqa: BLE001  a broken explain hook must not hide the real failure
            return
        if note and note not in result.reason:
            result.reason += ("; " if ": " in result.reason else ": ") + note

    def _record(self, res: ActionResult) -> None:
        log = self.__dict__.get("_action_log")
        if log is None:
            log = self._action_log = collections.deque(maxlen=200)
        log.append(res)

    def recent_actions(self, n: int = 8) -> List[dict]:
        """The last n actions of this adapter as plain dicts (the runner keeps them in failure.json)."""
        return [r.to_json() for r in list(self.__dict__.get("_action_log") or [])[-n:]]

    def act(self, do: Callable[[], Any], *, pre: Sequence[Callable[[], Any]] = (), post: Sequence[Callable[[dict], Any]] = (),
            free: bool = False, ignore: Iterable[str] = ()) -> ActionResult:
        """Run an action with its checks: `free` (nothing may block input), then each `pre()`, then `do()`, then each `post(data)`.
        A check fails by raising Fail (or by `ensure(cond, code, say)`); the phase of the failed check is added to the data as
        "phase": "pre", "do" or "post". `do()` returns a dict, a Result or None; a failed Result is passed on without running `post`."""
        phase = "pre"
        try:
            if free:
                self.require_free(ignore)
            for p in pre:
                p()
            phase = "do"
            out = do()
            if isinstance(out, Result) and not out.ok:
                return ActionResult.of(out, "")
            data = dict(out.data) if isinstance(out, Result) else dict(out or {})
            phase = "post"
            if MUTANT != "nopost":
                for q in post:
                    q(data)
        except Fail as f:
            f.data.setdefault("phase", phase)
            raise
        return ActionResult(True, "", data)


for _n, _d in {**ENGINE, **ACTIONS, **QUERIES}.items():
    setattr(ActionAdapter, _n, _stub(_n, _d))
