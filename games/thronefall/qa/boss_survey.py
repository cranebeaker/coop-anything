"""Load each level with the host copy alone and list its waves and Boss-tagged objects (run-connect is not needed:
start copy A with -coopqa host -coopqa-port 47801 -coopqa-unlock).

    python -I boss_survey.py Nordfels Durststein ...
"""
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tfqa import Conn  # noqa: E402


def wait(pred, timeout, step=1.0):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if pred():
            return True
        time.sleep(step)
    return False


def main(levels):
    c = Conn(47801)
    wait(lambda: c.call("status")["step"] in ("host-wait-peer", "connected"), 120)
    for level in levels:
        c.call("open_level", scene=level)
        wait(lambda: c.call("task").get("state") != "running", 60)
        if not wait(lambda: any(e.startswith(("Play Button", "New Run")) for e in c.call("ui")["elements"]), 10, 0.3):
            print(json.dumps({"level": level, "error": "no first page"}))
            continue
        names = [e.split(" [")[0] for e in c.call("ui")["elements"]]
        c.call("ui_apply", name="New Run" if "New Run" in names else "Play Button")
        time.sleep(1)
        try:
            c.call("pick_weapon")
        except RuntimeError:
            pass
        c.call("press_play")
        if not wait(lambda: c.call("status")["scene"] == level, 60):
            print(json.dumps({"level": level, "error": "did not load"}))
            continue
        time.sleep(4)
        w = c.call("waves")
        bosses = [(x["wave"], s) for x in w["waves"] for s in x["spawns"] if "BOSS" in s]
        print(json.dumps({"level": level, "waves": len(w["waves"]), "boss_spawns": bosses, "boss_objects": w["boss_objects"]}))
        sys.stdout.flush()
        c.call("back_to_map")
        wait(lambda: c.call("status")["scene"] == "_LevelSelect", 60)
        time.sleep(3)


if __name__ == "__main__":
    main(sys.argv[1:])
