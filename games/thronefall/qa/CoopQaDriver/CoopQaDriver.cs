// Test-only driver for Thronefall Multiplayer. Inert unless the game is started with "-coopqa host" or "-coopqa join".
//   -coopqa host|join          role of this copy (host creates a lobby, join joins the lobby id from the lobby file)
//   -coopqa-lobbyfile <path>   host writes the lobby id there, the joiner reads it
//   -coopqa-port <n>           JSON control port on 127.0.0.1 (see Rpc.cs and the command list in Handle)
// Writes <game dir>\coopqa-status.txt once per second and logs every step with the prefix "COOPQA".
// Test copies only: blocks clipboard writes, logs mod dialogs, turns on Steam networking debug output.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using BepInEx;
using HarmonyLib;
using Newtonsoft.Json.Linq;
using Steamworks;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace CoopQa
{
    [BepInPlugin("aaa.coopqa", "Co-op QA driver", "0.2.0")]
    public class CoopQaDriver : BaseUnityPlugin
    {
        private static BepInEx.Logging.ManualLogSource _log;
        private string _role;
        private string _lobbyFile;
        private string _statusFile;
        private string _step = "init";
        private float _stepSince;
        private float _nextStatus;
        private DateTime _startUtc;
        private Rpc _rpc;
        private static readonly List<string> Errors = new List<string>();
        private static readonly List<string> Dialogs = new List<string>();

        private void Awake()
        {
            var args = Environment.GetCommandLineArgs();
            _role = Arg(args, "-coopqa");
            if (_role == null)
            {
                enabled = false;
                return;
            }
            _log = Logger;
            _lobbyFile = Arg(args, "-coopqa-lobbyfile");
            _statusFile = Path.Combine(Paths.GameRootPath, "coopqa-status.txt");
            _startUtc = DateTime.UtcNow;
            Application.runInBackground = true;
            Application.logMessageReceived += (msg, stack, type) =>
            {
                if ((type == LogType.Exception || type == LogType.Error) && Errors.Count < 200) Errors.Add(msg.Split('\n')[0]);
            };
            Log($"role={_role} lobbyfile={_lobbyFile}");

            var harmony = new Harmony("aaa.coopqa");
            harmony.Patch(AccessTools.PropertySetter(typeof(GUIUtility), nameof(GUIUtility.systemCopyBuffer))
                          ?? throw new MissingMethodException("GUIUtility.set_systemCopyBuffer"),
                prefix: new HarmonyMethod(typeof(CoopQaDriver), nameof(BlockClipboard)));
            harmony.Patch(AccessTools.Method(typeof(ThronefallMP.UI.UIManager), "CreateMessageDialog")
                          ?? throw new MissingMethodException("UIManager.CreateMessageDialog"),
                prefix: new HarmonyMethod(typeof(CoopQaDriver), nameof(LogDialog)));
            BotInput.Install(harmony);
            BotInput.Active = true;   // the bot is the controller of a test copy; real keyboard/pad input is ignored for these actions

            if (Array.IndexOf(args, "-coopqa-unlock") >= 0)
            {
                // Fresh test profiles only have the tutorial unlocked; let the bot open any level flag.
                harmony.Patch(AccessTools.PropertyGetter(typeof(LevelInteractor), nameof(LevelInteractor.CanBePlayed))
                              ?? throw new MissingMethodException("LevelInteractor.get_CanBePlayed"),
                    postfix: new HarmonyMethod(typeof(CoopQaDriver), nameof(UnlockLevel)));
                Log("all level flags unlocked (test only)");
            }

            var port = Arg(args, "-coopqa-port");
            if (port != null) _rpc = new Rpc(int.Parse(port), Handle, Log);
            Step("wait-menu");
        }

        private static string Arg(string[] args, string name)
        {
            var i = Array.IndexOf(args, name);
            return i >= 0 && i + 1 < args.Length ? args[i + 1] : null;
        }

        private static void Log(string msg) => _log.LogInfo("COOPQA " + msg);

        private static bool BlockClipboard(string value)
        {
            Log("clipboard write blocked: " + value);
            return false;
        }

        private static void UnlockLevel(ref bool __result) => __result = true;

        private static void LogDialog(string title, string message)
        {
            var line = $"'{title}': {message?.Replace('\n', ' ')}";
            if (Dialogs.Count < 100) Dialogs.Add(line);
            Log("dialog " + line);
        }

        private void Step(string step)
        {
            _step = step;
            _stepSince = Time.realtimeSinceStartup;
            Log("step " + step);
        }

        private float InStep => Time.realtimeSinceStartup - _stepSince;

        private void Update()
        {
            try
            {
                Tick();
                Bot.Update();
            }
            catch (Exception e)
            {
                Log("driver exception: " + e);
                Step("FAIL driver exception");
            }
            _rpc?.Pump();
            if (Time.realtimeSinceStartup >= _nextStatus)
            {
                _nextStatus = Time.realtimeSinceStartup + 1f;
                WriteStatus();
            }
        }

        // ---- control commands -------------------------------------------------------------------------------------
        private JObject Handle(string cmd, JObject a)
        {
            switch (cmd)
            {
                case "ping": return new JObject { ["pong"] = true };
                case "status": return Status();
                case "snapshot": return State.Snapshot();
                case "slots": return new JObject { ["slots"] = State.Slots() };
                case "scenes":
                {
                    var arr = new JArray();
                    for (var i = 0; i < SceneManager.sceneCountInBuildSettings; i++)
                        arr.Add(Path.GetFileNameWithoutExtension(SceneUtility.GetScenePathByBuildIndex(i)));
                    return new JObject { ["scenes"] = arr };
                }
                case "start_level":
                {
                    var scene = (string)a["scene"];
                    Log("start_level " + scene);
                    SceneTransitionManager.instance.TransitionFromLevelSelectToLevel(scene);
                    return new JObject { ["requested"] = scene };
                }
                case "walk":
                    Bot.Start(Bot.Walk(new Vector3((float)a["x"], 0f, (float)a["z"]), (float?)a["tolerance"] ?? 1f));
                    return TaskInfo();
                case "build":
                    Bot.Start(Bot.Build((ushort)(int)a["id"]));
                    return TaskInfo();
                case "call_night":
                    Bot.Start(Bot.CallNight());
                    return TaskInfo();
                case "stop":
                    Bot.Stop();
                    return TaskInfo();
                case "levels":
                {
                    var arr = new JArray();
                    foreach (var li in UnityEngine.Object.FindObjectsByType<LevelInteractor>(FindObjectsSortMode.None))
                        arr.Add(new JObject
                        {
                            ["scene"] = li.levelInfo != null ? li.levelInfo.sceneName : null,
                            ["playable"] = li.CanBePlayed,
                            ["reason"] = li.CanBePlayed ? "" : li.CanNotBePlayedReason,
                            ["x"] = Math.Round(li.transform.position.x, 1), ["z"] = Math.Round(li.transform.position.z, 1),
                        });
                    return new JObject { ["levels"] = arr, ["popup_open"] = ThronefallMP.UI.LoadoutFrames.PopupOpen };
                }
                case "near":
                {
                    // What PlayerInteraction would consider for focus, and why.
                    var player = Bot.LocalPlayer();
                    var pi = player != null ? player.GetComponent<PlayerInteraction>() : null;
                    var arr = new JArray();
                    if (pi != null)
                    {
                        var p = player.transform.position;
                        foreach (var ib in UnityEngine.Object.FindObjectsByType<InteractorBase>(FindObjectsSortMode.None))
                        {
                            var col = ib.GetComponent<Collider>();
                            var cd = col != null ? Vector3.Distance(col.ClosestPoint(p), p) : -1f;
                            var td = Vector3.Distance(ib.transform.position, p);
                            if (td > 8f && (cd < 0 || cd > 8f)) continue;
                            arr.Add(new JObject
                            {
                                ["name"] = ib.name, ["type"] = ib.GetType().Name,
                                ["can_interact"] = ib.CanBeInteractedWith,
                                ["layer"] = LayerMask.LayerToName(ib.gameObject.layer),
                                ["in_mask"] = ((1 << ib.gameObject.layer) & pi.interactorLayer.value) != 0,
                                ["collider"] = col != null ? col.GetType().Name + (col.enabled ? "" : "(disabled)") : "none",
                                ["collider_dist"] = Math.Round(cd, 2), ["dist"] = Math.Round(td, 2),
                                ["collider_center"] = col != null ? $"{col.bounds.center.x:F1},{col.bounds.center.y:F1},{col.bounds.center.z:F1}" : "",
                                ["collider_size"] = col != null ? $"{col.bounds.size.x:F1},{col.bounds.size.y:F1},{col.bounds.size.z:F1}" : "",
                            });
                        }
                    }
                    return new JObject
                    {
                        ["radius"] = pi != null ? pi.interactionRadius : -1,
                        ["focussed"] = pi != null && pi.FocussedInteractor != null ? pi.FocussedInteractor.name : null,
                        ["interactors"] = arr,
                    };
                }
                case "teleport":
                    Bot.Teleport(new Vector3((float)a["x"], (float?)a["y"] ?? Bot.LocalPlayer().transform.position.y, (float)a["z"]));
                    return new JObject { ["done"] = true };
                case "open_level":
                    Bot.Start(Bot.OpenLevel((string)a["scene"]));
                    return TaskInfo();
                case "loadout":
                {
                    var picks = new JObject();
                    foreach (var p in ThronefallMP.Network.LoadoutState.WeaponPicks) picks[p.Key.ToString()] = p.Value.ToString();
                    var equipped = new JArray();
                    if (PerkManager.instance != null) foreach (var e in PerkManager.instance.CurrentlyEquipped) equipped.Add(e.name);
                    var ui = UIFrameManager.instance;
                    return new JObject
                    {
                        ["popup_open"] = ThronefallMP.UI.LoadoutFrames.PopupOpen,
                        ["active_frame"] = ui != null && ui.ActiveFrame != null ? ui.ActiveFrame.name : null,
                        ["weapon_picks"] = picks, ["equipped"] = equipped,
                        ["local_player_id"] = ThronefallMP.Plugin.Instance.PlayerManager.LocalId,
                    };
                }
                case "pick_weapon":
                case "press_play":
                {
                    // Select a tile in the active loadout frame and apply it, as Enter or a click on it would.
                    var ui = UIFrameManager.instance;
                    LoadoutUIHelper helper = null;
                    foreach (var h in UnityEngine.Object.FindObjectsByType<LoadoutUIHelper>(FindObjectsSortMode.None))
                        if (h.frame != null && ui != null && ui.ActiveFrame == h.frame) helper = h;
                    if (helper == null)
                        return new JObject { ["ok"] = false, ["error"] = "no loadout frame is active (active: " + (ui?.ActiveFrame != null ? ui.ActiveFrame.name : "none") + ")" };
                    ThronefallUIElement target;
                    if (cmd == "press_play") target = helper.playButton;
                    else
                    {
                        var want = (string)a["name"];
                        var tiles = Traverse.Create(helper).Field<List<TFUIEquippable>>("weapons").Value ?? new List<TFUIEquippable>();
                        target = tiles.FirstOrDefault(w => !w.Locked && (want == null || w.Data.name.IndexOf(want, StringComparison.OrdinalIgnoreCase) >= 0));
                        if (target == null)
                            return new JObject { ["ok"] = false, ["error"] = "no unlocked weapon tile" + (want != null ? " matching " + want : ""),
                                ["tiles"] = new JArray(tiles.Select(w => w.Data.name + (w.Locked ? " (locked)" : ""))) };
                    }
                    helper.frame.Select(target);
                    helper.frame.Apply();
                    return new JObject { ["applied"] = target.name, ["active_frame"] = ui.ActiveFrame != null ? ui.ActiveFrame.name : null };
                }
                case "ui":
                {
                    // The active UI frame and its elements, for a bot that clicks by name.
                    var ui = UIFrameManager.instance;
                    var frame = ui != null ? ui.ActiveFrame : null;
                    var arr = new JArray();
                    if (frame != null)
                        foreach (var e in frame.GetComponentsInChildren<ThronefallUIElement>(false))
                            arr.Add(e.name + " [" + e.GetType().Name + "]");
                    // Every UI frame that is visible, active or not (a frame left open behind a scene change shows here).
                    var open = new JArray();
                    foreach (var f in UnityEngine.Object.FindObjectsByType<UIFrame>(FindObjectsSortMode.None))
                        if (f.gameObject.activeInHierarchy) open.Add(f.name);
                    return new JObject { ["active_frame"] = frame != null ? frame.name : null, ["elements"] = arr, ["open_frames"] = open };
                }
                case "ui_apply":
                {
                    var ui = UIFrameManager.instance;
                    var frame = ui != null ? ui.ActiveFrame : null;
                    if (frame == null) return new JObject { ["ok"] = false, ["error"] = "no active frame" };
                    var want = (string)a["name"];
                    var el = frame.GetComponentsInChildren<ThronefallUIElement>(false).FirstOrDefault(e => e.name == want);
                    if (el == null) return new JObject { ["ok"] = false, ["error"] = $"no element '{want}' in {frame.name}" };
                    frame.Select(el);
                    frame.Apply();
                    return new JObject { ["applied"] = want, ["active_frame"] = ui.ActiveFrame != null ? ui.ActiveFrame.name : null };
                }
                case "press_start":
                {
                    // The loadout popup's play button is UnityEvent-wired to TransitionToSelectedLevelHelper.
                    var helpers = UnityEngine.Object.FindObjectsByType<TransitionToSelectedLevelHelper>(FindObjectsSortMode.None);
                    foreach (var h in helpers) { h.TransitionToSelectedLevel(); break; }
                    return new JObject { ["helpers"] = helpers.Length, ["popup_open"] = ThronefallMP.UI.LoadoutFrames.PopupOpen };
                }
                case "scenestate":
                {
                    // Internal scene transition state of the game and the mod, for "stuck between scenes" bugs.
                    var stm = SceneTransitionManager.instance;
                    var patch = typeof(ThronefallMP.Patches.SceneTransitionManagerPatch);
                    var lds = typeof(ThronefallMP.Network.Sync.LevelDataSync);
                    object Static(Type t, string f) => Traverse.Create(t).Field(f).GetValue();
                    return new JObject
                    {
                        ["transition_running"] = stm != null && stm.SceneTransitionIsRunning,
                        ["scene_state"] = stm != null ? Traverse.Create(stm).Field("currentSceneState").GetValue()?.ToString() : null,
                        ["patch_current_scene"] = ThronefallMP.Patches.SceneTransitionManagerPatch.CurrentScene,
                        ["patch_in_level_select"] = ThronefallMP.Patches.SceneTransitionManagerPatch.InLevelSelect,
                        ["patch_queued"] = Static(patch, "_queuedTransition")?.ToString(),
                        ["patch_hook_enabled"] = Static(patch, "_transitionHookEnabled")?.ToString(),
                        ["pending_level"] = Static(lds, "_pendingLevel")?.ToString(),
                    };
                }
                case "join_lobby":
                {
                    // What the Direct Connect / lobby list join does: Network.ConnectLobby.
                    var id = ulong.Parse((string)a["lobby"]);
                    Log($"join_lobby {id}");
                    ThronefallMP.Plugin.Instance.Network.ConnectLobby(new CSteamID(id), null);
                    Step("join-wait-online");
                    return new JObject { ["joining"] = id.ToString() };
                }
                case "back_to_map":
                {
                    // In a match the menu's "back to map" button calls InMatchResignHelper.TryBackToMap; elsewhere the
                    // pause menu's button calls PauseBackToMenuHelper.BackToMenu.
                    var inMatch = UnityEngine.Object.FindObjectsByType<InMatchResignHelper>(FindObjectsInactive.Include, FindObjectsSortMode.None);
                    if (inMatch.Length > 0) { inMatch[0].TryBackToMap(); return new JObject { ["via"] = "InMatchResignHelper.TryBackToMap" }; }
                    var helpers = UnityEngine.Object.FindObjectsByType<PauseBackToMenuHelper>(FindObjectsInactive.Include, FindObjectsSortMode.None);
                    if (helpers.Length == 0) return new JObject { ["ok"] = false, ["error"] = "no back-to-map helper in the loaded scenes" };
                    helpers[0].BackToMenu();
                    return new JObject { ["via"] = "PauseBackToMenuHelper.BackToMenu" };
                }
                case "confirm_weapon":
                {
                    // Press Confirm on the mod's weapon dialog (default pick: long bow), like a player would.
                    var dialogs = UnityEngine.Object.FindObjectsByType<ThronefallMP.UI.Dialogs.WeaponDialog>(FindObjectsSortMode.None);
                    foreach (var d in dialogs)
                        ClickButton(Traverse.Create(d).Field<ThronefallMP.UI.Controls.ButtonControl>("_button").Value);
                    return new JObject { ["confirmed"] = dialogs.Length };
                }
                case "task": return TaskInfo();
                case "timescale":
                    if (a["x"] != null) Time.timeScale = (float)a["x"];
                    return new JObject { ["timescale"] = Time.timeScale };
                case "errors":
                    return new JObject { ["errors"] = new JArray(Errors), ["dialogs"] = new JArray(Dialogs) };
                default:
                    return new JObject { ["ok"] = false, ["error"] = "unknown command " + cmd };
            }
        }

        private static JObject TaskInfo()
        {
            var t = Bot.Current;
            return t == null
                ? new JObject { ["task"] = null }
                : new JObject { ["task"] = t.Name, ["state"] = t.State, ["reason"] = t.Reason, ["elapsed"] = Math.Round(t.Elapsed, 1) };
        }

        private JObject Status()
        {
            var plugin = ThronefallMP.Plugin.Instance;
            var net = plugin != null ? plugin.Network : null;
            var snap = State.Snapshot();
            return new JObject
            {
                ["step"] = _step,
                ["seq"] = State.Seq((JObject)snap["shared"]),
                ["scene"] = State.ActiveScene(),
                ["online"] = net != null && net.Online,
                ["server"] = net != null && net.Server,
                ["peers"] = net != null ? net.Peers.Count() : 0,
                ["lobby"] = net != null ? net.Lobby.m_SteamID.ToString() : "0",
                ["players"] = plugin != null ? plugin.PlayerManager.GetAllPlayers().Count() : 0,
                ["steamid"] = SteamManager.Initialized ? SteamUser.GetSteamID().m_SteamID.ToString() : "none",
                ["errors"] = Errors.Count,
                // For the hub bot runner: counters that must never rise during a step.
                ["counters"] = new JObject { ["apply_failures"] = Errors.Count },
                ["timescale"] = Time.timeScale,
                ["task"] = TaskInfo(),
            };
        }

        // ---- automatic host / join --------------------------------------------------------------------------------
        private void Tick()
        {
            var plugin = ThronefallMP.Plugin.Instance;
            var net = plugin != null ? plugin.Network : null;
            if (!_netDebugOn && SteamManager.Initialized) EnableNetDebug();
            while (NetDebug.TryDequeue(out var line)) Log("steamnet " + line);
            switch (_step)
            {
                case "wait-menu":
                    if (net != null && SteamManager.Initialized && ThronefallMP.UI.UIManager.HostPanel != null
                        && SceneManager.GetSceneByName("_UI").isLoaded && InStep > 3f)
                    {
                        Step(_role == "host" ? "host-create" : "join-wait-lobbyfile");
                    }
                    break;

                case "host-create":
                    ClickButton(ThronefallMP.UI.UIManager.HostPanel.Host);
                    Step("host-wait-online");
                    break;

                case "host-wait-online":
                    if (net.Online && net.Server && net.Lobby.IsValid())
                    {
                        if (_lobbyFile != null)
                            File.WriteAllText(_lobbyFile, net.Lobby.m_SteamID + "\n" + DateTime.UtcNow.ToString("o") + "\n");
                        Log($"lobby {net.Lobby.m_SteamID} owner {SteamUser.GetSteamID().m_SteamID}");
                        Step("host-wait-peer");
                    }
                    else if (InStep > 60f) Step("FAIL host never went online");
                    break;

                case "host-wait-peer":
                    if (net.Peers.Any() && plugin.PlayerManager.GetAllPlayers().Count() >= 2) Step("connected");
                    else if (InStep > 180f) Step("FAIL no peer joined");
                    break;

                case "join-wait-lobbyfile":
                    if (_lobbyFile != null && File.Exists(_lobbyFile)
                        && File.GetLastWriteTimeUtc(_lobbyFile) > _startUtc.AddSeconds(-120))
                    {
                        var id = ulong.Parse(File.ReadAllLines(_lobbyFile)[0].Trim());
                        Log($"joining lobby {id} as {SteamUser.GetSteamID().m_SteamID}");
                        net.ConnectLobby(new CSteamID(id), null);
                        Step("join-wait-online");
                    }
                    else if (InStep > 180f) Step("FAIL no lobby file");
                    break;

                case "join-wait-online":
                    if (net.Online && !net.Server && !ThronefallMP.Network.PacketHandler.AwaitingConnectionApproval
                        && plugin.PlayerManager.GetAllPlayers().Count() >= 2)
                        Step("connected");
                    else if (InStep > 120f) Step("FAIL join timed out");
                    break;
            }
        }

        private static void ClickButton(ThronefallMP.UI.Controls.ButtonControl button)
        {
            var field = typeof(ThronefallMP.UI.Controls.ButtonControl).GetField("OnClick",
                BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public);
            if (field == null) throw new MissingFieldException("ButtonControl.OnClick backing field");
            var handler = field.GetValue(button) as Delegate;
            if (handler == null) throw new InvalidOperationException("button has no click handler");
            handler.DynamicInvoke();
        }

        // ---- Steam networking debug output ------------------------------------------------------------------------
        // Arrives on Steam's service thread: queue it, log it from Update.
        private static readonly System.Collections.Concurrent.ConcurrentQueue<string> NetDebug =
            new System.Collections.Concurrent.ConcurrentQueue<string>();
        private static FSteamNetworkingSocketsDebugOutput _netDebugFn;
        private bool _netDebugOn;

        private static void OnNetDebug(ESteamNetworkingSocketsDebugOutputType type, System.Text.StringBuilder msg)
        {
            NetDebug.Enqueue(type + " " + msg);
        }

        private void EnableNetDebug()
        {
            _netDebugFn = OnNetDebug;
            SteamNetworkingUtils.SetDebugOutputFunction(
                ESteamNetworkingSocketsDebugOutputType.k_ESteamNetworkingSocketsDebugOutputType_Msg, _netDebugFn);
            _netDebugOn = true;
            Log("steam networking debug output on (Msg level)");
        }

        // ---- status file ------------------------------------------------------------------------------------------
        private void WriteStatus()
        {
            var plugin = ThronefallMP.Plugin.Instance;
            var net = plugin != null ? plugin.Network : null;
            var lines = new List<string>
            {
                "time=" + DateTime.UtcNow.ToString("o"),
                "role=" + _role,
                "step=" + _step,
                "scenes=" + string.Join(",", Enumerable.Range(0, SceneManager.sceneCount).Select(i => SceneManager.GetSceneAt(i).name)),
                "steamid=" + (SteamManager.Initialized ? SteamUser.GetSteamID().m_SteamID.ToString() : "none"),
            };
            if (net != null)
            {
                lines.Add("online=" + net.Online);
                lines.Add("server=" + net.Server);
                lines.Add("lobby=" + net.Lobby.m_SteamID);
                lines.Add("peers=" + net.Peers.Count());
                var players = plugin.PlayerManager.GetAllPlayers().ToList();
                lines.Add("players=" + players.Count);
                foreach (var p in players)
                {
                    var pos = p.Object != null ? p.Object.transform.position : Vector3.zero;
                    lines.Add($"player.{p.Id}=steam:{p.SteamID.m_SteamID} spawned:{p.Object != null} pos:{pos.x:F1},{pos.y:F1},{pos.z:F1}");
                }
            }
            lines.Add("errors=" + Errors.Count);
            foreach (var e in Errors.Distinct().Take(10)) lines.Add("error=" + e);
            try
            {
                File.WriteAllLines(_statusFile + ".tmp", lines);
                if (File.Exists(_statusFile)) File.Delete(_statusFile);
                File.Move(_statusFile + ".tmp", _statusFile);
            }
            catch (IOException) { }
        }
    }
}
