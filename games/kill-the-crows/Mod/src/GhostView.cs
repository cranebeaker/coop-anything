using UnityEngine;
using Visual;
using Object = UnityEngine.Object;

namespace KtcCoop
{
    /// <summary>
    /// A stripped-down replica of a remote player: visuals + hurtbox only,
    /// driven entirely by network state. Used by the host (client ghost) and
    /// the client (host ghost).
    /// </summary>
    public class GhostView
    {
        public GameObject Go;
        public Player Player;
        public Animator Anim;
        public SpriteController Sprite;
        public ushort LastHash;

        public static GhostView Create(Player source, string name)
        {
            var go = Object.Instantiate(source.gameObject, source.transform.parent, true);
            go.name = name;
            foreach (var mb in go.GetComponentsInChildren<MonoBehaviour>(true))
                if (!(mb is Hurtbox)) mb.enabled = false;
            foreach (var a in go.GetComponentsInChildren<AudioSource>(true))
                a.enabled = false;
            var view = new GhostView
            {
                Go = go,
                Player = go.GetComponent<Player>(),
                Anim = go.GetComponentInChildren<Animator>(true),
                Sprite = go.GetComponentInChildren<SpriteController>(true)
            };
            view.Player.isAlive = true;
            view.Player.isControllable = false;
            view.Player.isMovable = false;
            return view;
        }

        public void Apply(Vector3 pos, float yaw, Vector3 aim, bool flip, bool dodging,
            bool invDodge, bool moving, bool alive, float sdScale, ushort hash, float norm)
        {
            if (Player == null) return;
            Player.transform.position = pos;
            Player.isDodging = dodging;
            Player.isInvincibleDodge = invDodge;
            Player.isMoving = moving;
            Player.playerAimVector = aim;
            if (Player.rotationPivot != null)
                Player.rotationPivot.rotation = Quaternion.Euler(0f, yaw, 0f);
            Player.faceRight = !flip;
            Sprite?.SetFlip(flip);
            if (Anim != null)
            {
                Anim.speed = sdScale;
                if (hash != 0 && hash != LastHash)
                {
                    Anim.Play(hash, 0, norm);
                    LastHash = hash;
                }
            }
            Player.isAlive = alive;
        }
    }
}
