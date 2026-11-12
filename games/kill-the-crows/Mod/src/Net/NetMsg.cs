using System.Collections.Concurrent;

namespace KtcCoop.Net
{
    public struct NetMsg
    {
        public byte Op;
        public byte[] Data;
        public bool Reliable;
    }

    public interface INetChannel
    {
        bool Connected { get; }
        ConcurrentQueue<NetMsg> Inbox { get; }
        void SendUnreliable(byte op, byte[] payload);
        void SendReliable(byte op, byte[] payload);
    }
}
