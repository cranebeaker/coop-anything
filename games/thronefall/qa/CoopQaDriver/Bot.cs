// Virtual controller: overrides the local player's sampled input (PlayerNetworkData.SharedData) after the mod reads the
// real controller, so the bot walks, holds Interact and holds Call Night through the same code paths as a person,
// including the mod's input sync to the other machine. One task at a time.
using System;
using HarmonyLib;
using ThronefallMP.Components;
using UnityEngine;

namespace CoopQa
{
    // Virtual controller at the Rewired level: prefixes on Rewired.Player.GetButton/GetButtonDown/GetButtonUp/GetAxis
    // answer for the gameplay actions, so the game and the mod read the bot exactly like a gamepad.
    public static class BotInput
    {
        public static bool Active;
        public static float Horizontal, Vertical;
        private static bool _interact, _callNight;
        private static int _interactDown = -10, _interactUp = -10, _nightDown = -10, _nightUp = -10;

        public static bool Interact
        {
            get => _interact;
            set { if (value != _interact) { if (value) _interactDown = Time.frameCount; else _interactUp = Time.frameCount; } _interact = value; }
        }

        public static bool CallNight
        {
            get => _callNight;
            set { if (value != _callNight) { if (value) _nightDown = Time.frameCount; else _nightUp = Time.frameCount; } _callNight = value; }
        }

        public static void Clear()
        {
            Horizontal = Vertical = 0f;
            Interact = CallNight = false;
        }

        // The press becomes visible on the frame after the bot set it (the bot ticks in its own Update).
        private static bool Edge(int frame) => Time.frameCount == frame + 1;

        public static bool GetButton(string actionName, ref bool __result)
        {
            if (!Active) return true;
            switch (actionName)
            {
                case "Interact": __result = _interact; return false;
                case "Call Night": __result = _callNight; return false;
                case "Sprint": case "Sprint Toggle": case "Command Units":
                case "Select All Army": case "Select All Melee": case "Select All Ranged": case "Select All Heroes":
                    __result = false; return false;
                default: return true;
            }
        }

        public static bool GetButtonDown(string actionName, ref bool __result)
        {
            if (!Active) return true;
            switch (actionName)
            {
                case "Interact": __result = _interact && Edge(_interactDown); return false;
                case "Call Night": __result = _callNight && Edge(_nightDown); return false;
                default: return true;
            }
        }

        public static bool GetButtonUp(string actionName, ref bool __result)
        {
            if (!Active) return true;
            switch (actionName)
            {
                case "Interact": __result = !_interact && Edge(_interactUp); return false;
                case "Call Night": __result = !_callNight && Edge(_nightUp); return false;
                default: return true;
            }
        }

        public static bool GetAxis(string actionName, ref float __result)
        {
            if (!Active) return true;
            switch (actionName)
            {
                case "Move Horizontal": __result = Horizontal; return false;
                case "Move Vertical": __result = Vertical; return false;
                default: return true;
            }
        }

        public static void Install(Harmony harmony)
        {
            var player = typeof(Rewired.Player);
            void P(string name, Type ret, string handler)
            {
                var m = AccessTools.Method(player, name, new[] { typeof(string) })
                        ?? throw new MissingMethodException("Rewired.Player." + name + "(string)");
                harmony.Patch(m, prefix: new HarmonyMethod(typeof(BotInput), handler));
            }
            P("GetButton", typeof(bool), nameof(GetButton));
            P("GetButtonDown", typeof(bool), nameof(GetButtonDown));
            P("GetButtonUp", typeof(bool), nameof(GetButtonUp));
            P("GetAxis", typeof(float), nameof(GetAxis));
        }
    }

    public class BotTask
    {
        public string Name;
        public string State = "running";   // running | done | failed
        public string Reason = "";
        public float Started = Time.realtimeSinceStartup;
        public Func<BotTask, bool> Tick;    // returns true when finished (State set by the tick)
        public float Elapsed => Time.realtimeSinceStartup - Started;

        public bool Fail(string why) { State = "failed"; Reason = why; return true; }
        public bool Done(string note = "") { State = "done"; Reason = note; return true; }
    }

    public static class Bot
    {
        public static BotTask Current;

        public static GameObject LocalPlayer()
        {
            var p = ThronefallMP.Plugin.Instance?.PlayerManager?.LocalPlayer;
            return p != null ? p.Object : null;
        }

        public static void Start(BotTask t)
        {
            if (Current != null && Current.State == "running") Current.Fail("replaced by " + t.Name);
            BotInput.Clear();
            BotInput.Active = true;
            Current = t;
        }

        public static void Stop()
        {
            if (Current != null && Current.State == "running") Current.Fail("stopped");
            BotInput.Clear();
        }

        public static void Update()
        {
            var t = Current;
            if (t == null || t.State != "running") { BotInput.Clear(); return; }
            bool finished;
            try { finished = t.Tick(t); }
            catch (Exception e) { finished = t.Fail("exception: " + e.Message); }
            if (finished) BotInput.Clear();
        }

        // Steer toward a world point through the camera-relative movement axes. Returns the remaining XZ distance.
        public static float SteerTo(Vector3 target)
        {
            var player = LocalPlayer();
            if (player == null) { BotInput.Clear(); return float.MaxValue; }
            var delta = target - player.transform.position;
            delta.y = 0f;
            var dist = delta.magnitude;
            if (dist < 0.05f) { BotInput.Horizontal = BotInput.Vertical = 0f; return dist; }
            var dir = delta / dist;
            var movement = player.GetComponent<PlayerMovement>();
            var view = movement != null ? Traverse.Create(movement).Field<Transform>("viewTransform").Value : null;
            var cam = view != null ? view : (Camera.main != null ? Camera.main.transform : null);
            if (cam == null) { BotInput.Clear(); return dist; }
            var fwd = Vector3.ProjectOnPlane(cam.forward, Vector3.up).normalized;
            var right = Vector3.ProjectOnPlane(cam.right, Vector3.up).normalized;
            // Slow down close to the target so the bot does not orbit it.
            var speed = Mathf.Clamp01(dist / 1.5f);
            BotInput.Vertical = Vector3.Dot(dir, fwd) * speed;
            BotInput.Horizontal = Vector3.Dot(dir, right) * speed;
            return dist;
        }

        public static BotTask Walk(Vector3 target, float tolerance = 1.0f, float timeout = 40f)
        {
            float bestDist = float.MaxValue, bestAt = 0f;
            return new BotTask
            {
                Name = $"walk {target.x:F1},{target.z:F1}",
                Tick = t =>
                {
                    var d = SteerTo(target);
                    if (d <= tolerance) return t.Done($"arrived ({d:F2} m)");
                    if (d < bestDist - 0.2f) { bestDist = d; bestAt = t.Elapsed; }
                    if (t.Elapsed - bestAt > 6f) return t.Fail($"stuck: {d:F1} m left, no progress for 6 s");
                    if (t.Elapsed > timeout) return t.Fail($"timeout: {d:F1} m left after {timeout:F0} s");
                    return false;
                }
            };
        }

        // Walk to a build slot until its interactor has focus, then hold Interact until the slot's level goes up.
        public static BotTask Build(ushort slotId, float timeout = 60f)
        {
            int startLevel = -1;
            float holdSince = -1f, bestDist = float.MaxValue, bestAt = 0f;
            var teleported = false;
            return new BotTask
            {
                Name = $"build slot {slotId}",
                Tick = t =>
                {
                    var go = Identifier.GetGameObject(IdentifierType.BuildSlot, slotId);
                    var slot = go != null ? go.GetComponent<BuildSlot>() : null;
                    if (slot == null) return t.Fail($"no build slot with id {slotId}");
                    if (startLevel < 0) startLevel = slot.Level;
                    if (slot.Level > startLevel) return t.Done($"level {startLevel} -> {slot.Level}");
                    var player = LocalPlayer();
                    var interaction = player != null ? player.GetComponent<PlayerInteraction>() : null;
                    if (interaction == null) return t.Fail("no local PlayerInteraction");
                    var interactor = slot.buildingInteractor;
                    if (interactor == null) return t.Fail("slot has no BuildingInteractor (not buildable now?)");
                    if (!teleported)
                    {
                        // Walking across the map is not what we test: land on the edge of the slot's interaction zone,
                        // on the side facing the player, then do the rest on foot.
                        teleported = true;
                        var zone = interactor.GetComponent<Collider>();
                        var here = player.transform.position;
                        if (zone != null && Vector3.Distance(zone.ClosestPoint(here), here) > 3f)
                        {
                            var edge = zone.ClosestPoint(here);
                            Teleport(new Vector3(edge.x, here.y, edge.z));
                            bestAt = t.Elapsed;
                            return false;
                        }
                    }
                    if (interaction.FocussedInteractor == interactor)
                    {
                        BotInput.Horizontal = BotInput.Vertical = 0f;
                        BotInput.Interact = true;
                        if (holdSince < 0f) holdSince = t.Elapsed;
                        if (t.Elapsed - holdSince > 12f)
                            return t.Fail($"held Interact 12 s, level still {slot.Level}; gold {ThronefallMP.GlobalData.Balance}, player balance {interaction.Balance}");
                    }
                    else
                    {
                        BotInput.Interact = false;
                        holdSince = -1f;
                        var d = SteerTo(interactor.transform.position);
                        if (d < bestDist - 0.2f) { bestDist = d; bestAt = t.Elapsed; }
                        if (t.Elapsed - bestAt > 8f)
                            return t.Fail($"never got focus: {d:F1} m from the interactor, focussed={(interaction.FocussedInteractor != null ? interaction.FocussedInteractor.name : "none")}");
                    }
                    if (t.Elapsed > timeout) return t.Fail($"timeout after {timeout:F0} s");
                    return false;
                }
            };
        }

        public static LevelInteractor FindLevel(string scene)
        {
            foreach (var li in UnityEngine.Object.FindObjectsByType<LevelInteractor>(FindObjectsSortMode.None))
                if (li.levelInfo != null && li.levelInfo.sceneName == scene) return li;
            return null;
        }

        // Move the local player instantly (the mod syncs the new position to the other machine).
        public static void Teleport(Vector3 pos)
        {
            var player = LocalPlayer();
            if (player == null) return;
            var cc = player.GetComponent<CharacterController>();
            if (cc != null) cc.enabled = false;
            player.transform.position = pos;
            if (cc != null) cc.enabled = true;
        }

        // In the level select map: go to a level's flag (teleport to the game's own arrival point for that flag, the
        // overworld is not what we test), get focus, then tap Interact until the loadout popup opens.
        public static BotTask OpenLevel(string scene, float timeout = 60f)
        {
            float bestDist = float.MaxValue, bestAt = 0f, tapAt = -1f;
            var teleported = false;
            return new BotTask
            {
                Name = "open level " + scene,
                Tick = t =>
                {
                    if (ThronefallMP.UI.LoadoutFrames.PopupOpen) return t.Done("loadout popup open");
                    var li = FindLevel(scene);
                    if (li == null) return t.Fail("no level flag for " + scene + " in this scene");
                    if (!li.CanBePlayed) return t.Fail("level cannot be played: " + li.CanNotBePlayedReason);
                    var player = LocalPlayer();
                    var interaction = player != null ? player.GetComponent<PlayerInteraction>() : null;
                    if (interaction == null) return t.Fail("no local PlayerInteraction");
                    if (!teleported)
                    {
                        teleported = true;
                        // Focus comes from an overlap with the flag's interaction collider, which sits away from the flag
                        // itself: land on the collider's point nearest to the game's own arrival point for this flag.
                        var col = li.GetComponent<Collider>();
                        var land = col != null ? col.ClosestPoint(li.PlayerTeleportPosition) : li.PlayerTeleportPosition;
                        Teleport(new Vector3(land.x, li.PlayerTeleportPosition.y, land.z));
                        bestAt = t.Elapsed;
                        return false;
                    }
                    if (interaction.FocussedInteractor == li)
                    {
                        BotInput.Horizontal = BotInput.Vertical = 0f;
                        // A tap: press for a few frames, release, retry every second.
                        if (tapAt < 0f || t.Elapsed - tapAt > 1f) tapAt = t.Elapsed;
                        BotInput.Interact = t.Elapsed - tapAt < 0.2f;
                    }
                    else
                    {
                        BotInput.Interact = false;
                        var col = li.GetComponent<Collider>();
                        var d = SteerTo(col != null ? col.bounds.center : li.transform.position);
                        if (d < bestDist - 0.2f) { bestDist = d; bestAt = t.Elapsed; }
                        if (t.Elapsed - bestAt > 8f) return t.Fail($"never got focus: {d:F1} m from the flag's interaction area");
                    }
                    if (t.Elapsed > timeout) return t.Fail($"timeout after {timeout:F0} s");
                    return false;
                }
            };
        }

        // Hold Call Night for a fixed time (stepping off any building first), whether or not the night starts.
        public static BotTask HoldNight(float seconds)
        {
            return new BotTask
            {
                Name = $"hold night {seconds:F0} s",
                Tick = t =>
                {
                    var me = LocalPlayer();
                    var pi = me != null ? me.GetComponent<PlayerInteraction>() : null;
                    if (pi != null && pi.FocussedInteractor != null && t.Elapsed < seconds)
                    {
                        BotInput.CallNight = false;
                        var away = me.transform.position - pi.FocussedInteractor.transform.position;
                        away.y = 0f;
                        SteerTo(me.transform.position + (away.sqrMagnitude < 0.01f ? Vector3.forward : away.normalized) * 3f);
                        return false;
                    }
                    BotInput.Horizontal = BotInput.Vertical = 0f;
                    BotInput.CallNight = true;
                    return t.Elapsed >= seconds && t.Done(DayNightCycle.Instance != null ? DayNightCycle.Instance.CurrentTimestate.ToString() : "?");
                }
            };
        }

        // Hold Call Night until night starts.
        public static BotTask CallNight(float timeout = 20f)
        {
            return new BotTask
            {
                Name = "call night",
                Tick = t =>
                {
                    if (DayNightCycle.Instance != null && DayNightCycle.Instance.CurrentTimestate == DayNightCycle.Timestate.Night)
                        return t.Done("night");
                    // The game only lets you call the night when no building or flag has your focus: step away first.
                    var me = LocalPlayer();
                    var pi = me != null ? me.GetComponent<PlayerInteraction>() : null;
                    if (pi != null && pi.FocussedInteractor != null)
                    {
                        BotInput.CallNight = false;
                        var away = me.transform.position - pi.FocussedInteractor.transform.position;
                        away.y = 0f;
                        if (away.sqrMagnitude < 0.01f) away = Vector3.forward;
                        SteerTo(me.transform.position + away.normalized * 3f);
                        return t.Elapsed > timeout && t.Fail($"could not leave the focus of {pi.FocussedInteractor.name}");
                    }
                    BotInput.Horizontal = BotInput.Vertical = 0f;
                    BotInput.CallNight = true;
                    if (t.Elapsed > timeout)
                        return t.Fail($"still day after {timeout:F0} s (IsFreeToCallNight={(pi != null && pi.IsFreeToCallNight)})");
                    return false;
                }
            };
        }
    }
}
