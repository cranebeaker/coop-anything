#!/usr/bin/env python3
"""Compile independent mutations; only failed behavioral runs count as detection."""
import pathlib
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / "net.c").read_text()
flags = ["cc", "-D_POSIX_C_SOURCE=200809L", "-I.", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-O2", "-g"]
cases = {
    "red-stub": None,
    "accept-bad-protocol": ("get32(b)==p->config.protocol", "get32(b)>0"),
    "guest-accept-input": ("(!p->host_side && kind!=NET_VIEW && kind!=NET_EVENT)",
                           "(!p->host_side && kind!=NET_INPUT && kind!=NET_VIEW && kind!=NET_EVENT)"),
    "guest-apply-replay": ("if(get64(b+8)<=p->rx_sequence)return 0;",
                           "if(p->host_side && get64(b+8)<=p->rx_sequence)return 0;"),
    "apply-duplicate": ("get64(b+8)<=p->rx_sequence", "get64(b+8)<p->rx_sequence"),
    "drop-short-write-tail": ("p->out_sent+=(size_t)n", "p->out_sent=p->out_used"),
    "broken-outbound-magic": ('memcpy(p->out,"OOC1",4)', 'memcpy(p->out,"BAD!",4)'),
}
for name, change in cases.items():
    src = root / "tests" / ("red_stub.c" if change is None else name + ".c")
    binary = root / "tests" / name
    try:
        if change:
            old, new = change
            assert source.count(old) == 1, name
            src.write_text(source.replace(old, new))
        test = "tests/test_short_write.c" if name == "drop-short-write-tail" else "tests/test_net.c"
        link = ["-Wl,--wrap=send"] if name == "drop-short-write-tail" else []
        subprocess.run(flags + [test, str(src), "-o", str(binary)] + link, cwd=root, check=True)
        result = subprocess.run([str(binary)], cwd=root, capture_output=True, text=True, timeout=25)
        (root / "tests" / (name + ".log")).write_text(result.stdout + result.stderr)
        assert result.returncode == 1 and "FAIL " in result.stderr, (name, result.returncode, result.stderr)
        print("DETECTED", name, result.stderr.strip().splitlines()[0])
    finally:
        binary.unlink(missing_ok=True)
        if change:
            src.unlink(missing_ok=True)
