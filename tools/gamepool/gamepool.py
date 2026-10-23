#!/usr/bin/env python3
"""Run several copies of a Windows game on one PC for tests. One command line for everything.

    python -I tools/gamepool/gamepool.py [--config game.json] COMMAND [options]

Copies (see README.md, "Quick start")
    provision --n 2        build n hard-linked, isolated installs (--pairs N: N host/client pairs)
    show / destroy         describe or remove them
    up --n 2               take game slots, start the copies one by one, wait until ready
    up --pairs 3           3 host/client pairs, each on its own port (refuses when the free RAM is too small)
    status [--pairs]       which copies exist and which run; --pairs: one row per host/client pair
    swap --instances 0,1   ask the adapter to swap new code into running copies (hot reload hook)
    down                   kill only the copies `up` started, free the slots
    job --id X --n 2 -- CMD     run CMD with GAMEPOOL_INSTANCES=0,1 and collect logs
    smoke --n 2 --minutes 10    start copies and check them for a while
    probe --seconds 30          log the window states of one fresh copy
Warm pool
    pool up --size 2       keep n copies running and hand them out as leases
    pool status | lease | release | down
Helpers
    lowcost apply|doc|verify    cheap per-instance settings from the config
    crashlog DEST               collect logs and the Windows crash record
    measure --label L           RAM and CPU of the pooled copies

--config FILE (or env GAMEPOOL_CONFIG) names the per-game JSON. State lives in env GAMEPOOL_DIR, else
<repo>/local/gamepool (git ignores local/). Every command only ever touches private instance folders and
processes it started itself.
"""
import argparse
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gplib.config import Config, ConfigError, expand  # noqa: E402

CONFIGLESS_POOL = {"status", "lease", "release", "down"}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config")
    ap.add_argument("command", choices=["provision", "show", "destroy", "up", "down", "status", "swap", "job", "smoke", "probe",
                                        "pool", "daemon", "lowcost", "crashlog", "measure"])
    ap.add_argument("rest", nargs=argparse.REMAINDER)
    a = ap.parse_args(argv)
    rest = list(a.rest)
    cfg_path = a.config or os.environ.get("GAMEPOOL_CONFIG")
    try:
        if a.command == "pool" and rest and rest[0] in CONFIGLESS_POOL:
            from gplib import poolcli
            return poolcli.main(cfg_path or "", rest)
        if a.command == "measure":
            from gplib import measure
            return measure.main(None, rest)
        cfg = Config.load(cfg_path)
        if a.command in ("provision", "show", "destroy"):
            from gplib import instances
            return instances.main(cfg, [a.command] + rest)
        if a.command in ("up", "down", "status", "swap", "job"):
            from gplib import launcher
            return launcher.main(cfg, [a.command] + rest)
        if a.command in ("smoke", "probe"):
            from gplib import smoke
            return smoke.main(cfg, [a.command] + rest)
        if a.command == "pool":
            from gplib import poolcli
            return poolcli.main(str(Path(expand(cfg_path)).resolve()), rest)
        if a.command == "daemon":
            from gplib import daemon
            return daemon.main(cfg, rest)
        if a.command == "lowcost":
            from gplib import lowcost
            return lowcost.main(cfg, rest)
        from gplib import crashlog
        return crashlog.main(cfg, rest)
    except ConfigError as e:
        print(f"config error: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
