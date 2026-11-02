"""Loot playbooks: loot_race, container_share."""
from typing import Optional, Set, Tuple

from ..core import Ctx, Machine, Playbook, StepFailure
from ..fakecoop import FakeCoop
from .common import total_held

ITEM_REF, ITEM_BASE = "world_item", "healing_pack"
CONTAINER = "chest"


class LootRacePlaybook(Playbook):
    name = "loot_race"
    summary = "Everyone grabs the same world item at once; exactly one wins, the others get 'already taken'."
    needs_refs = ("item", "item_base")
    requires = ("goto_ref", "pickup", "inventory_count", "ref_enabled", "drop")
    ROUNDS = 3
    resumable = True

    def run(self, ctx: Ctx) -> None:
        item, base = ctx.refs["item"], ctx.refs["item_base"]

        def walk_to_item():
            for n, r in ctx.on_all(lambda m: m.bot.goto_ref(item, 150)).items():
                ctx.must(r, "%s walks to the item" % n)

        def arrange_race():
            """Everyone at the item. Idempotent."""
            walk_to_item()

        def arrange_round():
            """Someone holds the item, as after round 1: everyone at the item, and if nobody has it yet the host picks it up."""
            walk_to_item()
            if total_held(ctx, base) == 0 and ctx.host.bot.ref_enabled(item):
                ctx.must(ctx.host.bot.pickup(item), "host picks up the item")
            ctx.settle()
            if total_held(ctx, base) == 0:
                raise StepFailure("nobody holds %s and the item is gone from the world, so a drop round cannot be arranged: run the whole "
                                  "playbook (or reload the scenario) first" % base, "arrange")

        def grab():
            before = total_held(ctx, base)
            res = ctx.on_all(lambda m: m.bot.pickup(item))
            winners = [n for n, r in res.items() if r]
            ctx.expect(len(winners) == 1, "expected exactly one winner, got %s; results: %s"
                       % (winners or "none", {n: (bool(r), r.reason) for n, r in res.items()}))
            ctx.settle()
            gained = total_held(ctx, base) - before
            ctx.expect(gained == 1, "%d copies of %s appeared in the party (must be exactly 1)" % (gained, base))
            ctx.same_everywhere("item still lying there", lambda m: m.bot.ref_enabled(item))
            ctx.expect(not ctx.host.bot.ref_enabled(item), "the item is still in the world after being picked up")
            for m in ctx.machines:
                if m.name not in winners:
                    ctx.expect(m.bot.inventory_count(base) == 0, "%s lost the race but still holds the item" % m.name)

        def drop_round(rd):
            def body():
                winner = next(m for m in ctx.machines if m.bot.inventory_count(base) > 0)
                d = winner.bot.drop(base, 1)
                ctx.must(d, "%s drops the item" % winner.name)
                dropped = (getattr(d, "data", None) or {}).get("ref")
                ctx.expect(bool(dropped), "round %d: drop did not report the new world object" % rd)
                ctx.settle()
                ctx.same_everywhere("dropped item visible", lambda m: m.bot.ref_enabled(dropped))
                total0 = total_held(ctx, base)
                res = ctx.on_all(lambda m: m.bot.pickup(dropped))
                winners = [n for n, r in res.items() if r]
                ctx.expect(len(winners) == 1, "round %d: expected one winner, got %s" % (rd, winners or "none"))
                ctx.settle()
                ctx.expect(total_held(ctx, base) == total0 + 1,
                           "round %d: item count in the party is wrong (%d after, %d before the race)" % (rd, total_held(ctx, base), total0))
            return body

        ctx.run_step("everyone stands at the item", walk_to_item, arrange=arrange_race)
        ctx.run_step("all grab at the same instant", grab, arrange=arrange_race)
        for rd in range(2, self.ROUNDS + 1):
            ctx.run_step("round %d: the winner drops it, everyone races again" % rd, drop_round(rd), arrange=arrange_round)


class ContainerSharePlaybook(Playbook):
    name = "container_share"
    summary = "Players take from and put into one container; one 'in use' lock at a time; nothing duplicated or lost."
    needs_refs = ("container", "item_a", "item_b")
    requires = ("goto_ref", "open_container", "close_container", "take", "put", "inventory_count")

    def run(self, ctx: Ctx) -> None:
        c, a, b = ctx.refs["container"], ctx.refs["item_a"], ctx.refs["item_b"]
        host, other = ctx.host, ctx.clients[0]

        def contents(m: Machine):
            return (m.bot.inventory_count(a, c), m.bot.inventory_count(b, c))

        with ctx.step("everyone walks to the container"):
            for n, r in ctx.on_all(lambda m: m.bot.goto_ref(c, 120)).items():
                ctx.must(r, "%s walks to the container" % n)
            init_a, init_b = ctx.same_everywhere("container contents", contents)
            ctx.expect(init_a >= 2, "scenario container must hold at least 2 x %s, has %s" % (a, init_a))
        init = {a: init_a, b: init_b}
        party_before = {k: total_held(ctx, k) for k in (a, b)}
        take_n = max(1, init_a // 2)
        with ctx.step("host takes half of A while the client waits on the lock"):
            ctx.must(host.bot.open_container(c), "host opens the container")
            blocked = other.bot.open_container(c)
            ctx.expect(not blocked, "client opened the container while the host had it open (lock missing)")
            ctx.must(host.bot.take(a, take_n, c), "host takes")
            ctx.must(host.bot.close_container(c), "host closes")
            ctx.settle()
            ctx.same_everywhere("container contents after host take", contents)
            ctx.expect(contents(host)[0] == init_a - take_n,
                       "container has %s x %s after taking %d of %d" % (contents(host)[0], a, take_n, init_a))
        with ctx.step("client takes the rest and puts something back"):
            ctx.must(other.bot.open_container(c), "client opens after the host closed")
            left = contents(other)[0]
            ctx.must(other.bot.take(a, left, c), "client takes the rest of A")
            ctx.must(other.bot.put(a, 1, c), "client puts one A back")
            ctx.must(other.bot.close_container(c), "client closes")
            ctx.settle()
            final = ctx.same_everywhere("container contents at the end", contents)
            ctx.expect(final[0] == 1, "expected 1 x %s left in the container, got %s" % (a, final[0]))
        with ctx.step("nothing duplicated or lost"):
            for k in (a, b):
                inside = host.bot.inventory_count(k, c)
                held = total_held(ctx, k)
                want = party_before[k] + init[k]
                ctx.expect(inside + held == want, "%s: %d in the container + %d held != %d at the start" % (k, inside, held, want))


# ---------------------------------------------------------------------------------------------
def build_loot(n: int = 2, bugs: Optional[Set[str]] = None, **kw) -> Tuple[FakeCoop, dict]:
    w = FakeCoop(n, bugs, **kw)
    w.add_ref(ITEM_REF, "item", pos=(500, 500, 0), cell="town", base=ITEM_BASE)
    for s in range(n):
        w.set_player(s, pos=(0, 0, 0), cell="town")
    return w, {"item": ITEM_REF, "item_base": ITEM_BASE}


def build_container(n: int = 2, bugs: Optional[Set[str]] = None, **kw) -> Tuple[FakeCoop, dict]:
    w = FakeCoop(n, bugs, **kw)
    w.add_ref(CONTAINER, "container", pos=(500, 500, 0), cell="town", items={"coin": 10, "healing_pack": 3})
    for s in range(n):
        w.set_player(s, pos=(0, 0, 0), cell="town")
    return w, {"container": CONTAINER, "item_a": "coin", "item_b": "healing_pack"}
