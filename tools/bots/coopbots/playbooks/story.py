"""Story playbooks: consent_vote (and consent_vote_guest_declines), quest_sync."""
from typing import Optional, Set, Tuple

from ..core import Ctx, Playbook
from ..fakecoop import FakeCoop, norm
from .common import all_stage_done, expect_stage_everywhere

BARRIER_Q, BARRIER_STAGE = "intro_quest", 30
SYNC_Q = "town_quest"
SYNC_STAGES = [(5, [(3, "displayed", 1)]), (10, []), (20, [(20, "completed", 1)]), (25, [])]
CONFIRM_NPC, CONFIRM_Q = "gate_keeper", "final_quest"


class ConsentVotePlaybook(Playbook):
    name = "consent_vote"
    summary = "A one-way step waits for every player: a barrier (each player passes) and a ready check (each player answers)."
    min_players = 2
    requires = ("goto_ref", "set_stage", "stage_done", "quest_stage", "barrier_pass", "activate", "answer_prompt", "consent")
    final_ready = True          # False: stop after the 'not yet' veto (the world stays reusable)

    def run(self, ctx: Ctx) -> None:
        ctx.expect("barrier_quest" in ctx.refs or "confirm_ref" in ctx.refs, "scenario gives neither a barrier nor a ready check")
        if "barrier_quest" in ctx.refs:
            self.barrier(ctx)
        if "confirm_ref" in ctx.refs:
            self.ready_check(ctx)

    def barrier(self, ctx: Ctx) -> None:
        q, n = ctx.refs["barrier_quest"], int(ctx.refs["barrier_stage"])
        with ctx.step("barrier: host asks for the one-way stage, nothing happens yet"):
            ctx.must(ctx.host.bot.set_stage(q, n), "host requests the stage")
            ctx.pause(1.0)
            expect_stage_everywhere(ctx, q, n, False)
        passers = ctx.machines[:]
        for i, m in enumerate(passers[:-1]):
            with ctx.step("barrier: %s passes (%d of %d), still waiting" % (m.name, i + 1, len(passers))):
                ctx.must(m.bot.barrier_pass(q), "%s passes" % m.name)
                ctx.pause(0.5)
                expect_stage_everywhere(ctx, q, n, False)
        with ctx.step("barrier: the last player passes, the stage applies everywhere once"):
            ctx.must(passers[-1].bot.barrier_pass(q), "%s passes" % passers[-1].name)
            ctx.wait_until("stage applied on every machine", lambda: all(all_stage_done(ctx, q, n).values()),
                           timeout_s=ctx.opts.get("barrier_s", 20.0))
            ctx.settle()
            after = ctx.same_everywhere("current stage", lambda m: m.bot.quest_stage(q))
            ctx.expect(after >= n, "current stage is %s, expected at least %s" % (after, n))

    def ready_check(self, ctx: Ctx) -> None:
        """The host triggers a confirm box and answers it; every other player answers Ready or Not yet on a consent prompt."""
        ref, q = ctx.refs["confirm_ref"], ctx.refs["confirm_quest"]
        s_from, s_to = int(ctx.refs["confirm_from"]), int(ctx.refs["confirm_to"])
        accept, decline = int(ctx.refs.get("accept_button", 1)), int(ctx.refs.get("decline_button", 0))
        host, others = ctx.host, ctx.clients
        timeout = ctx.opts.get("ui_s", 10.0)

        def ask() -> None:
            ctx.must(host.bot.activate(ref), "host activates the confirm object")
            ctx.must(host.bot.answer_prompt(accept), "host presses the accept button")

        def prompts_arrive() -> None:
            def seen() -> bool:
                got = ctx.optional("consent prompt query", lambda: all(m.bot.consent_pending() for m in others), True)
                return bool(got)
            ctx.wait_until("every other player has a Ready / Not yet prompt", seen, timeout_s=timeout)

        with ctx.step("ready check: everybody stands at the object, the quest is at the start stage"):
            for n_, r in ctx.on_all(lambda m: m.bot.goto_ref(ref, 150)).items():
                ctx.must(r, "%s walks to the object" % n_)
            ctx.same_everywhere("quest stage", lambda m: m.bot.quest_stage(q))
            ctx.expect(host.bot.quest_stage(q) == s_from, "quest should be at stage %d" % s_from)
        with ctx.step("ready check: the host declines: nothing changes"):
            ctx.must(host.bot.activate(ref), "host activates the confirm object")
            ctx.must(host.bot.answer_prompt(decline), "host presses the decline button")
            ctx.pause(1.0)
            expect_stage_everywhere(ctx, q, s_to, False)
            ctx.expect(all(m.bot.quest_stage(q) == s_from for m in ctx.machines), "the stage moved after a decline")
        with ctx.step("ready check: a guest answers 'Not yet': the step is vetoed"):
            ask()
            prompts_arrive()
            ctx.must(others[0].bot.consent("ready", quest=q, yes=False), "%s answers Not yet" % others[0].name)
            for m in others[1:]:
                ctx.must(m.bot.consent("ready", quest=q, yes=True), "%s answers Ready" % m.name)
            ctx.pause(1.0)
            expect_stage_everywhere(ctx, q, s_to, False)
            ctx.expect(all(m.bot.quest_stage(q) == s_from for m in ctx.machines), "a 'Not yet' did not stop the step")
        if not self.final_ready:
            return
        with ctx.step("ready check: everybody answers Ready: it applies once everywhere"):
            ask()
            prompts_arrive()
            for m in others:
                ctx.must(m.bot.consent("ready", quest=q, yes=True), "%s answers Ready" % m.name)
            ctx.wait_until("stage %d on every machine" % s_to, lambda: all(all_stage_done(ctx, q, s_to).values()),
                           timeout_s=ctx.opts.get("barrier_s", 30.0))


class ConsentVoteGuestDeclinesPlaybook(ConsentVotePlaybook):
    name = "consent_vote_guest_declines"
    summary = "Ready check where a guest answers 'Not yet': the stage holds on every machine and the step never applies."
    final_ready = False


class QuestSyncPlaybook(Playbook):
    name = "quest_sync"
    summary = "The host walks a quest through its stages; stages and objective flags match on every machine."
    needs_refs = ("quest", "stages")
    requires = ("set_stage", "stage_done", "quest_stage", "start_quest", "quest_running", "objective_displayed", "objective_completed")

    resumable = True

    def run(self, ctx: Ctx) -> None:
        q = ctx.refs["quest"]
        stages = ctx.refs["stages"]            # [(stage, [(objective, "displayed" | "completed", value), ...]), ...]
        stage_s = ctx.opts.get("stage_s", 15.0)
        start = ctx.refs.get("start", True)

        def arrange_before(k: int):
            """The quest as it stands before step k of the stage list: started, and every earlier stage set (idempotent)."""
            def arrange():
                if start and not ctx.host.bot.quest_running(q):
                    ctx.must(ctx.host.bot.start_quest(q), "host starts the quest")
                for n, _ in stages[:k]:
                    if not ctx.host.bot.stage_done(q, n):
                        ctx.must(ctx.host.bot.set_stage(q, n), "host sets stage %s" % n)
                ctx.wait_until("the quest at its earlier stages everywhere",
                               lambda: all(all(all_stage_done(ctx, q, n).values()) for n, _ in stages[:k]), timeout_s=stage_s)
                ctx.settle()
            return arrange

        def begin():
            ctx.must(ctx.host.bot.start_quest(q), "host starts the quest")
            ctx.wait_until("running everywhere", lambda: all(m.bot.quest_running(q) for m in ctx.machines), timeout_s=stage_s)

        def do_stage(n, effs):
            def body():
                ctx.must(ctx.host.bot.set_stage(q, n), "host sets stage %s" % n)
                ctx.wait_until("stage %s on every machine" % n, lambda: all(all_stage_done(ctx, q, n).values()), timeout_s=stage_s)
                ctx.settle()
                cur = ctx.same_everywhere("current stage", lambda m: m.bot.quest_stage(q))
                ctx.expect(cur >= n, "current stage %s is behind stage %s" % (cur, n))
                for obj, kind, val in effs:
                    ask = "objective_displayed" if kind == "displayed" else "objective_completed"
                    ctx.same_everywhere("objective %s %s" % (obj, kind), lambda m, o=obj, a=ask: getattr(m.bot, a)(q, o))
                    for m in ctx.machines:
                        ctx.expect(getattr(m.bot, ask)(q, obj) == bool(val), "%s: objective %s %s should be %s" % (m.name, obj, kind, bool(val)))
            return body

        def repeat():
            n = stages[-1][0]
            snap = {m.name: m.bot.quest_stage(q) for m in ctx.machines}
            ctx.must(ctx.host.bot.set_stage(q, n), "host repeats the stage")
            ctx.pause(0.5)
            ctx.settle()
            ctx.expect({m.name: m.bot.quest_stage(q) for m in ctx.machines} == snap, "repeating a stage moved the quest")

        if start:
            ctx.run_step("host starts the quest", begin, arrange=lambda: None)
        for k, (n, effs) in enumerate(stages):
            ctx.run_step("stage %s" % n, do_stage(n, effs), arrange=arrange_before(k))
        ctx.run_step("repeating a stage changes nothing", repeat, arrange=arrange_before(len(stages)))


# ---------------------------------------------------------------------------------------------
def build_consent(n: int = 3, bugs: Optional[Set[str]] = None, **kw) -> Tuple[FakeCoop, dict]:
    w = FakeCoop(n, bugs, barriers={(BARRIER_Q, BARRIER_STAGE)}, **kw)
    w.add_quest(BARRIER_Q)
    w.add_quest(CONFIRM_Q, [70])
    w.add_ref(CONFIRM_NPC, "actor", pos=(900, 0, 0), cell="hall")
    w.endgames[norm(CONFIRM_NPC)] = {"quest": CONFIRM_Q, "from": 70, "to": 100}
    for s in range(n):
        w.set_player(s, pos=(0, 0, 0), cell="hall")
    return w, {"barrier_quest": BARRIER_Q, "barrier_stage": BARRIER_STAGE, "confirm_ref": CONFIRM_NPC,
               "confirm_quest": CONFIRM_Q, "confirm_from": 70, "confirm_to": 100}


def build_sync(n: int = 2, bugs: Optional[Set[str]] = None, **kw) -> Tuple[FakeCoop, dict]:
    rules = {}
    for st, effs in SYNC_STAGES:
        rules[(SYNC_Q, st)] = [{"op": "obj", "q": norm(SYNC_Q), "o": o, "f": {kind: bool(v)}} for o, kind, v in effs]
    w = FakeCoop(n, bugs, quest_rules=rules, **kw)
    w.add_quest(SYNC_Q)
    return w, {"quest": SYNC_Q, "stages": SYNC_STAGES}
