using System;
using System.IO;
using BepInEx;
using BepInEx.Configuration;
using BepInEx.Logging;
using HarmonyLib;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace KtcCoop
{
    [BepInPlugin(PluginGuid, PluginName, PluginVersion)]
    public class Plugin : BaseUnityPlugin
    {
        public const string PluginGuid = "coop-anything.kill-the-crows";
        public const string PluginName = "Kill The Crows Co-op";
        public const string PluginVersion = "0.1.0";

        internal static ManualLogSource Log;

        // auto-test (command line: -ktcHost / -ktcClient)
        public static bool AutoHost;
        public static bool AutoClient;
        public static float AutoBootTime = -1f;
        public static float AutoConnectTime = -1f;
        public static bool AutoStarted;
        public static bool AutoRunStarted;
        public static float StatusTimer;

        private static string _logFile;
        private static ConfigEntry<string> _cfgHostIpFile;

        internal static void Flog(string msg)
        {
            if (_logFile == null) return;
            try { File.AppendAllText(_logFile, $"{DateTime.Now:HH:mm:ss.fff} {msg}\n"); } catch { }
        }

        private void Awake()
        {
            Log = Logger;

            _cfgHostIpFile = Config.Bind("Net", "HostIp", "127.0.0.1", "IP of the host to join as client");
            Coop.Cfg.HostIp = _cfgHostIpFile.Value;
            Coop.Cfg.StateRateHz = Config.Bind("Net", "PlayerStateRateHz", 30, "Player state packets per second").Value;
            Coop.Cfg.EnemyRateHz = Config.Bind("Net", "EnemyStateRateHz", 20, "Enemy state packets per second").Value;

            _logFile = Path.Combine(Path.GetDirectoryName(Application.dataPath) ?? ".", "ktc-mod.log");
            foreach (var a in Environment.GetCommandLineArgs())
            {
                if (a == "-ktcHost") AutoHost = true;
                if (a == "-ktcClient") AutoClient = true;
                if (a.StartsWith("-ktcLog=")) _logFile = a.Substring(8);
            }
            AutoBootTime = Time.unscaledTime;

            var h = new Harmony(PluginGuid);
            h.PatchAll(typeof(Patches));

            // the game kills DontDestroyOnLoad roots between scene loads —
            // recreate our pump object every time (session state is static)
            Coop.HookSceneLoads();
            Coop.EnsurePumpPublic();
            var mode = AutoHost ? "HOST" : AutoClient ? "CLIENT" : "manual";
            var msg = $"{PluginName} {PluginVersion} loaded (mode: {mode}). F9 = host, F10 = join, F11 = disconnect.";
            Logger.LogInfo(msg);
            Flog(msg);
        }
    }
}
