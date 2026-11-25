using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using UnityEngine;

namespace KtcCoop.Net
{
    /// <summary>
    /// UDP transport with one unreliable channel (raw datagrams) and one
    /// reliable event channel (seq + ack + resend). One datagram == one message.
    /// Frame: [u32 seq][u8 op][payload...]  (seq == 0 means unreliable)
    /// </summary>
    public class Transport : INetChannel, IDisposable
    {
        public const int Port = 7777;
        private const int ResendMs = 150;
        private const int KeepAliveMs = 1000;
        private const int TimeoutMs = 6000;
        private const long TicksPerMs = TimeSpan.TicksPerMillisecond;
        private const int MaxReliableInFlight = 64;

        private readonly UdpClient _udp;
        private readonly Thread _rxThread;
        private readonly Thread _txThread;
        private volatile bool _running = true;

        // remote endpoint (host learns it from HELLO; client sets it at start)
        private volatile IPEndPoint _remote;

        // outgoing reliable queue (sent from tx thread)
        private readonly object _reliableLock = new object();
        private readonly Queue<OutMsg> _reliableOutbox = new Queue<OutMsg>();
        private readonly List<OutMsg> _reliableInFlight = new List<OutMsg>();
        private uint _nextSeq = 1;

        // incoming dedupe
        private readonly HashSet<uint> _seenReliable = new HashSet<uint>();
        private readonly List<uint> _seenList = new List<uint>();

        // acks received (to free in-flight)
        private readonly ConcurrentQueue<uint[]> _ackQueue = new ConcurrentQueue<uint[]>();

        // received messages for main thread
        public ConcurrentQueue<NetMsg> Inbox { get; } = new ConcurrentQueue<NetMsg>();

        private struct OutMsg
        {
            public uint Seq;
            public byte[] Frame;
            public long LastSendTicks;
            public int Tries;
        }

        public bool Connected { get; private set; }
        public long LastReceiveTicks { get; private set; } = DateTime.UtcNow.Ticks;

        public Transport(bool isHost, string connectIp)
        {
            if (isHost)
            {
                _udp = new UdpClient(Port);
            }
            else
            {
                _udp = new UdpClient();
                _remote = new IPEndPoint(IPAddress.Parse(connectIp), Port);
                Connected = true; // client knows its remote endpoint from the start
            }
            _udp.Client.ReceiveBufferSize = 1 << 20;
            _udp.Client.SendBufferSize = 1 << 20;
            _rxThread = new Thread(RxLoop) { IsBackground = true, Name = "ktc-rx" };
            _txThread = new Thread(TxLoop) { IsBackground = true, Name = "ktc-tx" };
            _rxThread.Start();
            _txThread.Start();
        }

        public void SetRemote(IPEndPoint ep)
        {
            _remote = ep;
            if (!Connected)
            {
                Connected = true;
                Debug.Log($"[KTC-Coop] Remote endpoint: {ep}");
            }
        }

        public IPEndPoint RemoteEndpoint => _remote;

        // ---------------- send API (thread-safe) ----------------

        public void SendUnreliable(byte op, byte[] payload)
        {
            var ep = _remote;
            if (ep == null || !Connected) return;
            byte[] frame = new byte[5 + payload.Length];
            WriteU32(frame, 0, 0);
            frame[4] = op;
            Buffer.BlockCopy(payload, 0, frame, 5, payload.Length);
            try { _udp.Send(frame, frame.Length, ep); } catch { }
        }

        public void SendReliable(byte op, byte[] payload)
        {
            var ep = _remote;
            if (ep == null || !Connected) return;
            lock (_reliableLock)
            {
                if (_reliableInFlight.Count >= MaxReliableInFlight) return; // drop overload, never block gameplay
                uint seq = _nextSeq++;
                byte[] frame = new byte[5 + payload.Length];
                WriteU32(frame, 0, seq);
                frame[4] = op;
                Buffer.BlockCopy(payload, 0, frame, 5, payload.Length);
                _reliableInFlight.Add(new OutMsg { Seq = seq, Frame = frame, LastSendTicks = 0, Tries = 0 });
            }
        }

        private static void WriteU32(byte[] b, int off, uint v)
        {
            b[off] = (byte)v; b[off + 1] = (byte)(v >> 8); b[off + 2] = (byte)(v >> 16); b[off + 3] = (byte)(v >> 24);
        }

        private static uint ReadU32(byte[] b, int off)
        {
            return b[off] | (uint)b[off + 1] << 8 | (uint)b[off + 2] << 16 | (uint)b[off + 3] << 24;
        }

        // ---------------- threads ----------------

        private void RxLoop()
        {
            var anyEp = new IPEndPoint(IPAddress.Any, 0);
            while (_running)
            {
                try
                {
                    byte[] datagram;
                    IPEndPoint from = null;
                    datagram = _udp.Receive(ref from);
                    if (datagram == null || datagram.Length < 5) continue;
                    LastReceiveTicks = DateTime.UtcNow.Ticks;

                    if (_remote == null)
                    {
                        if (from == null) continue;
                        _remote = from; // host lock-in
                        Connected = true;
                    }
                    else if (Connected && !SameEp(from, _remote))
                    {
                        continue; // ignore strangers
                    }

                    uint seq = ReadU32(datagram, 0);
                    byte op = datagram[4];

                    if (op == Op.Ack)
                    {
                        int n = (datagram.Length - 5) / 4;
                        var acks = new uint[n];
                        for (int i = 0; i < n; i++) acks[i] = ReadU32(datagram, 5 + i * 4);
                        _ackQueue.Enqueue(acks);
                        continue;
                    }

                    bool reliable = seq != 0;
                    if (reliable)
                    {
                        if (!_seenReliable.Add(seq)) continue; // duplicate
                        _seenList.Add(seq);
                        SendAckBatch();
                    }

                    int payloadLen = datagram.Length - 5;
                    var payload = new byte[payloadLen];
                    Buffer.BlockCopy(datagram, 5, payload, 0, payloadLen);
                    Inbox.Enqueue(new NetMsg { Op = op, Data = payload, Reliable = reliable });
                }
                catch (SocketException)
                {
                    // interrupted / closed
                    if (!_running) break;
                }
                catch (ObjectDisposedException) { break; }
                catch (Exception e)
                {
                    Debug.LogWarning($"[KTC-Coop] rx error: {e.Message}");
                }
            }
        }

        private void SendAckBatch()
        {
            if (_seenList.Count == 0) return;
            var ep = _remote;
            if (ep == null) return;
            int n = Math.Min(_seenList.Count, 32);
            byte[] frame = new byte[5 + n * 4];
            WriteU32(frame, 0, 0);
            frame[4] = Op.Ack;
            for (int i = 0; i < n; i++) WriteU32(frame, 5 + i * 4, _seenList[i]);
            _seenList.RemoveRange(0, n);
            try { _udp.Send(frame, frame.Length, ep); } catch { }
        }

        private void TxLoop()
        {
            long lastKeepAlive = 0;
            while (_running)
            {
                Thread.Sleep(30);
                long now = DateTime.UtcNow.Ticks;

                // ack received -> drop from in-flight
                while (_ackQueue.TryDequeue(out var acks))
                {
                    lock (_reliableLock)
                    {
                        for (int i = _reliableInFlight.Count - 1; i >= 0; i--)
                        {
                            uint seq = _reliableInFlight[i].Seq;
                            for (int j = 0; j < acks.Length; j++)
                                if (acks[j] == seq) { _reliableInFlight.RemoveAt(i); break; }
                        }
                    }
                }

                var ep = _remote;
                if (ep == null || !Connected) continue;

                // resend / flush reliable
                List<OutMsg> toSend = null;
                lock (_reliableLock)
                {
                    // move from queue to in-flight
                    while (_reliableOutbox.Count > 0 && _reliableInFlight.Count < MaxReliableInFlight)
                        _reliableInFlight.Add(_reliableOutbox.Dequeue());
                    toSend = new List<OutMsg>();
                    for (int i = _reliableInFlight.Count - 1; i >= 0; i--)
                    {
                        var m = _reliableInFlight[i];
                        if (m.LastSendTicks == 0 || now - m.LastSendTicks >= ResendMs * TicksPerMs)
                        {
                            m.LastSendTicks = now;
                            m.Tries++;
                            if (m.Tries > 60) { _reliableInFlight.RemoveAt(i); continue; } // give up after ~9s
                            _reliableInFlight[i] = m;
                            toSend.Add(m);
                        }
                    }
                }
                if (toSend != null)
                    foreach (var m in toSend)
                        try { _udp.Send(m.Frame, m.Frame.Length, ep); } catch { }

                // keepalive
                if (now - lastKeepAlive >= KeepAliveMs * TicksPerMs)
                {
                    lastKeepAlive = now;
                    if (now - LastReceiveTicks > TimeoutMs * TicksPerMs)
                    {
                        Debug.LogWarning("[KTC-Coop] Timeout, disconnecting.");
                        Inbox.Enqueue(new NetMsg { Op = Op.Timeouted, Data = Array.Empty<byte>(), Reliable = false });
                        LastReceiveTicks = now; // fire once
                    }
                    else
                    {
                        SendUnreliable(Op.Ping, Array.Empty<byte>());
                    }
                }
            }
        }

        private static bool SameEp(IPEndPoint a, IPEndPoint b)
        {
            return a != null && b != null && a.Port == b.Port && a.Address.Equals(b.Address);
        }

        public void Dispose()
        {
            _running = false;
            try { _udp?.Close(); } catch { }
            try { _rxThread?.Join(500); } catch { }
            try { _txThread?.Join(500); } catch { }
        }
    }

    public static class Op
    {
        public const byte Ack = 1;
        public const byte Ping = 2;
        public const byte Timeouted = 3;
        public const byte Hello = 4;
        public const byte Welcome = 5;
        public const byte PlayerState = 6;
        public const byte EnemyStates = 7;
        public const byte Fire = 8;
        public const byte EnemyFire = 9;
        public const byte Spawn = 10;
        public const byte Death = 11;
        public const byte HitReq = 12;
        public const byte DamagePlayer = 13;
        public const byte ShowdownGain = 14;
        public const byte Global = 15;
        public const byte Scene = 16;
        public const byte GameOver = 17;
        public const byte ClientDied = 18;
        public const byte Mods = 19;
        public const byte Bye = 20;
        public const byte GunAnim = 21;
        public const byte HostState = 22;
        public const byte HostFire = 23;
        public const byte Revive = 24;
    }
}
