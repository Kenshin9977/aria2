#!/usr/bin/env python3
"""Minimal SOCKS5 proxy server for aria2 e2e tests.

Implements RFC 1928 (SOCKS5) + RFC 1929 (username/password auth).

Features:
  - No-auth and username/password authentication
  - CONNECT command with bidirectional relay
  - ATYP 0x01 (IPv4), 0x03 (domain), 0x04 (IPv6)
  - Request logging to JSONL (--log-requests FILE)
  - SIGTERM handler for clean shutdown
  - Threading for concurrent connections

Usage:
  python3 socks5_server.py --port 18210
  python3 socks5_server.py --port 18210 --auth user:pass
  python3 socks5_server.py --port 18210 --log-requests /tmp/socks5.log
"""

import argparse
import json
import os
import signal
import socket
import socketserver
import struct
import sys
import threading


class SOCKS5Handler(socketserver.StreamRequestHandler):
    """Handles a single SOCKS5 connection."""

    auth_credentials = None   # "user:pass" or None for no-auth
    log_requests_file = None  # path to JSONL log file

    def handle(self):
        try:
            if not self._negotiate_auth():
                return
            self._handle_request()
        except (ConnectionResetError, BrokenPipeError, OSError):
            pass

    def _negotiate_auth(self):
        """SOCKS5 greeting and authentication negotiation."""
        # Client greeting: VER NMETHODS METHODS...
        header = self._recv(2)
        if not header or header[0] != 0x05:
            return False

        nmethods = header[1]
        methods = self._recv(nmethods)
        if not methods:
            return False

        if self.auth_credentials:
            # Require username/password auth (method 0x02)
            if 0x02 not in methods:
                # No acceptable methods
                self.request.sendall(b"\x05\xFF")
                return False
            # Select username/password auth
            self.request.sendall(b"\x05\x02")
            return self._do_userpass_auth()
        else:
            # No auth required (method 0x00)
            if 0x00 not in methods:
                self.request.sendall(b"\x05\xFF")
                return False
            self.request.sendall(b"\x05\x00")
            return True

    def _do_userpass_auth(self):
        """RFC 1929 username/password sub-negotiation."""
        # VER ULEN USER PLEN PASS
        ver = self._recv(1)
        if not ver or ver[0] != 0x01:
            return False

        ulen = self._recv(1)
        if not ulen:
            return False
        username = self._recv(ulen[0])
        if not username:
            return False

        plen = self._recv(1)
        if not plen:
            return False
        password = self._recv(plen[0])
        if not password:
            return False

        cred = username.decode("utf-8", errors="replace") + ":" + \
            password.decode("utf-8", errors="replace")

        if cred == self.auth_credentials:
            # Success
            self.request.sendall(b"\x01\x00")
            return True
        else:
            # Failure
            self.request.sendall(b"\x01\x01")
            return False

    def _handle_request(self):
        """Handle SOCKS5 CONNECT request."""
        # VER CMD RSV ATYP DST.ADDR DST.PORT
        header = self._recv(4)
        if not header or len(header) < 4:
            return

        ver, cmd, _, atyp = header[0], header[1], header[2], header[3]
        if ver != 0x05:
            return

        if cmd != 0x01:
            # Only CONNECT supported
            self._send_reply(0x07)  # Command not supported
            return

        # Parse destination address
        if atyp == 0x01:
            # IPv4: 4 bytes
            addr_bytes = self._recv(4)
            if not addr_bytes:
                return
            dst_addr = socket.inet_ntoa(addr_bytes)
        elif atyp == 0x03:
            # Domain name: 1-byte length + name
            addr_len = self._recv(1)
            if not addr_len:
                return
            addr_bytes = self._recv(addr_len[0])
            if not addr_bytes:
                return
            dst_addr = addr_bytes.decode("utf-8", errors="replace")
        elif atyp == 0x04:
            # IPv6: 16 bytes
            addr_bytes = self._recv(16)
            if not addr_bytes:
                return
            dst_addr = socket.inet_ntop(socket.AF_INET6, addr_bytes)
        else:
            self._send_reply(0x08)  # Address type not supported
            return

        # Parse port (2 bytes, big-endian)
        port_bytes = self._recv(2)
        if not port_bytes:
            return
        dst_port = struct.unpack("!H", port_bytes)[0]

        # Log the request
        self._log_request(dst_addr, dst_port)

        # Connect to destination
        try:
            upstream = socket.create_connection(
                (dst_addr, dst_port), timeout=30)
        except Exception:
            self._send_reply(0x05)  # Connection refused
            return

        # Success reply
        self._send_reply(0x00)

        # Bidirectional relay
        client_sock = self.request
        self._relay(client_sock, upstream)

        try:
            upstream.close()
        except OSError:
            pass

    def _send_reply(self, status):
        """Send SOCKS5 reply with given status code."""
        # VER REP RSV ATYP BND.ADDR BND.PORT
        reply = struct.pack("!BBBB", 0x05, status, 0x00, 0x01)
        reply += socket.inet_aton("0.0.0.0")
        reply += struct.pack("!H", 0)
        try:
            self.request.sendall(reply)
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass

    def _relay(self, client, upstream):
        """Bidirectional relay between client and upstream sockets."""
        def forward(src, dst):
            try:
                while True:
                    data = src.recv(4096)
                    if not data:
                        break
                    dst.sendall(data)
            except (OSError, BrokenPipeError):
                pass
            finally:
                try:
                    dst.shutdown(socket.SHUT_WR)
                except OSError:
                    pass

        t1 = threading.Thread(
            target=forward, args=(client, upstream), daemon=True)
        t2 = threading.Thread(
            target=forward, args=(upstream, client), daemon=True)
        t1.start()
        t2.start()
        t1.join(timeout=120)
        t2.join(timeout=120)

    def _recv(self, n):
        """Receive exactly n bytes."""
        buf = b""
        while len(buf) < n:
            try:
                chunk = self.request.recv(n - len(buf))
            except (ConnectionResetError, OSError):
                return None
            if not chunk:
                return None
            buf += chunk
        return buf

    def _log_request(self, addr, port):
        """Log a CONNECT request to JSONL file."""
        if self.log_requests_file:
            entry = {
                "method": "CONNECT",
                "target": f"{addr}:{port}",
            }
            with open(self.log_requests_file, "a") as f:
                f.write(json.dumps(entry) + "\n")


class ThreadingSOCKS5Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    parser = argparse.ArgumentParser(
        description="Minimal SOCKS5 proxy for aria2 e2e tests")
    parser.add_argument("--port", type=int, default=18210)
    parser.add_argument("--auth", default=None,
                        help="user:pass for SOCKS5 auth")
    parser.add_argument("--log-requests", default=None,
                        help="Log each CONNECT as JSONL to FILE")
    args = parser.parse_args()

    SOCKS5Handler.auth_credentials = args.auth
    SOCKS5Handler.log_requests_file = args.log_requests

    server = ThreadingSOCKS5Server(
        ("127.0.0.1", args.port), SOCKS5Handler)

    def shutdown_handler(*_):
        threading.Thread(target=server.shutdown, daemon=True).start()

    signal.signal(signal.SIGTERM, shutdown_handler)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()


if __name__ == "__main__":
    main()
