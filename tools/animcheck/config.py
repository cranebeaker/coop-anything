"""Thresholds and tolerances. Distances are in game units, written for a world of about 70 units per meter; times are
in milliseconds. A profile can rescale them (`units_per_meter`) and override single values (`thresholds`), and code can
override any of them: Thresholds(slide_min_speed=80, ...). The defaults are starting points: run a clean session of
your game first and tune them from it (see README.md, "Tuning")."""
from __future__ import annotations

import os
from dataclasses import dataclass, field, fields, replace
from typing import Dict, Tuple


def mutant() -> str:
    """Name of the active mutation twin (tests only), '' in normal use."""
    return os.environ.get("ANIMCHECK_MUTANT", "")


@dataclass(frozen=True)
class Thresholds:
    # ---- sampling ----
    gap_ms: float = 600.0              # a gap longer than this between two samples of an actor breaks a streak
    min_dt_ms: float = 5.0             # samples closer than this are treated as one

    # ---- comparison across machines ----
    interp_delay_ms: float = 100.0     # the receiver shows state about this much older than the owner's
    latency_allow_ms: float = 150.0    # extra window (one way network latency + sampling jitter) searched on each side
    pose_tol_units: float = 12.0       # default bound used by the report's pass/fail summary only
    facing_tol_deg: float = 35.0
    aim_tol_deg: float = 20.0
    state_grace_ms: float = 400.0      # a weapon / group mismatch shorter than this is a transition, not a bug

    # ---- sliding (moving without matching locomotion animation) ----
    slide_min_speed: float = 40.0      # units/s horizontal below which the actor counts as standing
    slide_ms: float = 300.0
    loco_bands: Dict[str, Tuple[float, float]] = field(default_factory=lambda: {
        "walk": (8.0, 280.0),          # category -> plausible horizontal speed band while that animation plays
        "run": (120.0, 900.0),
        "dodge": (0.0, 1500.0),
    })
    band_ms: float = 500.0             # speed outside the band for this long = animation / speed mismatch

    # ---- T-pose ----
    tpose_ms: float = 300.0
    tpose_arm_dz: float = 5.0          # hands within this of their upper arm height ...
    tpose_arm_span: float = 36.0       # ... and at least this far (horizontally) from it: arms straight out

    # ---- floating / sinking ----
    ground_step: float = 30.0          # z change from the tracked ground that is still "terrain"
    float_ms: float = 400.0
    fall_vz: float = 400.0             # vertical speed above which the actor is falling / jumping

    # ---- jitter ----
    jitter_window: int = 8             # samples
    jitter_amp: float = 2.5            # minimum step (units) that counts as a direction reversal
    jitter_reversals: int = 5          # reversals inside the window

    # ---- rubber banding ----
    rubber_dist: float = 150.0         # one-sample jump
    rubber_speed: float = 2500.0       # units/s implied by that jump
    snapback_ms: float = 600.0         # jump followed by a return to (near) the old spot within this

    # ---- stuck ----
    stuck_ms: float = 1500.0
    stuck_speed: float = 5.0

    # ---- wrong death pose ----
    death_grace_ms: float = 2500.0

    def with_(self, **kw) -> "Thresholds":
        kw = self.coerce(kw)
        if "loco_bands" in kw:   # a partial override keeps the bands it does not mention
            kw["loco_bands"] = {**self.loco_bands, **kw["loco_bands"]}
        return replace(self, **kw)

    @classmethod
    def coerce(cls, kw: dict) -> dict:
        """Check the keys of an override dict (e.g. from a profile) and fix the loco_bands shape."""
        names = {f.name for f in fields(cls)}
        bad = sorted(k for k in kw if k not in names)
        if bad:
            raise ValueError(f"unknown threshold(s): {', '.join(bad)}")
        out = dict(kw)
        if "loco_bands" in out:
            out["loco_bands"] = {str(k): (float(v[0]), float(v[1])) for k, v in out["loco_bands"].items()}
        return out

    def scaled(self, k: float) -> "Thresholds":
        """Distances and speeds multiplied by k (world units per meter relative to the 70 the defaults assume)."""
        if k == 1.0:
            return self
        dist = ("pose_tol_units", "slide_min_speed", "tpose_arm_dz", "tpose_arm_span", "ground_step", "fall_vz",
                "jitter_amp", "rubber_dist", "rubber_speed", "stuck_speed")
        kw = {n: getattr(self, n) * k for n in dist}
        kw["loco_bands"] = {c: (lo * k, hi * k) for c, (lo, hi) in self.loco_bands.items()}
        return replace(self, **kw)


DEFAULT = Thresholds()
