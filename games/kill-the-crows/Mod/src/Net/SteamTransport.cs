using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using Steamworks;
using UnityEngine;

namespace KtcCoop.Net
{
    /// <summary>
    /// Steam P2P transport: friends-only lobby for discovery, SteamNetworking
    /// P2P datagrams for traffic (unreliable send; the reliable layer with
    /// seq/ack/resend lives here, mirroring the UDP transport). Driven from the
    /// main thread — the game's SteamManager already calls SteamAPI.RunCallbacks.
    /// </summary>
    public class SteamTransport : INetChannel, IDisposable
    {
        public const string LobbyKeyVersion = "ktc_version";
        public const string LobbyKeyName = "ktc_name";

        private struct OutMsg
        {
            public uint Seq;
            public byte[] Frame;
            public float LastSend;
            public int Tries;
        }

        private const float ResendSec = 0.15f;
        private const int MaxReliableInFlight = 64;
        private const float TimeoutSec = 6f;

        public ConcurrentQueue<NetMsg> Inbox { get; } = new ConcurrentQueue<NetMsg>();
        public bool Connected { get; private set; }
        public CSteamID LobbyId;
        public CSteamID RemotePeer;
        public string PeerName = "?";

        public bool IsHostMode { get; private set; }
        private bool _isHost;
        private uint _nextSeq = 1;
        private readonly List<OutMsg> _inFlight = new List<OutMsg>();
        private readonly HashSet<uint> _seen = new HashSet<uint>();
        private readonly List<uint> _seenBatch = new List<uint>();
        private float _lastReceive;

        private static Callback<LobbyCreated_t> _cbCreated;
        private static Callback<LobbyEnter_t> _cbEnter;
        private static CallResult<LobbyMatchList_t> _crMatchList;
        private static SteamAPICall_t _matchListCall;

        public static SteamTransport Pending; // lobby being created/joined

        // ---------------- lifecycle ----------------

        public static void BeginHost()
        {
            if (!SteamManager.Initialized) { Plugin.Flog("[steam] not initialized — use LAN (F9)"); return; }
            Pending = new SteamTransport { _isHost = true, IsHostMode = true, Connected = false };
            _cbCreated = Callback<LobbyCreated_t>.Create(t =>
            {
                if (Pending == null || t.m_eResult != EResult.k_EResultOK)
                {
                    Plugin.Flog("[steam] lobby create failed: " + t.m_eResult);
                    Pending = null;
                    return;
                }
                Pending.LobbyId = new CSteamID(t.m_ulSteamIDLobby);
                SteamMatchmaking.SetLobbyData(Pending.LobbyId, LobbyKeyVersion, PluginVersion());
                SteamMatchmaking.SetLobbyData(Pending.LobbyId, LobbyKeyName, "KTC Co-op (host)");
                Plugin.Flog($"[steam] lobby created {Pending.LobbyId} — friend presses F7 to join");
            });
            SteamMatchmaking.CreateLobby(ELobbyType.k_ELobbyTypeFriendsOnly, 2);
        }

        public static void BeginJoin()
        {
            if (!SteamManager.Initialized) { Plugin.Flog("[steam] not initialized — use LAN (F10)"); return; }
            Pending = new SteamTransport { _isHost = false, IsHostMode = false, Connected = false };
            SteamMatchmaking.AddRequestLobbyListStringFilter(LobbyKeyVersion, PluginVersion(), ELobbyComparison.k_ELobbyComparisonEqual);
            _crMatchList = CallResult<LobbyMatchList_t>.Create((t, ioFailure) =>
            {
                if (Pending == null || ioFailure) return;
                if (t.m_nLobbiesMatching <= 0)
                {
                    Plugin.Flog("[steam] no co-op lobbies found — ask the host to press F6");
                    Pending = null;
                    return;
                }
                CSteamID lobby = SteamMatchmaking.GetLobbyByIndex(0);
                Plugin.Flog("[steam] joining lobby " + lobby + "...");
                SteamMatchmaking.JoinLobby(lobby);
            });
            _matchListCall = SteamMatchmaking.RequestLobbyList();
            _crMatchList.Set(_matchListCall);
        }

        // Register once from Plugin.Awake
        public static void RegisterCallbacks()
        {
            if (!SteamManager.Initialized) return;
            _cbEnter = Callback<LobbyEnter_t>.Create(t =>
            {
                if (Pending == null) return;
                if ((EChatRoomEnterResponse)t.m_EChatRoomEnterResponse != EChatRoomEnterResponse.k_EChatRoomEnterResponseSuccess)
                {
                    Plugin.Flog("[steam] lobby join failed: " + t.m_EChatRoomEnterResponse);
                    Pending = null;
                    return;
                }
                Pending.LobbyId = new CSteamID(t.m_ulSteamIDLobby);
                CSteamID owner = SteamMatchmaking.GetLobbyOwner(Pending.LobbyId);
                if (Pending._isHost)
                {
                    Pending.Connected = true; // await first datagram to lock the peer
                    Plugin.Flog("[steam] lobby ready — waiting for client");
                }
                else
                {
                    Pending.RemotePeer = owner;
                    Pending.Connected = true;
                    Plugin.Flog($"[steam] joined lobby, owner {owner} — handshaking");
                    Pending.SendHello();
                }
            });
        }

        private void SendHello()
        {
            SendReliable(Op.Hello, Protocol.Hello(
                Environment.UserName,
                PrefManager.GearRevolver,
                PrefManager.GearShowdown,
                PrefManager.GearPassive1,
                PrefManager.GearPassive2,
                PrefManager.ProgressCurrentDifficultyLevel));
        }

        private static string PluginVersion() => "2";

        // ---------------- send (same surface as UDP Transport) ----------------

        public void SendUnreliable(byte op, byte[] payload)
        {
            SendFrame(0, op, payload);
        }

        public void SendReliable(byte op, byte[] payload)
        {
            if (_inFlight.Count >= MaxReliableInFlight) return;
            uint seq = _nextSeq++;
            _inFlight.Add(new OutMsg { Seq = seq, Frame = BuildFrame(seq, op, payload), LastSend = 0f, Tries = 0 });
        }

        private static byte[] BuildFrame(uint seq, byte op, byte[] payload)
        {
            byte[] frame = new byte[5 + payload.Length];
            frame[0] = (byte)seq; frame[1] = (byte)(seq >> 8); frame[2] = (byte)(seq >> 16); frame[3] = (byte)(seq >> 24);
            frame[4] = op;
            Buffer.BlockCopy(payload, 0, frame, 5, payload.Length);
            return frame;
        }

        private void SendFrame(uint seq, byte op, byte[] payload)
        {
            if (!Connected || RemotePeer == CSteamID.Nil) return;
            byte[] frame = BuildFrame(seq, op, payload);
            SendDatagram(frame);
        }

        private void SendDatagram(byte[] frame)
        {
            if (RemotePeer == CSteamID.Nil) return;
            SteamNetworking.SendP2PPacket(RemotePeer, frame, (uint)frame.Length, EP2PSend.k_EP2PSendUnreliable, 0);
        }

        private void SendAckBatch()
        {
            if (_seenBatch.Count == 0) return;
            int n = Math.Min(_seenBatch.Count, 32);
            byte[] frame = new byte[5 + n * 4];
            for (int i = 0; i < n; i++)
            {
                uint s = _seenBatch[i];
                frame[5 + i * 4] = (byte)s; frame[6 + i * 4] = (byte)(s >> 8); frame[7 + i * 4] = (byte)(s >> 16); frame[8 + i * 4] = (byte)(s >> 24);
            }
            frame[4] = 0xFF; // reserved op for acks
            _seenBatch.RemoveRange(0, n);
            SendDatagram(frame);
        }

        // ---------------- main-thread tick ----------------

        public void Tick()
        {
            // drain P2P
            while (SteamNetworking.IsP2PPacketAvailable(out uint size, 0))
            {
                byte[] buf = new byte[size];
                if (!SteamNetworking.ReadP2PPacket(buf, size, out size, out CSteamID from, 0)) break;

                if (RemotePeer == CSteamID.Nil && _isHost)
                {
                    RemotePeer = from; // first datagram locks the peer
                    Connected = true;
                    Plugin.Flog($"[steam] peer locked {from}");
                }
                else if (Connected && from != RemotePeer)
                {
                    continue; // strangers
                }
                _lastReceive = Time.unscaledTime;

                if (buf.Length > 5 && buf[4] == 0xFF)
                {
                    ApplyAcks(buf);
                    continue;
                }
                if (buf.Length < 5) continue;

                uint seq = (uint)(buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24);
                byte op = buf[4];

                if (seq != 0)
                {
                    if (!_seen.Add(seq)) continue;
                    _seenBatch.Add(seq);
                    SendAckBatch();
                }
                int payloadLen = buf.Length - 5;
                var payload = new byte[payloadLen];
                Buffer.BlockCopy(buf, 5, payload, 0, payloadLen);
                Inbox.Enqueue(new NetMsg { Op = op, Data = payload, Reliable = seq != 0 });
            }

            // resend reliable
            float now = Time.unscaledTime;
            for (int i = _inFlight.Count - 1; i >= 0; i--)
            {
                var m = _inFlight[i];
                if (m.LastSend == 0f || now - m.LastSend >= ResendSec)
                {
                    m.LastSend = now;
                    m.Tries++;
                    if (m.Tries > 60) { _inFlight.RemoveAt(i); continue; }
                    _inFlight[i] = m;
                    SendDatagram(m.Frame);
                }
            }

            // timeout
            if (Connected && RemotePeer != CSteamID.Nil && now - _lastReceive > TimeoutSec && _lastReceive > 0f)
            {
                Plugin.Flog("[steam] timeout, disconnecting");
                Inbox.Enqueue(new NetMsg { Op = Op.Timeouted, Data = Array.Empty<byte>(), Reliable = false });
                _lastReceive = now;
            }
        }

        private void ApplyAcks(byte[] b)
        {
            int n = (b.Length - 5) / 4;
            for (int i = _inFlight.Count - 1; i >= 0; i--)
            {
                uint seq = _inFlight[i].Seq;
                for (int j = 0; j < n; j++)
                {
                    int o = 5 + j * 4;
                    uint a = (uint)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24);
                    if (a == seq) { _inFlight.RemoveAt(i); break; }
                }
            }
        }

        public void Dispose()
        {
            if (LobbyId != CSteamID.Nil) SteamMatchmaking.LeaveLobby(LobbyId);
            if (RemotePeer != CSteamID.Nil) SteamNetworking.CloseP2PSessionWithUser(RemotePeer);
            if (Pending == this) Pending = null;
        }
    }
}
