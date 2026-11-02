// CoopQA.SaveIsolation - test-only MelonLoader plugin.
// Inert unless <game dir>/UserData/CoopQA.SaveRoot.txt exists (only test instances have it).
// Redirects FOLDERID_LocalAppDataLow (Unity's persistentDataPath root, i.e. saves, prefs, Player.log)
// to the path in that file by patching the SHGetKnownFolderPath import of the game's own modules,
// so a test copy never reads or writes %USERPROFILE%\AppData\LocalLow (and the Steam Cloud folder there).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using MelonLoader;

[assembly: MelonInfo(typeof(CoopQA.SaveIsolation.Plugin), "CoopQA.SaveIsolation", "0.1.0", "coop-anything")]
[assembly: MelonGame(null, null)]
[assembly: MelonPriority(-1000)]

namespace CoopQA.SaveIsolation;

public unsafe class Plugin : MelonPlugin
{
    static readonly Guid LocalLow = new("A520A1A4-1780-4FF6-BD18-167343C5AF16");
    static string _root;
    static IntPtr _orig;
    static int _redirected, _passed;

    public override void OnPreInitialization()
    {
        string gameDir = Path.GetDirectoryName(Process.GetCurrentProcess().MainModule!.FileName)!;
        string cfg = Path.Combine(gameDir, "UserData", "CoopQA.SaveRoot.txt");
        if (!File.Exists(cfg)) { LoggerInstance.Msg("inert: no UserData/CoopQA.SaveRoot.txt"); return; }

        _root = Path.GetFullPath(File.ReadAllText(cfg).Trim());
        Directory.CreateDirectory(_root);
        _orig = GetProcAddress(LoadLibraryW("shell32.dll"), "SHGetKnownFolderPath");
        if (_orig == IntPtr.Zero) throw new Exception("SHGetKnownFolderPath not found");

        IntPtr hook = (IntPtr)(delegate* unmanaged<Guid*, uint, IntPtr, IntPtr*, int>)&Hook;
        int patched = 0;
        foreach (ProcessModule m in Process.GetCurrentProcess().Modules)
        {
            string n = m.ModuleName.ToLowerInvariant();
            if (n is "unityplayer.dll" or "gameassembly.dll" or "steam_api64.dll" or "slimerancher2.exe")
            {
                int c = PatchIat(m.BaseAddress, "SHGetKnownFolderPath", hook);
                LoggerInstance.Msg($"patched {c} import(s) in {m.ModuleName}");
                patched += c;
            }
        }
        if (patched == 0) throw new Exception("no SHGetKnownFolderPath import patched; isolation NOT active");
        LoggerInstance.Msg($"ACTIVE: LocalLow -> {_root}");
    }

    public override void OnApplicationStarted()
    {
        if (_root != null) LoggerInstance.Msg($"LocalLow lookups: redirected={_redirected} passed={_passed}");
    }

    [UnmanagedCallersOnly]
    static int Hook(Guid* rfid, uint flags, IntPtr token, IntPtr* ppszPath)
    {
        if (*rfid == LocalLow)
        {
            System.Threading.Interlocked.Increment(ref _redirected);
            *ppszPath = Marshal.StringToCoTaskMemUni(_root);
            return 0; // S_OK
        }
        System.Threading.Interlocked.Increment(ref _passed);
        return ((delegate* unmanaged<Guid*, uint, IntPtr, IntPtr*, int>)_orig)(rfid, flags, token, ppszPath);
    }

    // Replace every import of `func` (by name, any DLL) in the module at `b` with `hook`.
    static int PatchIat(IntPtr b, string func, IntPtr hook)
    {
        byte* p = (byte*)b;
        int pe = *(int*)(p + 0x3C);
        // x64 optional header: data directory 1 (import) at OptionalHeader+0x78
        byte* opt = p + pe + 24;
        uint impRva = *(uint*)(opt + 0x78);
        if (impRva == 0) return 0;
        int count = 0;
        for (uint* d = (uint*)(p + impRva); d[3] != 0; d += 5) // IMAGE_IMPORT_DESCRIPTOR = 5 dwords
        {
            ulong* names = (ulong*)(p + (d[0] != 0 ? d[0] : d[4]));
            ulong* iat = (ulong*)(p + d[4]);
            for (int i = 0; names[i] != 0; i++)
            {
                if ((names[i] & 0x8000000000000000UL) != 0) continue; // by ordinal
                string n = Marshal.PtrToStringAnsi((IntPtr)(p + (uint)names[i] + 2))!;
                if (n != func) continue;
                if (!VirtualProtect((IntPtr)(&iat[i]), 8, 0x04, out uint old)) continue;
                iat[i] = (ulong)hook;
                VirtualProtect((IntPtr)(&iat[i]), 8, old, out _);
                count++;
            }
        }
        return count;
    }

    [DllImport("kernel32", CharSet = CharSet.Unicode)] static extern IntPtr LoadLibraryW(string n);
    [DllImport("kernel32", CharSet = CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr m, string n);
    [DllImport("kernel32")] static extern bool VirtualProtect(IntPtr a, nuint s, uint np, out uint op);
}
