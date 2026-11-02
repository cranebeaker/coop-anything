"""Impaired-link options for the runner: quiet windows and repair budgets per link profile.

The profile names match tools/chaos/profiles.json (lan, good_wifi, bad_wifi, mobile_tether, transatlantic, hell). This
module does not impair anything itself. Start the chaos relay (tools/chaos) between the game instances, then pass
`link_options(profile)` as the runner options. On an impaired link:

  * transient mismatches and repairs while traffic is in flight are recorded, not failed (strict_counters off),
  * every step ends with a quiet window (`quiesce_s`): nothing new is sent, every machine must converge, and no repair
    may happen during a second quiet window,
  * the whole run has a repairs-per-minute budget. The default is 0 (strict). A budget above 0 needs a written reason
    next to it in the profiles file (`repair_budget_reason`) or the run refuses to start,
  * hard counters (bad messages, failed applies, rebases) still fail immediately.
"""
import json
import os
from typing import Any, Dict, Optional

PROFILES = ("lan", "good_wifi", "bad_wifi", "mobile_tether", "transatlantic", "hell")
# seconds without new traffic after which the world must be converged with 0 repairs; scaled to latency plus jitter
QUIESCE_S = {"lan": 1.0, "good_wifi": 2.0, "bad_wifi": 6.0, "mobile_tether": 8.0, "transatlantic": 6.0, "hell": 20.0}
SETTLE_S = {"lan": 20.0, "good_wifi": 30.0, "bad_wifi": 60.0, "mobile_tether": 90.0, "transatlantic": 90.0, "hell": 180.0}
# fake world only: every Nth delivery to a client is dropped and retransmitted (0 = none)
FAKE_LOSS_EVERY = {"lan": 0, "good_wifi": 25, "bad_wifi": 5, "mobile_tether": 11, "transatlantic": 40, "hell": 3}


def repair_budget(profiles_path: Optional[str], profile: str) -> float:
    """Repairs per minute allowed for a profile: `repair_budget_per_min` in the profiles file, default 0."""
    if not profiles_path:
        return 0.0
    try:
        with open(profiles_path, encoding="utf-8") as f:
            p = json.load(f).get(profile, {})
    except (OSError, ValueError):
        return 0.0
    b = float(p.get("repair_budget_per_min", 0.0))
    if b > 0 and not p.get("repair_budget_reason"):
        raise ValueError("%s loosens repair_budget_per_min to %s without a repair_budget_reason" % (profile, b))
    return b


def link_options(profile: Optional[str], profiles_path: Optional[str] = None) -> Dict[str, Any]:
    """Runner options for a link profile. Empty for no profile or lan: the strict defaults apply."""
    if not profile or profile == "lan":
        return {}
    if profile not in PROFILES:
        raise ValueError("unknown link profile %r; choose from %s" % (profile, ", ".join(PROFILES)))
    t = SETTLE_S[profile]
    return {"strict_counters": False, "settle_s": t, "barrier_s": t, "stage_s": t, "ui_s": t, "chaos": profile,
            "quiesce_s": QUIESCE_S[profile], "repair_budget_per_min": repair_budget(profiles_path, profile)}


def default_profiles_path() -> Optional[str]:
    """tools/chaos/profiles.json of this repository, if present."""
    p = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "chaos", "profiles.json")
    return p if os.path.isfile(p) else None
