using System.Collections.Generic;
using UnityEngine;

namespace KtcCoop
{
    /// <summary>Shared partner-revive: bring a dead player back next to the survivor.</summary>
    public static class Revive
    {
        private struct InvulnRec { public Player P; public float Until; }

        private static readonly List<InvulnRec> _invuln = new List<InvulnRec>();

        public static void Perform(Player p, Vector3 at, float invulnSec)
        {
            if (p == null) return;
            p.transform.position = at + Vector3.right * 1.2f;
            p.hp = p.statData != null ? p.statData.maxHp : 1;
            var passive = p.playerPassive != null ? p.playerPassive.GetPassiveData(PlayerPassiveKey.Gear_Passive_OneCoinMore) : null;
            if (passive != null) p.hp++;

            p.isAlive = true;
            p.isMovable = true;
            p.isControllable = true;
            foreach (var col in p.GetComponentsInChildren<Collider>())
                col.enabled = true;
            if (p.animator != null)
            {
                p.animator.StopPlayback();
                p.animator.speed = 1f;
                p.animator.Play("PixelAnim_Player_Idle");
            }
            p.gunPivot?.gameObject.SetActive(true);
            p.gunPivot2?.gameObject.SetActive(true);
            if (IngameUIManager.instance != null) IngameUIManager.instance.gameObject.SetActive(true);
            var cm = CameraManager.instance;
            if (cm != null)
            {
                cm.isFollowingTarget = true;
                cm.ChangeCamTarget(p.transform);
            }
            if (TimeManager.instance != null)
            {
                Time.timeScale = 1f;
                TimeManager.instance.SetShowdownTimeScale(1f);
            }
            p.isInvincibleOneCoinMore = true;
            _invuln.Add(new InvulnRec { P = p, Until = Time.unscaledTime + invulnSec });
        }

        public static bool IsInvulnerable(Player p)
        {
            for (int i = _invuln.Count - 1; i >= 0; i--)
            {
                if (_invuln[i].P == p)
                {
                    if (Time.unscaledTime < _invuln[i].Until) return true;
                    _invuln[i].P.isInvincibleOneCoinMore = false;
                    _invuln.RemoveAt(i);
                }
            }
            return false;
        }
    }
}
