"""Compile CoopQA.Bot with the .NET SDK's own compiler, with no restore and no download.

    py -I games/tale-of-immortal/qa/build_bot.py <game copy> <out dll>

References come from the game copy's MelonLoader folder, the same DLLs a code mod builds against.
"""
import glob
import os
import subprocess
import sys
from pathlib import Path


def main():
    game = Path(sys.argv[1])
    out = Path(sys.argv[2])
    here = Path(__file__).resolve().parent
    csc = sorted(glob.glob(r"C:\Program Files\dotnet\sdk\*\Roslyn\bincore\csc.dll"))
    if not csc:
        print("BUILD_FAILED: no csc.dll in the .NET SDK")
        return 2
    ml = game / "MelonLoader"
    m = ml / "Managed"
    refs = [ml / "MelonLoader.dll", ml / "0Harmony.dll"] + [m / n for n in (
        "mscorlib.dll", "System.dll", "System.Core.dll", "Assembly-CSharp.dll", "Assembly-CSharp-firstpass.dll",
        "Il2Cppmscorlib.dll", "Il2CppSystem.dll", "Il2CppSystem.Core.dll", "UnhollowerBaseLib.dll", "UnhollowerRuntimeLib.dll",
        "UnityEngine.CoreModule.dll", "UnityEngine.UI.dll", "UnityEngine.UIModule.dll",
        "UnityEngine.TextRenderingModule.dll", "UnityEngine.InputLegacyModule.dll", "UnityEngine.AnimationModule.dll")]
    missing = [str(r) for r in refs if not r.exists()]
    if missing:
        print("BUILD_FAILED: missing references", missing)
        return 2
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd = ["dotnet", csc[-1], "-nologo", "-noconfig", "-nostdlib+", "-target:library", "-optimize+", "-langversion:latest",
           "-nowarn:CS0618,CS1701,CS1702", f"-out:{out}"] + [f"-r:{r}" for r in refs] + [str(here / "Bot" / "Bot.cs")]
    env = dict(os.environ, DOTNET_ROLL_FORWARD="Major")
    r = subprocess.run(cmd, capture_output=True, text=True, env=env)
    lines = (r.stdout + r.stderr).splitlines()
    errors = [x for x in lines if ": error " in x]
    for x in errors[:30]:
        print(x[x.find("Bot.cs"):][:200] if "Bot.cs" in x else x[:200])
    print(f"BUILD_OK {out.name} {out.stat().st_size}" if r.returncode == 0 else f"BUILD_FAILED errors={len(errors)} rc={r.returncode}")
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
