"""Combat playbooks: fight, ownership_handoff."""
from typing import Dict, List, Optional, Set, Tuple

from ..core import Ctx, Playbook, StepFailure
from ..fakecoop import FakeCoop
from .common import gather, volley, walk_to

RAIDER_BASE = "raider_type"
RAIDERS = ["raider_1", "raider_2", "raider_3"]


class FightPlaybook(Playbook):
    name = "fight"
    summary = "Everyone shoots the same enemies; deaths and counts match on every machine, each dies exactly once."
    needs_refs = ("raiders",)
    requires = ("goto_ref", "aim_at", "fire", "target_alive")
    resumable = True

    def run(self, ctx: Ctx) -> None:
        raiders: List[str] = list(ctx.refs["raiders"])
        base = ctx.refs.get("raider_base")
        if base and ctx.optional("duplicate-enemy checks", lambda: ctx.host.bot.ref_count(base)) is None:
            base = None                 # this adapter cannot count objects: skip those checks (a note is recorded)

        def arrange_before(kills: int):
            """The world the next step expects: everyone at the enemies, armed, and the first `kills` enemies dead."""
            def arrange():
                gather(ctx, raiders[0])
                ctx.equip_all()
                ctx.settle()
                for r in raiders[:kills]:
                    if ctx.host.bot.target_alive(r):
                        volley(ctx, r)
                for r in raiders[kills:]:
                    if not ctx.host.bot.target_alive(r):
                        raise StepFailure("%s is already dead, so this step cannot be arranged: run the whole playbook (or reload the "
                                          "scenario) first" % r, "arrange")
                ctx.settle()
            return arrange

        def walk():
            gather(ctx, raiders[0])
            for r in raiders:
                ctx.same_everywhere("%s alive" % r, lambda m, r=r: m.bot.target_alive(r))
                ctx.expect(ctx.host.bot.target_alive(r), "%s should start alive" % r)

        def count_before():
            n0 = ctx.same_everywhere("enemy count", lambda m: m.bot.ref_count(base))
            ctx.expect(n0 == len(raiders), "expected %d enemies, every machine sees %s" % (len(raiders), n0))

        def kill(i: int, r: str):
            def body():
                volley(ctx, r)
                ctx.settle()
                ctx.same_everywhere("%s alive" % r, lambda m, r=r: m.bot.target_alive(r))
                if ctx.host.bot.supports("death_count"):
                    for m in ctx.machines:
                        ctx.expect(m.bot.death_count(r) == 1, "%s died %s times on %s (must be exactly once)" % (r, m.bot.death_count(r), m.name))
            return body

        def final():
            for r in raiders:
                ctx.expect(not any(m.bot.target_alive(r) for m in ctx.machines), "%s is alive somewhere" % r)
            if base:
                n1 = ctx.same_everywhere("live enemy count", lambda m: m.bot.ref_count(base))
                ctx.expect(n1 == 0, "%s enemies still exist after the fight (duplicates or revived)" % n1)

        ctx.run_step("everyone walks to the enemies", walk, arrange=arrange_before(0))
        if base:
            ctx.run_step("enemy count before the fight", count_before, arrange=arrange_before(0), compare=False)
        for i, r in enumerate(raiders):
            ctx.run_step("everyone kills enemy %d" % (i + 1), kill(i, r), arrange=arrange_before(i))
        ctx.run_step("final state", final, arrange=arrange_before(len(raiders)))


class OwnershipHandoffPlaybook(Playbook):
    name = "ownership_handoff"
    summary = "The host walks away mid-fight; control of the enemies moves once, the fight continues, no flapping or duplicates."
    needs_refs = ("raiders", "far_pos", "near_pos")
    requires = ("goto_ref", "goto_pos", "aim_at", "fire", "target_alive", "owner_of")

    def run(self, ctx: Ctx) -> None:
        raiders: List[str] = list(ctx.refs["raiders"])
        far, near = ctx.refs["far_pos"], ctx.refs["near_pos"]     # specs for walk_to: a spot far away, and one near the enemies
        base = ctx.refs.get("raider_base")
        if base and ctx.optional("duplicate-enemy checks", lambda: ctx.host.bot.ref_count(base)) is None:
            base = None
        client = ctx.clients[0]
        epochs0: Dict[str, int] = {}
        epochs1: Dict[str, int] = {}
        with ctx.step("fight starts, everyone near the enemies"):
            gather(ctx, raiders[0])
            ctx.equip_all()
            ctx.on_all(lambda m: (m.bot.aim_at(raiders[0]), m.bot.fire(1)))
            ctx.settle()
            for r in raiders:
                o = ctx.same_everywhere("owner of %s" % r, lambda m, r=r: m.bot.owner_of(r))
                epochs0[r] = o[1]
        with ctx.step("host walks away"):
            ctx.must(walk_to(ctx.host, far), "host walks away")
            ctx.wait_until("control moves to the client for every live enemy on every machine",
                           lambda: all(m.bot.owner_of(r)[0] == client.idx for m in ctx.machines for r in raiders if m.bot.target_alive(r)),
                           timeout_s=ctx.opts.get("handoff_s", 15.0))
            ctx.settle()
            for r in raiders:
                if ctx.host.bot.target_alive(r):
                    o = ctx.same_everywhere("owner of %s" % r, lambda m, r=r: m.bot.owner_of(r))
                    ctx.expect(o[0] == client.idx, "%s owner is %s, expected the client (%s)" % (r, o[0], client.idx))
                    ctx.expect(o[1] > epochs0[r], "%s epoch did not increase on handoff (%s -> %s)" % (r, epochs0[r], o[1]))
                    epochs1[r] = o[1]
        with ctx.step("host comes back to the border: ownership must not flap"):
            ctx.must(walk_to(ctx.host, near), "host walks back")
            ctx.pause(2.0)
            for _ in range(5):
                ctx.settle()
            for r in raiders:
                if ctx.host.bot.target_alive(r):
                    o = ctx.same_everywhere("owner of %s" % r, lambda m, r=r: m.bot.owner_of(r))
                    ctx.expect(o[1] == epochs1[r] and o[0] == client.idx,
                               "%s ownership flapped: epoch %s -> %s -> %s, owner now %s" % (r, epochs0[r], epochs1[r], o[1], o[0]))
        with ctx.step("client finishes the fight"):
            for r in raiders:
                if client.bot.target_alive(r):
                    ctx.must(client.bot.aim_at(r), "client aims at %s" % r)
                    for _ in range(12):
                        client.bot.fire(1)
                        if not client.bot.target_alive(r):
                            break
            ctx.settle()
            for r in raiders:
                ctx.same_everywhere("%s alive" % r, lambda m, r=r: m.bot.target_alive(r))
                ctx.expect(not ctx.host.bot.target_alive(r), "%s is still alive: the fight did not continue" % r)
        with ctx.step("no duplicates after the fight"):
            if base:
                n = ctx.same_everywhere("enemy count", lambda m: m.bot.ref_count(base))
                ctx.expect(n == 0, "%s enemies exist after everyone died" % n)


# ---------------------------------------------------------------------------------------------
def build_fight(n: int = 2, bugs: Optional[Set[str]] = None, **kw) -> Tuple[FakeCoop, dict]:
    w = FakeCoop(n, bugs, **kw)
    for i, r in enumerate(RAIDERS):
        w.add_ref(r, "raider", pos=(20000 + 80 * i, 0, 0), cell="camp", base=RAIDER_BASE)
    for s in range(n):
        w.set_player(s, pos=(16500, 100 * s, 0), cell="camp", inv={"WEAPON": 1})
    return w, {"raiders": list(RAIDERS), "raider_base": RAIDER_BASE, "weapon": "WEAPON"}


def build_handoff(n: int = 2, bugs: Optional[Set[str]] = None, **kw) -> Tuple[FakeCoop, dict]:
    w = FakeCoop(n, bugs, **kw)
    for r in RAIDERS:
        w.add_ref(r, "raider", pos=(0, 0, 0), cell="camp", base=RAIDER_BASE, hp=200)
    w.set_player(0, pos=(400, 0, 0), cell="camp")
    for s in range(1, n):
        w.set_player(s, pos=(500, 0, 0), cell="camp")
    return w, {"raiders": list(RAIDERS), "raider_base": RAIDER_BASE, "weapon": "WEAPON",
               "far_pos": {"x": 6000, "y": 0, "z": 0, "cell": "camp"},
               "near_pos": {"x": 250, "y": 0, "z": 0, "cell": "camp"}}
