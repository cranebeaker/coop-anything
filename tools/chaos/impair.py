"""Impairment model: pure, seeded, virtual-time. No sockets.

One Impairment instance models one direction of one link. Feed it each datagram with the current time (ms) and
size (bytes); it answers with a Verdict: dropped (with a reason) or a list of delivery delays (2+ = duplicate).
The UDP relay (relay.py) and any simulator both use this same class and tools/chaos/profiles.json.
"""
import json
import os
from dataclasses import dataclass, field, fields, asdict
from pathlib import Path

PROFILES_PATH = Path(__file__).with_name("profiles.json")

# Mutation twin hook: tests set COOPCHAOS_MUTANT to break one behaviour and must then fail.
MUTANT = os.environ.get("COOPCHAOS_MUTANT", "")


M64 = (1 << 64) - 1


def fnv1a64(text: str) -> int:
    h = 0xCBF29CE484222325
    for b in text.encode("utf-8"):
        h = ((h ^ b) * 0x100000001B3) & M64
    return h


class Rng:
    """SplitMix64. Mirrored exactly by cpp/impair.h so both give the same outcomes."""
    def __init__(self, seed: int, label: str):
        self.s = fnv1a64(f"{seed}:{label}")

    def u64(self) -> int:
        self.s = (self.s + 0x9E3779B97F4A7C15) & M64
        z = self.s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
        return z ^ (z >> 31)

    def random(self) -> float:
        return (self.u64() >> 11) * (1.0 / 9007199254740992.0)

    def uniform(self, a: float, b: float) -> float:
        return a + (b - a) * self.random()

    def below(self, n: int) -> int:
        return self.u64() % n


@dataclass
class Params:
    seed: int = 1
    latency_ms: float = 0.0
    jitter_ms: float = 0.0          # delay is latency +/- jitter, uniform; clamped at 0
    loss: float = 0.0               # base loss while in the good state
    burst_enter: float = 0.0        # per-packet chance good -> bad
    burst_exit: float = 0.5         # per-packet chance bad -> good
    burst_loss: float = 1.0         # loss while in the bad state
    reorder: float = 0.0            # chance a packet skips the latency (overtakes earlier ones)
    duplicate: float = 0.0
    corrupt: float = 0.0            # chance of one flipped bit
    bandwidth_bps: int = 0          # 0 = unlimited
    buffer_ms: float = 200.0        # queue depth at the bandwidth cap before tail drop

    def expected_loss(self) -> float:
        """Long-run loss fraction (ignores bandwidth drops)."""
        s = self.burst_enter + self.burst_exit
        pi_bad = self.burst_enter / s if s > 0 else 0.0
        return (1 - pi_bad) * self.loss + pi_bad * self.burst_loss


@dataclass
class Verdict:
    delays_ms: list = field(default_factory=list)   # empty = dropped
    reason: str = ""                                # set when dropped
    corrupt: bool = False

    @property
    def dropped(self) -> bool:
        return not self.delays_ms


def load_profiles(path=None) -> dict:
    raw = json.loads(Path(path or PROFILES_PATH).read_text(encoding="utf-8"))
    names = {f.name for f in fields(Params)}
    out = {}
    for k, v in raw.items():
        if k.startswith("_"):
            continue
        out[k] = Params(**{a: b for a, b in v.items() if a in names})
    return out


def make_params(spec) -> Params:
    """spec: profile name, dict of overrides (optionally with a 'profile' base), or Params."""
    if isinstance(spec, Params):
        return Params(**asdict(spec))
    profiles = load_profiles()
    if isinstance(spec, str):
        if spec not in profiles:
            raise KeyError(f"unknown profile {spec!r}; have {sorted(profiles)}")
        return profiles[spec]
    base = profiles[spec["profile"]] if "profile" in spec else Params()
    d = asdict(base)
    names = {f.name for f in fields(Params)}
    for k, v in spec.items():
        if k == "profile":
            continue
        if k not in names:
            raise KeyError(f"unknown impairment key {k!r}")
        d[k] = v
    return Params(**d)


class Impairment:
    def __init__(self, params: Params, label: str = "dir"):
        self.label = label
        self.partitioned = False
        self.down_until_ms = -1.0
        self.set_params(params, reseed=True)

    def set_params(self, params: Params, reseed: bool = False):
        self.p = params
        if reseed:
            self.rng = Rng(params.seed, self.label)
            self.bad = False
            self.free_at_ms = 0.0

    def decide(self, now_ms: float, size: int) -> Verdict:
        p, rng = self.p, self.rng
        if self.partitioned and MUTANT != "no_partition":
            return Verdict(reason="partition")
        if now_ms < self.down_until_ms:
            return Verdict(reason="disconnect")
        # loss: Gilbert-Elliott
        if MUTANT != "no_burst":
            if self.bad:
                if rng.random() < p.burst_exit:
                    self.bad = False
            elif rng.random() < p.burst_enter:
                self.bad = True
        lossp = p.burst_loss if self.bad else p.loss
        if MUTANT == "no_loss":
            lossp = 0.0
        if rng.random() < lossp:
            return Verdict(reason="burst" if self.bad else "loss")
        # bandwidth: serialization plus tail-drop queue
        queue_ms = 0.0
        if p.bandwidth_bps > 0 and MUTANT != "no_bandwidth":
            tx_ms = size * 8 * 1000.0 / p.bandwidth_bps
            start = max(now_ms, self.free_at_ms)
            if start - now_ms > p.buffer_ms:
                return Verdict(reason="bandwidth")
            self.free_at_ms = start + tx_ms
            queue_ms = self.free_at_ms - now_ms
        # latency
        def one_delay() -> float:
            if rng.random() < p.reorder and MUTANT != "no_reorder":
                base = 0.0
            else:
                lat = p.latency_ms / 2 if MUTANT == "half_latency" else p.latency_ms
                base = max(0.0, lat + rng.uniform(-p.jitter_ms, p.jitter_ms))
            return base + queue_ms
        delays = [one_delay()]
        if p.duplicate > 0 and rng.random() < p.duplicate and MUTANT != "no_duplicate":
            delays.append(one_delay())
        corrupt = p.corrupt > 0 and rng.random() < p.corrupt
        return Verdict(delays_ms=delays, corrupt=corrupt)

    def partition(self, on: bool):
        self.partitioned = bool(on)

    def disconnect_until(self, until_ms: float):
        self.down_until_ms = until_ms

    def reconnect(self):
        self.down_until_ms = -1.0


def flip_bit(data: bytes, rng: Rng) -> bytes:
    if not data:
        return data
    b = bytearray(data)
    i = rng.below(len(b) * 8)
    b[i // 8] ^= 1 << (i % 8)
    return bytes(b)
