"""Glitch detectors on ONE machine's probe stream (one actor's time-ordered samples at a time).

Every detector returns a list of Event. Detectors are pure functions of (samples, Thresholds, Profile). Each has a
mutation twin (config.mutant() == '<kind>') that breaks it in one specific way; the tests require the twin to be caught.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Callable, Dict, List, Optional, Sequence, Tuple

from .config import DEFAULT, Thresholds, mutant
from .model import Actor, horizontal_speed
from .profile import AIRBORNE, ALIVE_ONLY, DEFAULT_PROFILE, LOCOMOTION, Profile

KINDS = ("sliding", "tpose", "floating", "sinking", "jitter", "rubberband", "stuck", "deathpose")


@dataclass
class Event:
    kind: str
    ref: int
    t0: float
    t1: float
    value: float = 0.0
    detail: str = ""
    machine: str = ""

    def as_dict(self) -> dict:
        return {"kind": self.kind, "ref": f"0x{self.ref:08X}", "machine": self.machine, "t0_ms": round(self.t0, 1),
                "t1_ms": round(self.t1, 1), "value": round(self.value, 2), "detail": self.detail}


def _cats(a: Actor, p: Profile) -> set:
    return {p.category(s.group) for s in a.animating()}


def _speeds(s: Sequence[Actor], win_ms: float = 200.0) -> List[float]:
    """Horizontal speed in units/s measured over about `win_ms` (net displacement, so back-and-forth jitter is ~0)."""
    sp = [0.0] * len(s)
    j = 0
    for i in range(1, len(s)):
        while j + 1 < i and s[i].t - s[j + 1].t >= win_ms:
            j += 1
        k = j if s[i].t - s[j].t >= win_ms * 0.5 else i - 1
        sp[i] = horizontal_speed(s[k], s[i])
    if len(s) > 1:
        sp[0] = sp[1]
    return sp


def _streaks(s: Sequence[Actor], pred: Callable[[int], bool], min_ms: float, gap_ms: float) -> List[Tuple[int, int]]:
    """Index ranges [i0, i1] of consecutive samples (no gap over gap_ms) where pred holds and the span is >= min_ms."""
    out = []
    i = 0
    n = len(s)
    while i < n:
        if not pred(i):
            i += 1
            continue
        j = i
        while j + 1 < n and pred(j + 1) and s[j + 1].t - s[j].t <= gap_ms:
            j += 1
        if s[j].t - s[i].t >= min_ms:
            out.append((i, j))
        i = j + 1
    return out


def _usable(a: Actor) -> bool:
    return a.valid3d   # the probe must report the full third-person body, also for a player in first person


# ---------------------------------------------------------------------------------------------------------------
def detect_sliding(s: Sequence[Actor], th: Thresholds = DEFAULT, profile: Optional[Profile] = None) -> List[Event]:
    p = profile or DEFAULT_PROFILE
    sp = _speeds(s)
    ev: List[Event] = []
    mut = mutant() == "sliding"

    def moving_no_loco(i: int) -> bool:
        a = s[i]
        if not _usable(a) or not a.have_anim or a.dead or a.swimming or a.knocked or a.ragdoll:
            return False
        if i > 0 and (s[i - 1].cell != a.cell):
            return False
        if sp[i] > th.rubber_speed:
            return False  # a teleport, not a slide (the rubber band detector owns that)
        cats = _cats(a, p)
        if cats & (LOCOMOTION | AIRBORNE):
            return False
        if mut:
            return False  # mutant: never fires
        return sp[i] > th.slide_min_speed

    for i0, i1 in _streaks(s, moving_no_loco, th.slide_ms, th.gap_ms):
        peak = max(sp[i0:i1 + 1])
        ev.append(Event("sliding", s[i0].ref, s[i0].t, s[i1].t, peak, f"moving {peak:.0f} u/s with no locomotion animation"))

    def out_of_band(i: int) -> Optional[str]:
        a = s[i]
        if not _usable(a) or not a.have_anim or a.dead or a.swimming or a.knocked or a.ragdoll:
            return None
        if sp[i] > th.rubber_speed:
            return None
        cats = _cats(a, p)
        for c in ("run", "walk"):  # the faster animation wins when both play
            if c in cats:
                lo, hi = th.loco_bands[c]
                if sp[i] > hi:
                    return f"{c} animation but {sp[i]:.0f} u/s (max {hi:.0f})"
                if c == "run" and sp[i] < lo:
                    return f"run animation but only {sp[i]:.0f} u/s (min {lo:.0f})"
                return None
        return None

    for i0, i1 in _streaks(s, lambda i: out_of_band(i) is not None, th.band_ms, th.gap_ms):
        ev.append(Event("sliding", s[i0].ref, s[i0].t, s[i1].t, max(sp[i0:i1 + 1]), out_of_band(i0) or ""))
    return ev


_FALLBACK_ARM_ROLES = {"left_upper_arm": "upper_arm_l", "left_hand": "hand_l",
                       "right_upper_arm": "upper_arm_r", "right_hand": "hand_r"}


def _arms_out(a: Actor, th: Thresholds, p: Profile) -> bool:
    ok = 0
    for side in ("left", "right"):
        if mutant() == "profile_bones":   # mutant: ignores the profile's bone names
            up_name, hand_name = _FALLBACK_ARM_ROLES[f"{side}_upper_arm"], _FALLBACK_ARM_ROLES[f"{side}_hand"]
        else:
            up_name, hand_name = p.bone(f"{side}_upper_arm"), p.bone(f"{side}_hand")
        up = a.bones.get(up_name)
        hand = a.bones.get(hand_name)
        if not up or not hand:
            return False
        if abs(hand[2] - up[2]) <= th.tpose_arm_dz and math.hypot(hand[0] - up[0], hand[1] - up[1]) >= th.tpose_arm_span:
            ok += 1
    return ok == 2


def detect_tpose(s: Sequence[Actor], th: Thresholds = DEFAULT, profile: Optional[Profile] = None) -> List[Event]:
    p = profile or DEFAULT_PROFILE
    mut = mutant() == "tpose"

    def bad(i: int) -> bool:
        a = s[i]
        if not _usable(a) or not a.have_anim or a.dead or a.ragdoll or a.knocked:
            return False
        if mut:
            return False
        return not a.animating() or _arms_out(a, th, p)

    out = []
    for i0, i1 in _streaks(s, bad, th.tpose_ms, th.gap_ms):
        why = "no sequence running" if not s[i0].animating() else "arms straight out (bind pose)"
        out.append(Event("tpose", s[i0].ref, s[i0].t, s[i1].t, s[i1].t - s[i0].t, why))
    return out


def detect_floating(s: Sequence[Actor], th: Thresholds = DEFAULT, profile: Optional[Profile] = None) -> List[Event]:
    """Floating and sinking, from the tracked ground height (a probe-provided ground_z when present, else the last
    grounded z). Falling, jumping, swimming, knocked-down and ragdoll samples are never judged."""
    p = profile or DEFAULT_PROFILE
    mut = mutant() == "floating"
    ground: Optional[float] = None
    state: List[int] = []   # 0 ok, +1 floating, -1 sinking, 2 not judged
    vz = [0.0] * len(s)
    for i in range(1, len(s)):
        if s[i].t > s[i - 1].t:
            vz[i] = abs(s[i].pos[2] - s[i - 1].pos[2]) / ((s[i].t - s[i - 1].t) / 1000.0)
    for i, a in enumerate(s):
        teleport = i > 0 and (s[i - 1].cell != a.cell or math.dist(s[i - 1].pos, a.pos) > th.rubber_dist * 3)
        # falling = fast vertical movement on two samples in a row; a single big step is a discontinuity, not a fall
        falling = i > 0 and vz[i] > th.fall_vz and vz[i - 1] > th.fall_vz
        airborne = (bool(_cats(a, p) & AIRBORNE) or falling or a.swimming or a.knocked > 0 or a.ragdoll or
                    a.dead or not _usable(a))
        if a.ground_z is not None:
            ground = a.ground_z
        if ground is None or teleport or airborne:
            ground = a.ground_z if a.ground_z is not None else a.pos[2]
            state.append(2 if airborne else 0)
            continue
        d = a.pos[2] - ground
        if abs(d) <= th.ground_step or mut:
            ground = a.pos[2] if a.ground_z is None else ground
            state.append(0)
        else:
            state.append(1 if d > 0 else -1)
    out = []
    for sign, kind in ((1, "floating"), (-1, "sinking")):
        for i0, i1 in _streaks(s, lambda i, sg=sign: state[i] == sg, th.float_ms, th.gap_ms):
            out.append(Event(kind, s[i0].ref, s[i0].t, s[i1].t, abs(s[i1].pos[2] - s[max(i0 - 1, 0)].pos[2]),
                             f"z off the ground by {abs(s[i1].pos[2] - s[max(i0 - 1, 0)].pos[2]):.0f} units with no jump/fall"))
    return out


def detect_jitter(s: Sequence[Actor], th: Thresholds = DEFAULT, profile: Optional[Profile] = None) -> List[Event]:
    mut = mutant() == "jitter"
    w = th.jitter_window
    n = len(s)
    flagged = [False] * n
    for start in range(0, max(0, n - w + 1)):
        win = s[start:start + w]
        if any(win[k + 1].t - win[k].t > th.gap_ms for k in range(w - 1)):
            continue
        if any(a.ragdoll or a.dead or not a.valid3d for a in win):
            continue
        best = 0
        for axis in (0, 1, 2):
            steps = [win[k + 1].pos[axis] - win[k].pos[axis] for k in range(w - 1)]
            big = [x for x in steps if abs(x) >= th.jitter_amp]
            rev = sum(1 for k in range(len(big) - 1) if (big[k] > 0) != (big[k + 1] > 0))
            best = max(best, rev)
        if mut:
            best = 0
        if best >= th.jitter_reversals:
            for k in range(start, start + w):
                flagged[k] = True
    out = []
    for i0, i1 in _streaks(s, lambda i: flagged[i], 0.0, th.gap_ms):
        out.append(Event("jitter", s[i0].ref, s[i0].t, s[i1].t, float(i1 - i0 + 1), "position oscillates sample to sample"))
    return out


def detect_rubberband(s: Sequence[Actor], th: Thresholds = DEFAULT, profile: Optional[Profile] = None) -> List[Event]:
    mut = mutant() == "rubberband"
    out = []
    for i in range(1, len(s)):
        a, b = s[i - 1], s[i]
        dt = b.t - a.t
        if dt <= 0 or dt > th.gap_ms or a.cell != b.cell:
            continue
        d = math.dist(a.pos, b.pos)
        if mut:
            continue
        if d > th.rubber_dist and d / (dt / 1000.0) > th.rubber_speed:
            snap = any(math.dist(s[k].pos, a.pos) < th.rubber_dist * 0.3 for k in range(i + 1, len(s))
                       if s[k].t - b.t <= th.snapback_ms)
            out.append(Event("rubberband", b.ref, a.t, b.t, d, f"jumped {d:.0f} units in {dt:.0f} ms" + (" and snapped back" if snap else "")))
    return out


def detect_stuck(s: Sequence[Actor], th: Thresholds = DEFAULT, expect: Sequence[Tuple[float, float]] = (),
                 profile: Optional[Profile] = None) -> List[Event]:
    """The actor should be moving (the caller says so with `expect` time windows: a test knows when it sent a bot
    somewhere) but stands still on an idle animation."""
    if not expect:
        return []
    p = profile or DEFAULT_PROFILE
    mut = mutant() == "stuck"
    sp = _speeds(s)

    def stuck(i: int) -> bool:
        a = s[i]
        if not _usable(a) or a.dead or not a.have_anim or mut:
            return False
        if not any(t0 <= a.t <= t1 for t0, t1 in expect):
            return False
        return sp[i] < th.stuck_speed and not (_cats(a, p) & LOCOMOTION)

    return [Event("stuck", s[i0].ref, s[i0].t, s[i1].t, s[i1].t - s[i0].t, "expected to move but stands on an idle animation")
            for i0, i1 in _streaks(s, stuck, th.stuck_ms, th.gap_ms)]


def detect_deathpose(s: Sequence[Actor], th: Thresholds = DEFAULT, profile: Optional[Profile] = None) -> List[Event]:
    p = profile or DEFAULT_PROFILE
    mut = mutant() == "deathpose"
    onset: Optional[float] = None
    flagged: List[str] = []
    for a in s:
        why = ""
        if a.dead:
            if onset is None:
                onset = a.t
        else:
            onset = None
        if a.have_anim and _usable(a) and not mut:
            cats = _cats(a, p)
            if a.dead and onset is not None and a.t - onset >= th.death_grace_ms and not a.ragdoll:
                if cats & ALIVE_ONLY:
                    why = "alive animation on a dead actor"
                elif not any(p.category(q.group) == "death" for q in a.seqs):
                    why = "dead actor with no death animation"
            elif (not a.dead) and "death" in cats:
                why = "death animation on a live actor"
        flagged.append(why)
    return [Event("deathpose", s[i0].ref, s[i0].t, s[i1].t, s[i1].t - s[i0].t, flagged[i0])
            for i0, i1 in _streaks(s, lambda i: bool(flagged[i]), th.state_grace_ms, th.gap_ms)]


DETECTORS: Dict[str, Callable] = {
    "sliding": detect_sliding, "tpose": detect_tpose, "floating": detect_floating, "jitter": detect_jitter,
    "rubberband": detect_rubberband, "stuck": detect_stuck, "deathpose": detect_deathpose,
}


def run_all(samples: Sequence[Actor], th: Thresholds = DEFAULT, expect_moving: Sequence[Tuple[float, float]] = (),
            machine: str = "", profile: Optional[Profile] = None) -> List[Event]:
    ev: List[Event] = []
    ev += detect_sliding(samples, th, profile)
    ev += detect_tpose(samples, th, profile)
    ev += detect_floating(samples, th, profile)   # yields floating and sinking
    ev += detect_jitter(samples, th, profile)
    ev += detect_rubberband(samples, th, profile)
    ev += detect_stuck(samples, th, expect_moving, profile)
    ev += detect_deathpose(samples, th, profile)
    for e in ev:
        e.machine = machine
    ev.sort(key=lambda e: (e.t0, e.kind))
    return ev
