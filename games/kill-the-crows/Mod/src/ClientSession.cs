using System;
using System.Collections.Generic;
using KtcCoop.Net;
using UnityEngine;
using UnityEngine.AI;
using UnityEngine.SceneManagement;
using Visual;
using Object = UnityEngine.Object;

namespace KtcCoop
{
    /// <summary>Client side: local player sim, enemy proxies, hit requests to host.</summary>
    public class ClientSession
    {
        private struct ProxyRec
        {
            public GameObject Go;
            public Enemy Enemy;
            public Animator Anim;
            public SpriteController Sprite;
            public ushort LastHash;
        }

        private readonly Dictionary<ushort, ProxyRec> _proxies = new Dictionary<ushort, ProxyRec>();
        private readonly HashSet<ushort> _deadProxies = new HashSet<ushort>();

        public bool AllowSceneLoad;
        public bool RunOver;
        public GhostView HostGhostView;
        public bool SyncedPause;
        public int SharedKillCount { get; private set; }
        public int ProxyCount => _proxies.Count;
        public int DeadProxyCount => _deadProxies.Count;
        private bool _ownDeathReported;
        private float _lastSentSdScale = 1f;
        private int _hostSdOwner; // 0 = client follows host, 1 = client owns (in showdown)

        private static readonly System.Reflection.FieldInfo FiEnemyAnim =
            HarmonyLib.AccessTools.Field(typeof(Enemy), "animator");
        private static readonly System.Reflection.FieldInfo FiEnemyBehaviour =
            HarmonyLib.AccessTools.Field(typeof(Enemy), "behaviourData");
        private static readonly System.Reflection.FieldInfo FiPlayerResultPanel =
            HarmonyLib.AccessTools.Field(typeof(Player), "resultPanel");

        // ---------------- handshake ----------------

        public void SendHello()
        {
            Coop.Net.SendReliable(Op.Hello, Protocol.Hello(
                Environment.UserName,
                PrefManager.GearRevolver,
                PrefManager.GearShowdown,
                PrefManager.GearPassive1,
                PrefManager.GearPassive2,
                PrefManager.ProgressCurrentDifficultyLevel));
        }

        public void OnWelcome(byte[] data)
        {
            var w = Protocol.ReadWelcome(data);
            int diff = w.Diff;
            Debug.Log($"[KTC-Coop] Connected! Host difficulty level: {diff}. Pick your loadout — host starts the run.");
        }

        public void OnMods(byte[] data)
        {
            byte[] mask = Protocol.ReadMods(data);
            var dict = ModifierManager.instance?.modifierDictionary;
            if (dict == null) return;
            // the host's modifier set governs the shared run simulation
            for (int i = 0; i < dict.modifierActivateDatas.Count; i++)
            {
                var v = dict.modifierActivateDatas[i];
                bool on = i / 8 < mask.Length && (mask[i / 8] & (1 << (i % 8))) != 0;
                v.isActivated = on;
                dict.modifierActivateDatas[i] = v;
            }
        }

        // ---------------- outgoing ----------------

        public void SendPlayerState()
        {
            var p = Player.instance;
            if (p == null || TimeManager.instance == null) return;

            ushort hash = 0;
            float norm = 0f;
            if (p.animator != null && p.animator.isActiveAndEnabled)
            {
                var st = p.animator.GetCurrentAnimatorStateInfo(0);
                hash = (ushort)(st.shortNameHash & 0xFFFF);
                norm = st.normalizedTime;
            }
            float yaw = p.rotationPivot != null ? p.rotationPivot.eulerAngles.y : 0f;
            var ownSprite = p.GetComponentInChildren<SpriteController>(true);

            Coop.Net.SendUnreliable(Op.PlayerState, Protocol.PlayerState(
                p.transform.position, yaw, p.playerAimVector,
                ownSprite != null && ownSprite.spriteFlip,
                p.isDodging, p.isInvincibleDodge, p.isMoving, p.isAlive,
                TimeManager.instance.showdownTimeScale, hash, norm));
        }

        public void SendHitRequest(Enemy proxy, int damage, float sdGain, Vector3 hitPos, Vector3 dir, BulletInfo info)
        {
            ushort id = IdOf(proxy);
            if (id == 0) return;
            Coop.Net.SendReliable(Op.HitReq, Protocol.HitReq(id, damage, sdGain, hitPos, dir, info));
        }

        public ushort IdOf(Enemy e)
        {
            foreach (var kv in _proxies)
                if (kv.Value.Enemy == e) return kv.Key;
            return 0;
        }

        public bool IsProxy(Enemy e) => e != null && IdOf(e) != 0;

        public void SendClientDied()
        {
            if (_ownDeathReported) return;
            _ownDeathReported = true;
            Coop.Net.SendReliable(Op.ClientDied, new byte[0]);
        }

        // ---------------- incoming ----------------

        public void OnSpawn(byte[] data)
        {
            Protocol.ReadSpawn(data, out ushort id, out int dictIdx, out var pos);
            if (_proxies.ContainsKey(id)) return;

            var dict = WaveManager.instance?.enemyCharacterDictionary?.dictionary;
            if (dict == null || dictIdx < 0 || dictIdx >= dict.Count) return;
            var prefab = dict[dictIdx].prefab;
            if (prefab == null) return;

            var go = Object.Instantiate(prefab, pos, Quaternion.identity, RuntimeObjectParent.instance.characterParent);
            go.name = $"Proxy_{id}_{prefab.name}";

            var rec = new ProxyRec { Go = go };
            foreach (var mb in go.GetComponentsInChildren<MonoBehaviour>(true))
            {
                if (mb is Hurtbox) continue;
                mb.enabled = false;
            }
            foreach (var a in go.GetComponentsInChildren<AudioSource>(true))
                a.enabled = false;
            var agent = go.GetComponent<NavMeshAgent>();
            if (agent != null) agent.enabled = false;

            rec.Enemy = go.GetComponent<Enemy>();
            rec.Anim = FiEnemyAnim?.GetValue(rec.Enemy) as Animator;
            if (rec.Anim == null) rec.Anim = go.GetComponentInChildren<Animator>(true);
            rec.Sprite = go.GetComponentInChildren<SpriteController>(true);
            // disabling the Enemy component ran OnDisable and dropped it from
            // Character.Characters — re-register so aiming/showdown targeting sees it
            if (rec.Enemy != null && !Character.Characters.Contains(rec.Enemy))
                Character.Characters.Add(rec.Enemy);
            _proxies[id] = rec;
        }

        public void OnDeath(byte[] data)
        {
            Protocol.ReadDeath(data, out ushort id, out _, out bool show, out ushort deathHash, out float deathNorm);
            if (!_proxies.TryGetValue(id, out var rec)) return;
            _proxies.Remove(id);
            _deadProxies.Add(id);

            if (rec.Go == null) return;
            var enemy = rec.Enemy;
            if (enemy != null)
            {
                var hb = enemy.GetComponentInChildren<Hurtbox>();
                if (hb != null) hb.enabled = false;
                foreach (var col in enemy.GetComponentsInChildren<Collider>(true))
                    col.enabled = false;
            }

            if (show && rec.Anim != null && deathHash != 0)
            {
                rec.Anim.enabled = true;
                rec.Anim.Play(deathHash, 0, 0f);
                Object.Destroy(rec.Go, 9f);
            }
            else
            {
                Object.Destroy(rec.Go);
            }
        }

        public void OnEnemyStates(byte[] data)
        {
            var arr = Protocol.ReadEnemyStates(data);
            foreach (var s in arr)
            {
                if (!_proxies.TryGetValue(s.Id, out var rec) || rec.Enemy == null) continue;
                var e = rec.Enemy;
                e.characterPivot.position = s.Pos;
                if (e.rotationPivot != null)
                    e.rotationPivot.rotation = Quaternion.Euler(0f, s.Yaw, 0f);
                e.faceRight = !s.Flip;
                rec.Sprite?.SetFlip(s.Flip);
                if (rec.Anim != null && s.Hash != 0)
                {
                    rec.Anim.enabled = true;
                    rec.Anim.speed = TimeManager.instance != null ? TimeManager.instance.showdownTimeScale : 1f;
                    if (s.Hash != rec.LastHash)
                    {
                        rec.Anim.Play(s.Hash, 0, s.Norm);
                        rec.LastHash = s.Hash;
                    }
                }
            }
        }

        public void OnEnemyFire(byte[] data)
        {
            Protocol.ReadEnemyFire(data, out var pos, out var dir, out var info, out ushort shooterId);
            // cosmetic bullet — damage decisions all happen on the host
            GameObject prefab = null;
            if (_proxies.TryGetValue(shooterId, out var rec) && rec.Enemy != null)
            {
                var data2 = FiEnemyBehaviour?.GetValue(rec.Enemy);
                if (data2 != null)
                {
                    var fi = HarmonyLib.AccessTools.Field(data2.GetType(), "bulletPF");
                    prefab = fi?.GetValue(data2) as GameObject;
                }
            }
            if (prefab == null && Player.instance != null)
                prefab = Player.instance.GetComponent<PlayerRevolver>()?.bulletPrefab;
            if (prefab == null || RuntimeObjectParent.instance == null) return;

            var go = Object.Instantiate(prefab, pos, Quaternion.LookRotation(dir.normalized), RuntimeObjectParent.instance.bulletParent);
            var b = go.GetComponent<Bullet>();
            b.masterCharacterCategory = CharacterCategory.Enemy;
            b.master = rec.Enemy; // reference to a proxy so OnTriggerEnter's master reads are safe
            b.SetBulletInfo(info, CharacterCategory.Enemy, DeathByCharacterKey.Gunman, rec.Enemy);
            b.Shoot(dir.normalized);
        }

        public void OnDamagePlayer(byte[] data)
        {
            Protocol.ReadDamagePlayer(data, out int dmg, out var dir, out var info);
            var p = Player.instance;
            if (p == null || !p.isAlive) return;

            var fake = MakeFakeBullet(dir, info);
            if (p.isInvincible)
            {
                p.OnDodgeSuccess();
                TaskManager.instance?.OnDodgeSuccessEvent(fake);
                Object.Destroy(fake.gameObject, 0.5f);
                return;
            }
            // run the real OnHurt (bypasses our client-skip patch).
            // if this kills us, the Player.OnDeath patch redirects to ReplicateDeath.
            PatchesState.PendingDeathBullet = fake;
            PatchesState.BypassPlayerHurt = true;
            try { p.OnHurt(fake); }
            finally { PatchesState.BypassPlayerHurt = false; }
            Object.Destroy(fake.gameObject, 0.5f);
        }

        /// <summary>Manual replica of Player.OnDeath without the result panel (GameOver packet shows it later).</summary>
        public void ReplicateDeath(Bullet deathblow)
        {
            var p = Player.instance;
            if (p == null || !p.isAlive) return;
            SendClientDied();

            SFXManager.instance?.PlayOneshot(p.playerData.hurtVoiceKey);
            SFXManager.instance?.PlayOneshot(p.playerData.deadSfxKey);
            Time.timeScale = 1f;
            p.isAlive = false;
            p.isMovable = false;
            p.isControllable = false;
            p.StopAllCoroutines();
            p.playerMovement.StopAllCoroutines();
            p.playerRevolver.StopAllCoroutines();
            p.playerRevolver.ExitReloadState();
            p.playerShowdown.CancelShowdown();
            foreach (var col in p.GetComponentsInChildren<Collider>())
                col.enabled = false;
            if (p.animator != null)
            {
                p.animator.StopPlayback();
                p.animator.Play("PixelAnim_Player_Dead");
            }
            var vfxHit = HarmonyLib.AccessTools.Field(typeof(Player), "vfxHit")?.GetValue(p) as ParticleSystem;
            vfxHit?.Emit(1);
            TimeManager.instance?.DeathSlow();
            p.gunPivot?.gameObject.SetActive(false);
            p.gunPivot2?.gameObject.SetActive(false);
            if (CameraManager.instance != null) CameraManager.instance.isFollowingTarget = false;
            if (IngameUIManager.instance != null) IngameUIManager.instance.gameObject.SetActive(false);
        }

        public void OnShowdownGain(byte[] data)
        {
            using var r = new Protocol.Reader(data);
            float amount = r.F();
            Player.instance?.playerShowdown?.IncreaseShowdownResource(amount);
        }

        public void OnGlobal(byte[] data)
        {
            Protocol.ReadGlobal(data, out int killCount, out int chunkLevel, out int phase,
                out bool spawnDisable, out float sdScale, out float gameTime, out bool paused);

            var wm = WaveManager.instance;
            var tm = TimeManager.instance;
            SharedKillCount = killCount;
            var ui = IngameUIManager.instance;
            if (ui != null) ui.killCountText.text = killCount.ToString();

            if (wm != null)
            {
                wm.chunkWaveIntensityLevel = chunkLevel;
                if ((int)wm.currentPhase != phase)
                {
                    wm.currentPhase = (IngamePhase)phase;
                    BGMManager.instance?.OnReceiveIngamePhaseChangeSignal((IngamePhase)phase);
                }
            }
            if (tm != null)
            {
                if (paused != tm.isPaused)
                {
                    SyncedPause = true;
                    try { tm.Pause(paused); } finally { SyncedPause = false; }
                }
                // host's showdown time scale wins unless we are the ones in showdown
                bool ownShowdown = Player.instance != null &&
                    (Player.instance.isShowdownPreparing || Player.instance.isShowdownShooting);
                if (!ownShowdown && Mathf.Abs(tm.showdownTimeScale - sdScale) > 0.001f)
                    tm.SetShowdownTimeScale(sdScale);
            }
        }

        public void OnScene(byte[] data)
        {
            using var r = new Protocol.Reader(data);
            string sceneName = r.S();
            Debug.Log($"[KTC-Coop] Host transitioned to scene: {sceneName}");
            AllowSceneLoad = true;
            try
            {
                var stm = SceneTransitionManager.instance;
                if (stm == null) { SceneManager.LoadScene(sceneName); return; }
                switch (sceneName)
                {
                    case "Scene_Ingame": stm.LoadIngameScene(); break;
                    case "Scene_Main": stm.LoadMainScene(); break;
                    case "Scene_Loadout": stm.LoadLoadoutScene(); break;
                    case "Scene_Intro": stm.LoadIntroScene(); break;
                    case "Scene_Ending": stm.LoadEndingScene(); break;
                    default: SceneManager.LoadScene(sceneName); break;
                }
            }
            finally { AllowSceneLoad = false; }
        }

        public void OnGameOver(byte[] data)
        {
            var p = Player.instance;
            if (p == null) return;
            var rp = FiPlayerResultPanel?.GetValue(p) as ResultPanel;
            if (rp == null) return;
            CameraProfileChanger.instance?.DeathTransition();
            rp.Init(SharedKillCount, p.accuracy, (int)TimeManager.instance.gameTime, DeathByCharacterKey.Gunman);
            rp.gameObject.SetActive(true);
            p.isRestartAvailable = false;
        }

        private static Bullet MakeFakeBullet(Vector3 dir, BulletInfo info)
        {
            var go = new GameObject("coopFakeBullet");
            go.AddComponent<Rigidbody>();
            var b = go.AddComponent<Bullet>();
            b.info = info;
            b.direction = dir == Vector3.zero ? Vector3.back : dir.normalized;
            return b;
        }

        // ---------------- host ghost (we must SEE the host player) ----------------

        public void EnsureHostGhost()
        {
            if (HostGhostView != null || Player.instance == null) return;
            if (SceneManager.GetActiveScene().name != "Scene_Ingame") return;
            HostGhostView = GhostView.Create(Player.instance, "CoopHostGhost");
            Plugin.Flog("[client] host ghost spawned");
        }

        public void OnHostState(byte[] data)
        {
            EnsureHostGhost();
            if (HostGhostView == null) return;
            Protocol.ReadPlayerState(data, out var pos, out float yaw, out var aim, out bool flip,
                out bool dodging, out bool invDodge, out bool moving, out bool alive,
                out float sdScale, out ushort hash, out float norm);
            HostGhostView.Apply(pos, yaw, aim, flip, dodging, invDodge, moving, alive, sdScale, hash, norm);
        }

        public void OnHostFire(byte[] data)
        {
            Protocol.ReadFire(data, out var pos, out var dir, out var info, out _);
            EnsureHostGhost();
            GameObject prefab = HostGhostView?.Player != null
                ? HostGhostView.Player.GetComponent<PlayerRevolver>()?.bulletPrefab : null;
            if (prefab == null && Player.instance != null)
                prefab = Player.instance.GetComponent<PlayerRevolver>()?.bulletPrefab;
            if (prefab == null || RuntimeObjectParent.instance == null) return;
            var go = Object.Instantiate(prefab, pos, Quaternion.LookRotation(dir.normalized), RuntimeObjectParent.instance.bulletParent);
            var b = go.GetComponent<Bullet>();
            b.masterCharacterCategory = CharacterCategory.Player;
            b.master = HostGhostView?.Player;
            b.SetBulletInfo(info, CharacterCategory.Player, DeathByCharacterKey.Player, HostGhostView?.Player);
            b.Shoot(dir.normalized);
        }

        public void OnHostGunAnim(byte[] data)
        {
            using var r = new Protocol.Reader(data);
            if (r.B() != 2) return;
            var p = HostGhostView?.Player;
            if (p == null) return;
            p.gunPivot?.GetComponent<Animator>()?.SetTrigger("Reload");
            p.gunPivot2?.GetComponent<Animator>()?.SetTrigger("Reload");
        }

        /// <summary>Spectate the surviving partner after our own death.</summary>
        public void UpdateSpectateCamera()
        {
            var p = Player.instance;
            if (p == null || p.isAlive) return;
            var gp = HostGhostView?.Player;
            if (gp == null || !gp.isAlive) return;
            var cm = CameraManager.instance;
            if (cm != null && !cm.isFollowingTarget)
            {
                cm.isFollowingTarget = true;
                cm.ChangeCamTarget(gp.transform);
            }
        }

        // ---------------- local death ----------------
    }
}
