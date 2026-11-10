using System;
using System.Collections.Concurrent;
using KtcCoop.Net;
using Steamworks;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace KtcCoop
{
    /// <summary>
    /// Session manager. The game destroys DontDestroyOnLoad roots between some
    /// scene loads, so ALL session state is static and the pump object is
    /// recreated on every scene load.
    /// </summary>
    public class Coop : MonoBehaviour
    {
        public static Coop Instance { get; private set; }

        // ---- session state survives pump recreation ----
        public enum Role { None, Host, Client }
        public static Role CurrentRole;
        public static INetChannel Net;
        public static HostSession Host;
        public static ClientSession Client;
        private static readonly ConcurrentQueue<Action> MainThreadQueue = new ConcurrentQueue<Action>();
        private static float _stateTimer;
        private static float _enemyTimer;
        private static float _hostStateTimer;
        private static float _steamSearchTime;
        private static bool _sceneHooked;

        public static bool Active => Net != null && Net.Connected;
        public static bool IsHost => CurrentRole == Role.Host;
        public static bool IsClient => CurrentRole == Role.Client;

        public static ConfigEntryShim Cfg = new ConfigEntryShim();

        public class ConfigEntryShim
        {
            public string HostIp = "127.0.0.1";
            public int StateRateHz = 30;
            public int EnemyRateHz = 20;
        }

        public static void RunOnMainThread(Action a) => MainThreadQueue.Enqueue(a);

        private void Awake()
        {
            Instance = this;
            DontDestroyOnLoad(gameObject);
            Plugin.Flog("[coop] pump created");
        }

        public static void HookSceneLoads()
        {
            if (_sceneHooked) return;
            _sceneHooked = true;
            SceneManager.sceneLoaded += OnSceneLoadedStatic;
        }

        private static void OnSceneLoadedStatic(Scene s, LoadSceneMode m)
        {
            EnsurePump();
        }

        private static void EnsurePump()
        {
            if (Instance != null) return;
            var go = new GameObject("KTC-Coop");
            go.AddComponent<Coop>(); // Awake sets Instance
        }

        public static void EnsurePumpPublic() => EnsurePump();

        public static void StartHost()
        {
            if (Active) return;
            Plugin.Flog("[host] StartHost begin");
            EnsurePump();
            try
            {
                CurrentRole = Role.Host;
                Plugin.Flog("[host] creating transport…");
                Net = new Transport(true, null);
                Plugin.Flog("[host] transport ok");
                Host = new HostSession();
                Plugin.Flog($"[host] listening on UDP {Transport.Port}");
            }
            catch (Exception e)
            {
                Plugin.Flog("[host] FAILED: " + e);
                CurrentRole = Role.None;
                (Net as IDisposable)?.Dispose();
                Net = null;
            }
        }

        public static void StartClient()
        {
            if (Active) return;
            EnsurePump();
            try
            {
                CurrentRole = Role.Client;
                Net = new Transport(false, Cfg.HostIp);
                Client = new ClientSession();
                Client.SendHello();
                Plugin.Flog($"[coop] connecting to {Cfg.HostIp}:{Transport.Port}");
            }
            catch (Exception e)
            {
                Plugin.Flog("[coop] connect failed: " + e.Message);
                CurrentRole = Role.None;
                (Net as IDisposable)?.Dispose();
                Net = null;
            }
        }

        public static void Disconnect(string reason = "bye")
        {
            try { Net?.SendReliable(Op.Bye, new byte[0]); } catch { }
            ResetSession();
            Plugin.Flog("[coop] disconnected");
        }

        private static void ResetSession()
        {
            (Net as IDisposable)?.Dispose();
            Net = null;
            Host = null;
            Client = null;
            CurrentRole = Role.None;
        }

        // ---------------- Steam lobby path ----------------

        public static void StartHostSteam()
        {
            if (Net != null) return;
            if (!SteamManager.Initialized) { Plugin.Flog("[steam] Steam not ready"); return; }
            Plugin.Flog("[steam] creating lobby…");
            SteamTransport.BeginHost();
        }

        public static void StartClientSteam()
        {
            if (Net != null) return;
            if (!SteamManager.Initialized) { Plugin.Flog("[steam] Steam not ready"); return; }
            Plugin.Flog("[steam] searching lobbies…");
            SteamTransport.BeginJoin();
        }

        private static void AdoptSteamPending()
        {
            var p = SteamTransport.Pending;
            if (p == null || Net != null || p.LobbyId == CSteamID.Nil) return;
            if (p.IsHostMode)
            {
                Net = p;
                CurrentRole = Role.Host;
                Host = new HostSession();
                Plugin.Flog("[coop] steam host adopted");
            }
            else if (p.Connected)
            {
                Net = p;
                CurrentRole = Role.Client;
                Client = new ClientSession();
                Client.SendHello();
                Plugin.Flog("[coop] steam client adopted");
            }
        }

        private void OnDestroy()
        {
            if (Instance == this) Instance = null; // session state (statics) survives
        }

        private void Update()
        {
            // hotkeys
            try
            {
                if (Input.GetKeyDown(KeyCode.F9)) StartHost();
                if (Input.GetKeyDown(KeyCode.F10)) StartClient();
                if (Input.GetKeyDown(KeyCode.F6)) StartHostSteam();
                if (Input.GetKeyDown(KeyCode.F7)) StartClientSteam();
                if (Input.GetKeyDown(KeyCode.F11) && Active) Disconnect();
            }
            catch (Exception e) { Plugin.Flog("[coop] input error: " + e.Message); }

            AdoptSteamPending();
            (Net as SteamTransport)?.Tick();

            RunAutoTest();

            while (MainThreadQueue.TryDequeue(out var a))
            {
                try { a(); } catch (Exception e) { Plugin.Flog("[coop] task error: " + e); }
            }

            if (Net == null || !Net.Connected) return;

            while (Net.Inbox.TryDequeue(out var msg))
            {
                try
                {
                    switch (msg.Op)
                    {
                        case Op.Bye:
                        case Op.Timeouted:
                            Plugin.Flog("[coop] peer left");
                            ResetSession();
                            return;
                        case Op.Hello: Host?.OnHello(msg.Data); break;
                        case Op.Welcome: Client?.OnWelcome(msg.Data); break;
                        case Op.PlayerState: Host?.OnPlayerState(msg.Data); break;
                        case Op.EnemyStates: Client?.OnEnemyStates(msg.Data); break;
                        case Op.Fire: Host?.OnFire(msg.Data); break;
                        case Op.EnemyFire: Client?.OnEnemyFire(msg.Data); break;
                        case Op.Spawn: Client?.OnSpawn(msg.Data); break;
                        case Op.Death: Client?.OnDeath(msg.Data); break;
                        case Op.HitReq: Host?.OnHitReq(msg.Data); break;
                        case Op.DamagePlayer: Client?.OnDamagePlayer(msg.Data); break;
                        case Op.ShowdownGain: Client?.OnShowdownGain(msg.Data); break;
                        case Op.Global: Client?.OnGlobal(msg.Data); break;
                        case Op.Scene: if (Client != null) Client.RunOver = false; Client?.OnScene(msg.Data); break;
                        case Op.GameOver: Client?.OnGameOver(msg.Data); break;
                        case Op.ClientDied: Host?.OnClientDied(); break;
                        case Op.Mods: Client?.OnMods(msg.Data); break;
                        case Op.GunAnim:
                            if (IsHost) Host?.OnGunAnim(msg.Data);
                            else Client?.OnHostGunAnim(msg.Data);
                            break;
                        case Op.HostState: Client?.OnHostState(msg.Data); break;
                        case Op.HostFire: Client?.OnHostFire(msg.Data); break;
                    }
                }
                catch (Exception e)
                {
                    Plugin.Flog($"[coop] msg {msg.Op} error: {e}");
                }
            }

            float dt = Time.unscaledDeltaTime;
            _stateTimer += dt;
            _enemyTimer += dt;

            if (CurrentRole == Role.Client && Client != null)
            {
                if (_stateTimer >= 1f / Math.Max(1, Cfg.StateRateHz))
                {
                    _stateTimer = 0f;
                    Client.SendPlayerState();
                }
                if (SceneManager.GetActiveScene().name == "Scene_Ingame")
                {
                    Client.EnsureHostGhost();
                    Client.UpdateSpectateCamera();
                }
            }
            else if (CurrentRole == Role.Host && Host != null)
            {
                Host.Tick(dt);
                if (_enemyTimer >= 1f / Math.Max(1, Cfg.EnemyRateHz))
                {
                    _enemyTimer = 0f;
                    Host.SendEnemyStates();
                }
                if (_stateTimer >= 0.2f) { _stateTimer = 0f; Host.SendGlobal(); }
                _hostStateTimer += dt;
                if (_hostStateTimer >= 1f / Math.Max(1, Cfg.StateRateHz))
                {
                    _hostStateTimer = 0f;
                    Host.SendHostState();
                }
            }
        }

        private static float _fireTimer;

        private static int _botDebug;

        private static void AutoCombat()
        {
            _fireTimer += Time.unscaledDeltaTime;
            if (_fireTimer < 0.6f) return;
            _fireTimer = 0f;

            var p = Player.instance;
            if (_botDebug < 3)
            {
                _botDebug++;
                Plugin.Flog($"[bot] p={p != null} alive={p != null && p.isAlive} controllable={p != null && p.isControllable} movable={p != null && p.isMovable} enemies={Character.Characters.Count}");
            }
            if (p == null || !p.isAlive || !p.isControllable || p.isShowdownPreparing || p.isShowdownShooting) return;

            // keep the bot's cylinder fed
            if (p.playerRevolver != null && p.playerRevolver.currentAmmo <= 0)
            {
                try { p.TryReload(); } catch { }
                return;
            }

            Enemy best = null;
            float bestD = float.MaxValue;
            var list = Character.Characters;
            for (int i = list.Count - 1; i >= 0; i--)
            {
                if (!(list[i] is Enemy e) || e == null || e.IsDead()) continue;
                float d = V3Ext.GetXZDistance(e.characterPivot.position, p.characterPivot.position);
                if (d < bestD) { bestD = d; best = e; }
            }
            if (best == null) return;

            Vector3 dir = (best.GetBodyCenter() - p.bulletSpawnPoint.position).WithY(0f);
            if (dir == Vector3.zero) dir = Vector3.forward;
            p.playerAimVector = dir;
            try
            {
                p.TryFire();
                if (_botDebug < 6)
                {
                    _botDebug++;
                    Plugin.Flog($"[bot] fired at {best.enemyCharacterKey?.Id} dist={bestD:F1}");
                }
            }
            catch (Exception ex)
            {
                if (_botDebug < 6) { _botDebug++; Plugin.Flog("[bot] fire error: " + ex); }
            }
        }

        // ---------------- auto-test driver (see playbook: automated-qa) ----------------

        private void RunAutoTest()
        {
            if (!Plugin.AutoHost && !Plugin.AutoClient && !Plugin.AutoSteamHost && !Plugin.AutoSteamClient) return;
            float t = Time.unscaledTime;

            if (Active && SceneManager.GetActiveScene().name == "Scene_Ingame")
            {
                if (Player.instance != null)
                    Player.instance.isGodMode = true; // bots must survive the automated test
                // vanilla edge: killing a deathPrefab boss leaves the camera on a
                // destroyed transform until EndCutScene fires — heal it
                var cm = CameraManager.instance;
                if (cm != null && cm.camTarget == null && Player.instance != null)
                    cm.ChangeCamTarget(Player.instance.transform);
                AutoCombat();
            }

            if (!Plugin.AutoStarted && t - Plugin.AutoBootTime > 4f)
            {
                Plugin.AutoStarted = true;
                if (Plugin.AutoSteamHost) { Plugin.Flog("[auto] steam hosting"); StartHostSteam(); }
                else if (Plugin.AutoSteamClient) { Plugin.Flog("[auto] steam joining"); StartClientSteam(); }
                else if (Plugin.AutoHost) { Plugin.Flog("[auto] hosting"); StartHost(); }
                else { Plugin.Flog("[auto] joining " + Cfg.HostIp); StartClient(); }
            }
            // steam client retries lobby search until found
            if (Plugin.AutoSteamClient && Net == null && t - _steamSearchTime > 6f)
            {
                _steamSearchTime = t;
                StartClientSteam();
            }
            if ((Plugin.AutoHost || Plugin.AutoSteamHost) && Active && !Plugin.AutoRunStarted)
            {
                if (Plugin.AutoConnectTime < 0f) Plugin.AutoConnectTime = t;
                // never start the run before the client actually handshaked
                if (!IsHost || !(Host?.HasClient ?? false)) return;
                if (t - Plugin.AutoConnectTime > 6f && SceneManager.GetActiveScene().name == "Scene_Main")
                {
                    Plugin.AutoRunStarted = true;
                    Plugin.Flog("[auto] host starts run -> Scene_Ingame");
                    SceneTransitionManager.instance?.LoadIngameScene();
                }
            }
            Plugin.StatusTimer += Time.unscaledDeltaTime;
            if (Plugin.StatusTimer > 5f)
            {
                Plugin.StatusTimer = 0f;
                if (!Active) { Plugin.Flog($"[status] scene={SceneManager.GetActiveScene().name} NOT connected role={CurrentRole}"); return; }
                if (IsHost)
                    Plugin.Flog($"[status] scene={SceneManager.GetActiveScene().name} enemies={Host?.TrackedCount ?? 0} sharedKills={Host?.SharedKillCount ?? 0} clientAlive={Host?.ClientAlive}");
                else
                    Plugin.Flog($"[status] scene={SceneManager.GetActiveScene().name} proxies={Client?.ProxyCount ?? 0} deadProxies={Client?.DeadProxyCount ?? 0} alive={Player.instance != null && Player.instance.isAlive}");
            }
        }
    }
}
