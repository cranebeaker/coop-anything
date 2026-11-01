// CoopQA.Bot - test-only MelonLoader mod for Ranching Together (SR2MP) QA on Slime Rancher 2.
// Inert unless <game dir>/UserData/CoopQA.Bot.ini exists (only test instances have it).
// - loads a save at boot through the game's own CLI option (CliOptions.SaveFileToLoad)
// - gives each test copy its own SR2MP player id (two copies on one PC would otherwise share one)
// - every 0.5 s writes UserData/CoopQA/state.json; executes numbered commands from UserData/CoopQA/cmd.txt
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.Json;
using HarmonyLib;
using Il2Cpp;
using Il2CppMonomiPark.SlimeRancher;
using Il2CppMonomiPark.SlimeRancher.Economy;
using MelonLoader;
using UnityEngine;

[assembly: MelonInfo(typeof(CoopQA.Bot.BotMod), "CoopQA.Bot", "0.1.0", "coop-anything")]
[assembly: MelonGame("MonomiPark", "SlimeRancher2")]
[assembly: MelonOptionalDependencies("SR2MP")]

namespace CoopQA.Bot;

public class BotMod : MelonMod
{
    internal static Dictionary<string, string> Cfg;
    string _dir, _cmdPath, _statePath;
    int _lastCmd;
    float _next;
    readonly List<string> _events = new();
    Assembly _sr2mp;

    public override void OnInitializeMelon()
    {
        string ini = Path.Combine(MelonLoader.Utils.MelonEnvironment.UserDataDirectory, "CoopQA.Bot.ini");
        if (!File.Exists(ini)) { LoggerInstance.Msg("inert: no UserData/CoopQA.Bot.ini"); return; }
        Cfg = File.ReadAllLines(ini).Where(l => l.Contains('=') && !l.TrimStart().StartsWith("#"))
            .Select(l => l.Split('=', 2)).ToDictionary(a => a[0].Trim(), a => a[1].Trim());
        _dir = Path.Combine(MelonLoader.Utils.MelonEnvironment.UserDataDirectory, "CoopQA");
        Directory.CreateDirectory(_dir);
        _cmdPath = Path.Combine(_dir, "cmd.txt");
        _statePath = Path.Combine(_dir, "state.json");
        File.Delete(_cmdPath);
        File.Delete(_statePath);

        _sr2mp = AppDomain.CurrentDomain.GetAssemblies().FirstOrDefault(a => a.GetName().Name == "SR2MP");
        if (Cfg.TryGetValue("player_id", out var pid) && _sr2mp != null)
        {
            var f = _sr2mp.GetType("SR2MP.Shared.Utils.PlayerIdGenerator")?.GetField("cachedPlayerId", BindingFlags.NonPublic | BindingFlags.Static);
            f?.SetValue(null, pid);
            Ev($"player_id set: {pid} (field found: {f != null})");
        }
        if (Cfg.TryGetValue("save", out var save))
        {
            HarmonyInstance.Patch(AccessTools.Method(typeof(CliOptions), nameof(CliOptions.Initialize)),
                postfix: new HarmonyMethod(typeof(BotMod), nameof(CliPostfix)));
            try { CliOptions.SaveFileToLoad = save; } catch (Exception e) { Ev("early set failed: " + e.Message); }
            Ev($"save to load: {save}");
        }
        LoggerInstance.Msg($"ACTIVE role={Get("role")}");
    }

    static void CliPostfix() { CliOptions.SaveFileToLoad = Cfg["save"]; MelonLogger.Msg("[CoopQA.Bot] CliOptions.SaveFileToLoad set after Initialize"); }

    string Get(string k, string d = "") => Cfg != null && Cfg.TryGetValue(k, out var v) ? v : d;
    void Ev(string s) { _events.Add($"{Time.realtimeSinceStartup:F1} {s}"); LoggerInstance.Msg(s); }

    public override void OnSceneWasLoaded(int buildIndex, string sceneName) { if (Cfg != null) Ev($"scene loaded: {sceneName}"); }

    public override void OnUpdate()
    {
        if (Cfg == null || Time.realtimeSinceStartup < _next) return;
        _next = Time.realtimeSinceStartup + 0.5f;
        try { RunCommands(); } catch (Exception e) { Ev("CMD ERROR " + e); }
        try { WriteState(); } catch (Exception e) { LoggerInstance.Warning("state: " + e.Message); }
    }

    object Sr(string type, string member)
    {
        var t = _sr2mp.GetType(type);
        var p = t.GetProperty(member, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
        if (p != null) return p.GetValue(null);
        return t.GetField(member, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static)?.GetValue(null);
    }

    static object Call(object o, string name, params object[] a)
    {
        var m = o.GetType().GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)
            .First(x => x.Name == name && x.GetParameters().Length == a.Length);
        return m.Invoke(o, a);
    }

    static object Prop(object o, string name) =>
        o.GetType().GetProperty(name, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(o);

    ICurrency Newbucks() => GameContext.Instance.LookupDirector._currencyList.GetDefaultCurrency().Cast<ICurrency>();

    void RunCommands()
    {
        if (!File.Exists(_cmdPath)) return;
        foreach (var line in File.ReadAllLines(_cmdPath))
        {
            var a = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            if (a.Length < 2 || !int.TryParse(a[0], out int n) || n <= _lastCmd) continue;
            _lastCmd = n;
            Ev($"cmd {n}: {string.Join(' ', a.Skip(1))}");
            switch (a[1])
            {
                case "host": Call(Sr("SR2MP.Main", "Server"), "Start", int.Parse(a[2]), true); break; // same as SR2MP UI and `host` command (IPv6 dual-mode)
                case "connect": Call(Sr("SR2MP.Main", "Client"), "Connect", a[2], int.Parse(a[3])); break;
                case "addcurrency":
                    SceneContext.Instance.PlayerState.AddCurrency(Newbucks(), int.Parse(a[2]), false, null); break;
                case "save": GameContext.Instance.AutoSaveDirector.SaveGameAndFlush(); break;
                case "quit": WriteState(); Application.Quit(); break;
                default: Ev("unknown command " + a[1]); break;
            }
        }
    }

    void WriteState()
    {
        var s = new Dictionary<string, object>
        {
            ["t"] = Math.Round(Time.realtimeSinceStartup, 1),
            ["role"] = Get("role"),
            ["last_cmd"] = _lastCmd,
            ["events"] = _events.ToArray(),
        };
        var sc = SceneContext.Instance;
        bool inGame = sc != null && sc.PlayerState != null && sc.Player != null;
        s["in_game"] = inGame;
        if (inGame)
        {
            s["currency"] = sc.PlayerState.GetCurrency(Newbucks());
            var p = sc.Player.transform.position;
            s["pos"] = new[] { p.x, p.y, p.z };
            try { s["save_name"] = GameContext.Instance.AutoSaveDirector.CurrentSaveGameName(); } catch { }
        }
        if (_sr2mp != null)
        {
            var server = Sr("SR2MP.Main", "Server");
            var client = Sr("SR2MP.Main", "Client");
            s["server_running"] = server != null && (bool)Prop(server, "IsRunning");
            s["server_clients"] = server != null && (bool)Prop(server, "IsRunning") ? Prop(server, "GetClientCount") : 0;
            s["client_connected"] = client != null && (bool)Prop(client, "IsConnected");
            s["local_id"] = Sr("SR2MP.GlobalVariables", "LocalID");
            var pm = Sr("SR2MP.GlobalVariables", "PlayerManager");
            var remotes = new List<object>();
            if (pm != null)
                foreach (var rp in (System.Collections.IEnumerable)Call(pm, "GetAllPlayers"))
                {
                    var id = rp.GetType().GetField("PlayerId").GetValue(rp);
                    var v = (Vector3)Prop(rp, "Position");
                    remotes.Add(new Dictionary<string, object> { ["id"] = id, ["pos"] = new[] { v.x, v.y, v.z } });
                }
            s["remote_players"] = remotes;
        }
        string tmp = _statePath + ".tmp";
        File.WriteAllText(tmp, JsonSerializer.Serialize(s));
        File.Move(tmp, _statePath, true);
    }
}
