// Line-delimited JSON over TCP on 127.0.0.1. Each request {"cmd": "...", "args": {...}} runs on Unity's main thread;
// the reply is {"ok": true, ...} or {"ok": false, "error": "..."}. Test-only: listens only when -coopqa-port is given.
using System;
using System.Collections.Concurrent;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using Newtonsoft.Json.Linq;

namespace CoopQa
{
    public class Rpc
    {
        private class Pending
        {
            public JObject Request;
            public JObject Reply;
            public readonly ManualResetEventSlim Done = new ManualResetEventSlim(false);
        }

        private readonly ConcurrentQueue<Pending> _queue = new ConcurrentQueue<Pending>();
        private readonly Func<string, JObject, JObject> _handler;
        private readonly Action<string> _log;
        private TcpListener _listener;

        public Rpc(int port, Func<string, JObject, JObject> handler, Action<string> log)
        {
            _handler = handler;
            _log = log;
            _listener = new TcpListener(IPAddress.Loopback, port);
            _listener.Start();
            new Thread(AcceptLoop) { IsBackground = true, Name = "coopqa-rpc" }.Start();
            _log($"rpc listening on 127.0.0.1:{port}");
        }

        private void AcceptLoop()
        {
            while (true)
            {
                TcpClient client;
                try { client = _listener.AcceptTcpClient(); }
                catch (Exception e) { _log("rpc accept stopped: " + e.Message); return; }
                new Thread(() => Serve(client)) { IsBackground = true, Name = "coopqa-rpc-client" }.Start();
            }
        }

        private void Serve(TcpClient client)
        {
            using (client)
            using (var stream = client.GetStream())
            using (var reader = new StreamReader(stream, new UTF8Encoding(false)))
            using (var writer = new StreamWriter(stream, new UTF8Encoding(false)) { AutoFlush = true, NewLine = "\n" })
            {
                string line;
                while ((line = SafeRead(reader)) != null)
                {
                    JObject reply;
                    try
                    {
                        var p = new Pending { Request = JObject.Parse(line) };
                        _queue.Enqueue(p);
                        reply = p.Done.Wait(TimeSpan.FromSeconds(20))
                            ? p.Reply
                            : new JObject { ["ok"] = false, ["error"] = "timeout: main thread did not answer in 20 s" };
                    }
                    catch (Exception e)
                    {
                        reply = new JObject { ["ok"] = false, ["error"] = "bad request: " + e.Message };
                    }
                    try { writer.WriteLine(reply.ToString(Newtonsoft.Json.Formatting.None)); }
                    catch (IOException) { return; }
                }
            }
        }

        private static string SafeRead(StreamReader r)
        {
            try { return r.ReadLine(); } catch (IOException) { return null; }
        }

        // Call from Update.
        public void Pump()
        {
            while (_queue.TryDequeue(out var p))
            {
                try
                {
                    var cmd = (string)p.Request["cmd"] ?? "";
                    var args = p.Request["args"] as JObject ?? new JObject();
                    p.Reply = _handler(cmd, args) ?? new JObject();
                    if (p.Reply["ok"] == null) p.Reply["ok"] = true;
                }
                catch (Exception e)
                {
                    p.Reply = new JObject { ["ok"] = false, ["error"] = e.GetType().Name + ": " + e.Message };
                }
                p.Done.Set();
            }
        }
    }
}
