"""Cross-check: the Python model and the C++ mirror (cpp/impair.h) must agree exactly.

python -I tools/chaos/xcheck.py <path to chaos_xcheck.exe>   exit 0 and prints OK when identical.
"""
import random
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import impair  # noqa: E402
from impair import Impairment, make_params, flip_bit  # noqa: E402


def make_script(seed=12345):
    r = random.Random(seed)            # script generator only; the models under test use their own RNG
    ops = []
    for name in sorted(impair.load_profiles()):
        for label in ("c2s", "s2c"):
            ops.append(f"new {name} {r.randrange(1, 10**6)} {label}")
            t = 0.0
            for i in range(3000):
                t += r.choice([0.5, 1.0, 10.0, 16.7, 33.3, r.random() * 20])
                size = r.choice([24, 64, 100, 576, 1200, 1400])
                if i == 500:
                    ops.append("set corrupt 0.25")
                if i == 900:
                    ops.append("part 1")
                if i == 1000:
                    ops.append("part 0")
                if i == 1500:
                    ops.append(f"down {t + 400.0!r}")
                if i == 1800:
                    ops.append("up")
                if i == 2000:
                    ops.append("set bandwidth_bps 300000")
                    ops.append("set buffer_ms 80")
                    ops.append("set duplicate 0.2")
                if i == 2500:
                    ops.append("set reorder 0.3")
                    ops.append("set loss 0.1")
                ops.append(f"d {t!r} {size}")
    return ops


def run_python(ops):
    out, imp, params = [], None, None
    for line in ops:
        f = line.split()
        if f[0] == "new":
            params = make_params(f[1])
            params.seed = int(f[2])
            imp = Impairment(params, f[3])
            out.append("ok")
        elif f[0] == "set":
            setattr(params, f[1], int(float(f[2])) if f[1] == "bandwidth_bps" else float(f[2]))
            imp.set_params(params, reseed=False)
            out.append("ok")
        elif f[0] == "d":
            v = imp.decide(float(f[1]), int(f[2]))
            s = f"D {v.reason or '-'} {1 if v.corrupt else 0} {len(v.delays_ms)}"
            for d in v.delays_ms:
                s += f" {d!r}"
            if v.corrupt:
                buf = flip_bit(bytes(min(int(f[2]), 64)), imp.rng)
                s += " flip " + " ".join(f"{b:02x}" for b in buf)
            out.append(s)
        elif f[0] == "part":
            imp.partition(f[1] == "1"); out.append("ok")
        elif f[0] == "down":
            imp.disconnect_until(float(f[1])); out.append("ok")
        elif f[0] == "up":
            imp.reconnect(); out.append("ok")
    return out


def same(a, b):
    ta, tb = a.split(), b.split()
    if len(ta) != len(tb):
        return False
    for x, y in zip(ta, tb):
        if x != y:
            try:
                if float(x) != float(y):
                    return False
            except ValueError:
                return False
    return True


def main():
    exe = sys.argv[1]
    ops = make_script()
    with tempfile.TemporaryDirectory() as td:
        sp = Path(td) / "script.txt"
        sp.write_text("\n".join(ops) + "\n", encoding="utf-8")
        p = subprocess.run([exe, str(sp)], capture_output=True, text=True)
    if p.returncode != 0:
        print("C++ driver failed:", p.stderr)
        return 1
    cpp = p.stdout.splitlines()
    py = run_python(ops)
    if len(cpp) != len(py):
        print(f"FAIL: {len(cpp)} C++ lines vs {len(py)} Python lines")
        return 1
    for i, (a, b, o) in enumerate(zip(py, cpp, ops)):
        if not same(a, b):
            print(f"FAIL at op {i} ({o}):\n  python {a}\n  c++    {b}")
            return 1
    nd = sum(1 for o in ops if o.startswith("d "))
    print(f"OK: {nd} decisions identical across {len(impair.load_profiles())} profiles x 2 directions")
    return 0


if __name__ == "__main__":
    sys.exit(main())
