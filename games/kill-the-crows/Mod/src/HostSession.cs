using System;
using System.Collections.Generic;
using System.Linq;
using KtcCoop.Net;
using UnityEngine;
using UnityEngine.AI;
using UnityEngine.SceneManagement;
using Visual;
using Object = UnityEngine.Object;

namespace KtcCoop
{
    /// <summary>Host side: owns world simulation, streams enemies, routes damage to the client.</summary>
    public class HostSession
    {
        public int SharedKillCount { get; private set; }
        public void AddSharedKill() => SharedKillCount++;

        private ushort _nextId = 1;
        private readonly Dictionary<Enemy, ushort> _ids = new Dictionary<Enemy, ushort>();
        private readonly Dictionary<ushort, Enemy> _byId = new Dictionary<ushort, Enemy>();
        private readonly HashSet<Enemy> _deathSent = new HashSet<Enemy>();

        // ghost = host's replica of the client's player
        private GhostView _ghostView;
        private float _clientSdScale = 1f;
        private int _sdOwner; // 0 = host, 1 = client

        private bool _clientAlive = true;

        private Protocol.HelloRec _loadout;

        public bool ClientAlive => _clientAlive;
        public bool HasClient { get; private set; }
        public int RevivesLeft { get; private set; }
        public float ClientInvulnUntil => _clientInvulnUntil;

        private float _reviveClientAt = -1f;
        private float _reviveHostAt = -1f;
        private float _clientInvulnUntil = -1f;
        private bool _finalized;
        public Player Ghost => _ghostView?.Player;
        public int TrackedCount => _byId.Count;

        private static readonly System.Reflection.FieldInfo FiEnemyPlayer =
            HarmonyLib.AccessTools.Field(typeof(Enemy), "player");
        private static readonly System.Reflection.FieldInfo FiDeathblowDir =
            HarmonyLib.AccessTools.Field(typeof(Enemy), "deathblowDirection");
        private static readonly System.Reflection.FieldInfo FiDeathPrefab =
            HarmonyLib.AccessTools.Field(typeof(Enemy), "deathPrefab");
        private static readonly System.Reflection.FieldInfo FiSprite =
            HarmonyLib.AccessTools.Field(typeof(Character), "mainSpriteController");
        private static readonly System.Reflection.FieldInfo FiEnemyAnim =
            HarmonyLib.AccessTools.Field(typeof(Enemy), "animator");

        internal static Animator AnimOf(Enemy e) => FiEnemyAnim?.GetValue(e) as Animator;

        // ---------------- handshake ----------------

        public void OnHello(byte[] data)
        {
            _loadout = Protocol.ReadHello(data);
            HasClient = true;
            Debug.Log($"[KTC-Coop] Client '{_loadout.Name}' connected (revolver={_loadout.Rev}).");
            var mods = ModifierMask();
            Coop.Net.SendReliable(Op.Welcome, Protocol.Welcome(PrefManager.ProgressCurrentDifficultyLevel, mods));
            Coop.Net.SendReliable(Op.Mods, Protocol.Mods(mods));
        }

        public static byte[] ModifierMask()
        {
            var mm = ModifierManager.instance;
            var dict = mm?.modifierDictionary;
            int n = dict != null ? dict.modifierActivateDatas.Count : 0;
            var mask = new byte[Mathf.Max(1, (n + 7) / 8)];
            for (int i = 0; i < n; i++)
                if (dict.modifierActivateDatas[i].isActivated)
                    mask[i / 8] |= (byte)(1 << (i % 8));
            return mask;
        }

        // ---------------- enemy tracking (called from patches) ----------------

        public void RegisterEnemy(Enemy e)
        {
            if (e == null || _ids.ContainsKey(e)) return;
            ushort id = _nextId++;
            _ids[e] = id;
            _byId[id] = e;

            int dictIdx = -1;
            var wm = WaveManager.instance;
            if (wm != null)
            {
                var dict = wm.enemyCharacterDictionary.dictionary;
                dictIdx = dict.FindIndex(x => x.key == e.enemyCharacterKey);
            }
            if (dictIdx >= 0)
                Coop.Net.SendReliable(Op.Spawn, Protocol.Spawn(id, dictIdx, e.characterPivot.position));
        }

        public void OnEnemyDeath(Enemy e)
        {
            if (e == null || !_deathSent.Add(e)) return;
            if (!_ids.TryGetValue(e, out ushort id)) return;
            ushort hash = 0;
            float norm = 0f;
            bool corpsePrefab = FiDeathPrefab != null && FiDeathPrefab.GetValue(e) != null;
            var eAnim = AnimOf(e);
            if (!corpsePrefab && eAnim != null && eAnim.isActiveAndEnabled)
            {
                var st = eAnim.GetCurrentAnimatorStateInfo(0);
                hash = (ushort)(st.shortNameHash & 0xFFFF);
                norm = st.normalizedTime;
            }
            _ids.Remove(e);
            _byId.Remove(id);
            Coop.Net.SendReliable(Op.Death, Protocol.Death(id, e.characterPivot.forward, !corpsePrefab, hash, norm));
        }

        // ---------------- client events ----------------

        public void OnPlayerState(byte[] data)
        {
            Protocol.ReadPlayerState(data, out var pos, out float yaw, out var aim, out bool flip,
                out bool dodging, out bool invDodge, out bool moving, out bool alive,
                out float sdScale, out ushort animHash, out float animNorm);

            bool wasAlive = _clientAlive;
            _clientAlive = alive;
            // UDP reordering can deliver a stale alive=true after the death packet —
            // ignore resurrections that we did not send (pending revive owns that)
            if (alive && _reviveClientAt > 0f) return;
            if (wasAlive && !alive) OnClientDied(); // every death, not just the first

            if (_ghostView == null) return;
            _ghostView.Apply(pos, yaw, aim, flip, dodging, invDodge, moving, alive, sdScale, animHash, animNorm);

            // showdown time-scale ownership
            if (sdScale < 1f)
            {
                TimeManager.instance.SetShowdownTimeScale(sdScale);
                _sdOwner = 1;
            }
            else if (_sdOwner == 1)
            {
                TimeManager.instance.SetShowdownTimeScale(1f);
                _sdOwner = 0;
            }
        }

        public void OnFire(byte[] data)
        {
            Protocol.ReadFire(data, out var pos, out var dir, out var info, out byte gunAnim);
            EnsureGhost();
            if (_ghostView?.Player == null) return;

            // rebuild the client's bullet on the host against real enemies
            var prefab = _ghostView.Player.GetComponent<PlayerRevolver>()?.bulletPrefab;
            if (prefab == null || RuntimeObjectParent.instance == null) return;
            var go = Object.Instantiate(prefab, pos, Quaternion.identity, RuntimeObjectParent.instance.bulletParent);
            var bullet = go.GetComponent<Bullet>();
            bullet.transform.rotation = Quaternion.LookRotation(dir.normalized);
            // client explosive revolvers must actually explode on the host
            var be = go.GetComponent<BulletExplosive>();
            if (be != null && _ghostView.Player.weaponData != null && _ghostView.Player.weaponData.isExplosiveBullet)
            {
                be.explosionRadius = _ghostView.Player.weaponData.explosionRadius;
                be.explosionLifetime = _ghostView.Player.weaponData.explosionLifetime;
            }
            bullet.masterCharacterCategory = CharacterCategory.Player;
            bullet.masterCharacterKey = DeathByCharacterKey.Player;
            bullet.master = _ghostView.Player;
            bullet.SetBulletInfo(info, CharacterCategory.Player, DeathByCharacterKey.Player, _ghostView.Player);
            bullet.Shoot(dir.normalized);

            if (gunAnim == 1 && _ghostView.Player.gunPivot != null)
                _ghostView.Player.gunPivot.GetComponent<Animator>()?.SetTrigger("Shoot");
        }

        public void OnGunAnim(byte[] data)
        {
            using var r = new Protocol.Reader(data);
            byte kind = r.B();
            EnsureGhost();
            if (_ghostView?.Player == null || kind != 2) return;
            _ghostView.Player.gunPivot?.GetComponent<Animator>()?.SetTrigger("Reload");
            _ghostView.Player.gunPivot2?.GetComponent<Animator>()?.SetTrigger("Reload");
        }

        public void OnHitReq(byte[] data)
        {
            Protocol.ReadHitReq(data, out ushort id, out int dmg, out float sdGain, out var hitPos, out var dir, out var info);
            if (!_byId.TryGetValue(id, out var enemy) || enemy == null || enemy.IsDead()) return;

            // emulate the visual part of Enemy.OnHurt
            if (enemy.bloodVfxPF != null)
            {
                var look = dir == Vector3.zero ? Vector3.forward : -dir.normalized;
                var vfx = Object.Instantiate(enemy.bloodVfxPF, hitPos, Quaternion.LookRotation(look), enemy.transform);
                Object.Destroy(vfx, vfx.GetComponent<ParticleSystem>().main.duration);
            }
            SFXManager.instance?.PlayOneshot(enemy.statData.hurtClipKey, hitPos);

            // resource belongs to the shooter (client), not the host player
            if (sdGain > 0f)
            {
                Player.instance.playerShowdown?.IncreaseShowdownResource(-sdGain);
                Coop.Net.SendUnreliable(Op.ShowdownGain, Protocol.ShowdownGain(sdGain));
            }

            enemy.hp -= dmg;
            if (enemy.hp <= 0)
            {
                FiDeathblowDir?.SetValue(enemy, dir == Vector3.zero ? Vector3.forward : dir.normalized);
                // fake bullet for OnDeath's info reads
                var go = new GameObject("coopFakeBullet");
                go.transform.position = hitPos;
                go.AddComponent<Rigidbody>();
                var fb = go.AddComponent<Bullet>();
                fb.info = info;
                enemy.OnDeath(fb);
                Object.Destroy(go, 0.5f);
            }
        }

        public void OnClientDied()
        {
            if (_reviveClientAt > 0f) return; // already down, revive pending
            _clientAlive = false;
            if (_ghostView?.Player != null) _ghostView.Player.isAlive = false;
            Debug.Log("[KTC-Coop] Client player died.");
            if (_reviveHostAt > 0f) _reviveHostAt = -1f; // host died mid-window
            if (Player.instance != null && Player.instance.isAlive && RevivesLeft > 0)
            {
                RevivesLeft--;
                _reviveClientAt = Time.unscaledTime + Coop.Cfg.ReviveDelaySec;
                Plugin.Flog("[revive] client down, reviving in " + Coop.Cfg.ReviveDelaySec + "s");
            }
            else
            {
                FinalizeRun();
            }
        }

        /// <summary>Team wipe: show the result panel on both sides and stop the run.</summary>
        public void FinalizeRun()
        {
            if (_finalized) return;
            _finalized = true;
            var wm = WaveManager.instance;
            if (wm != null) wm.spawnDisable = true;
            var p = Player.instance;
            if (p != null)
            {
                var rps = UnityEngine.Object.FindObjectsOfType<ResultPanel>(true);
                CameraProfileChanger.instance?.DeathTransition();
                if (rps.Length > 0)
                {
                    rps[0].Init(p.killCount, p.accuracy, (int)TimeManager.instance.gameTime, DeathByCharacterKey.Gunman);
                    rps[0].gameObject.SetActive(true);
                }
                p.isRestartAvailable = false;
            }
            Coop.Net.SendReliable(Op.GameOver, new byte[0]);
        }

        // ---------------- per-frame ----------------

        public void Tick(float dt)
        {
            EnsureGhost();
            RetargetEnemies();

            float now = Time.unscaledTime;
            if (_reviveClientAt > 0f && now >= _reviveClientAt)
            {
                _reviveClientAt = -1f;
                if (_ghostView?.Player != null) _ghostView.Player.isAlive = true;
                _clientInvulnUntil = now + Coop.Cfg.ReviveInvulnSec;
                Coop.Net.SendReliable(Op.Revive, new byte[0]);
                Plugin.Flog("[revive] client brought back (" + RevivesLeft + " left)");
            }
            if (_reviveHostAt > 0f && now >= _reviveHostAt)
            {
                _reviveHostAt = -1f;
                var gp = _ghostView?.Player;
                Revive.Perform(Player.instance, gp != null && gp.isAlive ? gp.transform.position : Player.instance.transform.position, Coop.Cfg.ReviveInvulnSec);
                Plugin.Flog("[revive] host brought back (" + RevivesLeft + " left)");
            }
        }

        /// <summary>Host player died: schedule a revive or end the run.</summary>
        public void OnHostDeath()
        {
            if (_reviveClientAt > 0f) _reviveClientAt = -1f; // partner died mid-window
            if (ClientAlive && RevivesLeft > 0)
            {
                RevivesLeft--;
                _reviveHostAt = Time.unscaledTime + Coop.Cfg.ReviveDelaySec;
                Plugin.Flog("[revive] host down, reviving in " + Coop.Cfg.ReviveDelaySec + "s");
            }
            else
            {
                FinalizeRun();
            }
        }

        private void EnsureGhost()
        {
            if (_ghostView != null) return;
            if (Player.instance == null) return;
            if (SceneManager.GetActiveScene().name != "Scene_Ingame") return;

            _ghostView = GhostView.Create(Player.instance, "CoopGhostPlayer");
            ApplyClientLoadout(_ghostView.Player);
            RevivesLeft = Coop.Cfg.RevivesPerRun;
            _finalized = false;
            Plugin.Flog("[host] ghost player spawned for client");
        }

        /// <summary>The ghost must behave like the CLIENT's gear (bullet prefab,
        /// explosion params, showdown resource gains, passives).</summary>
        private void ApplyClientLoadout(Player ghost)
        {
            try
            {
                var md = MasterDataDictionaryManager.instance?.masterDictionary;
                if (md == null || _loadout.Rev < 0) return;
                var revKey = md.loadoutDictionary.revolverLoadoutList[Mathf.Clamp(_loadout.Rev, 0, md.loadoutDictionary.revolverLoadoutList.Count - 1)];
                ghost.weaponData = md.revolverDictionary.GetPlayerRevolverData(revKey);
                var sdKey = md.loadoutDictionary.showdownLoadoutList[Mathf.Clamp(_loadout.Sd, 0, md.loadoutDictionary.showdownLoadoutList.Count - 1)];
                ghost.showdownData = md.showdownDictionary.GetPlayerShowdownData(sdKey);
                var pr = ghost.GetComponent<PlayerRevolver>();
                if (pr != null && ghost.weaponData.bulletPrefab != null)
                    pr.bulletPrefab = ghost.weaponData.bulletPrefab;
                var pp = ghost.playerPassive;
                if (pp != null)
                {
                    pp.playerEquipedPassives.Clear();
                    pp.playerEquipedPassives.Add(md.loadoutDictionary.passiveLoadoutList[Mathf.Clamp(_loadout.P1, 0, md.loadoutDictionary.passiveLoadoutList.Count - 1)]);
                    pp.playerEquipedPassives.Add(md.loadoutDictionary.passiveLoadoutList[Mathf.Clamp(_loadout.P2, 0, md.loadoutDictionary.passiveLoadoutList.Count - 1)]);
                }
                Plugin.Flog("[host] ghost loadout applied (revolver idx " + _loadout.Rev + ")");
            }
            catch (Exception e)
            {
                Plugin.Flog("[host] ghost loadout failed: " + e.Message);
            }
        }

        private void RetargetEnemies()
        {
            if (FiEnemyPlayer == null) return;
            Player local = Player.instance;
            Player target1 = local != null && local.isAlive ? local : null;
            Player target2 = _ghostView?.Player != null && _ghostView.Player.isAlive ? _ghostView.Player : null;
            if (target1 == null && target2 == null) return;

            // snapshot to avoid mutation during iteration
            var list = Character.Characters;
            for (int i = list.Count - 1; i >= 0; i--)
            {
                var c = list[i];
                if (!(c is Enemy e) || e.IsDead()) continue;

                Player best = null;
                float bestD = float.MaxValue;
                if (target1 != null)
                {
                    float d = V3Ext.GetXZDistance(e.characterPivot.position, target1.characterPivot.position);
                    if (d < bestD) { bestD = d; best = target1; }
                }
                if (target2 != null)
                {
                    float d = V3Ext.GetXZDistance(e.characterPivot.position, target2.characterPivot.position);
                    if (d < bestD) { bestD = d; best = target2; }
                }
                if (best != null) FiEnemyPlayer.SetValue(e, best);
            }
        }

        // ---------------- streams ----------------

        public void SendEnemyStates()
        {
            var arr = new List<Protocol.EnemyStateRec>();
            foreach (var kv in _byId)
            {
                var e = kv.Value;
                if (e == null || e.IsDead()) continue;
                ushort hash = 0;
                float norm = 0f;
                var eAnim = AnimOf(e);
                if (eAnim != null && eAnim.isActiveAndEnabled && eAnim.layerCount > 0)
                {
                    var st = eAnim.GetCurrentAnimatorStateInfo(0);
                    hash = (ushort)(st.shortNameHash & 0xFFFF);
                    norm = st.normalizedTime;
                }
                float agentSpeed = 0f;
                var agent = e.GetComponent<NavMeshAgent>();
                if (agent != null && agent.isActiveAndEnabled) agentSpeed = agent.speed;
                arr.Add(new Protocol.EnemyStateRec
                {
                    Id = kv.Key,
                    Pos = e.characterPivot.position,
                    Yaw = e.rotationPivot != null ? e.rotationPivot.eulerAngles.y : 0f,
                    Flip = FiSprite?.GetValue(e) is SpriteController sc && sc.spriteFlip,
                    Hp = e.hp,
                    Hash = hash,
                    Norm = norm,
                    AgentSpeed = agentSpeed
                });
            }
            if (arr.Count > 0)
                Coop.Net.SendUnreliable(Op.EnemyStates, Protocol.EnemyStates(arr.ToArray()));
        }

        public void SendHostState()
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
            var sprite = FiSprite?.GetValue(p) as SpriteController;
            Coop.Net.SendUnreliable(Op.HostState, Protocol.PlayerState(
                p.transform.position, yaw, p.playerAimVector,
                sprite != null && sprite.spriteFlip,
                p.isDodging, p.isInvincibleDodge, p.isMoving, p.isAlive,
                TimeManager.instance.showdownTimeScale, hash, norm));
        }

        public void SendGlobal()
        {
            var wm = WaveManager.instance;
            var tm = TimeManager.instance;
            if (wm == null || tm == null) return;
            Coop.Net.SendUnreliable(Op.Global, Protocol.Global(
                SharedKillCount, wm.chunkWaveIntensityLevel, (int)wm.currentPhase,
                wm.spawnDisable, tm.showdownTimeScale, tm.gameTime, tm.isPaused));
        }
    }
}
