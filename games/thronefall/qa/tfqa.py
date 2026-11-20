"""Client for the CoopQaDriver control port (line-delimited JSON over TCP on 127.0.0.1).

    python -I tfqa.py 47801 status
    python -I tfqa.py 47802 build '{"id": 12}'
"""
import json
import socket
import sys


class Conn:
    def __init__(self, port, timeout=25.0):
        self.port = port
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=timeout)
        self.file = self.sock.makefile("rwb")

    def call(self, cmd, **args):
        self.file.write((json.dumps({"cmd": cmd, "args": args}) + "\n").encode("utf-8"))
        self.file.flush()
        line = self.file.readline()
        if not line:
            raise ConnectionError("game on port %d closed the connection" % self.port)
        reply = json.loads(line)
        if not reply.get("ok", False):
            raise RuntimeError("%s on port %d: %s" % (cmd, self.port, reply.get("error")))
        return reply

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    args = json.loads(argv[3]) if len(argv) > 3 else {}
    c = Conn(int(argv[1]))
    try:
        print(json.dumps(c.call(argv[2], **args), indent=1))
    finally:
        c.close()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
