"""Social playbooks: dialogue_spectator."""
from typing import Optional, Set, Tuple

from ..core import Ctx, Playbook
from ..fakecoop import FakeCoop

NPC = "shopkeeper"
TOPICS = ["greeting", "ask_about_town"]


class DialogueSpectatorPlaybook(Playbook):
    name = "dialogue_spectator"
    summary = "One player talks, everybody else spectates and is released afterwards; roles swap."
    needs_refs = ("npc", "topics")
    requires = ("goto_ref", "talk", "pick_topic", "end_dialogue", "in_dialogue", "spectating")

    def run(self, ctx: Ctx) -> None:
        npc, topics = ctx.refs["npc"], list(ctx.refs["topics"])
        order = [ctx.host] + ctx.clients
        # refs["speakers"] == "guest": only the first guest talks (a real conversation cannot be repeated)
        speakers = order[1:2] if ctx.refs.get("speakers") == "guest" else order[:2]
        wait_s = ctx.opts.get("ui_s", 10.0)
        for speaker in speakers:
            others = [m for m in ctx.machines if m is not speaker]
            with ctx.step("%s talks, the others spectate" % speaker.name):
                ctx.must(speaker.bot.goto_ref(npc, 150), "%s walks to the NPC" % speaker.name)
                ctx.must(speaker.bot.talk(npc), "%s starts the dialogue" % speaker.name)
                ctx.wait_until("every other machine shows the spectator view", lambda: all(m.bot.spectating() for m in others), timeout_s=wait_s)
                ctx.expect(sum(1 for m in ctx.machines if m.bot.in_dialogue()) == 1, "more than one player is in this dialogue as speaker")
                ctx.expect(not others[0].bot.talk(npc), "%s started a second dialogue with the same NPC" % others[0].name)
                for t in topics:
                    ctx.must(speaker.bot.pick_topic(t), "%s picks topic %s" % (speaker.name, t))
                    ctx.settle()
                    ctx.expect(all(m.bot.spectating() for m in others), "a spectator was dropped mid-dialogue")
            with ctx.step("%s ends the dialogue, the others are released" % speaker.name):
                ctx.must(speaker.bot.end_dialogue(), "%s ends the dialogue" % speaker.name)
                ctx.wait_until("spectators released", lambda: not any(m.bot.spectating() for m in ctx.machines), timeout_s=wait_s)
                ctx.expect(not any(m.bot.in_dialogue() for m in ctx.machines), "somebody is still in dialogue")
                ctx.settle()
                dq = ctx.refs.get("done_quest")
                if dq:       # the conversation's end script must have run, on every machine
                    st = int(ctx.refs["done_stage"])
                    ctx.wait_until("%s stage %s on every machine" % (dq, st), lambda: all(m.bot.stage_done(dq, st) for m in ctx.machines),
                                   timeout_s=ctx.opts.get("stage_s", 15.0))


# ---------------------------------------------------------------------------------------------
def build_dialogue(n: int = 3, bugs: Optional[Set[str]] = None, **kw) -> Tuple[FakeCoop, dict]:
    w = FakeCoop(n, bugs, **kw)
    w.add_ref(NPC, "actor", pos=(300, 0, 0), cell="town")
    for s in range(n):
        w.set_player(s, pos=(0, 0, 0), cell="town")
    return w, {"npc": NPC, "topics": list(TOPICS)}
