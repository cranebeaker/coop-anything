"""Write local/toi.json, or fill each instance's private folders and give it its own company name.

    py -I setup_lab.py <repo> config
    py -I setup_lab.py <repo> <number of instances>

The game copy goes in <repo>/local/lab/base. It is a real copy of the install, never hard links to it.
"""
import json
import shutil
import sys
from pathlib import Path

repo = Path(sys.argv[1])
lab = repo / "local" / "lab"
base = lab / "base"

if sys.argv[2] == "config":
    cfg = {
        "_notes": ["Local gamepool config. game_dir is a real copy of the install, never the install itself."],
        "name": "Tale of Immortal",
        "game_dir": str(base),
        "instances_root": str(lab / "inst"),
        "process_name": "guigubahuang.exe",
        "launch": {"exe": "guigubahuang.exe",
                   "args": ["-logFile", "{instance_dir}\\Player.log", "-screen-fullscreen", "0",
                            "-screen-width", "960", "-screen-height", "540"],
                   "env": {"SteamAppId": "1468810", "SteamGameId": "1468810"},
                   "window": "normal", "spawns_game": False, "stagger_s": 8, "child_wait_s": 30, "watch_s": 10},
        "link": {"modes": ["hardlink"],
                 "per_instance_files": ["steam_appid.txt", "guigubahuang_Data/globalgamemanagers", "guigubahuang_Data/app.info",
                                        "guigubahuang_Data/boot.config"],
                 "empty_dirs": [],
                 "per_instance_dirs": ["MelonLoader", "Mods", "Plugins", "UserData", "UserLibs", "ModExportData"],
                 "root_skip_suffixes": [".log", ".dmp", ".mdmp"],
                 "pinned_files": ["guigubahuang.exe", "GameAssembly.dll", "UnityPlayer.dll"]},
        "write_files": {"steam_appid.txt": "1468810"},
        "protected_paths": ["%USERPROFILE%\\AppData\\LocalLow\\guigugame",
                            "C:\\Program Files (x86)\\Steam\\steamapps\\common\\TOI"],
        "ready": {"type": "log_line", "path": "MelonLoader/Latest.log", "text": "CoopQA.Bot READY", "timeout_s": 180},
        "crash": {"logs": ["Player.log", "MelonLoader/Latest.log"], "markers": ["Crash!!!", "FATAL", "Unhandled"],
                  "wer_match": "guigubahuang"},
        "collect_logs": ["Player.log", "MelonLoader/Latest.log", "UserData/CoopQA/*"],
        "pairs": {"port_base": 47813, "copy_gb": 3.5, "reserve_gb": 3.0, "proto": "tcp"},
    }
    (repo / "local" / "toi.json").write_text(json.dumps(cfg, indent=1), encoding="utf-8")
    print("config written")
    sys.exit(0)

n = int(sys.argv[2])
for i in range(n):
    d = lab / "inst" / str(i)
    company = f"toicoop0{i}"
    assert len(company) == 9
    for sub in ("MelonLoader", "Mods", "Plugins", "UserData", "UserLibs", "ModExportData"):
        dst = d / sub
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(base / sub, dst)
    latest = d / "MelonLoader" / "Latest.log"
    if latest.exists():
        latest.unlink()
    for gone in (d / "MelonLoader" / "Logs",):
        if gone.exists():
            shutil.rmtree(gone)
    data = d / "guigubahuang_Data"
    gg = bytearray((base / "guigubahuang_Data" / "globalgamemanagers").read_bytes())
    old = b"\x09\x00\x00\x00guigugame"
    hits = [j for j in range(0, 64000) if gg[j:j + len(old)] == old]
    if len(hits) != 1:
        raise SystemExit(f"expected one company field near the start, found {hits}")
    at = hits[0] + 4
    gg[at:at + 9] = company.encode("ascii")
    (data / "globalgamemanagers").write_bytes(gg)
    lines = (base / "guigubahuang_Data" / "app.info").read_text(encoding="utf-8").split("\n")
    lines[0] = company
    (data / "app.info").write_text("\n".join(lines), encoding="utf-8")
    boot = (base / "guigubahuang_Data" / "boot.config").read_text(encoding="utf-8").splitlines()
    kept = [x for x in boot if not x.startswith("single-instance")]
    (data / "boot.config").write_text("\n".join(kept) + "\n", encoding="utf-8")
    print(f"instance {i}: company field at {at}, company={company}, single-instance line removed={len(kept) < len(boot)}")
