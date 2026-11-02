"""Shared helpers for the bot tests: run one playbook on the fake co-op world."""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))     # tools/bots
if HERE not in sys.path:
    sys.path.insert(0, HERE)

from coopbots import core, fakecoop, links, registry  # noqa: E402,F401

OPTS = {"fast": True, "ui_s": 0.2, "barrier_s": 0.2, "stage_s": 0.2, "handoff_s": 0.2, "spawn_s": 0.2, "settle_s": 0.3, "settle_poll_s": 0.0}


def run(name, bugs=(), n=None, lag=1, opts=None, outdir=None, **world_kw):
    """Returns (RunResult, world)."""
    e = registry.REGISTRY[name]
    w, refs = e.fake(n or e.players, set(bugs), lag=lag, **world_kw)
    d = outdir or tempfile.mkdtemp(prefix="coopbots-")
    o = dict(OPTS)
    o.update(opts or {})
    return core.run_playbook(registry.make(name), fakecoop.make_machines(w), fakecoop.FakeComparer(), d, refs, o), w


def run_pb(pb, world, refs, opts=None, outdir=None):
    o = dict(OPTS)
    o.update(opts or {})
    return core.run_playbook(pb, fakecoop.make_machines(world), fakecoop.FakeComparer(), outdir or tempfile.mkdtemp(prefix="coopbots-"), refs, o)


def lossy_opts(**extra):
    """Options for a bad link on the fake: strict counters off, a quiet window, budget 0."""
    o = links.link_options("bad_wifi")
    o.update(settle_s=0.3, ui_s=0.2, barrier_s=0.3, stage_s=0.3, handoff_s=0.2, spawn_s=0.2, quiesce_s=o["quiesce_s"], fast=True, settle_poll_s=0.0)
    o.update(extra)
    return o
