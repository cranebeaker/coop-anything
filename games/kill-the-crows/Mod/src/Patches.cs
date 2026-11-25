using System;
using HarmonyLib;
using KtcCoop.Net;
using UnityEngine;
using UnityEngine.SceneManagement;
using Object = UnityEngine.Object;

namespace KtcCoop
{
    public static class PatchesState
    {
        public static bool BypassPlayerHurt;
        public static Bullet PendingDeathBullet;
    }

    [HarmonyPatch]
    public static class Patches
    {
        private static bool Ingame => SceneManager.GetActiveScene().name == "Scene_Ingame";
        private static Player Ghost => Coop.Host?.Ghost;

        // ---------------- client bullet spawn capture ----------------

        [HarmonyPostfix]
        [HarmonyPatch(typeof(PlayerAttack), "ShootBullet")]
        private static void ShootBullet_Postfix(Bullet bullet, Vector3 direction)
        {
            if (!Coop.Active || !Ingame || bullet == null) return;
            if (Coop.IsClient)
            {
                // gameplay: the host rebuilds this bullet against real enemies
                Coop.Net.SendReliable(Op.Fire, Protocol.Fire(bullet.transform.position, direction, bullet.info, 1));
            }
            else if (Coop.IsHost)
            {
                // cosmetics: the client renders a tracer for the host's shot
                Coop.Net.SendUnreliable(Op.HostFire, Protocol.Fire(bullet.transform.position, direction, bullet.info, 1));
            }
        }

        [HarmonyPostfix]
        [HarmonyPatch(typeof(PlayerRevolver), nameof(PlayerRevolver.TryReload))]
        private static void TryReload_Postfix()
        {
            if (!Coop.Active || !Ingame) return;
            Coop.Net.SendReliable(Op.GunAnim, Protocol.GunAnim(2)); // both roles: ghost gun anims
        }

        // ---------------- player damage routing ----------------

        [HarmonyPrefix]
        [HarmonyPatch(typeof(Player), nameof(Player.OnHurt))]
        private static bool Player_OnHurt_Prefix(Player __instance, Bullet bullet)
        {
            if (PatchesState.BypassPlayerHurt) return true;

            // client never applies damage locally — host decides
            if (Coop.IsClient) return false;

            // host own player inside the revive-invulnerability window
            if (Coop.IsHost && __instance == Player.instance && Revive.IsInvulnerable(__instance))
                return false;

            // host: ghost was hit by an enemy bullet -> route damage to the client
            if (Coop.IsHost && __instance == Ghost && bullet != null)
            {
                if (Time.unscaledTime < Coop.Host.ClientInvulnUntil)
                    return false;
                var dir = bullet.direction == Vector3.zero ? Vector3.back : bullet.direction.normalized;
                Coop.Net.SendReliable(Op.DamagePlayer, Protocol.DamagePlayer(bullet.info.damage, dir, bullet.info));
                // blood feedback for the host viewer
                var vfx = __instance.bloodVfxPF;
                if (vfx != null)
                {
                    var g = Object.Instantiate(vfx, __instance.GetBodyCenter(), Quaternion.LookRotation(-dir), RuntimeObjectParent.instance.vfxParent);
                    Object.Destroy(g, g.GetComponent<ParticleSystem>().main.duration);
                }
                return false;
            }
            return true;
        }

        [HarmonyPrefix]
        [HarmonyPatch(typeof(Player), nameof(Player.OnDeath))]
        private static bool Player_OnDeath_Prefix(Player __instance, Bullet deathblowBullet)
        {
            if (Coop.IsClient) { Coop.Client.ReplicateDeath(deathblowBullet); return false; }
            if (Coop.IsHost && __instance == Ghost) return false; // ghost never dies for real
            return true;
        }

        [HarmonyPostfix]
        [HarmonyPatch(typeof(Player), nameof(Player.OnDeath))]
        private static void Player_OnDeath_Postfix(Player __instance)
        {
            if (!Coop.IsHost || !Coop.Active || __instance != Player.instance) return;
            if (Coop.Host.ClientAlive)
            {
                // partner still alive: hide the panel — a revive may bring us back
                var rp = Object.FindObjectsOfType<ResultPanel>(true);
                if (rp.Length > 0) rp[0].gameObject.SetActive(false);
                __instance.isRestartAvailable = false;
                Coop.Host.OnHostDeath();
            }
            else
            {
                Coop.Host.OnHostDeath(); // schedules revive only if credits remain
            }
        }

        // host kill-count UI shows the shared counter
        [HarmonyPostfix]
        [HarmonyPatch(typeof(Player), nameof(Player.IncreaseKillCount))]
        private static void IncreaseKillCount_Postfix()
        {
            if (!Coop.IsHost) return;
            var ui = IngameUIManager.instance;
            if (ui != null) ui.killCountText.text = Coop.Host.SharedKillCount.ToString();
        }

        // ---------------- enemy damage ----------------

        [HarmonyPrefix]
        [HarmonyPatch(typeof(Enemy), nameof(Enemy.OnHurt))]
        private static bool Enemy_OnHurt_Prefix(Enemy __instance, Bullet bullet)
        {
            // client: proxies never take local damage
            if (Coop.IsClient) return false;
            return true;
        }

        [HarmonyPostfix]
        [HarmonyPatch(typeof(Enemy), nameof(Enemy.OnHurt))]
        private static void Enemy_OnHurt_Postfix(Enemy __instance, Bullet bullet)
        {
            if (!Coop.IsHost || bullet == null) return;
            // resource belongs to the shooter — if the ghost fired it, give it to the client
            if (bullet.master == Ghost && bullet.info.showdownResourceGainAmount > 0f)
            {
                Player.instance.playerShowdown?.IncreaseShowdownResource(-bullet.info.showdownResourceGainAmount);
                Coop.Net.SendUnreliable(Op.ShowdownGain, Protocol.ShowdownGain(bullet.info.showdownResourceGainAmount));
            }
        }

        [HarmonyPostfix]
        [HarmonyPatch(typeof(Enemy), nameof(Enemy.OnDeath))]
        private static void Enemy_OnDeath_Postfix(Enemy __instance)
        {
            if (Coop.IsHost) Coop.Host?.OnEnemyDeath(__instance);
        }

        [HarmonyPostfix]
        [HarmonyPatch(typeof(Enemy), "Start")]
        private static void Enemy_Start_Postfix(Enemy __instance)
        {
            if (Coop.IsHost) Coop.Host?.RegisterEnemy(__instance);
        }

        // client showdown/tackle melee hits -> damage requests
        [HarmonyPrefix]
        [HarmonyPatch(typeof(Player), nameof(Player.OnHit))]
        private static bool Player_OnHit_Prefix(Player __instance, BulletOnHit bullet, Character victim)
        {
            if (!Coop.IsClient || !Coop.Active || !Ingame || victim == null || bullet == null) return true;
            if (victim is Enemy proxy && Coop.Client.IsProxy(proxy))
            {
                var dir = Vector3.zero;
                var hitPos = victim.GetBodyCenter();
                Coop.Client.SendHitRequest(proxy, Mathf.Max(1, bullet.info.damage), 0f, hitPos, dir, bullet.info);
            }
            return true;
        }

        // client-side: all hurtbox collisions are decided by the host
        [HarmonyPrefix]
        [HarmonyPatch(typeof(Bullet), nameof(Bullet.OnTriggerEnter))]
        private static bool Bullet_OnTriggerEnter_Prefix(Bullet __instance, Collider other)
        {
            if (!Coop.IsClient || !Coop.Active || !Ingame) return true;
            var hb = other != null ? other.GetComponent<Hurtbox>() : null;
            if (hb == null || hb.master == null) return true;

            // our own bullet grazing an enemy proxy: local blood feedback only
            if (__instance.masterCharacterCategory == CharacterCategory.Player && hb.master is Enemy)
            {
                var vfx = Player.instance != null ? Player.instance.bloodVfxPF : null;
                var targetVfx = hb.master.bloodVfxPF != null ? hb.master.bloodVfxPF : vfx;
                if (targetVfx != null)
                {
                    var dir = __instance.direction == Vector3.zero ? Vector3.forward : __instance.direction;
                    var g = Object.Instantiate(targetVfx, hb.master.GetBodyCenter(), Quaternion.LookRotation(-dir), RuntimeObjectParent.instance.vfxParent);
                    Object.Destroy(g, g.GetComponent<ParticleSystem>().main.duration);
                }
                return false;
            }
            // every other hurtbox interaction (enemy bullets vs me/proxies) is host-owned
            return false;
        }

        // ---------------- wave / kill count ----------------

        [HarmonyPrefix]
        [HarmonyPatch(typeof(WaveManager), nameof(WaveManager.OnKillCountIncrease))]
        private static bool KillCount_Prefix(WaveManager __instance, ref int newKillCount)
        {
            if (!Coop.Active) return true;
            if (Coop.IsHost)
            {
                Coop.Host.AddSharedKill();
                newKillCount = Coop.Host.SharedKillCount; // boss pacing uses the shared counter
                return true;
            }
            return false; // client kill count comes from Global packets
        }

        [HarmonyPrefix]
        [HarmonyPatch(typeof(WaveManager), "Update")]
        private static bool WaveUpdate_Prefix()
        {
            // client never spawns its own enemies
            return !Coop.IsClient;
        }

        // host-dead-but-client-alive: keep spawning as long as any player lives
        [HarmonyPrefix]
        [HarmonyPatch(typeof(WaveManager), "SpawnAtRandomPoint")]
        private static void SpawnAliveOverride_Prefix()
        {
            if (!Coop.IsHost || !Coop.Active) return;
            var p = Player.instance;
            if (p != null && !p.isAlive && Ghost != null && Ghost.isAlive)
                p.isAlive = true;
        }

        [HarmonyFinalizer]
        [HarmonyPatch(typeof(WaveManager), "SpawnAtRandomPoint")]
        private static Exception SpawnAliveOverride_Finalizer(Exception __exception)
        {
            if (Coop.IsHost)
            {
                var p = Player.instance;
                if (p != null && !p.isAlive && Ghost != null && Ghost.isAlive)
                    p.isAlive = false; // restore truth
            }
            return __exception; // rethrow as before
        }

        // ---------------- scenes ----------------

        [HarmonyPostfix]
        [HarmonyPatch(typeof(SceneTransitionManager), nameof(SceneTransitionManager.LoadIngameScene))]
        private static void Scene_Ingame_Post() { HostBroadcastScene("Scene_Ingame"); }

        [HarmonyPostfix]
        [HarmonyPatch(typeof(SceneTransitionManager), nameof(SceneTransitionManager.LoadMainScene))]
        private static void Scene_Main_Post() { HostBroadcastScene("Scene_Main"); }

        [HarmonyPostfix]
        [HarmonyPatch(typeof(SceneTransitionManager), nameof(SceneTransitionManager.LoadLoadoutScene))]
        private static void Scene_Loadout_Post() { HostBroadcastScene("Scene_Loadout"); }

        [HarmonyPostfix]
        [HarmonyPatch(typeof(SceneTransitionManager), nameof(SceneTransitionManager.LoadIntroScene))]
        private static void Scene_Intro_Post() { HostBroadcastScene("Scene_Intro"); }

        [HarmonyPostfix]
        [HarmonyPatch(typeof(SceneTransitionManager), nameof(SceneTransitionManager.LoadEndingScene))]
        private static void Scene_Ending_Post() { HostBroadcastScene("Scene_Ending"); }

        private static void HostBroadcastScene(string name)
        {
            if (Coop.IsHost && Coop.Active)
                Coop.Net.SendReliable(Op.Scene, Protocol.Scene(name));
        }

        [HarmonyPrefix]
        [HarmonyPatch(typeof(SceneTransitionManager), nameof(SceneTransitionManager.LoadIngameScene))]
        [HarmonyPatch(typeof(SceneTransitionManager), nameof(SceneTransitionManager.LoadMainScene))]
        [HarmonyPatch(typeof(SceneTransitionManager), nameof(SceneTransitionManager.LoadLoadoutScene))]
        [HarmonyPatch(typeof(SceneTransitionManager), nameof(SceneTransitionManager.LoadIntroScene))]
        [HarmonyPatch(typeof(SceneTransitionManager), nameof(SceneTransitionManager.LoadEndingScene))]
        private static bool Scene_Client_Block()
        {
            if (Coop.IsClient && Coop.Active && !Coop.Client.AllowSceneLoad && !Coop.Client.RunOver)
            {
                Debug.Log("[KTC-Coop] You are a co-op client — wait for the host to start/transition.");
                return false;
            }
            return true;
        }

        // ---------------- pause ----------------

        [HarmonyPrefix]
        [HarmonyPatch(typeof(TimeManager), nameof(TimeManager.Pause))]
        private static bool Pause_Prefix(bool isOn)
        {
            if (Coop.IsClient && Coop.Active && isOn && !Coop.Client.SyncedPause)
            {
                Debug.Log("[KTC-Coop] Only the host can pause in co-op.");
                return false;
            }
            return true;
        }

        [HarmonyPostfix]
        [HarmonyPatch(typeof(TimeManager), nameof(TimeManager.Pause))]
        private static void Pause_Postfix()
        {
            if (Coop.IsHost && Coop.Active) Coop.Host?.SendGlobal();
        }
    }
}
