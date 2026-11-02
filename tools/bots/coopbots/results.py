"""The action result model: what every bot action returns, and how an action says WHY it failed.

    ActionResult(ok, reason, data, action, elapsed_s)

`reason` reads "code: what the bot saw". The code is short and stable ("stuck", "too_close", "holstered", "menu"), so a
playbook or a script can branch on `result.code`. The part after the colon is for the human who reads a failed step:
the distance that was left, the menu that was in the way, the weapon state. A failure with only a code is a weak
failure: say what you saw.

Actions are written with a precondition (is the world in a state where this can work?), the action itself, and a
postcondition (did the world really change?). See `ActionAdapter.act` in adapter.py. A failed check raises `Fail`, which the
adapter turns into a failed ActionResult. An ordinary refusal is never an exception: only a dead game or a dead link is.

The `Why` class builds the failures that most games share, so every adapter words them the same way.
"""
import time
from dataclasses import dataclass, field
from typing import Any, Dict

# Failure codes the shared helpers use. An adapter may add its own; keep them short and lower case.
CODES = ("stuck", "too_close", "too_steep", "menu", "message_box", "holstered", "precondition", "postcondition", "timeout",
         "refused", "not_found", "dead", "unsupported")


@dataclass
class Result:
    ok: bool
    reason: str = ""
    data: Dict[str, Any] = field(default_factory=dict)

    def __bool__(self) -> bool:
        return self.ok

    @property
    def code(self) -> str:
        """The short failure code. Compare codes with this, never with the whole reason."""
        return self.reason.split(":", 1)[0].strip()


@dataclass
class ActionResult(Result):
    action: str = ""
    elapsed_s: float = 0.0

    def __str__(self) -> str:
        return "%s: %s (%.2fs)%s" % (self.action or "action", "ok" if self.ok else "FAILED " + self.reason, self.elapsed_s,
                                     " %s" % (self.data,) if self.data else "")

    def to_json(self) -> dict:
        return {"action": self.action, "ok": self.ok, "reason": self.reason, "code": "" if self.ok else self.code,
                "elapsed_s": round(self.elapsed_s, 3), "data": self.data}

    @classmethod
    def of(cls, res: Result, action: str, elapsed_s: float = 0.0) -> "ActionResult":
        if isinstance(res, ActionResult):
            return res
        return cls(res.ok, res.reason, dict(res.data), action, elapsed_s)


class Fail(Exception):
    """Raise inside an action (or a pre or postcondition) to fail it with a code and a say-why text."""

    def __init__(self, code: str, say: str = "", **data: Any):
        self.code = code
        self.say = say
        self.data = data
        self.reason = "%s: %s" % (code, say) if say else code
        super().__init__(self.reason)


class ActionFailed(Exception):
    """Raised instead of returning a failed result when an adapter runs in strict mode (adapter.strict = True)."""

    def __init__(self, result: ActionResult):
        super().__init__(str(result))
        self.result = result


def ensure(cond: Any, code: str, say: str = "", **data: Any) -> None:
    """A one line check for pre and postconditions: ensure(count == 1, "postcondition", "the item count did not move")."""
    if not cond:
        raise Fail(code, say, **data)


class Why:
    """Ready made failures with the wording every adapter should share. Use as `raise Why.stuck(left, 150)`."""

    @staticmethod
    def stuck(left: float, wanted: float, note: str = "") -> Fail:
        """A walk that ended too far from where it should: how many units were left."""
        say = "%.0f units left (wanted within %g)" % (left, wanted)
        return Fail("stuck", say + ("; " + note if note else ""), distance=left, wanted=wanted)

    @staticmethod
    def too_close(distance: float, minimum: float) -> Fail:
        return Fail("too_close", "the player is %.0f units from the target (minimum %g); stand further away" % (distance, minimum),
                    distance=distance, minimum=minimum)

    @staticmethod
    def too_steep(pitch: float, limit: float, distance: float) -> Fail:
        return Fail("too_steep", "aiming at the target needs %.0f degrees of pitch (limit %g) at %.0f units; "
                    "move so the target is closer to eye level" % (pitch, limit, distance), pitch=pitch, limit=limit, distance=distance)

    @staticmethod
    def menu_open(menu: str = "a menu") -> Fail:
        return Fail("menu", "%s is open and takes the input; close it first" % menu, menu=menu)

    @staticmethod
    def message_box(text: str = "") -> Fail:
        return Fail("message_box", "a message box is up%s and blocks the action" % (" (%s)" % text if text else ""), text=text)

    @staticmethod
    def holstered(weapon: str = "") -> Fail:
        return Fail("holstered", "the weapon%s is holstered; draw it first" % (" " + weapon if weapon else ""), weapon=weapon)

    @staticmethod
    def postcondition(say: str, **data: Any) -> Fail:
        return Fail("postcondition", say, **data)

    @staticmethod
    def precondition(say: str, **data: Any) -> Fail:
        return Fail("precondition", say, **data)


def now() -> float:
    return time.monotonic()
