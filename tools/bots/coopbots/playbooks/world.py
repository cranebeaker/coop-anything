"""World-object playbooks: door_lock, fast_travel_tether."""
from typing import Optional, Set, Tuple

from ..core import Ctx, Playbook
from ..fakecoop import FakeCoop
from .common import walk_to

DOOR, LOCKED_DOOR, DEVICE, LOCKED_BOX = "door", "locked_door", "terminal", "locked_box"
MARKER, ANCHOR, HOSTILE = "far_marker", "far_anchor", "guard"


class DoorLockPlaybook(Playbook):
    name = "door_lock"
    summary = "Door, device and lock matrix: each acts from host and from client; state is the same everywhere."
    needs_refs = ("door", "locked_door", "device", "device_entry", "locked_box", "key")
    requires = ("goto_ref", "open_door", "close_door", "unlock", "use_device", "device_entry_used", "is_locked",
                "door_open", "open_container", "close_container")

    def run(self, ctx: Ctx) -> None:
        door, ldoor, dev = ctx.refs["door"], ctx.refs["locked_door"], ctx.refs["device"]
        entry, box, key = ctx.refs["device_entry"], ctx.refs["locked_box"], ctx.refs["key"]
        host, client = ctx.host, ctx.clients[0]
        with ctx.step("everyone walks to the objects"):
            for ref in (door, dev):
                for n, r in ctx.on_all(lambda m, ref=ref: m.bot.goto_ref(ref, 120)).items():
                    ctx.must(r, "%s walks to %s" % (n, ref))
        # every action is done by the host once and by the client once
        for who in (host, client):
            with ctx.step("%s opens and closes the plain door" % who.name):
                ctx.must(who.bot.open_door(door), "%s opens the door" % who.name)
                ctx.settle()
                ctx.expect(ctx.same_everywhere("door open", lambda m: m.bot.door_open(door)) is True, "door should be open everywhere")
                ctx.must(who.bot.close_door(door), "%s closes the door" % who.name)
                ctx.settle()
                ctx.expect(ctx.same_everywhere("door open", lambda m: m.bot.door_open(door)) is False, "door should be closed everywhere")
        with ctx.step("locked door refuses without a key"):
            ctx.expect(ctx.same_everywhere("locked", lambda m: m.bot.is_locked(ldoor)) is True, "door should start locked everywhere")
            ctx.expect(not client.bot.open_door(ldoor), "client opened a locked door without unlocking it")
            ctx.settle()
            ctx.expect(not host.bot.door_open(ldoor), "host sees the locked door open")
        with ctx.step("client unlocks with the key, host opens it"):
            ctx.must(client.bot.unlock(ldoor, key), "client unlocks the door")
            ctx.settle()
            ctx.expect(ctx.same_everywhere("locked", lambda m: m.bot.is_locked(ldoor)) is False, "door should be unlocked everywhere")
            ctx.must(host.bot.open_door(ldoor), "host opens the unlocked door")
            ctx.settle()
            ctx.expect(ctx.same_everywhere("door open", lambda m: m.bot.door_open(ldoor)) is True, "unlocked door should be open everywhere")
        for who in (client, host):
            with ctx.step("%s uses the device" % who.name):
                ctx.must(who.bot.use_device(dev, entry), "%s uses device entry %s" % (who.name, entry))
                ctx.settle()
                # many engines keep no per-device flag for other machines, so this asks the player that used it
                ctx.expect(who.bot.device_entry_used(dev, entry) is True, "%s's device entry %s is not recorded" % (who.name, entry))
        with ctx.step("the device unlocks the box; client opens it"):
            ctx.expect(ctx.same_everywhere("box locked", lambda m: m.bot.is_locked(box)) is False,
                       "the device entry should have unlocked the box on every machine")
            ctx.must(client.bot.open_container(box), "client opens the box")
            ctx.must(client.bot.close_container(box), "client closes the box")


class FastTravelTetherPlaybook(Playbook):
    name = "fast_travel_tether"
    summary = "Fast travel needs party consent and no combat; a tethered player cannot wander off."
    needs_refs = ("marker", "anchor", "tether_radius", "far_pos")
    requires = ("goto_ref", "fast_travel", "consent", "distance_to", "cell")
    MARKER_SLACK = 800

    def run(self, ctx: Ctx) -> None:
        marker, anchor = ctx.refs["marker"], ctx.refs["anchor"]
        radius, far = float(ctx.refs["tether_radius"]), ctx.refs["far_pos"]
        hostile = ctx.refs.get("hostile")
        host, others = ctx.host, ctx.clients
        if hostile:
            with ctx.step("fast travel is refused in combat", compare=False):
                for m in others:        # consent is given, so only the combat rule can refuse
                    ctx.must(m.bot.consent("fasttravel", yes=True), "%s consents" % m.name)
                ctx.expect(not host.bot.fast_travel(marker), "fast travel started while hostile actors were awake")
                for m in others:
                    m.bot.consent("fasttravel", yes=False)
            with ctx.step("everybody kills the hostile first"):
                ctx.equip_all()
                ctx.on_all(lambda m: m.bot.goto_ref(hostile, 600))
                for _ in range(12):
                    ctx.on_all(lambda m: (m.bot.aim_at(hostile), m.bot.fire(1)))
                    if not host.bot.target_alive(hostile):
                        break
                ctx.settle()
                ctx.same_everywhere("hostile alive", lambda m: m.bot.target_alive(hostile))
                ctx.expect(not host.bot.target_alive(hostile), "hostile survived 12 volleys")
        with ctx.step("fast travel without consent is refused", compare=False):
            ctx.expect(not host.bot.fast_travel(marker), "fast travel went ahead without the other players' consent")
        with ctx.step("everyone consents: all arrive at the marker"):
            for m in others:
                ctx.must(m.bot.consent("fasttravel", yes=True), "%s consents" % m.name)
            ctx.must(host.bot.fast_travel(marker), "host fast travels")
            ctx.settle()
            ctx.pause(3.0)          # long enough for a glitch right after the jump to show
            cells = {m.name: m.bot.cell() for m in ctx.machines}
            ctx.expect(len(set(cells.values())) == 1, "players ended in different areas: %s" % cells)
            for m in ctx.machines:
                d = m.bot.distance_to(marker)
                ctx.expect(d <= self.MARKER_SLACK, "%s is %.0f units from the marker after arriving" % (m.name, d))
        with ctx.step("the tether holds a wandering player", compare=False):
            wanderer = others[0]
            walk_to(wanderer, far)
            ctx.settle()
            d = wanderer.bot.distance_to(anchor)
            ctx.expect(d <= radius * 1.1, "%s is %.0f units from the anchor, tether radius is %g" % (wanderer.name, d, radius))


# ---------------------------------------------------------------------------------------------
def build_doors(n: int = 2, bugs: Optional[Set[str]] = None, **kw) -> Tuple[FakeCoop, dict]:
    w = FakeCoop(n, bugs, **kw)
    w.add_ref(DOOR, "door", pos=(100, 0, 0), cell="vault")
    w.add_ref(LOCKED_DOOR, "door", pos=(200, 0, 0), cell="vault", locked=True, key="KEY")
    w.add_ref(DEVICE, "terminal", pos=(120, 0, 0), cell="vault", unlocks=LOCKED_BOX)
    w.add_ref(LOCKED_BOX, "container", pos=(150, 0, 0), cell="vault", locked=True, items={"coin": 5})
    for s in range(n):
        w.set_player(s, pos=(0, 0, 0), cell="vault", inv={"WEAPON": 1, "KEY": 1})
    return w, {"door": DOOR, "locked_door": LOCKED_DOOR, "device": DEVICE, "device_entry": "unlock",
               "locked_box": LOCKED_BOX, "key": "KEY"}


def build_travel(n: int = 2, bugs: Optional[Set[str]] = None, **kw) -> Tuple[FakeCoop, dict]:
    w = FakeCoop(n, bugs, tether={"anchor": ANCHOR, "radius": 3000}, **kw)
    w.add_ref(MARKER, "marker", pos=(50000, 0, 0), cell="far_town")
    w.add_ref(ANCHOR, "marker", pos=(50000, 0, 0), cell="far_town")
    w.add_ref(HOSTILE, "raider", pos=(1500, 0, 0), cell="home", base="guard_type")
    for s in range(n):
        w.set_player(s, pos=(0, 100 * s, 0), cell="home")
    return w, {"marker": MARKER, "anchor": ANCHOR, "tether_radius": 3000, "hostile": HOSTILE, "weapon": "WEAPON",
               "far_pos": {"x": 90000, "y": 0, "z": 0, "cell": "far_town"}}
