#!/usr/bin/env python3
"""Original behavioral mutation runner. Never loads the real Steam library."""
import pathlib
import subprocess
import argparse

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
    "publish-wrong-key": ('s->lobby,"game",s->config.game', 's->lobby,"title",s->config.game'),
    "publish-wrong-value": ('s->lobby,"protocol",s->config.protocol', 's->lobby,"protocol","1"'),
    "wrong-recipient": ("identity(s,&id,s->peer);\n        r=s->api.send", "identity(s,&id,s->self);\n        r=s->api.send"),
    "overwrite-busy-payload": ("if(s->has_output)return 0;", "if(s->has_output){if(size)memcpy(s->output,data,size);s->queued=size;return 0;}"),
    "remove-send-deadline": ("if(now>=s->send_deadline)return failed(s,e);", "/* Deliberately omit only the queued-send deadline. */"),
}
focused = {
    "publish-wrong-key": "--oracle-metadata",
    "publish-wrong-value": "--oracle-metadata",
    "wrong-recipient": "--oracle-send",
    "overwrite-busy-payload": "--oracle-send",
    "remove-send-deadline": "--oracle-deadline",
}
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--case", choices=cases, action="append", help="run only selected controls")
parser.add_argument("--log-dir", type=pathlib.Path, default=root / "tests" / "oracles-negative-logs")
args = parser.parse_args()
args.log_dir.mkdir(parents=True, exist_ok=True)
subprocess.run(["make", "all"], cwd=root, check=True)
for name, change in cases.items():
    if args.case and name not in args.case:
        continue
    src = root / "tests" / ("red_stub.c" if change is None else name + ".c")
    binary = root / "tests" / name
    try:
        if change:
            old, new = change
            assert source.count(old) == 1, name
            src.write_text(source.replace(old, new))
        subprocess.run(flags + ["tests/test_steam.c", str(src), "steam_load.c", "-ldl", "-o", str(binary)], cwd=root, check=True)
        command = [str(binary)] + ([focused[name]] if name in focused else [])
        result = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=25)
        (args.log_dir / (name + ".log")).write_text(result.stdout + result.stderr)
        assert result.returncode == 1 and "FAIL " in result.stderr, (name, result.returncode, result.stderr)
        print("DETECTED", name, result.stderr.strip().splitlines()[0])
    finally:
        binary.unlink(missing_ok=True)
        if change:
            src.unlink(missing_ok=True)
