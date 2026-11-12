// What each machine sees. "shared" must be equal on host and client once synced; "per_player" may differ.
using System;
using System.Linq;
using System.Text;
using Newtonsoft.Json.Linq;
using ThronefallMP.Components;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace CoopQa
{
    public static class State
    {
        public static string ActiveScene()
        {
            for (var i = 0; i < SceneManager.sceneCount; i++)
            {
                var s = SceneManager.GetSceneAt(i);
                if (s.name != "_UI" && s.name != "_Boot" && s.isLoaded) return s.name;
            }
            return "";
        }

        public static JArray Slots()
        {
            var arr = new JArray();
            foreach (var slot in UnityEngine.Object.FindObjectsByType<BuildSlot>(FindObjectsInactive.Include, FindObjectsSortMode.None))
            {
                var id = slot.GetComponent<Identifier>();
                if (id == null) continue;
                var next = slot.CanBeUpgraded && slot.Level < slot.upgrades.Count ? slot.upgrades[slot.Level] : null;
                var pos = slot.buildingInteractor != null ? slot.buildingInteractor.transform.position : slot.transform.position;
                arr.Add(new JObject
                {
                    ["id"] = id.Id,
                    ["name"] = slot.buildingName,
                    ["level"] = slot.Level,
                    ["state"] = slot.State.ToString(),
                    ["active"] = slot.gameObject.activeInHierarchy,
                    ["interactable"] = slot.buildingInteractor != null && slot.buildingInteractor.gameObject.activeInHierarchy
                                       && slot.buildingInteractor.CanBeInteractedWith,
                    ["next_cost"] = next != null ? next.cost : -1,
                    ["choice"] = slot.CanBeUpgraded && slot.NextUpgradeIsChoice,
                    ["hp"] = HpOf(slot),
                    ["x"] = Math.Round(pos.x, 2), ["z"] = Math.Round(pos.z, 2),
                });
            }
            return new JArray(arr.OrderBy(s => (int)s["id"]));
        }

        private static double HpOf(BuildSlot slot)
        {
            var hp = HarmonyLib.Traverse.Create(slot).Field<Hp>("hpParent").Value;
            return hp != null ? Math.Round(hp.HpValue, 1) : -1;
        }

        public static JObject Snapshot()
        {
            var shared = new JObject();
            var per = new JObject();
            var plugin = ThronefallMP.Plugin.Instance;
            shared["scene"] = ActiveScene();
            shared["gamestate"] = LocalGamestate.Instance != null ? LocalGamestate.Instance.CurrentState.ToString() : "none";
            shared["time"] = DayNightCycle.Instance != null ? DayNightCycle.Instance.CurrentTimestate.ToString() : "none";
            shared["wave"] = EnemySpawner.instance != null ? EnemySpawner.instance.Wavenumber : -1;
            shared["wave_count"] = EnemySpawner.instance != null ? EnemySpawner.instance.WaveCount : -1;
            shared["match_over"] = EnemySpawner.instance != null && EnemySpawner.instance.MatchOver;
            shared["gold"] = ThronefallMP.GlobalData.Internal.Balance;
            per["gold_shown"] = ThronefallMP.GlobalData.Balance;
            var buildings = new JObject();
            var hps = new JObject();
            foreach (var s in Slots())
            {
                buildings[s["id"].ToString()] = $"{s["name"]}:L{s["level"]}:{s["state"]}";
                hps[s["id"].ToString()] = s["hp"];
            }
            shared["buildings"] = buildings;
            per["building_hp"] = hps;   // changes continuously in a fight; compared only where a check asks for it
            if (TagManager.instance != null)
            {
                per["enemies"] = TagManager.instance.EnemyUnits.Count;
                per["allies"] = TagManager.instance.PlayerUnits.Count;
            }
            if (plugin != null)
            {
                var players = new JObject();
                foreach (var p in plugin.PlayerManager.GetAllPlayers())
                {
                    var pos = p.Object != null ? p.Object.transform.position : Vector3.zero;
                    var hp = p.Object != null ? p.Object.GetComponent<Hp>() : null;
                    players[p.SteamID.m_SteamID.ToString()] = new JObject
                    {
                        ["x"] = Math.Round(pos.x, 1), ["z"] = Math.Round(pos.z, 1),
                        ["hp"] = hp != null ? Math.Round(hp.HpValue, 1) : -1,
                    };
                }
                per["players"] = players;
                shared["player_count"] = players.Count;
            }
            return new JObject { ["shared"] = shared, ["per_player"] = per };
        }

        // A number that is equal on every machine once nothing is in flight: a hash of the discrete shared state.
        public static long Seq(JObject shared)
        {
            var s = shared.ToString(Newtonsoft.Json.Formatting.None);
            unchecked
            {
                long h = 1469598103934665603;
                foreach (var b in Encoding.UTF8.GetBytes(s)) { h ^= b; h *= 1099511628211; }
                return h & 0x7fffffffffff;
            }
        }
    }
}
