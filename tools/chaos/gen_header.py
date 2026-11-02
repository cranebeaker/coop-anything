"""Generate tools/chaos/cpp/profiles_gen.h from tools/chaos/profiles.json.

python -I tools/chaos/gen_header.py --write   regenerate
python -I tools/chaos/gen_header.py --check   exit 1 if the header is stale
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from impair import load_profiles  # noqa: E402

OUT = Path(__file__).resolve().parent / "cpp" / "profiles_gen.h"
KEYS = ["latency_ms", "jitter_ms", "loss", "burst_enter", "burst_exit", "burst_loss", "reorder", "duplicate",
        "corrupt", "bandwidth_bps", "buffer_ms"]


def render() -> str:
    lines = ["// GENERATED from profiles.json by tools/chaos/gen_header.py. Do not edit.",
             "#pragma once", "#include \"impair.h\"", "", "namespace coopchaos {", "",
             "inline const std::vector<std::pair<std::string, Params>>& allProfiles() {",
             "    static const std::vector<std::pair<std::string, Params>> v = {"]
    for name, p in load_profiles().items():
        vals = ", ".join(repr(float(getattr(p, k))) if k != "bandwidth_bps" else str(int(p.bandwidth_bps)) for k in KEYS)
        lines.append(f'        {{"{name}", Params::make({p.seed}ULL, {vals})}},')
    lines += ["    };", "    return v;", "}", "", "}  // namespace coopchaos", ""]
    return "\n".join(lines)


def main():
    text = render()
    if "--write" in sys.argv:
        OUT.write_text(text, encoding="utf-8", newline="\n")
        print("wrote", OUT)
        return 0
    cur = OUT.read_text(encoding="utf-8") if OUT.exists() else ""
    if "--mutate" in sys.argv:
        text = text.replace("0.", "9.", 1)
    if cur != text:
        print("profiles_gen.h is stale: run gen_header.py --write")
        return 1
    print("profiles_gen.h up to date")
    return 0


if __name__ == "__main__":
    sys.exit(main())
