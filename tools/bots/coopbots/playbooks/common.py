"""Helpers shared by the playbooks. They only call the ActionAdapter interface."""
from typing import Dict

from ..core import Ctx, Machine, StepFailure


def walk_to(m: Machine, spec):
    """spec is {"ref": id, "dist": units} or {"x": .., "y": .., "z": .., "cell": ..}."""
    if "ref" in spec:
        return m.bot.goto_ref(spec["ref"], spec.get("dist", 300))
    return m.bot.goto_pos(spec["x"], spec["y"], spec["z"], spec.get("cell"))


def gather(ctx: Ctx, near: str, dist: int = 600) -> None:
    """Everyone walks to within `dist` of one object."""
    for name, r in ctx.on_all(lambda m: m.bot.goto_ref(near, dist)).items():
        ctx.must(r, "%s walks to %s" % (name, near))


def volley(ctx: Ctx, target: str, max_volleys: int = 12) -> None:
    """Every machine aims at the same actor and fires at the same time until it is dead on every machine."""
    ctx.equip_all()
    for _ in range(max_volleys):
        def shoot(m: Machine):
            r = m.bot.aim_at(target)
            return r if not r else m.bot.fire(1)
        for name, r in ctx.on_all(shoot).items():
            # "already dead" for the second shooter is expected when two players kill in the same instant
            if not r and "dead" not in (r.reason or "") and "gone" not in (r.reason or ""):
                raise StepFailure("%s could not shoot %s: %s" % (name, target, r.reason), "action")
        if all(not m.bot.target_alive(target) for m in ctx.machines):
            return
        ctx.settle()
    raise StepFailure("%s is still alive on some machine after %d volleys: %s"
                      % (target, max_volleys, {m.name: m.bot.target_alive(target) for m in ctx.machines}), "check")


def total_held(ctx: Ctx, item: str) -> int:
    """How many of `item` the whole party holds, each machine asked about its own player."""
    return sum(m.bot.inventory_count(item) for m in ctx.machines)


def all_stage_done(ctx: Ctx, q: str, n: int) -> Dict[str, bool]:
    return {m.name: m.bot.stage_done(q, n) for m in ctx.machines}


def expect_stage_everywhere(ctx: Ctx, q: str, n: int, want: bool = True) -> None:
    got = all_stage_done(ctx, q, n)
    if any(v != want for v in got.values()):
        raise StepFailure("quest %s stage %s done=%s expected on every machine, got %s" % (q, n, want, got), "check")
