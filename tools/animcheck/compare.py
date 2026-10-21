"""Compare what one machine shows (the observer) against the machine that owns the actor (the source).

Time model: the observer renders owner state about `interp_delay_ms` old (the interpolation buffer). So the observer
sample at time t is compared with the source at t - interp_delay_ms, searching +-latency_allow_ms for the best match
(best of the source samples inside the window and the source interpolated at the centre). Anything that matches at
some instant inside the window counts as in sync; this keeps network latency and sampling jitter out of the score.
"""
from __future__ import annotations

import bisect
import math
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence

from .config import DEFAULT, Thresholds, mutant
from .model import Actor, Stream, Vec
from .profile import DEFAULT_PROFILE, Profile


def percentile(vals: Sequence[float], p: float) -> float:
    """Nearest-rank percentile, p in 0..100. Empty -> 0."""
    if not vals:
        return 0.0
    s = sorted(vals)
    k = max(0, min(len(s) - 1, int(math.ceil(p / 100.0 * len(s))) - 1))
    return s[k]


def angdiff(a: float, b: float) -> float:
    d = (a - b) % (2 * math.pi)
    if d > math.pi:
        d -= 2 * math.pi
    return abs(d)


def local_bones(bones: Dict[str, Vec], rotz: float, root: Optional[str] = None) -> Dict[str, Vec]:
    """Bone positions relative to the root bone (else the centroid), rotated into the actor's facing frame."""
    if not bones:
        return {}
    root = root or DEFAULT_PROFILE.root_bone
    if root in bones:
        o = bones[root]
    else:
        n = len(bones)
        o = (sum(v[0] for v in bones.values()) / n, sum(v[1] for v in bones.values()) / n, sum(v[2] for v in bones.values()) / n)
    c, s = math.cos(-rotz), math.sin(-rotz)
    out = {}
    for k, v in bones.items():
        x, y, z = v[0] - o[0], v[1] - o[1], v[2] - o[2]
        out[k] = (x * c - y * s, x * s + y * c, z)
    return out


def pose_error(a: Actor, b: Actor, facing_free: bool = True, root: Optional[str] = None) -> Optional[float]:
    """Mean distance (game units) between the common bones after root alignment. None if fewer than 3 common bones."""
    root = root or DEFAULT_PROFILE.root_bone
    la = local_bones(a.bones, a.rotz if facing_free else 0.0, root)
    lb = local_bones(b.bones, b.rotz if facing_free else 0.0, root)
    common = [k for k in la if k in lb and k != root]
    if len(common) < 3:
        return None
    return sum(math.dist(la[k], lb[k]) for k in common) / len(common)


def _lerp(a: Vec, b: Vec, f: float) -> Vec:
    return (a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f)


def _lerp_angle(a: float, b: float, f: float) -> float:
    d = (b - a + math.pi) % (2 * math.pi) - math.pi
    return a + d * f


def interpolate(samples: Sequence[Actor], times: Sequence[float], t: float, gap_ms: float) -> Optional[Actor]:
    """Source state at time t: linear in position / bones / facing, discrete fields from the earlier sample."""
    i = bisect.bisect_right(times, t)
    if i == 0:
        return None
    if i >= len(samples):  # after the last sample: hold it for a moment
        return samples[-1] if t - samples[-1].t <= gap_ms else None
    a, b = samples[i - 1], samples[i]
    if b.t - a.t > gap_ms or a.cell != b.cell:
        return None
    f = (t - a.t) / (b.t - a.t) if b.t > a.t else 0.0
    out = Actor(ref=a.ref, t=t, pos=_lerp(a.pos, b.pos, f), rotz=_lerp_angle(a.rotz, b.rotz, f), rotx=_lerp_angle(a.rotx, b.rotx, f), cell=a.cell, dead=a.dead,
                valid3d=a.valid3d, seqs=a.seqs, have_anim=a.have_anim, weapon_out=a.weapon_out, weapon_form=a.weapon_form,
                aiming=a.aiming, knocked=a.knocked, swimming=a.swimming, ragdoll=a.ragdoll, sneaking=a.sneaking,
                bones={k: _lerp(v, b.bones[k], f) for k, v in a.bones.items() if k in b.bones})
    return out


def anim_sig(a: Actor) -> frozenset:
    return frozenset((s.slot, s.group) for s in a.animating())


@dataclass
class PairResult:
    source: str
    observer: str
    actors: int = 0
    compared: int = 0
    pose_errs: List[float] = field(default_factory=list)
    pos_errs: List[float] = field(default_factory=list)
    facing_errs_deg: List[float] = field(default_factory=list)
    group_mismatch_samples: int = 0
    group_mismatch_episodes: int = 0
    wrong_weapon: int = 0
    wrong_facing: int = 0
    dead_mismatch: int = 0
    wrong_aim: int = 0
    stance_mismatch: int = 0
    aim_errs_deg: List[float] = field(default_factory=list)
    worst: List[dict] = field(default_factory=list)


def _episodes(flags: List[bool], times: List[float], grace_ms: float, gap_ms: float) -> int:
    n = i = 0
    while i < len(flags):
        if not flags[i]:
            i += 1
            continue
        j = i
        while j + 1 < len(flags) and flags[j + 1] and times[j + 1] - times[j] <= gap_ms:
            j += 1
        if times[j] - times[i] >= grace_ms:
            n += 1
        i = j + 1
    return n


def compare_actor(src: List[Actor], obs: List[Actor], th: Thresholds, res: PairResult, offset_ms: float = 0.0,
                  profile: Optional[Profile] = None) -> None:
    root = (profile or DEFAULT_PROFILE).root_bone
    times = [a.t for a in src]
    flags_group: List[bool] = []
    flags_weapon: List[bool] = []
    flags_facing: List[bool] = []
    flags_dead: List[bool] = []
    flags_aim: List[bool] = []
    flags_stance: List[bool] = []
    tt: List[float] = []
    for o in obs:
        t_ref = o.t + offset_ms - th.interp_delay_ms
        lo = bisect.bisect_left(times, t_ref - th.latency_allow_ms)
        hi = bisect.bisect_right(times, t_ref + th.latency_allow_ms)
        cands = list(src[lo:hi])
        mid = interpolate(src, times, t_ref, th.gap_ms)
        if mid is not None:
            cands.append(mid)
        if not cands:
            continue
        res.compared += 1
        tt.append(o.t)
        best_pos = min(math.dist(c.pos, o.pos) for c in cands)
        res.pos_errs.append(best_pos)
        pes = [pe for pe in (pose_error(c, o, True, root) for c in cands) if pe is not None]
        if pes and o.bones:
            res.pose_errs.append(0.0 if mutant() == "pose_compare" else min(pes))
        fe = min(math.degrees(angdiff(c.rotz, o.rotz)) for c in cands)
        res.facing_errs_deg.append(fe)
        flags_facing.append(fe > th.facing_tol_deg)
        if o.have_anim:
            sig = anim_sig(o)
            bad = not any(anim_sig(c) == sig for c in cands if c.have_anim)
            if mutant() == "group_compare":
                bad = False
        else:
            bad = False
        flags_group.append(bad)
        res.group_mismatch_samples += int(bad)

        def weapon_ok(c: Actor) -> bool:
            if c.weapon_out != o.weapon_out:
                return False
            return (not c.weapon_out) or c.weapon_form == o.weapon_form

        wbad = not any(weapon_ok(c) for c in cands)
        if mutant() == "weapon_compare":
            wbad = False
        flags_weapon.append(wbad)
        flags_dead.append(not any(c.dead == o.dead for c in cands))
        # aim: while the owner aims or has the weapon out, the pitch must match too
        if any(c.aiming or c.weapon_out for c in cands) or o.aiming:
            ae = min(math.degrees(angdiff(c.rotx, o.rotx)) for c in cands)
            res.aim_errs_deg.append(ae)
            flags_aim.append(ae > th.aim_tol_deg and mutant() != "aim_compare")
        else:
            flags_aim.append(False)
        flags_stance.append(mutant() != "stance_compare" and not any(
            (c.sneaking, c.swimming, c.knocked > 0, c.aiming) == (o.sneaking, o.swimming, o.knocked > 0, o.aiming) for c in cands))
    res.group_mismatch_episodes += _episodes(flags_group, tt, th.state_grace_ms, th.gap_ms)
    res.wrong_weapon += _episodes(flags_weapon, tt, th.state_grace_ms, th.gap_ms)
    wf = _episodes(flags_facing, tt, th.state_grace_ms, th.gap_ms)
    res.wrong_facing += 0 if mutant() == "facing_compare" else wf
    res.dead_mismatch += _episodes(flags_dead, tt, th.state_grace_ms, th.gap_ms)
    res.wrong_aim += _episodes(flags_aim, tt, th.state_grace_ms, th.gap_ms)
    res.stance_mismatch += _episodes(flags_stance, tt, th.state_grace_ms, th.gap_ms)


def estimate_offset(src: Stream, obs: Stream, th: Thresholds = DEFAULT, span_ms: float = 4000.0, step_ms: float = 50.0) -> float:
    """Clock offset (obs time + offset = source time) that minimises position error. Only needed when the two streams
    use different clocks (t_ms of different processes); with rx_ms streams the offset is 0."""
    best, best_off = math.inf, 0.0
    refs = [r for r in obs.by_ref if r in src.by_ref]
    if not refs:
        return 0.0
    off = -span_ms
    while off <= span_ms:
        tot = n = 0
        for r in refs:
            s = src.by_ref[r]
            times = [a.t for a in s]
            for o in obs.by_ref[r][::3]:
                m = interpolate(s, times, o.t + off - th.interp_delay_ms, th.gap_ms)
                if m is not None:
                    tot += math.dist(m.pos, o.pos)
                    n += 1
        if n >= 5 and tot / n < best:
            best, best_off = tot / n, off
        off += step_ms
    return best_off


def compare_streams(src: Stream, obs: Stream, th: Thresholds = DEFAULT, ref_map: Optional[Dict[int, int]] = None,
                    offset_ms: float = 0.0, only: Optional[Dict[int, int]] = None, skip: Sequence[int] = (),
                    profile: Optional[Profile] = None) -> PairResult:
    """ref_map maps source ref -> observer ref (a puppet has a different id than the real actor); default identity.
    only: compare just these {source ref: observer ref} pairs (avatar fidelity). skip: source refs to leave out."""
    res = PairResult(source=src.machine, observer=obs.machine)
    items = [(r, src.by_ref[r]) for r in only if r in src.by_ref] if only is not None else list(src.by_ref.items())
    for sref, ssamples in items:
        if sref in skip:
            continue
        oref = (only or ref_map or {}).get(sref, sref)
        osamples = obs.by_ref.get(oref)
        if not osamples:
            continue
        res.actors += 1
        compare_actor(ssamples, osamples, th, res, offset_ms, profile)
    return res
