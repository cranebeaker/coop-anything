// CoopQA.Bot: test-only MelonLoader mod for Tale of Immortal co-op runs. Inert without UserData/CoopQA/config.txt.
//
// It walks the same handlers the Tale of Immortal Multiplayer buttons call, loads a prepared save, clicks the ready
// banner, then reports state to state.json and runs numbered commands from cmd.txt. Everything goes through the
// game's own objects. It references the mod only by name at run time, so it ships none of the mod's code.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Text;
using HarmonyLib;
using MelonLoader;
using UnityEngine;
using UnityEngine.UI;

[assembly: MelonInfo(typeof(CoopQA.Bot), "CoopQA.Bot", "1.0.0", "CoopQA")]
[assembly: MelonGame(null, null)]

namespace CoopQA
{
    public class Bot : MelonMod
    {
        const int Port = 47813;
        const string Ns = "MOD_z7Qkml_Menu.";

        static string _dir;
        static Dictionary<string, string> _cfg;
        static bool _active;
        internal static bool DropDeaths;
        internal static int DeathsDropped;
        internal static object MenuInstance;

        string _scene = "?";
        string _step = "wait_title";
        float _stepAt, _nextState, _nextCmd, _nextBanner, _nextPopup, _mapAt = -1f;
        int _frames, _cmdDone, _bannerClicks, _patchTries, _loadTries, _popups, _dramas;
        bool _patched;
        Assembly _mod;
        Type _lobby, _menu, _ready, _npc, _fire;
        readonly List<string> _watch = new List<string>();
        float _saveStart, _popupSeen = -1f;
        string _saveOk = "";
        int _saveMs;

        string Role => _cfg.TryGetValue("role", out var v) ? v : "?";
        string Slot => _cfg.TryGetValue("slot", out var v) ? v : "";

        public override void OnApplicationStart()
        {
            _dir = Path.Combine(Directory.GetCurrentDirectory(), "UserData", "CoopQA");
            string cfg = Path.Combine(_dir, "config.txt");
            if (!File.Exists(cfg)) return;
            _cfg = new Dictionary<string, string>();
            foreach (string line in File.ReadAllLines(cfg))
            {
                int i = line.IndexOf('=');
                if (i > 0) _cfg[line.Substring(0, i).Trim()] = line.Substring(i + 1).Trim();
            }
            _active = true;
            DropDeaths = _cfg.TryGetValue("mutant", out var m) && m == "drop_npc_death";
            Application.runInBackground = true;
            foreach (string f in new[] { "cmd.txt", "ack.txt", "events.log", "state.json" })
                try { File.Delete(Path.Combine(_dir, f)); } catch { }
            Ev("bot start role=" + Role + " slot=" + Slot + " mutant=" + (DropDeaths ? "drop_npc_death" : "none"));
            MelonLogger.Msg("CoopQA.Bot READY role=" + Role);
        }

        public override void OnSceneWasLoaded(int buildIndex, string sceneName)
        {
            if (!_active) return;
            _scene = sceneName;
            Ev("scene " + sceneName);
        }

        public override void OnUpdate()
        {
            if (!_active) return;
            _frames++;
            float t = Time.unscaledTime;
            try
            {
                if (!_patched && t >= _nextCmd) TryPatch(t);
                if (t >= _nextPopup) { _nextPopup = t + 1f; Popups(); }
                Advance(t);
                if (t >= _nextCmd) { _nextCmd = t + 0.5f; Commands(); }
                if (t >= _nextState) { _nextState = t + 1f; WriteState(); }
            }
            catch (Exception e)
            {
                Ev("EXCEPTION " + _step + ": " + e.GetType().Name + ": " + e.Message);
                _nextPopup = t + 2f;
            }
        }

        // ---------- locating the mod and installing the test-only patches ----------

        void TryPatch(float t)
        {
            _nextCmd = t + 0.5f;
            _mod = AppDomain.CurrentDomain.GetAssemblies().FirstOrDefault(a => a.GetName().Name == "TOI_MP_MenuMod");
            if (_mod == null) return;
            _lobby = _mod.GetType(Ns + "TitleLobby");
            _menu = _mod.GetType(Ns + "MenuMod");
            _ready = _mod.GetType(Ns + "ReadyIndicator");
            _npc = _mod.GetType(Ns + "NpcSync");
            _fire = _mod.GetType(Ns + "FirewallHelper");
            if (_lobby == null || _menu == null || _ready == null || _npc == null || _fire == null) { Ev("mod types missing"); return; }
            var h = new HarmonyLib.Harmony("CoopQA.Bot");
            const BindingFlags All = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static | BindingFlags.Instance;
            h.Patch(_fire.GetMethod("EnsureHostRulesAsync", All), new HarmonyMethod(typeof(Hooks).GetMethod(nameof(Hooks.Skip))));
            h.Patch(_npc.GetMethod("OnNpcDeathPacket", All), new HarmonyMethod(typeof(Hooks).GetMethod(nameof(Hooks.DeathPacket))));
            h.Patch(_menu.GetMethod("OnUpdate", All), new HarmonyMethod(typeof(Hooks).GetMethod(nameof(Hooks.CaptureMenu))));
            h.Patch(typeof(TcpListener).GetConstructor(new[] { typeof(IPAddress), typeof(int) }),
                new HarmonyMethod(typeof(Hooks).GetMethod(nameof(Hooks.ListenerCtor))));
            h.Patch(typeof(UdpClient).GetConstructor(new[] { typeof(IPEndPoint) }),
                new HarmonyMethod(typeof(Hooks).GetMethod(nameof(Hooks.UdpCtor))));
            _patched = true;
            Ev("patches installed; mod found");
        }

        object Mf(string name)
        {
            if (MenuInstance == null) return null;
            return _menu.GetField(name, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(MenuInstance);
        }

        void Lobby(string method, params object[] args)
        {
            var m = _lobby.GetMethod(method, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
            Ev("lobby " + method);
            m.Invoke(null, args);
        }

        object Transport() => Mf("transport");

        bool Connected()
        {
            var t = Transport();
            if (t == null) return false;
            return (bool)t.GetType().GetProperty("IsConnected").GetValue(t, null);
        }

        int Peers()
        {
            var t = Transport();
            if (t == null) return -1;
            return (int)t.GetType().GetProperty("PeerCount").GetValue(t, null);
        }

        // ---------- the walk from title screen to shared world ----------

        void Go(string step, float t)
        {
            Ev("step " + _step + " -> " + step);
            _step = step;
            _stepAt = t;
        }

        void Advance(float t)
        {
            switch (_step)
            {
                case "wait_title":
                    if (_patched && MenuInstance != null && _scene == "Login" && Login() != null && t - _stepAt > 8f)
                    {
                        if (Role == "solo") Go("enter", t);
                        else if (Role == "host") { Lobby("OnCreateClick"); Go("wait_peer", t); }
                        else Go("connect", t);
                    }
                    break;
                case "connect":
                    if (t - _stepAt > 15f)
                    {
                        _lobby.GetField("_ipBuffer", BindingFlags.NonPublic | BindingFlags.Static).SetValue(null, _cfg["ip"]);
                        Lobby("OnConnectClick");
                        Go("wait_peer", t);
                    }
                    break;
                case "wait_peer":
                    if (Role == "host" ? Peers() >= 1 : Connected())
                    {
                        Ev("session up peers=" + Peers() + " connected=" + Connected());
                        Go("enter", t);
                    }
                    break;
                case "enter":
                    if (t - _stepAt > 3f) { Lobby("OnCreateCharClick"); Go("load", t); }
                    break;
                case "load":
                    if (t - _stepAt > 3f + _loadTries * 25f && TryLoad()) _loadTries++;
                    if (_scene == "Map") Go("map", t);
                    break;
                case "map":
                    if (InWorld())
                    {
                        if (_mapAt < 0f) { _mapAt = t; Ev("in world as " + PlayerId()); }
                        if (t - _mapAt > 12f)
                        {
                            if (Role == "client") Rename();
                            Go(Role == "solo" ? "solo" : "banner", t);
                        }
                    }
                    break;
                case "banner":
                    bool done = Role == "host" ? B("_hostReady") : B("_shippedFinal");
                    if (done) { Go("wait_shared", t); break; }
                    if (t >= _nextBanner)
                    {
                        _nextBanner = t + 6f;
                        var cb = _ready.GetField("BannerClicked", BindingFlags.Public | BindingFlags.Static)?.GetValue(null) as Action;
                        _bannerClicks++;
                        Ev("banner click " + _bannerClicks + (cb == null ? " (no handler)" : ""));
                        cb?.Invoke();
                    }
                    break;
                case "wait_shared":
                    if (B("_sharedActive")) Go("shared", t);
                    break;
            }
        }

        bool B(string f) => Mf(f) is bool b && b;

        // Both copies start from the same clean world, so the client takes a different name before it ships its
        // character. The mod matches players by name in places, and the import already gives it a fresh unit id.
        void Rename()
        {
            var pd = g.world.playerUnit.data.unitData.propertyData;
            pd.name = new UnhollowerBaseLib.Il2CppStringArray(new[] { "QA", "Client" });
            Ev("renamed client character to " + pd.GetName());
        }

        static UILogin Login()
        {
            try { return g.ui?.GetUI<UILogin>(UIType.Login); } catch { return null; }
        }

        bool TryLoad()
        {
            var login = Login();
            var items = login?.allCacheItem;
            if (items == null || items.Count == 0) return false;
            if (_loadTries == 0)
                for (int i = 0; i < items.Count; i++)
                {
                    var d = items[i]?.cacheData?.data;
                    if (d != null) Ev("slot " + d.folder + " day=" + d.worldDay + " name=" + d.name);
                }
            for (int i = 0; i < items.Count; i++)
            {
                var it = items[i];
                string f = it?.cacheData?.data?.folder;
                if (f == null || !(f == Slot || f.EndsWith("/" + Slot) || f.EndsWith("\\" + Slot))) continue;
                Ev("load click " + f);
                it.OnItemClick();
                return true;
            }
            Ev("slot not found: " + Slot);
            return true;
        }

        static bool InWorld()
        {
            try { return g.world != null && g.world.isIntoWorld && g.world.playerUnit != null; } catch { return false; }
        }

        static string PlayerId()
        {
            try { return g.world.playerUnit.data.unitData.unitID; } catch { return ""; }
        }

        // ---------- dialogs a player would answer ----------

        void Popups()
        {
            if (_step == "wait_title") return;
            try
            {
                var p = g.ui?.GetUI<UICheckPopup>(UIType.CheckPopup);
                bool open = p != null && p.gameObject.activeInHierarchy;
                if (!open) _popupSeen = -1f;
                else if (_popupSeen < 0f) _popupSeen = Time.unscaledTime;
                if (open && Time.unscaledTime - _popupSeen >= 4f)   // the mod answers its own popups first
                {
                    _popupSeen = -1f;
                    _popups++;
                    Ev("popup yes: " + Text(p.gameObject));
                    var yes = p.onYesCall;
                    g.ui.CloseUI(p, true);
                    yes?.Invoke();
                    return;
                }
            }
            catch (Exception e) { Ev("popup error: " + e.Message); }
            Drama();
            PressFirst(() => g.ui?.GetUI<UITextInfoLong>(UIType.TextInfoLong)?.gameObject);
            PressFirst(() => g.ui?.GetUI<UIStartGameTip>(UIType.StartGameTip)?.gameObject);
        }

        // A story dialog waits for the player, as it would for a person. Decline when the game offers it, never pick a
        // fight, and press Next when there is no choice. Every answer is logged as test input.
        void Drama()
        {
            try
            {
                UIDramaBase drama = null;
                foreach (var kv in g.ui.allUI)
                {
                    var list = kv.Value;
                    for (int i = 0; list != null && i < list.Count; i++)
                    {
                        var d = list[i]?.TryCast<UIDramaBase>();
                        if (d != null && d.gameObject.activeInHierarchy) { drama = d; break; }
                    }
                    if (drama != null) break;
                }
                if (drama == null) return;
                var opts = drama.activeOptionsData;
                int id = -1;
                try { id = drama.item.id; } catch { }
                if (opts != null && opts.Count > 0)
                {
                    int pick = -1;
                    var texts = new List<string>();
                    for (int i = 0; i < opts.Count; i++)
                    {
                        string s = GameTool.LS(opts[i].text) ?? "";
                        texts.Add(opts[i].id + ":" + s);
                        string low = s.ToLowerInvariant();
                        bool leave = low.Contains("leave") || low.Contains("decline") || low.Contains("refuse") || low.Contains("no ")
                            || low.Contains("ignore") || s.Contains("离开") || s.Contains("拒绝") || s.Contains("算了") || s.Contains("告辞");
                        if (leave && pick < 0) pick = i;
                    }
                    if (pick < 0) pick = opts.Count - 1;
                    _dramas++;
                    Ev("drama " + id + " option " + texts[pick] + " of " + opts.Count);
                    drama.ClickOption(opts[pick]);
                    return;
                }
                var dlg = drama.TryCast<UIDramaDialogue>();
                if (dlg != null && dlg.btnNext != null && dlg.btnNext.gameObject.activeInHierarchy && dlg.btnNext.interactable)
                {
                    _dramas++;
                    Ev("drama " + id + " next");
                    dlg.btnNext.onClick.Invoke();
                }
            }
            catch (Exception e) { Ev("drama error: " + e.Message); }
        }

        void PressFirst(Func<GameObject> find)
        {
            try
            {
                var go = find();
                if (go == null || !go.activeInHierarchy) return;
                var buttons = go.GetComponentsInChildren<Button>();
                Button pick = null;
                string names = "";
                for (int i = 0; i < buttons.Length; i++)
                {
                    var b = buttons[i];
                    if (b == null || !b.interactable || !b.gameObject.activeInHierarchy) continue;
                    string n = b.name.ToLowerInvariant();
                    names += b.name + ",";
                    if (n.Contains("ok") || n.Contains("yes") || n.Contains("confirm")) { pick = b; break; }
                    if (pick == null && !n.Contains("close") && !n.Contains("cancel") && !n.Contains("no")) pick = b;
                }
                if (pick == null) { Ev("dialog with no OK button " + go.name + " buttons=" + names); return; }
                _popups++;
                Ev("dialog press " + go.name + "/" + pick.name + ": " + Text(go));
                pick.onClick.Invoke();
            }
            catch (Exception e) { Ev("dialog error: " + e.Message); }
        }

        static string Text(GameObject go)
        {
            try
            {
                var parts = new List<string>();
                var ts = go.GetComponentsInChildren<Text>();
                for (int i = 0; i < ts.Length && i < 4; i++)
                    if (!string.IsNullOrEmpty(ts[i].text)) parts.Add(ts[i].text.Replace("\n", " "));
                string s = string.Join(" | ", parts);
                return s.Length > 120 ? s.Substring(0, 120) : s;
            }
            catch { return ""; }
        }

        // ---------- commands ----------

        void Commands()
        {
            string path = Path.Combine(_dir, "cmd.txt");
            if (!File.Exists(path)) return;
            string[] lines;
            try { lines = File.ReadAllLines(path); } catch { return; }
            foreach (string line in lines)
            {
                var p = line.Split(' ');
                if (p.Length < 2 || !int.TryParse(p[0], out int n) || n <= _cmdDone) continue;
                _cmdDone = n;
                string r;
                try { r = Run(p); } catch (Exception e) { r = "error " + e.GetType().Name + " " + e.Message; }
                File.AppendAllText(Path.Combine(_dir, "ack.txt"), n + " " + r + "\n");
                Ev("cmd " + line + " => " + r);
            }
        }

        string Run(string[] p)
        {
            switch (p[1])
            {
                case "watch":
                    _watch.Add(p[2]);
                    return "ok";
                case "kill":
                    var u = g.world.unit.GetUnit(p[2], true);
                    if (u == null) return "no-such-unit";
                    u.Die();
                    return "ok died=" + u.isDie;
                case "move":
                    var pt = g.world.playerUnit.data.unitData.GetPoint();
                    var to = new Vector2Int(pt.x + int.Parse(p[2]), pt.y + int.Parse(p[3]));
                    g.world.playerUnit.CreateAction(new UnitActionMovePlayer(to), false);
                    return "ok to=" + to.x + "," + to.y;
                case "skipmonth":
                    var map = g.ui.GetUI<UIMapMain>(UIType.MapMain);
                    if (map == null) return "no-map-ui";
                    map.uiPlayerInfo.OnSkipMonth();
                    return "ok";
                case "dump":
                    return "ok n=" + Dump(Path.Combine(_dir, p[2]));
                case "save":
                    _saveStart = Time.realtimeSinceStartup;
                    _saveOk = "pending";
                    System.Action<bool> done = ok =>
                    {
                        _saveMs = (int)((Time.realtimeSinceStartup - _saveStart) * 1000f);
                        _saveOk = ok ? "true" : "false";
                        Ev("save done ok=" + ok + " ms=" + _saveMs);
                    };
                    g.data.SaveData((Il2CppSystem.Action<bool>)done);
                    return "ok started";
                default:
                    return "unknown";
            }
        }

        int Dump(string path)
        {
            var sb = new StringBuilder();
            var list = g.world.unit.GetUnits(true);
            int n = 0;
            for (int i = 0; i < list.Count; i++)
            {
                var u = list[i];
                if (u == null) continue;
                var d = u.data.unitData;
                var pt = d.GetPoint();
                sb.Append(d.unitID).Append('\t').Append(u.isDie ? 1 : 0).Append('\t').Append(pt.x).Append('\t').Append(pt.y).Append('\n');
                n++;
            }
            File.WriteAllText(path, sb.ToString());
            return n;
        }

        // ---------- state ----------

        void WriteState()
        {
            var o = new StringBuilder("{");
            Q(o, "utc", DateTime.UtcNow.ToString("o"));
            Q(o, "step", _step);
            Q(o, "scene", _scene);
            Q(o, "role", Role);
            Q(o, "player", InWorld() ? PlayerId() : "");
            Q(o, "my_folder", Mf("_myFolder") as string ?? "");
            Q(o, "peer_folder", Mf("_peerFolder") as string ?? "");
            Q(o, "save_ok", _saveOk);
            N(o, "save_ms", _saveMs);
            N(o, "frames", _frames);
            N(o, "peers", Peers());
            N(o, "banner_clicks", _bannerClicks);
            N(o, "popups", _popups);
            N(o, "dramas", _dramas);
            N(o, "deaths_dropped", DeathsDropped);
            N(o, "cmd_done", _cmdDone);
            T(o, "connected", Connected());
            T(o, "host_ready", B("_hostReady"));
            T(o, "shipped", B("_shippedFinal"));
            T(o, "merge_saved", B("_mergeSaved"));
            T(o, "shared", B("_sharedActive"));
            T(o, "patched", _patched);
            if (InWorld())
            {
                N(o, "month", g.world.run.roundMonth);
                T(o, "month_running", g.world.run.isRunning);
                var pt = g.world.playerUnit.data.unitData.GetPoint();
                N(o, "px", pt.x);
                N(o, "py", pt.y);
                var w = new StringBuilder();
                foreach (string id in _watch)
                {
                    var u = g.world.unit.GetUnit(id, true);
                    if (w.Length > 0) w.Append(';');
                    if (u == null) { w.Append(id).Append("=gone"); continue; }
                    var q = u.data.unitData.GetPoint();
                    w.Append(id).Append('=').Append(u.isDie ? "dead" : "alive").Append(',').Append(q.x).Append(',').Append(q.y);
                }
                Q(o, "watch", w.ToString());
            }
            o.Length--;
            o.Append('}');
            string tmp = Path.Combine(_dir, "state.tmp");
            File.WriteAllText(tmp, o.ToString());
            string dst = Path.Combine(_dir, "state.json");
            try
            {
                File.Delete(dst);
                File.Move(tmp, dst);
            }
            catch (IOException) { }   // the runner is reading it; the next second writes it again
        }

        static void Q(StringBuilder o, string k, string v) =>
            o.Append('"').Append(k).Append("\":\"").Append((v ?? "").Replace("\\", "\\\\").Replace("\"", "\\\"")).Append("\",");
        static void N(StringBuilder o, string k, int v) => o.Append('"').Append(k).Append("\":").Append(v).Append(',');
        static void T(StringBuilder o, string k, bool v) => o.Append('"').Append(k).Append("\":").Append(v ? "true" : "false").Append(',');

        internal static void Ev(string s)
        {
            try { File.AppendAllText(Path.Combine(_dir, "events.log"), DateTime.UtcNow.ToString("HH:mm:ss.f") + " " + s + "\n"); } catch { }
            MelonLogger.Msg("[QA] " + s);
        }
    }

    public static class Hooks
    {
        public static bool Skip() => false;   // no firewall rules and no admin prompt during tests

        public static bool DeathPacket()      // negative control: the client ignores the host's NPC death messages
        {
            if (!Bot.DropDeaths) return true;
            Bot.DeathsDropped++;
            return false;
        }

        public static void CaptureMenu(object __instance) => Bot.MenuInstance = __instance;

        public static void ListenerCtor(ref IPAddress localaddr, int port)
        {
            if (port == 47813 && Equals(localaddr, IPAddress.Any)) localaddr = IPAddress.Loopback;
        }

        public static void UdpCtor(ref IPEndPoint localEP)
        {
            if (localEP != null && localEP.Port == 47813 && Equals(localEP.Address, IPAddress.Any))
                localEP = new IPEndPoint(IPAddress.Loopback, 47813);
        }
    }
}
