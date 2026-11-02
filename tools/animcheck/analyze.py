"""Run the glitch detectors on every machine and the pose comparison between machines; produce one report dict."""
from __future__ import annotations

import dataclasses
import math
from typing import Dict, Iterable, List, Optional, Sequence, Union

from . import detectors
from .compare import PairResult, compare_streams, estimate_offset, percentile
from .config import DEFAULT, Thresholds
from .counters import check
from .detectors import Event
from .model import Stream
from .profile import DEFAULT_PROFILE, Profile

SCHEMA = "animcheck.report.v1"
ALL_KINDS = ("sliding", "tpose", "floating", "sinking", "jitter", "rubberband", "stuck", "deathpose")


def _stats(vals: Sequence[float]) -> dict:
    return {"n": len(vals), "p50": round(percentile(vals, 50), 3), "p95": round(percentile(vals, 95), 3),
            "max": round(max(vals), 3) if vals else 0.0, "mean": round(sum(vals) / len(vals), 3) if vals else 0.0}


def _suppressed(e: Event, rules: Iterable[dict]) -> bool:
    for r in rules:
        if r.get("kind") not in (None, e.kind):
            continue
        if r.get("ref") not in (None, e.ref):
            continue
        if r.get("machine") not in (None, e.machine):
            continue
        if e.t1 >= r.get("t0", -math.inf) and e.t0 <= r.get("t1", math.inf):
            return True
    return False


def _unknown_groups(streams: Dict[str, Stream], profile: Profile) -> Dict[str, int]:
    """Animation groups seen playing that the profile does not know, with how many samples each appeared in."""
    seen: Dict[str, int] = {}
    for st in streams.values():
        for samples in st.by_ref.values():
            for a in samples:
                for s in a.animating():
                    if profile.category(s.group) == "unknown":
                        k = str(s.group)
                        seen[k] = seen.get(k, 0) + 1
    return seen


def analyze(streams: Union[Dict[str, Stream], Sequence[Stream]], th: Thresholds = DEFAULT, owner: Optional[str] = None,
            ref_maps: Optional[Dict[str, Dict[int, int]]] = None,
            expect_moving: Optional[Dict[int, Sequence[tuple]]] = None, suppress: Sequence[dict] = (),
            auto_offset: bool = False, label: str = "", probe_cost_us: Optional[dict] = None,
            avatars: Sequence[dict] = (), profile: Optional[Profile] = None) -> dict:
    """streams: {machine: Stream} (or a list). owner: the machine whose actors are the source of truth for the
    comparison (default: first). ref_maps: {observer machine: {source ref: observer ref}}. expect_moving:
    {ref: [(t0_ms, t1_ms), ...]} windows in which the actor must be walking (enables the stuck detector).
    avatars: [{'name','owner','owner_ref','observer','observer_ref'}] player A's own character on A's machine vs A's
    puppet on B ("avatar fidelity", reported apart from the NPC numbers under the `avatar.*` counters).
    suppress: [{'kind':..., 'ref':..., 'machine':..., 't0':..., 't1':...}] ignore matching events (e.g. a deliberate teleport).
    profile: bone names and animation group table of the game (default: the generic profile)."""
    profile = profile or DEFAULT_PROFILE
    if not isinstance(streams, dict):
        streams = {s.machine: s for s in streams}
    names = list(streams)
    owner = owner or (names[0] if names else None)
    av_refs = {}   # machine -> refs that are avatars (real or puppet)
    for av in avatars:
        av_refs.setdefault(av["owner"], set()).add(av["owner_ref"])
        av_refs.setdefault(av["observer"], set()).add(av["observer_ref"])

    machines: Dict[str, dict] = {}
    all_events: List[Event] = []
    avatar_events: List[Event] = []
    for name, st in streams.items():
        evs: List[Event] = []
        for ref, samples in st.by_ref.items():
            if len(samples) < 2:
                continue
            evs += detectors.run_all(samples, th, (expect_moving or {}).get(ref, ()), machine=name, profile=profile)
        evs = [e for e in evs if not _suppressed(e, suppress)]
        avatar_events += [e for e in evs if e.ref in av_refs.get(name, ())]
        evs = [e for e in evs if e.ref not in av_refs.get(name, ())]
        evs.sort(key=lambda e: (e.t0, e.kind, e.ref))
        counts = {k: 0 for k in ALL_KINDS}
        for e in evs:
            counts[e.kind] += 1
        machines[name] = {"samples": st.sample_count, "actors": len(st.by_ref),
                          "t_first_ms": None if st.t_first == math.inf else round(st.t_first, 1),
                          "t_last_ms": None if st.t_last == -math.inf else round(st.t_last, 1),
                          "counts": counts, "events": [e.as_dict() for e in evs]}
        all_events += evs

    pairs: List[dict] = []
    pose_all: List[float] = []
    pos_all: List[float] = []
    face_all: List[float] = []
    agg = {"compared": 0, "group_samples": 0, "group_eps": 0, "weapon": 0, "facing": 0, "dead": 0, "actors": 0}
    if owner is not None:
        src = streams[owner]
        for name, obs in streams.items():
            if name == owner:
                continue
            off = estimate_offset(src, obs, th) if auto_offset else 0.0
            pr: PairResult = compare_streams(src, obs, th, (ref_maps or {}).get(name), off,
                                             skip=tuple(av_refs.get(owner, ())), profile=profile)
            pose_all += pr.pose_errs
            pos_all += pr.pos_errs
            face_all += pr.facing_errs_deg
            agg["compared"] += pr.compared
            agg["group_samples"] += pr.group_mismatch_samples
            agg["group_eps"] += pr.group_mismatch_episodes
            agg["weapon"] += pr.wrong_weapon
            agg["facing"] += pr.wrong_facing
            agg["dead"] += pr.dead_mismatch
            agg["actors"] += pr.actors
            pairs.append({"source": pr.source, "observer": pr.observer, "actors": pr.actors, "compared": pr.compared,
                          "clock_offset_ms": off, "pose_err": _stats(pr.pose_errs), "pos_err": _stats(pr.pos_errs),
                          "facing_err_deg": _stats(pr.facing_errs_deg),
                          "group_mismatch_pct": round(100.0 * pr.group_mismatch_samples / pr.compared, 2) if pr.compared else 0.0,
                          "group_mismatch_episodes": pr.group_mismatch_episodes, "wrong_weapon": pr.wrong_weapon,
                          "wrong_facing": pr.wrong_facing, "dead_mismatch": pr.dead_mismatch,
                          "wrong_aim": pr.wrong_aim, "stance_mismatch": pr.stance_mismatch})

    av_report: List[dict] = []
    av_pose: List[float] = []
    av_pos: List[float] = []
    av_face: List[float] = []
    av_aim: List[float] = []
    av_agg = {"compared": 0, "gs": 0, "ge": 0, "weapon": 0, "facing": 0, "aim": 0, "dead": 0, "stance": 0}
    for av in avatars:
        src, obs = streams.get(av["owner"]), streams.get(av["observer"])
        if src is None or obs is None:
            continue
        off = estimate_offset(src, obs, th) if auto_offset else 0.0
        pr = compare_streams(src, obs, th, offset_ms=off, only={av["owner_ref"]: av["observer_ref"]}, profile=profile)
        av_pose += pr.pose_errs
        av_pos += pr.pos_errs
        av_face += pr.facing_errs_deg
        av_aim += pr.aim_errs_deg
        av_agg["compared"] += pr.compared
        av_agg["gs"] += pr.group_mismatch_samples
        av_agg["ge"] += pr.group_mismatch_episodes
        av_agg["weapon"] += pr.wrong_weapon
        av_agg["facing"] += pr.wrong_facing
        av_agg["aim"] += pr.wrong_aim
        av_agg["dead"] += pr.dead_mismatch
        av_agg["stance"] += pr.stance_mismatch
        av_report.append({"name": av.get("name", ""), "owner": av["owner"], "observer": av["observer"], "compared": pr.compared,
                          "pose_err": _stats(pr.pose_errs), "pos_err": _stats(pr.pos_errs), "facing_err_deg": _stats(pr.facing_errs_deg),
                          "aim_err_deg": _stats(pr.aim_errs_deg), "group_mismatch_episodes": pr.group_mismatch_episodes,
                          "wrong_weapon": pr.wrong_weapon, "wrong_facing": pr.wrong_facing, "wrong_aim": pr.wrong_aim,
                          "stance_mismatch": pr.stance_mismatch, "dead_mismatch": pr.dead_mismatch})

    unknown = _unknown_groups(streams, profile)
    counters: Dict[str, float] = {
        "machines": len(streams), "samples": sum(m["samples"] for m in machines.values()),
        "actors": max([m["actors"] for m in machines.values()] or [0]),
        "compared": agg["compared"], "compared_actors": agg["actors"],
        "pose_err_p50": round(percentile(pose_all, 50), 3), "pose_err_p95": round(percentile(pose_all, 95), 3),
        "pose_err_max": round(max(pose_all), 3) if pose_all else 0.0, "pose_samples": len(pose_all),
        "pos_err_p95": round(percentile(pos_all, 95), 3), "facing_err_p95_deg": round(percentile(face_all, 95), 3),
        "group_mismatch_pct": round(100.0 * agg["group_samples"] / agg["compared"], 2) if agg["compared"] else 0.0,
        "group_mismatch_episodes": agg["group_eps"], "wrong_weapon": agg["weapon"], "wrong_facing": agg["facing"],
        "dead_mismatch": agg["dead"], "unknown_groups": len(unknown),
    }
    total = 0
    for k in ALL_KINDS:
        n = sum(m["counts"][k] for m in machines.values())
        counters[f"glitch.{k}"] = n
        total += n
    counters["glitches"] = total
    for name, m in machines.items():
        counters[f"glitches@{name}"] = sum(m["counts"].values())
    if avatars:
        counters.update({
            "avatar.compared": av_agg["compared"],
            "avatar.pose_err_p50": round(percentile(av_pose, 50), 3), "avatar.pose_err_p95": round(percentile(av_pose, 95), 3),
            "avatar.pose_err_max": round(max(av_pose), 3) if av_pose else 0.0, "avatar.pose_samples": len(av_pose),
            "avatar.pos_err_p95": round(percentile(av_pos, 95), 3),
            "avatar.facing_err_p95_deg": round(percentile(av_face, 95), 3), "avatar.aim_err_p95_deg": round(percentile(av_aim, 95), 3),
            "avatar.group_mismatch_pct": round(100.0 * av_agg["gs"] / av_agg["compared"], 2) if av_agg["compared"] else 0.0,
            "avatar.group_mismatch_episodes": av_agg["ge"], "avatar.anim_mismatches": av_agg["ge"],
            "avatar.wrong_weapon": av_agg["weapon"], "avatar.wrong_facing": av_agg["facing"], "avatar.wrong_aim": av_agg["aim"],
            "avatar.stance_mismatch": av_agg["stance"], "avatar.dead_mismatch": av_agg["dead"],
            "avatar.glitches": len(avatar_events)})
        for k in ALL_KINDS:
            counters[f"avatar.glitch.{k}"] = sum(1 for e in avatar_events if e.kind == k)
    counters["wrong_aim"] = sum(p["wrong_aim"] for p in pairs)
    counters["stance_mismatch"] = sum(p["stance_mismatch"] for p in pairs)
    if probe_cost_us:
        counters["probe_cost_p95_us"] = probe_cost_us.get("p95_us", 0.0)
        counters["probe_cost_us_per_actor"] = probe_cost_us.get("us_per_actor", 0.0)

    report = {"schema": SCHEMA, "label": label, "profile": profile.name, "owner": owner,
              "thresholds": dataclasses.asdict(th), "machines": machines, "pairs": pairs, "avatars": av_report,
              "avatar_events": [e.as_dict() for e in avatar_events], "unknown_groups": unknown, "counters": counters}
    default_spec = {"glitches": "==0", "wrong_weapon": "==0", "wrong_facing": "==0"}
    if pose_all:
        default_spec["pose_err_p95"] = f"<={th.pose_tol_units}"
    if avatars:
        default_spec.update({"avatar.glitches": "==0", "avatar.wrong_weapon": "==0", "avatar.wrong_facing": "==0",
                             "avatar.wrong_aim": "==0", "avatar.anim_mismatches": "==0"})
        if av_pose:
            default_spec["avatar.pose_err_p95"] = f"<={th.pose_tol_units}"
    report["failures"] = check(report, default_spec)
    report["ok"] = not report["failures"]
    return report
