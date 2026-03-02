#!/usr/bin/env python3
"""Minimal WebSocket JSON-RPC client for aria2 e2e tests.

Uses only Python stdlib (no websockets library needed).
Performs: TCP connect -> HTTP upgrade -> send masked text frame -> read response.

Usage:
  python3 ws_client.py --port PORT --method METHOD [--params JSON]
  python3 ws_client.py --port PORT --method METHOD --wait-notifications 5
  python3 ws_client.py --port PORT --method METHOD --ssl --no-verify
  python3 ws_client.py --port PORT --method METHOD --secret TOKEN
"""

import argparse
import base64
import hashlib
import json
import os
import socket
import ssl
import struct
import sys
import time


def _mask_payload(payload, mask_key):
    """Apply XOR mask to payload bytes."""
    masked = bytearray(len(payload))
    for i, b in enumerate(payload):
        masked[i] = b ^ mask_key[i % 4]
    return bytes(masked)


def _recv_exactly(sock, n):
    """Receive exactly n bytes from socket."""
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("connection closed")
        buf += chunk
    return buf


def _read_frame(sock):
    """Read a single WebSocket frame, return (opcode, payload)."""
    header = _recv_exactly(sock, 2)
    opcode = header[0] & 0x0F
    masked = (header[1] & 0x80) != 0
    length = header[1] & 0x7F

    if length == 126:
        length = struct.unpack("!H", _recv_exactly(sock, 2))[0]
    elif length == 127:
        length = struct.unpack("!Q", _recv_exactly(sock, 8))[0]

    if masked:
        mask_key = _recv_exactly(sock, 4)
        payload = _recv_exactly(sock, length)
        payload = _mask_payload(payload, mask_key)
    else:
        payload = _recv_exactly(sock, length)

    return opcode, payload


def _send_text_frame(sock, text):
    """Send a masked text frame."""
    payload = text.encode("utf-8")
    mask_key = os.urandom(4)
    masked = _mask_payload(payload, mask_key)

    frame = bytearray()
    # FIN + text opcode
    frame.append(0x81)
    length = len(payload)
    if length < 126:
        frame.append(0x80 | length)
    elif length < 65536:
        frame.append(0x80 | 126)
        frame.extend(struct.pack("!H", length))
    else:
        frame.append(0x80 | 127)
        frame.extend(struct.pack("!Q", length))
    frame.extend(mask_key)
    frame.extend(masked)
    sock.sendall(frame)


def _send_close_frame(sock):
    """Send a masked close frame."""
    mask_key = os.urandom(4)
    frame = bytearray()
    frame.append(0x88)  # FIN + close opcode
    frame.append(0x82)  # masked, 2-byte payload
    frame.extend(mask_key)
    # Close code 1000 (normal closure)
    close_payload = struct.pack("!H", 1000)
    frame.extend(_mask_payload(close_payload, mask_key))
    sock.sendall(frame)


def main():
    parser = argparse.ArgumentParser(
        description="WebSocket JSON-RPC client for aria2 e2e tests")
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--method", required=True)
    parser.add_argument("--params", default=None,
                        help="JSON array of parameters")
    parser.add_argument("--timeout", type=int, default=10,
                        help="Socket timeout in seconds")
    parser.add_argument("--wait-notifications", type=float, default=0,
                        help="After RPC call, read notification frames "
                             "for N seconds, print each as JSON line")
    parser.add_argument("--ssl", action="store_true",
                        help="Use wss:// (TLS)")
    parser.add_argument("--no-verify", action="store_true",
                        help="Skip TLS certificate verification")
    parser.add_argument("--secret", default=None,
                        help="RPC secret token (prepended to params)")
    args = parser.parse_args()

    # Build JSON-RPC request
    rpc_req = {
        "jsonrpc": "2.0",
        "id": "ws1",
        "method": args.method,
    }
    params = []
    if args.secret:
        params.append(f"token:{args.secret}")
    if args.params:
        parsed_params = json.loads(args.params)
        if isinstance(parsed_params, list):
            params.extend(parsed_params)
        else:
            params.append(parsed_params)
    if params:
        rpc_req["params"] = params

    # TCP connect
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(args.timeout)
    sock.connect(("127.0.0.1", args.port))

    # Wrap with TLS if --ssl
    if args.ssl:
        ctx = ssl.create_default_context()
        if args.no_verify:
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
        sock = ctx.wrap_socket(sock, server_hostname="127.0.0.1")

    # WebSocket upgrade handshake
    ws_key = base64.b64encode(os.urandom(16)).decode("ascii")
    handshake = (
        f"GET /jsonrpc HTTP/1.1\r\n"
        f"Host: 127.0.0.1:{args.port}\r\n"
        f"Upgrade: websocket\r\n"
        f"Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {ws_key}\r\n"
        f"Sec-WebSocket-Version: 13\r\n"
        f"\r\n"
    )
    sock.sendall(handshake.encode("ascii"))

    # Read HTTP response headers
    response = b""
    while b"\r\n\r\n" not in response:
        chunk = sock.recv(4096)
        if not chunk:
            print("error: connection closed during handshake",
                  file=sys.stderr)
            sys.exit(1)
        response += chunk

    status_line = response.split(b"\r\n")[0].decode("ascii")
    if "101" not in status_line:
        print(f"error: handshake failed: {status_line}", file=sys.stderr)
        sys.exit(1)

    # Verify Sec-WebSocket-Accept
    expected_accept = base64.b64encode(
        hashlib.sha1(
            (ws_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()
        ).digest()
    ).decode("ascii")
    if expected_accept not in response.decode("ascii", errors="replace"):
        print("error: invalid Sec-WebSocket-Accept", file=sys.stderr)
        sys.exit(1)

    # Send JSON-RPC request as text frame
    _send_text_frame(sock, json.dumps(rpc_req))

    # Read response frame
    opcode, payload = _read_frame(sock)
    if opcode == 1:  # text frame
        print(payload.decode("utf-8"))
    elif opcode == 8:  # close frame
        print("error: server sent close frame", file=sys.stderr)
        sys.exit(1)
    else:
        print(f"error: unexpected opcode {opcode}", file=sys.stderr)
        sys.exit(1)

    # If --wait-notifications, keep reading frames for N seconds
    if args.wait_notifications > 0:
        deadline = time.time() + args.wait_notifications
        sock.settimeout(0.5)
        while time.time() < deadline:
            try:
                opcode, payload = _read_frame(sock)
                if opcode == 1:  # text frame
                    print(payload.decode("utf-8"))
                elif opcode == 8:  # close frame
                    break
            except (socket.timeout, ConnectionError, OSError):
                continue

    # Clean close
    _send_close_frame(sock)
    sock.close()


if __name__ == "__main__":
    main()
