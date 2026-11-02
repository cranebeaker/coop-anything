"""Session playbooks: save_load_rejoin, spawn_once."""
from typing import Optional, Set, Tuple

from ..core import Ctx, Machine, Playbook
from ..fakecoop import FakeCoop, norm

QUEST, REWARD = "side_quest", "coin"


class SaveLoadRejoinPlaybook(Playbook):
    name = "save_load_rejoin"
    summary = "Host saves; a client drops and rejoins with no duplicate rewards; host loads the older save and everyone rebases."
    needs_refs = ("quest", "stage_a", "stage_b")
    requires = ("set_stage", "stage_done", "quest_stage", "save_game", "load_game", "session_leave", "session_join", "rebases")

    def run(self, ctx: Ctx) -> None:
        q, a, b = ctx.refs["quest"], int(ctx.refs["stage_a"]), int(ctx.refs["stage_b"])
        item = ctx.refs.get("reward_item")
        save = ctx.refs.get("save_name", "playbook_save")
        host, guest = ctx.host, ctx.clients[0]
        stage_s = ctx.opts.get("stage_s", 15.0)

        def held(m: Machine) -> int:
            return m.bot.inventory_count(item) if item else 0
        if not item:
            ctx.notes.append("no reward_item in the refs: the reward-duplication checks are skipped")
        with ctx.step("host sets stage %d and saves" % a):
            ctx.must(host.bot.set_stage(q, a), "host sets stage %d" % a)
            ctx.wait_until("stage %d everywhere" % a, lambda: all(m.bot.stage_done(q, a) for m in ctx.machines), timeout_s=stage_s)
            ctx.settle()
            ctx.must(host.bot.save_game(save), "host saves")
        before = {}
        with ctx.step("a client drops out; the host moves the quest on", settle=False, compare=False):
            ctx.must(guest.bot.session_leave(), "%s leaves" % guest.name)
            ctx.absent.add(guest.name)
            before = {m.name: held(m) for m in ctx.machines}
            ctx.must(host.bot.set_stage(q, b), "host sets stage %d" % b)
            ctx.settle()
            ctx.expect(not guest.bot.stage_done(q, b), "the absent client saw the new stage")
        with ctx.step("the client rejoins and catches up; rewards arrive once"):
            ctx.must(guest.bot.session_join(), "%s rejoins" % guest.name)
            ctx.absent.discard(guest.name)
            ctx.wait_until("client caught up", lambda: guest.bot.stage_done(q, b), timeout_s=stage_s)
            ctx.settle()
            if item:
                host_gain, got = held(host) - before[host.name], held(guest) - before[guest.name]
                ctx.expect(got == host_gain, "%s gained %d x %s on rejoin, the host gained %d for the same stage (duplicate or missing reward)"
                           % (guest.name, got, item, host_gain))
        after_rejoin = held(guest)
        with ctx.step("host loads the older save: everyone rebases", allow_rebase=True):
            rb0 = {m.name: m.bot.rebases() for m in ctx.clients}
            ctx.must(host.bot.load_game(save), "host loads the save")
            ctx.wait_until("clients follow the host back", lambda: all(not m.bot.stage_done(q, b) for m in ctx.machines), timeout_s=stage_s * 2)
            ctx.settle()
            for m in ctx.clients:
                ctx.expect(m.bot.rebases() > rb0[m.name], "%s did not rebase (rebases still %d)" % (m.name, rb0[m.name]))
            ctx.same_everywhere("stage", lambda m: m.bot.quest_stage(q))
            if item:
                ctx.expect(held(guest) == after_rejoin, "%s item count changed on rebase (%d -> %d): rewards re-delivered" % (guest.name, after_rejoin, held(guest)))
        with ctx.step("the client drops and rejoins again: still no duplicate rewards"):
            c0 = held(guest)
            ctx.must(guest.bot.session_leave(), "%s leaves again" % guest.name)
            ctx.absent.add(guest.name)
            ctx.pause(0.5)
            ctx.must(guest.bot.session_join(), "%s rejoins again" % guest.name)
            ctx.absent.discard(guest.name)
            ctx.settle()
            if item:
                ctx.expect(held(guest) == c0, "rewards duplicated on the second rejoin (%d -> %d)" % (c0, held(guest)))


class SpawnOncePlaybook(Playbook):
    name = "spawn_once"
    summary = "The host spawns an encounter; every machine sees exactly one, and it stays one (no double spawn)."
    needs_refs = ("marker", "base")
    requires = ("goto_ref", "spawn", "ref_count")
    SETTLE_ROUNDS = 5

    def run(self, ctx: Ctx) -> None:
        marker, base = ctx.refs["marker"], ctx.refs["base"]

        def count(m: Machine) -> int:
            return m.bot.ref_count(base)

        with ctx.step("everyone stands at the marker, nothing of that kind exists yet"):
            for n, r in ctx.on_all(lambda m: m.bot.goto_ref(marker, 300)).items():
                ctx.must(r, "%s walks to the marker" % n)
            ctx.expect(ctx.same_everywhere("count before", count) == 0, "the encounter already exists before the spawn")
        with ctx.step("host spawns one: exactly one appears on every machine"):
            ctx.must(ctx.host.bot.spawn(base, marker), "host spawns")
            ctx.wait_until("one on every machine", lambda: all(count(m) >= 1 for m in ctx.machines), timeout_s=ctx.opts.get("spawn_s", 15.0))
            ctx.settle()
            ctx.expect(ctx.same_everywhere("count after spawn", count) == 1, "counts after the spawn: %s (must be 1 everywhere)" % ctx.on_all(count))
        with ctx.step("it stays one after a quiet period"):
            for _ in range(self.SETTLE_ROUNDS):
                ctx.pause(1.0)
                ctx.settle()
            ctx.expect(ctx.same_everywhere("count later", count) == 1, "counts later: %s (double spawn)" % ctx.on_all(count))


# ---------------------------------------------------------------------------------------------
def build_saveload(n: int = 3, bugs: Optional[Set[str]] = None, **kw) -> Tuple[FakeCoop, dict]:
    rules = {(QUEST, 20): [{"op": "reward", "id": "r20", "item": REWARD, "count": 5}],
             (QUEST, 10): [{"op": "reward", "id": "r10", "item": REWARD, "count": 2}]}
    w = FakeCoop(n, bugs, quest_rules=rules, **kw)
    w.add_quest(QUEST)
    return w, {"quest": QUEST, "stage_a": 10, "stage_b": 20, "reward_item": REWARD}


def build_spawn(n: int = 2, bugs: Optional[Set[str]] = None, **kw) -> Tuple[FakeCoop, dict]:
    w = FakeCoop(n, bugs, **kw)
    w.add_ref("spawn_point", "marker", pos=(100, 0, 0), cell="field", base="marker_type")
    for s in range(n):
        w.set_player(s, pos=(0, 0, 0), cell="field")
    return w, {"marker": "spawn_point", "base": "enemy_type"}
