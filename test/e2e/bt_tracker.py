#!/usr/bin/env python3
"""Minimal HTTP-based BitTorrent tracker for aria2 e2e tests.

Always returns a single peer in compact format (6 bytes: 4 IP + 2 port).

Usage:
  python3 bt_tracker.py --port 18135 --peer-ip 127.0.0.1 --peer-port 16881
"""

import argparse
import signal
import socket
import struct
from http.server import HTTPServer, BaseHTTPRequestHandler


def bencode(obj):
    """Bencode a Python object (dict, list, int, bytes)."""
    if isinstance(obj, int):
        return b"i" + str(obj).encode("ascii") + b"e"
    if isinstance(obj, bytes):
        return str(len(obj)).encode("ascii") + b":" + obj
    if isinstance(obj, str):
        return bencode(obj.encode("utf-8"))
    if isinstance(obj, list):
        return b"l" + b"".join(bencode(item) for item in obj) + b"e"
    if isinstance(obj, dict):
        items = sorted(obj.items(), key=lambda kv: kv[0].encode("utf-8")
                       if isinstance(kv[0], str) else kv[0])
        encoded = b"d"
        for k, v in items:
            encoded += bencode(k) + bencode(v)
        return encoded + b"e"
    raise TypeError(f"unsupported type: {type(obj)}")


class TrackerHandler(BaseHTTPRequestHandler):
    peer_ip = "127.0.0.1"
    peer_port = 6881

    def log_message(self, format, *args):
        pass  # suppress request logging

    def do_GET(self):
        if not self.path.startswith("/announce"):
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return

        # Build compact peer: 4 bytes IP + 2 bytes port (big-endian)
        ip_bytes = socket.inet_aton(self.peer_ip)
        port_bytes = struct.pack("!H", self.peer_port)
        peers_compact = ip_bytes + port_bytes

        response = bencode({
            "interval": 1800,
            "peers": peers_compact,
        })

        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(response)))
        self.end_headers()
        self.wfile.write(response)


class ReuseHTTPServer(HTTPServer):
    allow_reuse_address = True


def main():
    parser = argparse.ArgumentParser(
        description="Minimal BitTorrent tracker for e2e tests")
    parser.add_argument("--port", type=int, default=18135)
    parser.add_argument("--peer-ip", default="127.0.0.1",
                        help="IP address to return as the peer")
    parser.add_argument("--peer-port", type=int, default=6881,
                        help="Port to return as the peer")
    args = parser.parse_args()

    TrackerHandler.peer_ip = args.peer_ip
    TrackerHandler.peer_port = args.peer_port

    server = ReuseHTTPServer(("127.0.0.1", args.port), TrackerHandler)
    signal.signal(signal.SIGTERM, lambda *_: server.shutdown())
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()


if __name__ == "__main__":
    main()
