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

        // Errors the game logs by itself, checked offline in single player (-coopqa solo, mod loaded but not
        // connected). They are listed under "known" by the errors command and do not count as failures.
        //  - Eternal Trials on Durststein: its Blacksmith is an Eternal-Trials-only build slot that starts inactive
        //    (3 of 3 solo runs with different perks, buildid 26487600).
        private static readonly string[] KnownGameErrors =
        {
            "Coroutine couldn't be started because the the game object 'Blacksmith' is inactive!",
        };
        private static readonly List<string> KnownErrors = new List<string>();

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
                if (type != LogType.Exception && type != LogType.Error) return;
                var first = msg.Split('\n')[0];
                if (Array.IndexOf(KnownGameErrors, first) >= 0) { if (KnownErrors.Count < 200) KnownErrors.Add(first); }
                else if (Errors.Count < 200) Errors.Add(first);
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
                case "waves":
                {
                    // Every wave spawn of the level, and every object tagged Boss in the loaded scenes.
                    var waves = new JArray();
                    var spawner = EnemySpawner.instance;
                    if (spawner != null)
                    {
                        for (var w = 0; w < spawner.waves.Count; w++)
                        {
                            var spawns = new JArray();
                            foreach (var s in spawner.waves[w].spawns)
                            {
                                var tagged = s.enemyPrefab != null ? s.enemyPrefab.GetComponentInChildren<TaggedObject>(true) : null;
                                var boss = tagged != null && tagged.Tags.Contains(TagManager.ETag.Boss);
                                spawns.Add($"{(s.enemyPrefab != null ? s.enemyPrefab.name : "null")} x{s.count}{(boss ? " BOSS" : "")}{(s.spawnLine == (s.enemyPrefab != null ? s.enemyPrefab.transform : null) ? " placed" : "")}");
                            }
                            waves.Add(new JObject { ["wave"] = w, ["multi"] = spawner.waves[w].difficultyMulti, ["spawns"] = spawns });
                        }
                    }
                    var bosses = new JArray();
                    foreach (var t in UnityEngine.Object.FindObjectsByType<TaggedObject>(FindObjectsInactive.Include, FindObjectsSortMode.None))
                        if (t.Tags.Contains(TagManager.ETag.Boss))
                        {
                            var hp = t.GetComponentInChildren<Hp>(true);
                            bosses.Add($"{t.name} active={t.gameObject.activeInHierarchy} hp={(hp != null ? hp.maxHp : -1)} id={(t.GetComponentInParent<ThronefallMP.Components.Identifier>() != null)}");
                        }
                    return new JObject { ["scene"] = State.ActiveScene(), ["waves"] = waves, ["boss_objects"] = bosses };
                }
                case "open_pause":
                    // What Esc does in the map: UIFrameManager.OpenMenu (the mod adds its buttons there).
                    AccessTools.Method(typeof(UIFrameManager), "OpenMenu").Invoke(UIFrameManager.instance, null);
                    return new JObject { ["active_frame"] = UIFrameManager.instance.ActiveFrame != null ? UIFrameManager.instance.ActiveFrame.name : null };
                case "coop_panel":
                {
                    // The Co-op Settings panel: "state" lists every row (value, whether < > are shown, colour greyed),
                    // "click" presses < (dir -1) or > (dir +1) of one row, like a mouse click.
                    var panel = ThronefallMP.UI.UIManager.CoopSettingsPanel;
                    if (panel == null) return new JObject { ["ok"] = false, ["error"] = "no CoopSettingsPanel" };
                    if ((string)a["action"] == "click")
                    {
                        panel.ShowTabOf((string)a["key"]);
                        var row = panel.GetComponentsInChildren<Transform>(true).FirstOrDefault(t => t.name == (string)a["key"]);
                        if (row == null) return new JObject { ["ok"] = false, ["error"] = "no row " + a["key"] };
                        var btn = row.GetComponentsInChildren<ThronefallMP.UI.Controls.ButtonControl>(true)
                            .FirstOrDefault(b => b.name == ((int)a["dir"] < 0 ? "prev" : "next"));
                        if (btn == null || !btn.gameObject.activeInHierarchy) return new JObject { ["ok"] = false, ["error"] = "button not available (read only?)" };
                        ClickButton(btn);
                    }
                    if ((string)a["action"] == "close")
                    {
                        panel.Enabled = false;
                    }
                    var rows = new JObject();
                    foreach (var r in ThronefallMP.CoopDifficulty.Rows)
                    {
                        var t = panel.GetComponentsInChildren<Transform>(true).FirstOrDefault(x => x.name == r.Key);
                        var value = t != null ? t.GetComponentsInChildren<TMPro.TextMeshProUGUI>(true).FirstOrDefault(x => x.name == "value") : null;
                        var next = t != null ? t.GetComponentsInChildren<ThronefallMP.UI.Controls.ButtonControl>(true).FirstOrDefault(b => b.name == "next") : null;
                        rows[r.Key] = (value != null ? value.text : "?") + (next != null && next.gameObject.activeSelf ? "" : " (read only)");
                    }
                    var details = new JObject();
                    foreach (var r in ThronefallMP.CoopDifficulty.Rows)
                    {
                        var t = panel.GetComponentsInChildren<Transform>(true).FirstOrDefault(x => x.name == r.Key);
                        var d = t != null ? t.GetComponentsInChildren<TMPro.TextMeshProUGUI>(true).FirstOrDefault(x => x.name == "detail") : null;
                        if (d != null && !string.IsNullOrEmpty(d.text)) details[r.Key] = d.text;
                    }
                    var total = panel.GetComponentsInChildren<TMPro.TextMeshProUGUI>(true).FirstOrDefault(x => x.name == "total");
                    return new JObject { ["open"] = panel.Enabled, ["rows"] = rows, ["details"] = details, ["total"] = total != null ? total.text : null };
                }
                case "force_boss_spawn":
                {
                    // TEST ONLY: run the host's own spawn step (EnemySpawnerPatch.UpdateSpawn) for the level's boss spawn
                    // now, instead of playing to its wave. The packet, the boss difficulty and the sync are the real ones.
                    var spawner = EnemySpawner.instance;
                    for (var w = 0; w < spawner.waves.Count; w++)
                    for (var s = 0; s < spawner.waves[w].spawns.Count; s++)
                    {
                        var spawn = spawner.waves[w].spawns[s];
                        if (spawn.enemyPrefab == null || !spawn.enemyPrefab.GetComponentsInChildren<TaggedObject>(true).Any(t => t.Tags.Contains(TagManager.ETag.Boss))) continue;
                        // What the wave start does first (it fills the coin array), then the spawn step.
                        spawn.Reset(true);
                        Traverse.Create(spawn).Field("finished").SetValue(false);
                        Traverse.Create(spawn).Field("waitBeforeNextSpawn").SetValue(0f);
                        try
                        {
                            AccessTools.Method(typeof(ThronefallMP.Patches.EnemySpawnerPatch), "UpdateSpawn").Invoke(null, new object[] { spawn, w, s });
                        }
                        catch (System.Reflection.TargetInvocationException e)
                        {
                            return new JObject { ["ok"] = false, ["error"] = e.InnerException?.ToString() ?? e.ToString() };
                        }
                        return new JObject { ["wave"] = w, ["spawn"] = s, ["prefab"] = spawn.enemyPrefab.name };
                    }
                    return new JObject { ["ok"] = false, ["error"] = "no boss spawn in this level" };
                }
                case "boss_hp":
                {
                    var arr = new JArray();
                    foreach (var t in UnityEngine.Object.FindObjectsByType<TaggedObject>(FindObjectsInactive.Include, FindObjectsSortMode.None))
                        if (t.Tags.Contains(TagManager.ETag.Boss))
                            foreach (var hp in t.GetComponentsInChildren<Hp>(true))
                                arr.Add($"{t.name}/{hp.name}: active={t.gameObject.activeInHierarchy} maxHp={hp.maxHp:F0}");
                    return new JObject { ["bosses"] = arr };
                }
                case "screenshot":
                {
                    // Written at the end of the frame by Unity; a minimized window may come out black.
                    var path = (string)a["path"];
                    ScreenCapture.CaptureScreenshot(path);
                    return new JObject { ["path"] = path };
                }
                case "map_ping":
                {
                    // "now": what the ping key does (PingPanel.TryPing: the ground under the mouse, or the player's
                    // position with PingAt=Player). Always returns how many pings this copy has shown and the last one.
                    // "x"/"y" (0..1 of the screen) ping at that screen point instead of the mouse pointer.
                    Vector2? at = a["x"] != null ? new Vector2((float)a["x"] * Screen.width, (float)a["y"] * Screen.height) : null;
                    // "kind": a PingKind name (what the ping wheel picks); default Look.
                    var kind = a["kind"] != null
                        ? (ThronefallMP.Network.Packets.Game.PingKind)Enum.Parse(typeof(ThronefallMP.Network.Packets.Game.PingKind), (string)a["kind"])
                        : ThronefallMP.Network.Packets.Game.PingKind.Look;
                    var sent = (string)a["action"] == "now" ? (bool?)ThronefallMP.UI.Panels.PingPanel.TryPing(at, kind) : null;
                    var last = ThronefallMP.UI.Panels.PingPanel.LastPosition;
                    // Where the last ping is on this screen (0..1), to check the cursor -> ground -> screen round trip.
                    var cam = Camera.main;
                    var sp = cam != null ? cam.WorldToScreenPoint(last) : Vector3.zero;
                    var local = ThronefallMP.Plugin.Instance.PlayerManager.LocalPlayer?.Object;
                    var marker = ThronefallMP.UI.UIManager.PingPanel?.PanelRoot.transform.Find("ping");
                    return new JObject
                    {
                        ["sent"] = sent,
                        ["active"] = ThronefallMP.CoopDifficulty.MapPingActive,
                        ["shown"] = ThronefallMP.UI.Panels.PingPanel.Shown,
                        ["last"] = new JArray(last.x, last.y, last.z),
                        ["player"] = local != null ? new JArray(local.transform.position.x, local.transform.position.y, local.transform.position.z) : null,
                        ["marker"] = marker != null ? ThronefallMP.UI.Panels.PingPanel.LastLabel : null,
                        ["kind"] = ThronefallMP.UI.Panels.PingPanel.LastKind.ToString(),
                        ["sign"] = ThronefallMP.UI.Panels.PingPanel.LastSign,
                        ["markerVisible"] = marker != null && marker.gameObject.activeInHierarchy,
                        ["screen"] = new JArray(Screen.width, Screen.height),
                        ["lastOnScreen"] = Screen.width > 0 ? new JArray(sp.x / Screen.width, sp.y / Screen.height) : null,
                    };
                }
                case "revive_state":
                {
                    // Down / up, deaths in this level, revive progress, the respawn wait the game uses, and whether
                    // the camera watches a teammate.
                    var players = new JObject();
                    foreach (var p in ThronefallMP.Plugin.Instance.PlayerManager.GetAllPlayers())
                    {
                        var hp = ThronefallMP.Network.TeammateRevive.HpOf(p);
                        var ar = p.Data != null ? p.Data.GetComponent<AutoRevive>() : null;
                        ThronefallMP.Network.TeammateRevive.Progress.TryGetValue(p.Id, out var prog);
                        players[p.Id.ToString()] = new JObject
                        {
                            ["down"] = hp != null && hp.KnockedOut,
                            ["deaths"] = ThronefallMP.Network.TeammateRevive.DeathsOf(p.Id),
                            ["progress"] = prog.progress,
                            ["wait"] = ar != null ? Traverse.Create(ar).Property<float>("ReviveAfterBeingKnockedOutFor").Value : -1f,
                            ["left"] = ar != null ? ar.TimeTillRevive : -1f,
                            ["pos"] = p.Object != null ? new JArray(p.Object.transform.position.x, p.Object.transform.position.y, p.Object.transform.position.z) : null,
                        };
                    }
                    return new JObject
                    {
                        ["players"] = players,
                        ["watching"] = ThronefallMP.Network.TeammateRevive.Watching,
                        ["watchedId"] = ThronefallMP.Network.TeammateRevive.WatchedId,
                        ["watchText"] = ThronefallMP.UI.Panels.CoopHudPanel.WatchText,
                        ["revived"] = ThronefallMP.Network.TeammateRevive.Revived,
                    };
                }
                case "plans":
                {
                    // Plans / rebuilds as this machine knows them, the open vote, plates and blueprints drawn.
                    var arr = new JArray();
                    foreach (var p in ThronefallMP.Network.BuildPlanner.Plans)
                        arr.Add($"{p.SlotId}|{p.Level}|{p.Choice}|{(p.PlayerId == ThronefallMP.Network.BuildPlanner.Nobody ? -1 : p.PlayerId)}|{(p.Rebuild ? "rebuild" : "plan")}|{ThronefallMP.Network.BuildPlanner.Describe(p)}");
                    var vote = ThronefallMP.Network.BuildPlanner.CurrentVote;
                    if ((string)a["action"] == "confirm") ThronefallMP.UI.UIManager.PlanPanel.TestConfirm((bool?)a["near"] ?? false);
                    if ((string)a["action"] == "vote") ThronefallMP.UI.UIManager.PlanPanel.TestVote((bool?)a["yes"] ?? true);
                    return new JObject
                    {
                        ["plans"] = arr,
                        ["vote"] = vote == null ? null : $"{vote.Id}|{vote.Status}|{vote.Yes}/{vote.Needed}|{vote.No}|{vote.Text}",
                        ["voteText"] = ThronefallMP.UI.Panels.PlanPanel.VoteText,
                        ["plates"] = ThronefallMP.UI.Panels.PlanPanel.PlatesShown,
                        ["blueprints"] = ThronefallMP.UI.Panels.PlanPanel.BlueprintsDrawn,
                        ["executed"] = ThronefallMP.Network.BuildPlanner.Executed,
                        ["message"] = ThronefallMP.Network.BuildPlanner.LastMessage,
                        ["gold"] = ThronefallMP.GlobalData.Balance,
                    };
                }
                case "endless":
                {
                    var spawner = EnemySpawner.instance;
                    return new JObject
                    {
                        ["on"] = ThronefallMP.CoopDifficulty.Endless,
                        ["original"] = ThronefallMP.Network.EndlessMode.OriginalWaves,
                        ["appended"] = ThronefallMP.Network.EndlessMode.Appended,
                        ["wave"] = spawner != null ? spawner.Wavenumber : -1,
                        ["count"] = spawner != null ? spawner.waves.Count : -1,
                        ["matchOver"] = spawner != null && spawner.MatchOver,
                        ["bonusNext"] = spawner != null ? ThronefallMP.Network.EndlessMode.Bonus(spawner.Wavenumber) : 0f,
                    };
                }
                case "et":
                {
                    // Eternal Trials: game mode, this machine's run, the three map choices and the level's waves.
                    var o = new JObject
                    {
                        ["active"] = ThronefallMP.Network.EternalTrialsCoop.Active,
                        ["remotePicks"] = ThronefallMP.Network.EternalTrialsCoop.RemotePicks,
                        ["scene"] = UnityEngine.SceneManagement.SceneManager.GetActiveScene().name,
                        ["state"] = LocalGamestate.Instance != null ? LocalGamestate.Instance.CurrentState.ToString() : "",
                    };
                    var run = ThronefallMP.Network.EternalTrialsCoop.Loaded;
                    if (run != null)
                    {
                        o["run"] = new JObject
                        {
                            ["stage"] = run.stage, ["seed"] = run.currentStageSeed, ["map"] = run.currentMap,
                            ["score"] = run.score, ["inGame"] = run.inGame, ["inNight"] = run.inNight,
                            ["complete"] = run.runComplete,
                            ["perks"] = new JArray(run.acquiredPerks.Where(p => p != null).Select(p => p.name).ToArray()),
                            ["weapon"] = run.currentWeapon != null ? run.currentWeapon.name : "",
                        };
                        if (ThronefallMP.Network.EternalTrialsCoop.Active && !run.inGame)
                        {
                            o["choices"] = new JArray(EternalTrialsRunManager.GetMapChoices().Select(c =>
                                c.id + ":" + c.mapName + ":" + (c.containedWeapon != null ? c.containedWeapon.name : "") + ":" +
                                string.Join("+", c.containedPerks.Where(p => p != null).Select(p => p.name)) + ":" + c.goldAtStart).ToArray());
                        }
                    }
                    if (PerkManager.instance != null)
                    {
                        o["equipped"] = new JArray(PerkManager.instance.CurrentlyEquipped.Where(e => e != null).Select(e => e.name).OrderBy(n => n).ToArray());
                    }
                    // The run files (single-player and co-op) as fingerprints, to show which ones a test changed.
                    var files = new JObject();
                    foreach (var name in new[] { "EternalTrials_V4.json", "mp_EternalTrials_V4.json" })
                    {
                        var path = Thronefall.SaveServices.SaveServicesLocator.Instance.GetFullSavePath(name);
                        if (!System.IO.File.Exists(path)) { files[name] = "none"; continue; }
                        uint fh = 2166136261;
                        foreach (var b in System.IO.File.ReadAllBytes(path)) { fh = (fh ^ b) * 16777619; }
                        files[name] = fh.ToString("x8");
                    }
                    o["files"] = files;
                    var spawner = EnemySpawner.instance;
                    if (spawner != null && spawner.waves != null)
                    {
                        var sb = new System.Text.StringBuilder();
                        foreach (var w in spawner.waves)
                        {
                            foreach (var s in w.spawns)
                            {
                                sb.Append(s.enemyPrefab != null ? s.enemyPrefab.name : "-").Append('*').Append(s.count)
                                  .Append('@').Append(s.spawnLine != null ? s.spawnLine.name : "-").Append(s.eliteEnemies ? "E" : "")
                                  .Append('$').Append(s.goldCoins).Append(',');
                            }
                            sb.Append('|');
                        }
                        uint h = 2166136261;
                        foreach (var ch in sb.ToString()) { h = (h ^ ch) * 16777619; }
                        o["waves"] = spawner.waves.Count;
                        o["wavesHash"] = h.ToString("x8");
                        o["wave"] = spawner.Wavenumber;
                    }
                    return o;
                }
                case "et_banner":
                {
                    // What a player does at the Eternal Trials banner on the map: press it.
                    var banner = UnityEngine.Object.FindObjectOfType<EternalTrialsInteractor>();
                    if (banner == null) return new JObject { ["ok"] = false, ["error"] = "no Eternal Trials banner here" };
                    // The first press shows the game's explanation pop-up; a player closes it and presses again.
                    if ((bool?)a["close"] == true && UIFrameManager.instance.ActiveFrame != null) UIFrameManager.instance.CloseAllFrames();
                    banner.InteractionBegin(null);
                    return new JObject { ["ok"] = true, ["frame"] = UIFrameManager.instance?.ActiveFrame?.name };
                }
                case "et_new":
                {
                    // The select screen's "New run" button (the select screen must be open: et_banner).
                    var screen = UnityEngine.Object.FindObjectOfType<EternalTrialsSelectScreen>();
                    if (screen == null) return new JObject { ["ok"] = false, ["why"] = "select screen not open", ["frame"] = UIFrameManager.instance?.ActiveFrame?.name };
                    screen.NewRun();
                    return new JObject { ["ok"] = true, ["frame"] = UIFrameManager.instance?.ActiveFrame?.name };
                }
                case "et_continue":
                {
                    // The end screen's Continue button in Eternal Trials: opens the map choices.
                    var ok = UIFrameManager.OpenEternalTrialsLoadoutPick();
                    return new JObject { ["ok"] = ok, ["frame"] = UIFrameManager.instance?.ActiveFrame?.name };
                }
                case "et_pick":
                {
                    // Confirm one of the shown map choices (by its id), the way clicking its Play button does.
                    var id = (int)a["id"];
                    var shown = UnityEngine.Object.FindObjectsOfType<ETMapChoiceDisplay>();
                    var display = shown.FirstOrDefault(d => d.Data != null && d.Data.id == id);
                    if (display == null) return new JObject { ["ok"] = false, ["why"] = "choice " + id + " not shown", ["shown"] = shown.Length };
                    display.Confirm();
                    return new JObject { ["ok"] = true, ["map"] = display.Data.mapName };
                }
                case "force_end":
                {
                    // TEST ONLY (host): end the match now, as the mod does at the real end (victory or the castle falls).
                    var victory = (bool?)a["victory"] ?? true;
                    if (victory)
                    {
                        ThronefallMP.Plugin.Instance.Network.Send(new ThronefallMP.Network.Packets.Game.VictoryPacket(), true);
                        LocalGamestate.Instance.SetState(LocalGamestate.State.AfterMatchVictory, false, true);
                    }
                    else
                    {
                        ThronefallMP.Plugin.Instance.Network.Send(new ThronefallMP.Network.Packets.Game.ResignPacket());
                        LocalGamestate.Instance.SetState(LocalGamestate.State.AfterMatchDefeat, false, true);
                    }
                    return new JObject { ["state"] = LocalGamestate.Instance.CurrentState.ToString() };
                }
                case "score":
                {
                    // The end-of-match score parts this machine's game uses (a client: the host's, see ScoreSync).
                    var p = ThronefallMP.Network.ScoreSync.Local();
                    if (p == null) return new JObject { ["ok"] = true, ["inLevel"] = false,
                        ["lastTotal"] = SceneTransitionManager.instance != null ? SceneTransitionManager.instance.TotalScoreFromLastMatch : -1 };
                    return new JObject
                    {
                        ["ok"] = true, ["inLevel"] = true, ["fromHost"] = ThronefallMP.Network.ScoreSync.HasHostScore,
                        ["current"] = p.Current, ["gold"] = p.GoldBonus, ["mutator"] = p.MutatorBonus,
                        ["victoryNoRetry"] = p.VictoryNoRetry, ["defeatNoRetry"] = p.DefeatNoRetry,
                        ["lastTotal"] = SceneTransitionManager.instance != null ? SceneTransitionManager.instance.TotalScoreFromLastMatch : -1,
                    };
                }
                case "quests":
                {
                    // The level's quests and whether this player's save has them done (the game's own check).
                    var scene = (string)a["scene"];
                    var lpm = LevelProgressManager.instance;
                    var info = lpm.GetLevelInfoFromSceneName(scene);
                    var data = lpm.GetLevelDataForScene(scene);
                    if (info == null || data == null) return new JObject { ["ok"] = false, ["error"] = "unknown level " + scene };
                    return new JObject
                    {
                        ["ok"] = true,
                        ["total"] = info.QuestsTotal(),
                        ["complete"] = info.QuestsComplete(),
                        ["beaten"] = data.beatenBest,
                        ["highscore"] = data.highscoreBest,
                        ["wins"] = data.levelHasBeenBeatenWith.Count,
                        ["lastWin"] = data.levelHasBeenBeatenWith.Count > 0
                            ? new JArray(data.levelHasBeenBeatenWith.Last().Where(e => e != null).Select(e => e.name).OrderBy(n => n).ToArray()) : new JArray(),
                        ["list"] = new JArray(info.quests.Select(q => q.GetMissionStatement() + " => " + q.CheckBeaten(data)).ToArray()),
                    };
                }
                case "set_wave":
                {
                    // TEST ONLY: jump the wave counter (to reach the level's last wave quickly), on this machine.
                    var spawner = EnemySpawner.instance;
                    Traverse.Create(spawner).Field<int>("wavenumber").Value = (int)a["wave"];
                    ThronefallMP.Network.EndlessMode.Ensure((int)a["wave"] + 3);
                    return new JObject { ["wave"] = spawner.Wavenumber, ["count"] = spawner.waves.Count, ["matchOver"] = spawner.MatchOver };
                }
                case "chat":
                {
                    if (a["say"] != null) ThronefallMP.Plugin.Instance.Network.SendChatMessage((string)a["say"]);
                    return new JObject { ["last"] = ThronefallMP.UI.Panels.ChatPanel.LastLine, ["history"] = ThronefallMP.UI.UIManager.ChatPanel.HistoryCount };
                }
                case "wardrobe":
                {
                    var arr = new JObject();
                    foreach (var p in ThronefallMP.Plugin.Instance.PlayerManager.GetAllPlayers())
                        arr[p.Id.ToString()] = ThronefallMP.Network.WardrobeSync.Wearing(p);
                    var lobby = ThronefallMP.Plugin.Instance.Network.Lobby;
                    var published = new JObject();
                    foreach (var p in ThronefallMP.Plugin.Instance.PlayerManager.GetAllPlayers())
                        published[p.Id.ToString()] = SteamMatchmaking.GetLobbyMemberData(lobby, p.SteamID, ThronefallMP.Network.WardrobeSync.MemberKey);
                    return new JObject { ["wearing"] = arr, ["published"] = published, ["own"] = ThronefallMP.Network.WardrobeSync.Own };
                }
                case "set_wardrobe":
                {
                    // TEST ONLY: dress the local king like the wardrobe tent would (items and materials by list index),
                    // without the wardrobe manager, so this test copy's prefs stay as they are.
                    var lp = ThronefallMP.Plugin.Instance.PlayerManager.LocalPlayer?.Object;
                    var w = lp != null ? lp.GetComponentInChildren<WardrobePlayer>(true) : null;
                    if (w == null) return new JObject { ["ok"] = false, ["error"] = "no WardrobePlayer" };
                    var manager = Traverse.Create(w).Field("playerWardrobeManager");
                    var saved = manager.GetValue();
                    manager.SetValue(null);
                    try
                    {
                        if (a["crown"] != null && w.CrownWardrobeItems.Count > (int)a["crown"]) w.SetCrown(w.CrownWardrobeItems[(int)a["crown"]], true);
                        if (a["cape"] != null && w.CapeWardrobeItems.Count > (int)a["cape"]) w.SetCape(w.CapeWardrobeItems[(int)a["cape"]], true);
                        if (a["capeMat"] != null && w.CapeColorCombos.Count > (int)a["capeMat"]) w.SetCapeMaterial(w.CapeColorCombos[(int)a["capeMat"]], true);
                        if (a["crownMat"] != null && w.CrownMaterials.Count > (int)a["crownMat"]) w.SetCrownMaterial(w.CrownMaterials[(int)a["crownMat"]], true);
                    }
                    finally { manager.SetValue(saved); }
                    return new JObject { ["now"] = ThronefallMP.Network.WardrobeSync.Wearing(ThronefallMP.Plugin.Instance.PlayerManager.LocalPlayer),
                        ["crowns"] = w.CrownWardrobeItems.Count, ["capes"] = w.CapeWardrobeItems.Count, ["capeMats"] = w.CapeColorCombos.Count, ["crownMats"] = w.CrownMaterials.Count };
                }
                case "part_colors":
                {
                    var arr = new JObject();
                    foreach (var p in ThronefallMP.Plugin.Instance.PlayerManager.GetAllPlayers())
                    {
                        var horse = ThronefallMP.Network.PlayerColors.MaterialColor(p, "Horse");
                        var body = ThronefallMP.Network.PlayerColors.BodyColor(p);
                        var cape = ThronefallMP.Network.PlayerColors.CapeColor(p);
                        arr[p.Id.ToString()] = $"body={(body.HasValue ? ColorUtility.ToHtmlStringRGB(body.Value) : "-")} cape={(cape.HasValue ? ColorUtility.ToHtmlStringRGB(cape.Value) : "-")} horse={(horse.HasValue ? ColorUtility.ToHtmlStringRGB(horse.Value) : "-")}";
                    }
                    return new JObject { ["players"] = arr };
                }
                case "bindings":
                {
                    // The game's keyboard and mouse keys per action (Rewired maps of player 0), read only.
                    var result = new JObject();
                    var player = Rewired.ReInput.players.GetPlayer(0);
                    foreach (var action in Rewired.ReInput.mapping.Actions)
                    {
                        var keys = new List<string>();
                        foreach (var type in new[] { Rewired.ControllerType.Keyboard, Rewired.ControllerType.Mouse })
                            foreach (var aem in player.controllers.maps.ElementMapsWithAction(type, action.id, false))
                                keys.Add(aem.elementIdentifierName + (aem.axisContribution == Rewired.Pole.Negative ? "(-)" : ""));
                        if (keys.Count > 0) result[action.name] = string.Join(", ", keys.Distinct());
                    }
                    return new JObject { ["actions"] = result };
                }
                case "sprint":
                    return new JObject { ["mode"] = ThronefallMP.GameKeys.Sprint.ToString(), ["on"] = ThronefallMP.GameKeys.SprintToggledOn };
                case "destroy_building":
                {
                    // TEST ONLY (host): destroy a building with the game's own damage, as enemies would.
                    var go = ThronefallMP.Components.Identifier.GetGameObject(ThronefallMP.Components.IdentifierType.BuildSlot, (ushort)(int)a["id"]);
                    var slot = go != null ? go.GetComponent<BuildSlot>() : null;
                    var hp = slot != null ? slot.GetComponentInChildren<Hp>(true) : null;
                    if (hp == null) return new JObject { ["ok"] = false, ["error"] = "no building Hp in slot " + a["id"] };
                    hp.TakeDamage(hp.maxHp * 10f, null, false, true);
                    return new JObject { ["hp"] = hp.HpValue, ["level"] = slot.Level, ["state"] = slot.State.ToString() };
                }
                case "night_report":
                    return new JObject
                    {
                        ["goldBack"] = ThronefallMP.Network.NightReport.LastGoldBack,
                        ["lost"] = ThronefallMP.Network.NightReport.LostCount,
                        ["rows"] = new JArray(ThronefallMP.UI.Panels.NightSummaryPanel.LastRows.ToArray()),
                        ["lostText"] = ThronefallMP.UI.Panels.NightSummaryPanel.LastLost,
                        ["panel"] = ThronefallMP.UI.UIManager.NightSummaryPanel != null && ThronefallMP.UI.UIManager.NightSummaryPanel.Enabled,
                        ["teamHp"] = ThronefallMP.UI.Panels.CoopHudPanel.HpText,
                        ["alerts"] = ThronefallMP.Network.TeammateRevive.Alerts,
                    };
                case "watch_cycle":
                    // What the Watch next (+1) / previous (-1) key does.
                    ThronefallMP.Network.TeammateRevive.Cycle((int?)a["dir"] ?? 1);
                    return new JObject { ["done"] = true };
                case "knock_out":
                {
                    // TEST ONLY (host): knock a player out with the game's own damage, as an enemy would.
                    var p = ThronefallMP.Plugin.Instance.PlayerManager.Get((ushort)(int)a["id"]);
                    var hp = ThronefallMP.Network.TeammateRevive.HpOf(p);
                    if (hp == null) return new JObject { ["ok"] = false, ["error"] = "no Hp for player " + a["id"] };
                    hp.TakeDamage(hp.maxHp * 10f, null, false, true);
                    return new JObject { ["down"] = hp.KnockedOut };
                }
                case "hud":
                {
                    var kills = new JObject();
                    foreach (var kv in ThronefallMP.UI.Panels.CoopHudPanel.Kills) kills[kv.Key.ToString()] = kv.Value;
                    return new JObject
                    {
                        ["kills"] = kills,
                        ["text"] = ThronefallMP.UI.Panels.CoopHudPanel.KillText,
                        ["pings"] = ThronefallMP.UI.UIManager.PingPanel.PanelRoot.transform.Cast<Transform>().Count(t => t.name == "ping"),
                        ["platesMoved"] = ThronefallMP.UI.Panels.PingPanel.PlatesMoved,
                        ["extraSpawned"] = ThronefallMP.Patches.EnemySpawnerPatch.ExtraSpawned,
                        ["extraCoins"] = ThronefallMP.Patches.EnemySpawnerPatch.ExtraCoins,
                    };
                }
                case "profile":
                {
                    var panel = ThronefallMP.UI.UIManager.CoopSettingsPanel;
                    var slot = (int?)a["slot"] ?? 1;
                    if ((string)a["action"] == "save") panel.SaveProfile(slot);
                    if ((string)a["action"] == "load") panel.LoadProfile(slot);
                    return new JObject { ["text"] = panel.ProfileText, ["stored"] = ThronefallMP.CoopProfiles.Get(slot) };
                }
                case "save_hash":
                {
                    // The host's own match save for a level: hash of the file on disk (unchanged by a late join).
                    var path = Thronefall.SaveServices.SaveServicesLocator.Instance.GetFullSavePath("mp_" + (string)a["level"] + ".json");
                    if (!Thronefall.SaveServices.SaveServicesLocator.Instance.Exists(path)) return new JObject { ["hash"] = null };
                    var text = Thronefall.SaveServices.SaveServicesLocator.Instance.ReadAllText(path);
                    using var sha = System.Security.Cryptography.SHA256.Create();
                    var hash = BitConverter.ToString(sha.ComputeHash(System.Text.Encoding.UTF8.GetBytes(text))).Replace("-", "");
                    return new JObject { ["hash"] = hash, ["length"] = text.Length };
                }
                case "late_join":
                    return new JObject { ["admitted"] = ThronefallMP.Network.LateJoin.Admitted, ["replayed"] = ThronefallMP.Network.LateJoin.Replayed, ["joinDuringLevel"] = ThronefallMP.CoopDifficulty.JoinDuringLevel };
                case "endgate":
                    // Host: how many players the end screen still waits for (EndScreenGate).
                    return new JObject { ["ok"] = true, ["waiting"] = ThronefallMP.Network.EndScreenGate.WaitingCount };
                case "to_map":
                    // Solo copies (-coopqa solo): from the main menu to the map, as the game's Play button does.
                    SceneTransitionManager.instance.TransitionToBaseLevelSelect();
                    return new JObject { ["ok"] = true };
                case "et_solo":
                {
                    // Solo copies only (offline, base game code): roll fresh Eternal Trials runs until one offers the
                    // wanted map, then pick it the way the game does. For checking what the game does without co-op.
                    if (ThronefallMP.Plugin.Instance.Network.Online) return new JObject { ["ok"] = false, ["why"] = "online" };
                    var map = (string)a["map"];
                    for (var i = 0; i < 200; i++)
                    {
                        EternalTrialsRunManager.GenerateNewFreshRunSeed();
                        EternalTrialsRunManager.CreateFreshRun();
                        var choice = EternalTrialsRunManager.GetMapChoices().FirstOrDefault(c => c.mapName == map);
                        if (choice == null) continue;
                        LocalGamestate.SelectedGameMode = LocalGamestate.GameMode.EternalTrial;
                        EternalTrialsRunManager.ConfirmChoice(choice);
                        EternalTrialsRunManager.LoadNextMap(null);
                        return new JObject { ["ok"] = true, ["rolls"] = i + 1, ["weapon"] = choice.containedWeapon != null ? choice.containedWeapon.name : "" };
                    }
                    return new JObject { ["ok"] = false, ["why"] = "map never offered" };
                }
                case "find_go":
                {
                    // Reverse engineering: every GameObject with this name in the loaded scenes (inactive too), with
                    // its path, active flags and components.
                    var want = (string)a["name"];
                    var arr = new JArray();
                    foreach (var go in Resources.FindObjectsOfTypeAll<GameObject>())
                    {
                        if (go.name != want || !go.scene.IsValid()) continue;
                        var path = go.name;
                        for (var t = go.transform.parent; t != null; t = t.parent) path = t.name + (t.gameObject.activeSelf ? "" : "(off)") + "/" + path;
                        var comps = new JArray();
                        foreach (var c in go.GetComponents<Component>()) comps.Add(c == null ? "null" : c.GetType().Name + (c is Behaviour bh && !bh.enabled ? "(disabled)" : ""));
                        arr.Add(new JObject { ["path"] = path, ["scene"] = go.scene.name, ["activeSelf"] = go.activeSelf, ["inHierarchy"] = go.activeInHierarchy, ["components"] = comps });
                    }
                    return new JObject { ["objects"] = arr };
                }
                case "renderers_dump":
                {
                    // Reverse engineering: every renderer under the local character, with its material colours.
                    var lp = ThronefallMP.Plugin.Instance.PlayerManager.LocalPlayer?.Object;
                    if (lp == null) return new JObject { ["ok"] = false, ["error"] = "no local player" };
                    var arr = new JArray();
                    foreach (var r in lp.GetComponentsInChildren<Renderer>(true))
                    {
                        var path = r.name;
                        for (var t = r.transform.parent; t != null && t != lp.transform; t = t.parent) path = t.name + "/" + path;
                        var mats = new JArray();
                        foreach (var m in r.sharedMaterials)
                        {
                            if (m == null) continue;
                            var cols = new List<string>();
                            foreach (var pn in new[] { "_BaseColor", "_ColorDim", "_Color1", "_Color2", "_Color3", "_Color" })
                                if (m.HasProperty(pn)) cols.Add(pn + "=" + ColorUtility.ToHtmlStringRGB(m.GetColor(pn)));
                            mats.Add($"{m.name} [{m.shader.name}] {string.Join(" ", cols)}");
                        }
                        arr.Add(new JObject { ["path"] = path, ["type"] = r.GetType().Name, ["active"] = r.gameObject.activeInHierarchy && r.enabled, ["materials"] = mats });
                    }
                    return new JObject { ["renderers"] = arr };
                }
                case "wardrobe_dump":
                {
                    // Reverse engineering: the local character's wardrobe parts, their renderers, materials, shaders
                    // and colour properties, plus the game's armour material choices.
                    var lp = ThronefallMP.Plugin.Instance.PlayerManager.LocalPlayer?.Object;
                    var wp = lp != null ? lp.GetComponentInChildren<WardrobePlayer>(true) : null;
                    if (wp == null) return new JObject { ["ok"] = false, ["error"] = "no WardrobePlayer" };
                    var tr = Traverse.Create(wp);
                    var parts = new JObject();
                    foreach (var field in new[] { "Armour", "Armour_DeadStatic", "Cape", "Crown", "Mount" })
                    {
                        var go = tr.Field(field).GetValue() as GameObject;
                        if (go == null) { parts[field] = null; continue; }
                        var rends = new JArray();
                        foreach (var r in go.GetComponentsInChildren<Renderer>(true))
                        {
                            var mats = new JArray();
                            foreach (var m in r.sharedMaterials)
                            {
                                if (m == null) continue;
                                var props = new JArray();
                                for (var i = 0; i < m.shader.GetPropertyCount(); i++)
                                {
                                    var pn = m.shader.GetPropertyName(i);
                                    var pt = m.shader.GetPropertyType(i);
                                    if (pt == UnityEngine.Rendering.ShaderPropertyType.Color)
                                        props.Add(pn + "=" + ColorUtility.ToHtmlStringRGBA(m.GetColor(pn)));
                                    else if (pt == UnityEngine.Rendering.ShaderPropertyType.Texture)
                                        props.Add(pn + "=tex:" + (m.GetTexture(pn) != null ? m.GetTexture(pn).name : "null"));
                                }
                                mats.Add(new JObject { ["name"] = m.name, ["shader"] = m.shader.name, ["props"] = props });
                            }
                            rends.Add(new JObject { ["renderer"] = r.name, ["type"] = r.GetType().Name, ["active"] = r.gameObject.activeInHierarchy, ["materials"] = mats });
                        }
                        parts[field] = new JObject { ["object"] = go.name, ["active"] = go.activeInHierarchy, ["renderers"] = rends };
                    }
                    var armours = new JArray();
                    var list = tr.Field("armourMaterials").GetValue() as System.Collections.IList;
                    if (list != null)
                        foreach (var o in list)
                            if (o is WardrobeMaterial wm)
                                armours.Add(wm.name + ": " + (wm.material != null ? wm.material.name : "null") + " / " + (wm.accentMat != null ? wm.accentMat.name : "null"));
                    return new JObject { ["parts"] = parts, ["armourMaterials"] = armours,
                        ["curArmourMat"] = (tr.Field("curArmourMat").GetValue() as WardrobeMaterial)?.name };
                }
                case "wheel_pick":
                    // Which ping the wheel picks for a mouse offset (pixels, y up) from where the key went down.
                    return new JObject { ["kind"] = ThronefallMP.UI.Panels.PingPanel.WheelPick(new Vector2((float)a["dx"], (float)a["dy"])).ToString() };
                case "colors":
                {
                    // Every player's colour choice as this machine sees it, the colour used, and the cape colour.
                    var arr = new JArray();
                    foreach (var p in ThronefallMP.Plugin.Instance.PlayerManager.GetAllPlayers())
                    {
                        var c = ThronefallMP.Network.PlayerColors.For(p.Id);
                        var cape = ThronefallMP.Network.PlayerColors.CapeColor(p);
                        arr.Add(new JObject
                        {
                            ["id"] = p.Id,
                            ["local"] = p.Id == ThronefallMP.Plugin.Instance.PlayerManager.LocalId,
                            ["choice"] = ThronefallMP.Network.PlayerColors.ChoiceOf(p.SteamID),
                            ["color"] = ColorUtility.ToHtmlStringRGB(c),
                            ["cape"] = cape.HasValue ? ColorUtility.ToHtmlStringRGB(cape.Value) : null,
                            ["body"] = ThronefallMP.Network.PlayerColors.BodyColor(p) is Color b ? ColorUtility.ToHtmlStringRGB(b) : null,
                        });
                    }
                    return new JObject { ["players"] = arr };
                }
                case "arrows":
                {
                    var arr = new JObject();
                    foreach (var kv in ThronefallMP.UI.Panels.FriendArrowPanel.Visible)
                        arr[kv.Key.ToString()] = new JArray(kv.Value.x, kv.Value.y);
                    return new JObject { ["visible"] = arr, ["on"] = ThronefallMP.ModKeys.FriendArrow };
                }
                case "loadout_info":
                {
                    // The bar above the level start screen; "ready" presses its Ready button like a click.
                    if ((string)a["action"] == "ready")
                    {
                        var btn = ThronefallMP.UI.UIManager.LoadoutInfoPanel.GetComponentsInChildren<ThronefallMP.UI.Controls.ButtonControl>(true)
                            .FirstOrDefault(b => b.name == "ready_button");
                        if (btn == null || !btn.gameObject.activeInHierarchy) return new JObject { ["ok"] = false, ["error"] = "no Ready button" };
                        ClickButton(btn);
                    }
                    var ready = new JObject();
                    foreach (var p in ThronefallMP.Plugin.Instance.PlayerManager.GetAllPlayers())
                        ready[p.Id.ToString()] = ThronefallMP.Network.ReadyCheck.IsReady(p.Id);
                    return new JObject
                    {
                        ["bar"] = ThronefallMP.UI.Panels.LoadoutInfoPanel.BarVisible,
                        ["settings"] = ThronefallMP.UI.Panels.LoadoutInfoPanel.SettingsText,
                        ["readyText"] = ThronefallMP.UI.Panels.LoadoutInfoPanel.ReadyText,
                        ["button"] = ThronefallMP.UI.Panels.LoadoutInfoPanel.ButtonVisible,
                        ["dialog"] = ThronefallMP.Network.ReadyCheck.Dialog != null,
                        ["ready"] = ready,
                    };
                }
                case "match_stats":
                {
                    var current = new JObject();
                    foreach (var kv in ThronefallMP.Network.MatchStats.Current)
                        current[kv.Key.ToString()] = $"{kv.Value.Kills}|{Mathf.RoundToInt(kv.Value.Damage)}|{kv.Value.Built}|{kv.Value.Upgrades}";
                    var panel = ThronefallMP.UI.UIManager.MatchStatsPanel;
                    return new JObject
                    {
                        ["current"] = current,
                        ["panel"] = panel != null && panel.Enabled,
                        ["rows"] = new JArray(ThronefallMP.UI.Panels.MatchStatsPanel.LastRows.ToArray()),
                        ["showing"] = ThronefallMP.UI.Panels.MatchStatsPanel.Showing,
                    };
                }
                case "stats_key":
                {
                    // What the Stats key does (Key mode of Match stats / Night summary).
                    ThronefallMP.UI.UIManager.MatchStatsPanel?.PressStatsKey();
                    ThronefallMP.UI.UIManager.NightSummaryPanel?.PressStatsKey();
                    return new JObject { ["ok"] = true };
                }
                case "keys":
                {
                    // "Your keys" in the Co-op Settings panel. "click": press the key button of a row (starts the
                    // capture, like a mouse click). "set": what pressing a key during the capture does (SetKey).
                    var panel = ThronefallMP.UI.UIManager.CoopSettingsPanel;
                    var action = (string)a["action"];
                    if (action == "click")
                    {
                        panel.ShowTabOf((string)a["row"]);
                        var row = panel.GetComponentsInChildren<Transform>(true).FirstOrDefault(t => t.name == (string)a["row"]);
                        var btn = row?.GetComponentsInChildren<ThronefallMP.UI.Controls.ButtonControl>(true).FirstOrDefault(b => b.name == "key" || b.name == ((int?)a["dir"] < 0 ? "prev" : "next"));
                        if (btn == null || !btn.gameObject.activeInHierarchy) return new JObject { ["ok"] = false, ["error"] = "no button in row " + a["row"] };
                        ClickButton(btn);
                    }
                    else if (action == "set")
                    {
                        panel.SetKey((string)a["row"], (KeyCode)Enum.Parse(typeof(KeyCode), (string)a["key"]));
                    }
                    var rows = new JObject();
                    foreach (var r in ThronefallMP.ModKeys.Rows.Concat(ThronefallMP.GameKeys.Rows()))
                    {
                        var t = panel.GetComponentsInChildren<Transform>(true).FirstOrDefault(x => x.name == r.Key);
                        var text = t?.GetComponentsInChildren<TMPro.TextMeshProUGUI>(true).Where(x => x.name != "label").Select(x => x.text).FirstOrDefault(x => x != "<" && x != ">");
                        rows[r.Key] = text;
                    }
                    return new JObject
                    {
                        ["capturing"] = panel.Capturing,
                        ["rows"] = rows,
                        ["ping"] = ThronefallMP.ModKeys.Ping.ToString(),
                        ["pingAt"] = ThronefallMP.ModKeys.PingAt.ToString(),
                        ["chat"] = ThronefallMP.ModKeys.Chat.ToString(),
                        ["list"] = ThronefallMP.ModKeys.PlayerList.ToString(),
                        ["wheel4"] = ThronefallMP.ModKeys.WheelKey(4).ToString(),
                        ["chatUp"] = ThronefallMP.ModKeys.ChatScrollUp.ToString(),
                    };
                }
                case "hold_night":
                    Bot.Start(Bot.HoldNight((float?)a["seconds"] ?? 6f));
                    return TaskInfo();
                case "lobbydata":
                {
                    var net = ThronefallMP.Plugin.Instance.Network;
                    var key = (string)a["key"];
                    return new JObject { ["value"] = net.Online ? SteamMatchmaking.GetLobbyData(net.Lobby, key) : null };
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
                    return new JObject { ["errors"] = new JArray(Errors), ["known"] = new JArray(KnownErrors), ["dialogs"] = new JArray(Dialogs) };
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
                        Step(_role == "host" ? "host-create" : _role == "solo" ? "solo" : "join-wait-lobbyfile");
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
