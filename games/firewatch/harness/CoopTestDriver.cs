using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Text;
using BepInEx;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace CoopTest
{
    /// <summary>
    /// Out-of-tree test driver for Shoshone Co-op. Never patches the mod.
    ///
    /// Config (per game copy): BepInEx/cooptest.ini
    ///   role=host|client|none      autoconnect=1     autostart=1 (click New Game)
    ///   host=127.0.0.1             port=7778         name=Host
    ///   shots=1                    shotinterval=10
    ///
    /// Commands: BepInEx/cooptest_cmd.txt (consumed, one per line)
    ///   host [port] [name] | join [ip] [port] | leave | newgame | bring | goto
    ///   shot | dump | fact <name> <float> | event <name>
    ///
    /// State: BepInEx/cooptest_&lt;role&gt;.json every 2 s.
    /// </summary>
    [BepInPlugin("cooptest.driver", "Co-op Test Driver", "0.2.0")]
    public class Driver : BaseUnityPlugin
    {
        internal static Driver I;
        internal string Role = "none", HostAddr = "127.0.0.1", Name = "player";
        internal int Port = 7778;
        internal bool AutoStart, AutoConnect, Shots;
        internal float ShotInterval = 10f;

        void Awake()
        {
            I = this;
            LoadIni();
            Info("driver: role=" + Role + " host=" + HostAddr + ":" + Port + " autostart=" + AutoStart + " autoconnect=" + AutoConnect);
            StartCoroutine(Attach());
        }

        internal void Info(string s) { Logger.LogInfo(s); }
        internal void Warn(string s) { Logger.LogWarning(s); }

        void LoadIni()
        {
            try
            {
                string p = Path.Combine(Paths.BepInExRootPath, "cooptest.ini");
                if (!File.Exists(p)) return;
                foreach (string raw in File.ReadAllLines(p))
                {
                    string line = raw.Trim();
                    if (line.Length == 0 || line.StartsWith("#") || line.StartsWith(";")) continue;
                    int eq = line.IndexOf('=');
                    if (eq <= 0) continue;
                    string k = line.Substring(0, eq).Trim().ToLowerInvariant();
                    string v = line.Substring(eq + 1).Trim();
                    switch (k)
                    {
                        case "role": Role = v; break;
                        case "host": HostAddr = v; break;
                        case "port": int.TryParse(v, out Port); break;
                        case "name": Name = v; break;
                        case "autostart": AutoStart = v == "1" || v.ToLower() == "true"; break;
                        case "autoconnect": AutoConnect = v == "1" || v.ToLower() == "true"; break;
                        case "shots": Shots = v == "1" || v.ToLower() == "true"; break;
                        case "shotinterval": float.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out ShotInterval); break;
                    }
                }
            }
            catch (Exception e) { Warn("ini: " + e.Message); }
        }

        IEnumerator Attach()
        {
            GameObject target = null;
            for (int i = 0; i < 600 && target == null; i++)
            {
                target = GameObject.Find("StoryCoop_Runner");
                if (target == null) yield return new WaitForSeconds(0.1f);
            }
            if (target == null)
            {
                target = new GameObject("CoopTest_Runner");
                DontDestroyOnLoad(target);
                try { target.tag = "MAIN"; } catch { }
                Warn("mod runner not found; own object created");
            }
            if (target.GetComponent<DriverBehaviour>() == null) target.AddComponent<DriverBehaviour>();
            Info("driver attached to " + target.name);
        }
    }

    public class DriverBehaviour : MonoBehaviour
    {
        Driver D { get { return Driver.I; } }
        Type coopType;
        object coop;
        PropertyInfo pIp, pPort, pName, pStatus, pPartner, pSynced, pAvatar;
        FieldInfo fInstance;
        MethodInfo mHost, mJoin, mLeave, mBring, mGoTo;
        string statePath, cmdPath, shotDir, startLogPath;
        float stateTimer, cmdTimer, shotTimer;
        int shotIndex;
        bool acted, startTried, startDone;
        float startAttemptAt;
        int startAttempt;
        string lastScene = "", lastStatus = "";
        readonly List<string> log = new List<string>();

        void Awake()
        {
            statePath = Path.Combine(Paths.BepInExRootPath, "cooptest_" + D.Role + ".json");
            cmdPath = Path.Combine(Paths.BepInExRootPath, "cooptest_cmd.txt");
            shotDir = Path.Combine(Paths.BepInExRootPath, "cooptest_shots");
            startLogPath = Path.Combine(Paths.BepInExRootPath, "cooptest_start.txt");
            try { Directory.CreateDirectory(shotDir); } catch { }
            coopType = Type.GetType("StoryCoop.CoopRunner, StoryCoop");
            if (coopType == null) { Log("MOD MISSING: StoryCoop.CoopRunner not found"); return; }
            fInstance = coopType.GetField("Instance", BindingFlags.Public | BindingFlags.Static);
            pIp = coopType.GetProperty("IpField");
            pPort = coopType.GetProperty("PortField");
            pName = coopType.GetProperty("NameField");
            pStatus = coopType.GetProperty("Status");
            pPartner = coopType.GetProperty("PartnerName");
            pSynced = coopType.GetProperty("Synced");
            pAvatar = coopType.GetProperty("Avatar");
            mHost = coopType.GetMethod("Host");
            mJoin = coopType.GetMethod("Join");
            mLeave = coopType.GetMethod("Leave");
            mBring = coopType.GetMethod("BringPartner");
            mGoTo = coopType.GetMethod("GoToPartner");
            SetCoopField(pName, D.Name);
            SetCoopField(pPort, D.Port.ToString());
            SetCoopField(pIp, D.HostAddr);
            Log("behaviour ready");
        }

        void SetCoopField(PropertyInfo p, string v)
        {
            object c = Coop();
            if (p == null || c == null) return;
            try { p.SetValue(c, v, null); } catch { }
        }

        object Coop()
        {
            if (coop != null) return coop;
            if (fInstance == null) return null;
            coop = fInstance.GetValue(null);
            return coop;
        }

        void Log(string s)
        {
            log.Add(DateTime.Now.ToString("HH:mm:ss.fff") + " " + s);
            if (log.Count > 60) log.RemoveAt(0);
            if (D != null) D.Info("[drv] " + s);
        }

        void Update()
        {
            if (coopType == null) return;
            string scene = SceneManager.GetActiveScene().name;
            if (scene != lastScene) { Log("scene " + lastScene + " -> " + scene); lastScene = scene; }

            object c = Coop();
            if (c != null)
            {
                if (!acted && Time.realtimeSinceStartup > 3f)
                {
                    acted = true;
                    if (D.Role == "host" && mHost != null) { Log("auto host"); mHost.Invoke(c, null); }
                    else if (D.Role == "client" && D.AutoConnect && mJoin != null) { Log("auto join " + D.HostAddr + ":" + D.Port); mJoin.Invoke(c, null); }
                }

                if (D.AutoStart && D.Role == "host" && !startDone && scene == "Main_Menu")
                {
                    if (!startTried || Time.realtimeSinceStartup - startAttemptAt > 6f)
                    {
                        startTried = true;
                        startAttemptAt = Time.realtimeSinceStartup;
                        startAttempt++;
                        if (startAttempt > 3) { Log("newgame: giving up"); startDone = true; }
                        else TryStartNewGame(startAttempt);
                    }
                }
                if (D.AutoStart && vgPlayerController.playerGameObject != null) startDone = true;

                string st = Str(pStatus, c);
                if (st != lastStatus) { lastStatus = st; Log("status: " + st); }
            }

            cmdTimer -= Time.unscaledDeltaTime;
            if (cmdTimer <= 0f) { cmdTimer = 0.5f; PumpCommands(); }
            stateTimer -= Time.unscaledDeltaTime;
            if (stateTimer <= 0f) { stateTimer = 2f; WriteState(scene); }

            if (D.Shots)
            {
                shotTimer -= Time.unscaledDeltaTime;
                if (shotTimer <= 0f) { shotTimer = D.ShotInterval; Shot("auto"); }
            }
        }

        void TryStartNewGame(int attempt)
        {
            try
            {
                Type mm = Type.GetType("vgMainMenuController, Assembly-CSharp");
                UnityEngine.Object ctl = UnityEngine.Object.FindObjectOfType(mm);
                if (ctl == null) { Log("newgame: controller missing"); return; }
                MethodInfo m = null;
                if (attempt == 1) m = mm.GetMethod("OnClickNewGame", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
                else if (attempt == 2) m = mm.GetMethod("OnNewGameStart", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
                if (m != null)
                {
                    m.Invoke(ctl, null);
                    Log("newgame: " + m.Name + " invoked");
                    File.AppendAllText(startLogPath, DateTime.Now.ToString("HH:mm:ss") + " attempt " + attempt + " " + m.Name + "\r\n");
                    return;
                }
                Log("newgame: method not found at attempt " + attempt);
            }
            catch (Exception e) { Log("newgame failed: " + e.Message); }
        }

        void PumpCommands()
        {
            if (!File.Exists(cmdPath)) return;
            string[] lines;
            try { lines = File.ReadAllLines(cmdPath); } catch { return; }
            try { File.Delete(cmdPath); } catch { }
            foreach (string raw in lines)
            {
                string line = raw.Trim();
                if (line.Length == 0 || line.StartsWith("#")) continue;
                string[] a = line.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
                try { RunCommand(a); } catch (Exception e) { Log("cmd '" + line + "': " + e.Message); }
            }
        }

        void RunCommand(string[] a)
        {
            string cmd = a[0].ToLowerInvariant();
            object c = Coop();
            Log("cmd " + string.Join(" ", a));
            switch (cmd)
            {
                case "host":
                    if (pPort != null && a.Length > 1) pPort.SetValue(c, a[1], null);
                    if (mHost != null) mHost.Invoke(c, null);
                    break;
                case "join":
                    if (pIp != null && a.Length > 1) pIp.SetValue(c, a[1], null);
                    if (pPort != null && a.Length > 2) pPort.SetValue(c, a[2], null);
                    if (mJoin != null) mJoin.Invoke(c, null);
                    break;
                case "leave": if (mLeave != null) mLeave.Invoke(c, null); break;
                case "bring": if (mBring != null) mBring.Invoke(c, null); break;
                case "goto": if (mGoTo != null) mGoTo.Invoke(c, null); break;
                case "newgame": startAttempt = 0; startDone = false; startTried = false; break;
                case "shot": Shot("cmd"); break;
                case "fact":
                    if (a.Length > 2 && vgEventManager.Instance != null)
                        vgEventManager.Instance.SetFact(a[1], vgFactOperation.Set, float.Parse(a[2], CultureInfo.InvariantCulture), vgBlackboardLocationHint.Global);
                    break;
                case "event":
                    if (a.Length > 1 && vgEventManager.Instance != null)
                        Log("event -> " + vgEventManager.Instance.TriggerEvent(a[1]));
                    break;
                case "tp":
                    if (a.Length >= 4) TeleportLocal(float.Parse(a[1], CultureInfo.InvariantCulture), float.Parse(a[2], CultureInfo.InvariantCulture), float.Parse(a[3], CultureInfo.InvariantCulture));
                    else Log("tp needs x y z");
                    break;
                case "factlist": DumpFacts(); break;
                case "factget": if (a.Length > 1) Log("fact " + a[1] + " = " + GetFact(a[1])); break;
                case "dump": WriteState(SceneManager.GetActiveScene().name); break;
                case "fsms": DumpFsms(a.Length > 1 ? a[1] : ""); break;
                case "fsmevent": if (a.Length > 2) SendFsm(a[1], a[2]); break;
                case "fsmevents": DumpFsmEvents(a.Length > 1 ? string.Join(" ", a, 1, a.Length - 1) : ""); break;
                case "continue": MenuCall("OnClickContinue"); break;
                case "loadgame": MenuCall("OnClickLoadGame"); break;
                case "debugstart": MenuCall("OnClickDebugStart"); break;
                case "freeroam": MenuCall("OnClickDebugFreeRoamStart"); break;
                case "menu": if (a.Length > 1) MenuCall(a[1]); break;
                case "ui": ListUi(a.Length > 1 ? a[1] : ""); break;
                case "hideui": HideUi(a.Length > 1 ? a[1] : "zzzz"); break;
                case "interacts": ListInteracts(a.Length > 1 ? float.Parse(a[1], CultureInfo.InvariantCulture) : 40f); break;
                case "interact": if (a.Length > 2) DoInteract(a[1], a[2]); break;
                case "pathto": if (a.Length > 1) PathTo(string.Join(" ", a, 1, a.Length - 1)); break;
                case "scenes": DumpScenes(a.Length > 1 ? string.Join(" ", a, 1, a.Length - 1) : ""); break;
                default: Log("unknown command " + cmd); break;
            }
        }


        void DumpFsms(string filter)
        {
            var all = UnityEngine.Object.FindObjectsOfType<PlayMakerFSM>();
            Log("fsms: " + all.Length + (string.IsNullOrEmpty(filter) ? "" : " filter=" + filter));
            foreach (var f in all)
            {
                if (f == null) continue;
                string name = f.FsmName ?? "";
                string go = f.gameObject != null ? f.gameObject.name : "";
                if (!string.IsNullOrEmpty(filter) && name.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0 && go.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0) continue;
                string state = "?";
                try { state = f.Fsm != null ? f.Fsm.ActiveStateName : "?"; } catch { }
                Log("fsm " + go + " [" + name + "] state=" + state);
            }
        }

        void DumpFsmEvents(string filter)
        {
            foreach (var f in UnityEngine.Object.FindObjectsOfType<PlayMakerFSM>())
            {
                if (f == null) continue;
                string name = f.FsmName ?? "";
                string go = f.gameObject != null ? f.gameObject.name : "";
                if (filter.Length > 0 && name.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0 && go.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0) continue;
                string state = "?";
                try { state = f.Fsm != null ? f.Fsm.ActiveStateName : "?"; } catch { }
                Log("fsm " + go + " [" + name + "] state=" + state);
                try
                {
                    if (f.Fsm != null && f.Fsm.Events != null)
                        foreach (var ev in f.Fsm.Events)
                            if (ev != null) Log("  event " + ev.Name);
                }
                catch (Exception e) { Log("  events failed: " + e.Message); }
            }
        }

        void SendFsm(string who, string ev)
        {
            var all = UnityEngine.Object.FindObjectsOfType<PlayMakerFSM>();
            int n = 0;
            foreach (var f in all)
            {
                if (f == null) continue;
                string name = f.FsmName ?? "";
                string go = f.gameObject != null ? f.gameObject.name : "";
                if (name != who && go != who) continue;
                f.SendEvent(ev);
                n++;
                Log("fsm " + go + " [" + name + "] <- " + ev);
            }
            if (n == 0) Log("fsm: no match for " + who);
        }

        void MenuCall(string method)
        {
            Type mm = Type.GetType("vgMainMenuController, Assembly-CSharp");
            UnityEngine.Object ctl = UnityEngine.Object.FindObjectOfType(mm);
            if (ctl == null) { Log("menu: controller missing"); return; }
            MethodInfo m = mm.GetMethod(method, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            if (m == null) { Log("menu: " + method + " missing"); return; }
            m.Invoke(ctl, null);
            Log("menu: " + method + " invoked");
        }
        void TeleportLocal(float x, float y, float z)
        {
            try
            {
                Type ga = Type.GetType("StoryCoop.GameApi, StoryCoop");
                MethodInfo m = ga == null ? null : ga.GetMethod("Teleport");
                if (m == null) { Log("tp: GameApi.Teleport missing"); return; }
                m.Invoke(null, new object[] { new Vector3(x, y, z), 0f });
                Log("tp -> " + x + "," + y + "," + z);
            }
            catch (Exception e) { Log("tp failed: " + e.Message); }
        }

        void DumpFacts()
        {
            Type ga = Type.GetType("StoryCoop.GameApi, StoryCoop");
            if (ga == null) { Log("factlist: GameApi missing"); return; }
            int shown = 0;
            foreach (string prop in new[] { "GlobalBoard", "DayBoard" })
            {
                object board = null;
                try { board = ga.GetProperty(prop).GetValue(null, null); } catch { }
                if (board == null) continue;
                try
                {
                    FieldInfo f = board.GetType().GetField("Facts");
                    IList list = f == null ? null : f.GetValue(board) as IList;
                    if (list == null) continue;
                    Log("facts " + prop + ": " + list.Count);
                    for (int i = 0; i < list.Count && shown < 40; i++, shown++)
                    {
                        object it = list[i];
                        Log("  " + prop + "." + FactField(it, "name") + "=" + FactField(it, "value"));
                    }
                }
                catch (Exception e) { Log("factlist failed: " + e.Message); }
            }
        }

        string GetFact(string name)
        {
            Type ga = Type.GetType("StoryCoop.GameApi, StoryCoop");
            if (ga == null) return "?";
            foreach (string prop in new[] { "GlobalBoard", "DayBoard" })
            {
                object board = null;
                try { board = ga.GetProperty(prop).GetValue(null, null); } catch { }
                if (board == null) continue;
                try
                {
                    FieldInfo f = board.GetType().GetField("Facts");
                    IList list = f == null ? null : f.GetValue(board) as IList;
                    if (list == null) continue;
                    for (int i = 0; i < list.Count; i++)
                    {
                        object it = list[i];
                        if ((FactField(it, "name") ?? "") == name) return prop + ":" + FactField(it, "value");
                    }
                }
                catch { }
            }
            return "not found";
        }

        static string FactField(object fact, string field)
        {
            if (fact == null) return "";
            try
            {
                FieldInfo f = fact.GetType().GetField(field);
                if (f == null) return "";
                object v = f.GetValue(fact);
                return v == null ? "" : v.ToString();
            }
            catch { return ""; }
        }

        void ListUi(string filter)
        {
            int n = 0;
            foreach (var root in SceneManager.GetActiveScene().GetRootGameObjects())
            {
                if (root == null) continue;
                if (filter.Length > 0 && root.name.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0) continue;
                n++;
                Log("ui root " + root.name + " active=" + root.activeSelf);
            }
            foreach (var t in UnityEngine.Object.FindObjectsOfType<Canvas>())
            {
                if (t == null || t.gameObject == null) continue;
                string path = t.gameObject.name;
                if (filter.Length > 0 && path.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0) continue;
                n++;
                Log("ui canvas " + path + " activeInHierarchy=" + t.gameObject.activeInHierarchy);
            }
            if (n == 0) Log("ui: no match for " + filter);
        }

        void HideUi(string filter)
        {
            int n = 0;
            foreach (var root in SceneManager.GetActiveScene().GetRootGameObjects())
            {
                if (root == null) continue;
                if (root.name.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0) continue;
                root.SetActive(false);
                n++;
                Log("ui hidden root " + root.name);
            }
            foreach (var canvas in UnityEngine.Object.FindObjectsOfType<Canvas>())
            {
                if (canvas == null || canvas.gameObject == null) continue;
                if (canvas.gameObject.name.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0) continue;
                canvas.gameObject.SetActive(false);
                n++;
                Log("ui hidden canvas " + canvas.gameObject.name);
            }
            if (n == 0) Log("hideui: no match for " + filter);
        }

        void DumpScenes(string name)
        {
            int n = SceneManager.sceneCount;
            for (int i = 0; i < n; i++)
            {
                Scene s = SceneManager.GetSceneAt(i);
                Log("scene[" + i + "] '" + s.name + "' valid=" + s.IsValid() + " loaded=" + s.isLoaded + " roots=" + s.rootCount);
            }
            if (string.IsNullOrEmpty(name)) return;
            Scene by = SceneManager.GetSceneByName(name);
            Log("byname '" + name + "' valid=" + by.IsValid() + " loaded=" + by.isLoaded + " roots=" + by.rootCount);
            if (by.IsValid())
                foreach (GameObject r in by.GetRootGameObjects())
                    Log("root '" + r.name + "' children=" + r.transform.childCount);
            foreach (vgTargetInteract ti in UnityEngine.Object.FindObjectsOfType<vgTargetInteract>())
            {
                if (ti == null || ti.gameObject.name.IndexOf(name, StringComparison.OrdinalIgnoreCase) < 0) continue;
                Scene os = ti.gameObject.scene;
                Log("obj '" + ti.gameObject.name + "' scene='" + os.name + "' valid=" + os.IsValid() + " loaded=" + os.isLoaded + " roots=" + os.rootCount);
            }
        }

        void PathTo(string filter)
        {
            {
                foreach (var found in UnityEngine.Object.FindObjectsOfType<vgTargetInteract>())
                {
                    if (found == null) continue;
                    Transform t = found.transform;
                    if (t == null || t.gameObject.name.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0) continue;
                    var parts = new List<string>();
                    Transform c = t;
                    while (c != null) { parts.Add(c.name); c = c.parent; }
                    parts.Reverse();
                    string id = "";
                    try
                    {
                        Type oid = Type.GetType("StoryCoop.ObjectId, StoryCoop");
                        MethodInfo m = oid == null ? null : oid.GetMethod("Of");
                        if (m != null) id = (string)m.Invoke(null, new object[] { t.gameObject });
                    }
                    catch (Exception e) { id = "err " + e.Message; }
                    Log("path scene='" + t.gameObject.scene.name + "' valid=" + t.gameObject.scene.IsValid() + " path=" + string.Join("/", parts.ToArray()) + " id=" + id);
                }
            }
        }

        void ListInteracts(float radius)
        {
            GameObject me = vgPlayerController.playerGameObject;
            if (me == null) { Log("interacts: no player"); return; }
            Vector3 p = me.transform.position;
            int n = 0;
            foreach (vgTargetInteract ti in UnityEngine.Object.FindObjectsOfType<vgTargetInteract>())
            {
                if (ti == null) continue;
                float d = Vector3.Distance(ti.transform.position, p);
                if (d > radius) continue;
                n++;
                Log("interact " + ti.gameObject.name + " d=" + d.ToString("F1") + " pos=" + ti.transform.position.ToString("F1"));
            }
            Log("interacts within " + radius + "m: " + n);
        }

        void DoInteract(string filter, string ev)
        {
            int n = 0;
            foreach (vgTargetInteract ti in UnityEngine.Object.FindObjectsOfType<vgTargetInteract>())
            {
                if (ti == null) continue;
                if (ti.gameObject.name.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0) continue;
                MethodInfo m = typeof(vgTargetInteract).GetMethod("TriggerEvent", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
                if (m == null) { Log("interact: TriggerEvent missing"); return; }
                m.Invoke(ti, new object[] { ev });
                n++;
                Log("interact " + ti.gameObject.name + " <- " + ev);
            }
            if (n == 0) Log("interact: no match for " + filter);
        }

        void Shot(string why)
        {
            try
            {
                string p = Path.Combine(shotDir, D.Role + "_" + (shotIndex++) + "_" + why + ".png");
                ScreenCapture.CaptureScreenshot(p);
                Log("shot " + Path.GetFileName(p));
            }
            catch (Exception e) { Log("shot failed: " + e.Message); }
        }

        void WriteState(string scene)
        {
            object c = Coop();
            StringBuilder sb = new StringBuilder();
            sb.Append("{");
            sb.Append("\"t\":\"").Append(DateTime.Now.ToString("HH:mm:ss")).Append("\",");
            sb.Append("\"role\":\"").Append(D.Role).Append("\",");
            sb.Append("\"scene\":\"").Append(scene).Append("\",");
            sb.Append("\"mod\":").Append(c != null ? "true" : "false").Append(",");
            sb.Append("\"status\":\"").Append(Esc(Str(pStatus, c))).Append("\",");
            sb.Append("\"partner\":\"").Append(Esc(Str(pPartner, c))).Append("\",");
            sb.Append("\"synced\":").Append(Bool(pSynced, c) ? "true" : "false").Append(",");
            GameObject pcGo = vgPlayerController.playerGameObject;
            sb.Append("\"player\":").Append(pcGo != null ? "true" : "false").Append(",");
            if (pcGo != null)
            {
                Vector3 p = pcGo.transform.position;
                sb.Append("\"pos\":[").Append(p.x.ToString("F2")).Append(",").Append(p.y.ToString("F2")).Append(",").Append(p.z.ToString("F2")).Append("],");
            }
            float day = vgEventManager.Instance != null ? vgEventManager.Instance.GetCurrentDayValue() : -1f;
            sb.Append("\"day\":").Append(day.ToString("F2")).Append(",");
            sb.Append("\"facts\":").Append(FactCount()).Append(",");
            sb.Append("\"avatar\":").Append(AvatarExists(c) ? "true" : "false").Append(",");
            sb.Append("\"log\":[");
            for (int i = 0; i < log.Count; i++)
            {
                if (i > 0) sb.Append(",");
                sb.Append("\"").Append(Esc(log[i])).Append("\"");
            }
            sb.Append("]}");
            try { File.WriteAllText(statePath, sb.ToString()); } catch { }
        }

        static int FactCount()
        {
            Type ga = Type.GetType("StoryCoop.GameApi, StoryCoop");
            if (ga == null) return 0;
            int n = 0;
            try
            {
                n += BoardCount(ga.GetProperty("GlobalBoard").GetValue(null, null));
                n += BoardCount(ga.GetProperty("DayBoard").GetValue(null, null));
            }
            catch { }
            return n;
        }

        static int BoardCount(object board)
        {
            if (board == null) return 0;
            try
            {
                FieldInfo f = board.GetType().GetField("Facts");
                IList list = f == null ? null : f.GetValue(board) as IList;
                return list == null ? 0 : list.Count;
            }
            catch { return 0; }
        }

        bool AvatarExists(object c)
        {
            if (pAvatar == null || c == null) return false;
            try
            {
                object a = pAvatar.GetValue(c, null);
                return a != null && a is UnityEngine.Object && (UnityEngine.Object)a != null;
            }
            catch { return false; }
        }

        static bool Bool(PropertyInfo p, object o)
        {
            if (p == null || o == null) return false;
            try { object v = p.GetValue(o, null); return v is bool && (bool)v; } catch { return false; }
        }

        static string Str(PropertyInfo p, object o)
        {
            if (p == null || o == null) return "";
            try { object v = p.GetValue(o, null); return v == null ? "" : v.ToString(); } catch { return ""; }
        }

        static string Esc(string s)
        {
            if (s == null) return "";
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\r", " ").Replace("\n", " ");
        }
    }
}










