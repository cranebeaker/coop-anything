"""A machine that left (ctx.absent) is never polled: a real game copy that crashed or quit has no port to answer on.

Found on Thronefall (games/thronefall): the client copy was killed mid-match to test a rejoin, and ctx.wait_until kept
polling it through _tick, so the step died with a connection error instead of checking the host.
"""
import tempfile
import unittest

import support  # noqa: F401  (puts coopbots on the path)
from coopbots import core
from coopbots.adapter import ActionAdapter


class Copy(ActionAdapter):
    """A game copy whose control port goes away when it is killed."""

    def __init__(self):
        self.dead = False
        self.polls_after_death = 0

    def status(self):
        if self.dead:
            self.polls_after_death += 1
            raise ConnectionResetError("the game copy is gone")
        return {"seq": 1}

    def snapshot(self):
        if self.dead:
            raise ConnectionResetError("the game copy is gone")
        return {"shared": {"x": 1}, "per_player": {}}


class ClientCrashes(core.Playbook):
    name = "client_crashes"
    summary = "the client copy dies mid-step; the host is checked on its own"

    def run(self, ctx):
        client = ctx.clients[0]
        with ctx.step("client crashes", compare=False):
            client.bot.dead = True
            ctx.absent.add(client.name)
            ctx.wait_until("host still answers", lambda: ctx.host.bot.status()["seq"] == 1, 2)
            ctx.pause(0.2)
        with ctx.step("host alone"):
            ctx.expect(ctx.host.bot.status()["seq"] == 1, "host answers")


class AbsentMachines(unittest.TestCase):
    def test_a_machine_that_left_is_not_polled(self):
        host, client = Copy(), Copy()
        machines = [core.Machine(0, "host", host, 0), core.Machine(1, "client1", client, 1)]
        res = core.run_playbook(ClientCrashes(), machines, core.DictComparer(), tempfile.mkdtemp(prefix="coopbots-"),
                                {}, {"settle_s": 2.0, "settle_poll_s": 0.01}, lambda s: None)
        self.assertTrue(res.ok, res.reason)
        self.assertEqual(client.polls_after_death, 0, "the absent client was polled")


if __name__ == "__main__":
    unittest.main()
