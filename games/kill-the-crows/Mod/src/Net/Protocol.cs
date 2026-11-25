using System;
using System.IO;
using System.Text;
using UnityEngine;

namespace KtcCoop.Net
{
    public static class Protocol
    {
        public static byte[] Build(Action<BinaryWriter> w)
        {
            var ms = new MemoryStream();
            using (var bw = new BinaryWriter(ms, Encoding.UTF8))
                w(bw);
            return ms.ToArray();
        }

        public sealed class Reader : IDisposable
        {
            public readonly BinaryReader Br;
            public Reader(byte[] data) { Br = new BinaryReader(new MemoryStream(data), Encoding.UTF8); }
            public float F() => Br.ReadSingle();
            public int I() => Br.ReadInt32();
            public uint U() => Br.ReadUInt32();
            public ushort Us() => Br.ReadUInt16();
            public byte B() => Br.ReadByte();
            public bool Bool() => Br.ReadBoolean();
            public string S() => Br.ReadString();
            public Vector3 V3() => new Vector3(F(), F(), F());
            public void Dispose() => Br.Dispose();
        }

        public struct EnemyStateRec
        {
            public ushort Id;
            public Vector3 Pos;
            public float Yaw;
            public bool Flip;
            public int Hp;
            public ushort Hash;
            public float Norm;
            public float AgentSpeed;
        }

        public struct HelloRec
        {
            public string Name;
            public int Rev, Sd, P1, P2, Diff;
        }

        public struct WelcomeRec
        {
            public int Diff;
            public byte[] Mods;
        }

        public static void W(this BinaryWriter bw, Vector3 v)
        {
            bw.Write(v.x); bw.Write(v.y); bw.Write(v.z);
        }

        public static void W(this BinaryWriter bw, BulletInfo info)
        {
            bw.Write(info.damage);
            bw.Write(info.range);
            bw.Write(info.muzzleVelocity);
            bw.Write(info.penetrationCount);
            bw.Write(info.showdownResourceGainAmount);
            bw.Write(info.changeAccuracy);
            bw.Write(info.isShowdownBullet);
            bw.Write(info.isLastBullet);
            bw.Write(info.isMelee);
            bw.Write(info.isFlail);
            bw.Write(info.isExplosion);
            bw.Write(info.isConstellationBullet);
            bw.Write(info.isCannonBullet);
        }

        public static BulletInfo ReadBulletInfo(BinaryReader br)
        {
            return new BulletInfo
            {
                damage = br.ReadInt32(),
                range = br.ReadSingle(),
                muzzleVelocity = br.ReadSingle(),
                penetrationCount = br.ReadInt32(),
                showdownResourceGainAmount = br.ReadSingle(),
                changeAccuracy = br.ReadBoolean(),
                isShowdownBullet = br.ReadBoolean(),
                isLastBullet = br.ReadBoolean(),
                isMelee = br.ReadBoolean(),
                isFlail = br.ReadBoolean(),
                isExplosion = br.ReadBoolean(),
                isConstellationBullet = br.ReadBoolean(),
                isCannonBullet = br.ReadBoolean()
            };
        }

        // ---- Hello (C->H) ----
        public static byte[] Hello(string name, int revolver, int showdown, int p1, int p2, int difficulty)
        {
            return Build(w =>
            {
                w.Write(name);
                w.Write(revolver);
                w.Write(showdown);
                w.Write(p1);
                w.Write(p2);
                w.Write(difficulty);
            });
        }

        public static HelloRec ReadHello(byte[] data)
        {
            using var r = new Reader(data);
            return new HelloRec { Name = r.S(), Rev = r.I(), Sd = r.I(), P1 = r.I(), P2 = r.I(), Diff = r.I() };
        }

        // ---- Welcome (H->C) ----
        public static byte[] Welcome(int difficulty, byte[] modsMask)
        {
            return Build(w =>
            {
                w.Write(difficulty);
                w.Write((byte)modsMask.Length);
                w.Write(modsMask);
            });
        }

        public static WelcomeRec ReadWelcome(byte[] data)
        {
            using var r = new Reader(data);
            int diff = r.I();
            int n = r.B();
            return new WelcomeRec { Diff = diff, Mods = r.Br.ReadBytes(n) };
        }

        // ---- PlayerState (C->H) ----
        public static byte[] PlayerState(Vector3 pos, float pivotYaw, Vector3 aimDir, bool flip,
            bool dodging, bool invDodge, bool moving, bool alive, float showdownScale,
            ushort animHash, float animNorm)
        {
            return Build(w =>
            {
                w.W(pos);
                w.Write(pivotYaw);
                w.W(aimDir);
                w.Write(flip);
                w.Write(dodging);
                w.Write(invDodge);
                w.Write(moving);
                w.Write(alive);
                w.Write(showdownScale);
                w.Write(animHash);
                w.Write(animNorm);
            });
        }

        public static void ReadPlayerState(byte[] data, out Vector3 pos, out float yaw, out Vector3 aim,
            out bool flip, out bool dodging, out bool invDodge, out bool moving, out bool alive,
            out float sdScale, out ushort animHash, out float animNorm)
        {
            using var r = new Reader(data);
            pos = r.V3(); yaw = r.F(); aim = r.V3(); flip = r.Bool();
            dodging = r.Bool(); invDodge = r.Bool(); moving = r.Bool(); alive = r.Bool();
            sdScale = r.F(); animHash = r.Us(); animNorm = r.F();
        }

        // ---- EnemyStates (H->C): batch ----
        public static byte[] EnemyStates(EnemyStateRec[] list)
        {
            return Build(w =>
            {
                w.Write((byte)list.Length);
                foreach (var e in list)
                {
                    w.Write(e.Id);
                    w.W(e.Pos);
                    w.Write(e.Yaw);
                    w.Write(e.Flip);
                    w.Write(e.Hp);
                    w.Write(e.Hash);
                    w.Write(e.Norm);
                    w.Write(e.AgentSpeed);
                }
            });
        }

        public static EnemyStateRec[] ReadEnemyStates(byte[] data)
        {
            using var r = new Reader(data);
            int n = r.B();
            var arr = new EnemyStateRec[n];
            for (int i = 0; i < n; i++)
            {
                arr[i] = new EnemyStateRec
                {
                    Id = r.Us(),
                    Pos = r.V3(),
                    Yaw = r.F(),
                    Flip = r.Bool(),
                    Hp = r.I(),
                    Hash = r.Us(),
                    Norm = r.F(),
                    AgentSpeed = r.F()
                };
            }
            return arr;
        }

        // ---- Fire (C->H): one bullet spawn ----
        public static byte[] Fire(Vector3 pos, Vector3 dir, BulletInfo info, byte gunAnim)
        {
            return Build(w =>
            {
                w.W(pos);
                w.W(dir);
                w.W(info);
                w.Write(gunAnim);
            });
        }

        public static void ReadFire(byte[] data, out Vector3 pos, out Vector3 dir, out BulletInfo info, out byte gunAnim)
        {
            using var r = new Reader(data);
            pos = r.V3(); dir = r.V3();
            info = ReadBulletInfo(r.Br);
            gunAnim = r.B();
        }

        // ---- EnemyFire (H->C) ----
        public static byte[] EnemyFire(Vector3 pos, Vector3 dir, BulletInfo info, ushort shooterId)
        {
            return Build(w =>
            {
                w.W(pos);
                w.W(dir);
                w.W(info);
                w.Write(shooterId);
            });
        }

        public static void ReadEnemyFire(byte[] data, out Vector3 pos, out Vector3 dir, out BulletInfo info, out ushort shooterId)
        {
            using var r = new Reader(data);
            pos = r.V3(); dir = r.V3();
            info = ReadBulletInfo(r.Br);
            shooterId = r.Us();
        }

        // ---- GunAnim (C->H) ----
        public static byte[] GunAnim(byte kind) => Build(w => w.Write(kind));

        // ---- Spawn (H->C) ----
        public static byte[] Spawn(ushort id, int dictIdx, Vector3 pos)
        {
            return Build(w => { w.Write(id); w.Write(dictIdx); w.W(pos); });
        }

        public static void ReadSpawn(byte[] data, out ushort id, out int dictIdx, out Vector3 pos)
        {
            using var r = new Reader(data);
            id = r.Us(); dictIdx = r.I(); pos = r.V3();
        }

        // ---- Death (H->C) ----
        public static byte[] Death(ushort id, Vector3 deathblowDir, bool showOnClient, ushort deathHash, float deathNorm)
        {
            return Build(w => { w.Write(id); w.W(deathblowDir); w.Write(showOnClient); w.Write(deathHash); w.Write(deathNorm); });
        }

        public static void ReadDeath(byte[] data, out ushort id, out Vector3 dir, out bool show, out ushort deathHash, out float deathNorm)
        {
            using var r = new Reader(data);
            id = r.Us(); dir = r.V3(); show = r.Bool(); deathHash = r.Us(); deathNorm = r.F();
        }

        // ---- HitReq (C->H) ----
        public static byte[] HitReq(ushort enemyId, int damage, float sdGain, Vector3 hitPos, Vector3 dir, BulletInfo info)
        {
            return Build(w =>
            {
                w.Write(enemyId);
                w.Write(damage);
                w.Write(sdGain);
                w.W(hitPos);
                w.W(dir);
                w.W(info);
            });
        }

        public static void ReadHitReq(byte[] data, out ushort id, out int dmg, out float sdGain, out Vector3 hitPos, out Vector3 dir, out BulletInfo info)
        {
            using var r = new Reader(data);
            id = r.Us(); dmg = r.I(); sdGain = r.F();
            hitPos = r.V3(); dir = r.V3();
            info = ReadBulletInfo(r.Br);
        }

        // ---- DamagePlayer (H->C) ----
        public static byte[] DamagePlayer(int damage, Vector3 dir, BulletInfo info)
        {
            return Build(w => { w.Write(damage); w.W(dir); w.W(info); });
        }

        public static void ReadDamagePlayer(byte[] data, out int dmg, out Vector3 dir, out BulletInfo info)
        {
            using var r = new Reader(data);
            dmg = r.I(); dir = r.V3(); info = ReadBulletInfo(r.Br);
        }

        // ---- ShowdownGain (H->C) ----
        public static byte[] ShowdownGain(float amount) => Build(w => w.Write(amount));

        // ---- Global (H->C) ----
        public static byte[] Global(int killCount, int chunkLevel, int phase, bool spawnDisable, float sdScale, float gameTime, bool paused)
        {
            return Build(w =>
            {
                w.Write(killCount);
                w.Write(chunkLevel);
                w.Write(phase);
                w.Write(spawnDisable);
                w.Write(sdScale);
                w.Write(gameTime);
                w.Write(paused);
            });
        }

        public static void ReadGlobal(byte[] data, out int killCount, out int chunkLevel, out int phase,
            out bool spawnDisable, out float sdScale, out float gameTime, out bool paused)
        {
            using var r = new Reader(data);
            killCount = r.I(); chunkLevel = r.I(); phase = r.I(); spawnDisable = r.Bool();
            sdScale = r.F(); gameTime = r.F(); paused = r.Bool();
        }

        // ---- Scene (H->C) ----
        public static byte[] Scene(string sceneName)
        {
            return Build(w => w.Write(sceneName));
        }

        // ---- Mods (H->C): bitmask ----
        public static byte[] Mods(byte[] mask)
        {
            return Build(w => { w.Write((byte)mask.Length); w.Write(mask); });
        }

        public static byte[] ReadMods(byte[] data)
        {
            using var r = new Reader(data);
            int n = r.B();
            return r.Br.ReadBytes(n);
        }
    }
}
