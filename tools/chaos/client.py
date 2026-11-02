"""Thin functions for scripts (your own test scripts). rpc = port int, "host:port" string, or the info-file path
written by `chaos.py relay --info-file F`. Import with tools/chaos on sys.path: `import client`."""
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from relay import ChaosClient  # noqa: E402


def _open(rpc):
    if isinstance(rpc, str) and Path(rpc).is_file():
        rpc = json.loads(Path(rpc).read_text(encoding="utf-8"))["rpc"]
    if isinstance(rpc, str) and ":" in rpc:
        h, p = rpc.rsplit(":", 1)
        return ChaosClient(int(p), h)
    return ChaosClient(int(rpc))


def _call(rpc, method, **kw):
    c = _open(rpc)
    try:
        return c.call(method, **kw)
    finally:
        c.close()


def set_profile(rpc, name, seed=None, dir=None):
    return _call(rpc, "set_profile", profile=name, seed=seed, dir=dir)


def partition(rpc, direction="both", seconds=None, on=True):
    """direction c2s|s2c|both. With seconds, it heals itself after that long."""
    if os.environ.get("COOPCHAOS_MUTANT") == "client_partition_both":   # twin hook
        direction = "both"
    return _call(rpc, "partition", dir=direction, on=on, duration_ms=seconds * 1000 if seconds else None)


def disconnect(rpc, seconds):
    return _call(rpc, "disconnect", duration_ms=seconds * 1000)


def stats(rpc):
    return _call(rpc, "stats")


def packet_log_path(rpc):
    return _call(rpc, "log_path")
