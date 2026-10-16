#!/usr/bin/env python3
"""Original behavioral mutation runner. Never loads the real Steam library."""
import pathlib
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / "steam.c").read_text()
flags = ["cc", "-D_POSIX_C_SOURCE=200809L", "-I.", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-O2", "-g"]
cases = {
    "red-stub": None,
    "public-lobby": ("s->api.create(s->match,1,2)", "s->api.create(s->match,2,2)"),
    "accept-stranger": ("peer==s->peer && partner(s)==s->peer", "peer!=0 && partner(s)==s->peer"),
    "foreign-silence-bypass": ("return now-s->last_rx>=s->config.timeout_ms?failed(s,e):0;", "return 0;"),
    "deliver-stranger": ("if(sender!=s->peer)", "if(sender==0)"),
    "ignore-metadata": ("strncmp(value,values[i],sizes[i])!=0", "strncmp(value,values[i],sizes[i])==12345"),
    "lose-backpressure": ("if(s->has_output)return 0;", "if(s->has_output)return 1;"),
    "leak-message": ("s->api.release(message);if(!valid)", "if(!valid)"),
    "wrong-linux-lobby-offset": ("struct steam_created c;memcpy(&c,data,sizeof c);", "struct steam_created c;memcpy(&c,data,sizeof c);memcpy(&c.lobby,data+8,8);"),
}
subprocess.run(["make", "all"], cwd=root, check=True)
for name, change in cases.items():
    src = root / "tests" / ("red_stub.c" if change is None else name + ".c")
    binary = root / "tests" / name
    try:
        if change:
            old, new = change
            assert source.count(old) == 1, name
            src.write_text(source.replace(old, new))
        subprocess.run(flags + ["tests/test_steam.c", str(src), "steam_load.c", "-ldl", "-o", str(binary)], cwd=root, check=True)
        result = subprocess.run([str(binary)], cwd=root, capture_output=True, text=True, timeout=25)
        (root / "tests" / (name + ".log")).write_text(result.stdout + result.stderr)
        assert result.returncode == 1 and "FAIL " in result.stderr, (name, result.returncode, result.stderr)
        print("DETECTED", name, result.stderr.strip().splitlines()[0])
    finally:
        binary.unlink(missing_ok=True)
        if change:
            src.unlink(missing_ok=True)
