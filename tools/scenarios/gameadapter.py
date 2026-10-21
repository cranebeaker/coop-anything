"""The game side of the scenario runner: one GameAdapter per running game instance (one per player).

Write one class for your game and the runner does the rest. The runner never talks to the game directly. It gives each
adapter plain strings: ids taken from the scenario's `objects` glossary and the numbers and names written in the steps.

    class MyGame(GameAdapter):
        OPS = {"op:kill", "op:move", "check:dead", "check:state", "action:fast_travel"}

        def do(self, op, args):              # args: {"ref": "<id>"} for op "kill"
            ...
        def read(self, kind, args):          # kind "dead", args {"ref": "<id>"} -> 0 or 1
            ...
        def load_save(self, name): ...
        def save_game(self, name): ...
        def reset_state(self, ids): ...      # put these state keys back to their starting value, no reload

Anything the adapter cannot do is simply left out of OPS. A step that needs it is reported `blocked`, never `pass`.

Optional hooks. Each one has a default that means "this game cannot tell", and the runner then keeps its older behaviour
(a fixed pause, no prepared save, no id check). Implement the ones your game can answer:

    sync_status()            -> dict | None   readings for event-driven sync waits (see below)
    popup_count()            -> int | None    blocking popups open right now (message boxes, notices); None = cannot tell
    dismiss_popup()          -> bool          press away one open popup; True when one was pressed
    prepared_names(save)     -> [str]         names a prepared copy of `save` is tried under, best first
    save_exists(name)        -> bool | None   is this save there; None = cannot tell
    save_stamp(name)         -> json | None   fingerprint of a save (size and time are enough); None = cannot tell
    ROLLING_SAVES            -> tuple of names the game rewrites by itself; their stamp proves nothing
    data_index()             -> dict | DataIndex | None   the ids the game data knows, for validating scenarios

sync_status() answers for THIS instance, every key optional:

    role         "host" or "client" (anything else, or None, means this instance is not in a session)
    head         host: how far the host's committed stream is (any number that only grows)
    applied      client: how much of that stream the client applied (same unit as head)
    joined       client: True once it joined the host
    syncing      client: True while it still applies its starting baseline
    connected    client: False when the link is down (default True)
    rejected     client: text of a join refusal, or None
    digests      host: counters {"received", "ok", "mismatch", "stale", "rateLimited", "repairs"} of state digests
                 the clients sent and the host compared (a digest is a cheap checksum of the shared state)
    desyncs      host: newest mismatching entries [{"id": report number (grows), "key", "host", "client", "repaired"}]

The runner waits on these after a reload and returns the moment the condition holds. It gives up early, with a plain
reason, when nothing moves. A game that returns None (or raises NotImplementedError or ScenarioError) keeps the fixed pause.

data_index() returns {"<id>": "<kind>"} where kind is one of scenlib.KINDS, a list of kinds, or None for "any kind".
Ids the scenario uses that are missing get an error that names the nearest known id. See scenlib.DataIndex.
"""


class ScenarioError(Exception):
    """An action was refused or failed. The runner records it as an `error` step with this text."""


class GameAdapter:
    OPS = None   # None = "I can do everything"; otherwise a set of 'op:<op>', 'check:<kind>' and 'action:<name>' names
    ROLLING_SAVES = ()   # save names the game rewrites by itself (autosave, quicksave): a prepared copy is not checked against them

    def capabilities(self):
        return None if self.OPS is None else set(self.OPS)

    def do(self, op, args):
        """Run one non-check op on this instance. `op` is one of scenlib.OPS (not `wait`, `save`, `load`).
        Raise ScenarioError when the game refuses or the result is not what was asked for."""
        raise NotImplementedError("do(%r)" % op)

    def read(self, kind, args):
        """Answer one check kind (see scenlib.CHECK_KINDS) as a number. Booleans are 1 and 0."""
        raise NotImplementedError("read(%r)" % kind)

    def load_save(self, name):
        raise NotImplementedError("load_save")

    def save_game(self, name):
        raise NotImplementedError("save_game")

    def reset_state(self, ids):
        """Put the named state keys (quests, flags, script variables) back to their starting value without a reload."""
        raise NotImplementedError("reset_state")

    # ---- optional hooks: the defaults say "this game cannot tell"
    def sync_status(self):
        """Readings of this instance for event-driven sync waits (module docstring). None = no readings."""
        return None

    def popup_count(self):
        """How many blocking popups are open on this instance. None = cannot tell (prepared saves are then not possible)."""
        return None

    def dismiss_popup(self):
        """Press away one open popup. Return True when one was pressed. Raise ScenarioError when it is closing already."""
        raise NotImplementedError("dismiss_popup")

    def prepared_names(self, save):
        """Names a prepared copy of `save` is tried under, best first. A second name is for an engine that only takes
        certain prefixes. Names use letters, digits, underscore and hyphen."""
        return ["coop_%s_ready" % save]

    def save_exists(self, name):
        """True or False when the game can tell whether a save is there, None when it cannot."""
        return None

    def save_stamp(self, name):
        """A JSON value that changes when the save file changes (size and time), or None when the game cannot tell."""
        return None

    def data_index(self):
        """The ids the game data knows: {"<id>": "<kind>" | [kinds] | None}, a scenlib.DataIndex, or None for no index."""
        return None
