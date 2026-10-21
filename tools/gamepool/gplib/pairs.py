"""Host and client pairs: which copies belong together, which port each pair uses, and whether the PC has the RAM for them.

A co-op test needs copies in pairs: one host and one client that joins it. Run several pairs at once and each pair must have its
own port, or the second host cannot bind and clients join the wrong host. This module only does the arithmetic and the checks.
It starts nothing; `launcher.py` and `instances.py` call it.

    pair k      = two copies, the host first and its client second
    its port    = pairs.port_base + the host's instance number
    instances   = the numbers `up` took from the game slots, taken two at a time, or 2k and 2k+1 when provisioning with --pairs

The port is derived from the host's instance number, and instance numbers are exclusive per game slot, so two agents running
pairs at the same time never get the same port.

Config (the "pairs" block of the game config):

    port_base    first port, default 27960
    copy_gb      memory of one running copy in GiB, default 2.0 (measure yours with `gamepool.py measure`)
    reserve_gb   memory left for Windows and the test tools, default 2.0
    proto        "udp", "tcp" or "both": what to bind when checking that a pair's port is free, default "both"
    edits        {"File.ini": [["Section", "Key", "{port}"], ...]}: lines written into a private settings file of every
                 instance before it starts. {port} {pair} {role} {host} {client} {i} are filled in.
                 The same placeholders work in launch.args and launch.env.
"""
from __future__ import annotations

import socket
from typing import Callable, Dict, Iterable, List, Optional, Sequence, Tuple

GIB = 2 ** 30
DEFAULT_PORT_BASE = 27960
DEFAULT_COPY_GB = 2.0
DEFAULT_RESERVE_GB = 2.0
PROTOS = ("udp", "tcp", "both")


# ---------------------------------------------------------------- layout
def aligned_ids(pairs: int, first_pair: int = 0) -> List[int]:
    """Instances 2k and 2k+1 for pairs first_pair .. first_pair + pairs - 1."""
    if pairs < 1:
        raise ValueError(f"--pairs must be 1 or more, got {pairs}")
    if first_pair < 0:
        raise ValueError(f"--first-pair must not be negative, got {first_pair}")
    return list(range(2 * first_pair, 2 * (first_pair + pairs)))


def port_of(host_instance: int, port_base: int = DEFAULT_PORT_BASE) -> int:
    return int(port_base) + int(host_instance)


def plan_pairs(ids: Sequence[int], port_base: int = DEFAULT_PORT_BASE) -> List[Dict]:
    """Group instance numbers two at a time (host, client), in the order given. Raises ValueError for an odd count or a repeat."""
    ids = [int(i) for i in ids]
    if len(ids) < 2 or len(ids) % 2:
        raise ValueError(f"pairs need an even number of instances, got {len(ids)}")
    if len(set(ids)) != len(ids):
        raise ValueError(f"an instance appears twice: {ids}")
    out = []
    for k in range(len(ids) // 2):
        host, client = ids[2 * k], ids[2 * k + 1]
        out.append({"pair": k, "host": host, "client": client, "port": port_of(host, port_base)})
    ports = [p["port"] for p in out]
    if max(ports) > 65535 or min(ports) < 1024:
        raise ValueError(f"ports {min(ports)}..{max(ports)} must lie within 1024..65535 (lower pairs.port_base)")
    return out


def aligned_plan(pairs: int, port_base: int = DEFAULT_PORT_BASE, first_pair: int = 0) -> List[Dict]:
    plan = plan_pairs(aligned_ids(pairs, first_pair), port_base)
    for p in plan:
        p["pair"] += first_pair
    return plan


def context_for(plan: Iterable[Dict], i: int) -> Optional[Dict[str, object]]:
    """The placeholders for instance i ({port} {pair} {role} {host} {client}), or None when i is in no pair of the plan."""
    for p in plan:
        if i in (p["host"], p["client"]):
            return {"port": p["port"], "pair": p["pair"], "role": "host" if i == p["host"] else "client",
                    "host": p["host"], "client": p["client"]}
    return None


# ---------------------------------------------------------------- RAM
def free_ram_bytes() -> int:
    """Available physical memory in bytes. 0 when it cannot be read, which makes the RAM check refuse."""
    try:
        import ctypes
        if hasattr(ctypes, "windll"):
            class MemStatus(ctypes.Structure):
                _fields_ = [("dwLength", ctypes.c_ulong), ("dwMemoryLoad", ctypes.c_ulong), ("ullTotalPhys", ctypes.c_ulonglong),
                            ("ullAvailPhys", ctypes.c_ulonglong), ("ullTotalPageFile", ctypes.c_ulonglong),
                            ("ullAvailPageFile", ctypes.c_ulonglong), ("ullTotalVirtual", ctypes.c_ulonglong),
                            ("ullAvailVirtual", ctypes.c_ulonglong), ("sullAvailExtendedVirtual", ctypes.c_ulonglong)]
            st = MemStatus()
            st.dwLength = ctypes.sizeof(st)
            if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(st)):
                return 0
            return int(st.ullAvailPhys)
        with open("/proc/meminfo", encoding="ascii", errors="replace") as f:
            for line in f:
                if line.startswith("MemAvailable:"):
                    return int(line.split()[1]) * 1024
    except (OSError, ValueError, AttributeError):
        pass
    return 0


def max_pairs_by_ram(free_bytes: int, up_copies: int = 0, copy_gb: float = DEFAULT_COPY_GB,
                     reserve_gb: float = DEFAULT_RESERVE_GB) -> int:
    """How many pairs fit. Copies already running count as pairs-in-the-making (their memory is already out of free_bytes);
    every further copy needs copy_gb out of what is free beyond reserve_gb."""
    spare = free_bytes / GIB - reserve_gb
    more = int(spare // copy_gb) if spare > 0 else 0
    return (max(0, up_copies) + more) // 2


def ram_problem(pairs: int, up_copies: int, free_bytes: int, copy_gb: float = DEFAULT_COPY_GB,
                reserve_gb: float = DEFAULT_RESERVE_GB) -> Optional[str]:
    """None when the PC has the RAM for `pairs` pairs, else the reason (printed before refusing)."""
    fit = max_pairs_by_ram(free_bytes, up_copies, copy_gb, reserve_gb)
    if pairs <= fit:
        return None
    new = max(0, 2 * pairs - up_copies)
    return (f"refusing {pairs} pair(s): {new} more game copies need {new * copy_gb:.1f} GB plus {reserve_gb:.1f} GB reserve, "
            f"but only {free_bytes / GIB:.1f} GB is free ({up_copies} copies already running); at most {fit} pair(s) fit. "
            "Close other programs, run fewer pairs, set pairs.copy_gb in the config if a copy needs less, or pass --no-ram-check")


# ---------------------------------------------------------------- ports
def port_free(port: int, proto: str = "both") -> bool:
    """True when nothing on this PC holds the port for the protocols checked (a bind to 127.0.0.1 succeeds)."""
    kinds = {"udp": [socket.SOCK_DGRAM], "tcp": [socket.SOCK_STREAM], "both": [socket.SOCK_DGRAM, socket.SOCK_STREAM]}[proto]
    for kind in kinds:
        s = socket.socket(socket.AF_INET, kind)
        try:
            s.bind(("127.0.0.1", int(port)))
        except OSError:
            return False
        finally:
            s.close()
    return True


def port_problems(plan: Iterable[Dict], proto: str = "both", skip_pairs: Iterable[int] = (),
                  is_free: Callable[[int, str], bool] = port_free) -> List[str]:
    """One message per pair whose port is taken. Pairs in skip_pairs (already running copies of ours) are not checked."""
    skip = set(skip_pairs)
    out = []
    for p in plan:
        if p["pair"] in skip:
            continue
        if not is_free(p["port"], proto):
            out.append(f"pair {p['pair']}: port {p['port']} is already in use on this PC (another pair, or another program)")
    return out


# ---------------------------------------------------------------- state
def pair_state(host_alive: bool, client_alive: bool) -> str:
    if host_alive and client_alive:
        return "up"
    if host_alive or client_alive:
        return "partial"
    return "down"
