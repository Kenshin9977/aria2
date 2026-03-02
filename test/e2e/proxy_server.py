#!/usr/bin/env python3
"""Minimal HTTP forward/CONNECT proxy for aria2 e2e tests.

Features:
  - Forward proxy for HTTP (GET/HEAD/POST with full URL)
  - CONNECT tunnel for HTTPS (bidirectional relay via threading)
  - Optional basic auth (--auth user:pass, 407 on failure)
  - Health check endpoint (GET /health -> 200 "ok")
  - Request logging to JSONL (--log-requests FILE)
  - SIGTERM handler for clean shutdown
  - Binds to 127.0.0.1 only
"""

import argparse
import base64
import json
import select
import signal
import socket
import socketserver
import sys
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse
from http.client import HTTPConnection


class ReuseHTTPServer(HTTPServer):
    allow_reuse_address = True


class ThreadingHTTPServer(socketserver.ThreadingMixIn, ReuseHTTPServer):
    daemon_threads = True


class ProxyHandler(BaseHTTPRequestHandler):
    auth_credentials = None   # "user:pass"
    log_requests_file = None  # path to JSONL log file

    def log_message(self, format, *args):
        pass  # suppress default stderr logging

    def _log_request(self, method, target):
        if self.log_requests_file:
            entry = {"method": method, "url": target}
            with open(self.log_requests_file, "a") as f:
                f.write(json.dumps(entry) + "\n")

    def _check_auth(self):
        """Return True if auth passes (or no auth required)."""
        if not self.auth_credentials:
            return True
        auth_header = self.headers.get("Proxy-Authorization", "")
        if not auth_header.startswith("Basic "):
            self._respond(407, b"Proxy Authentication Required",
                          [("Proxy-Authenticate",
                            'Basic realm="aria2 proxy"')])
            return False
        try:
            decoded = base64.b64decode(
                auth_header[6:]).decode("utf-8", errors="replace")
        except Exception:
            decoded = ""
        if decoded != self.auth_credentials:
            self._respond(407, b"Proxy Authentication Required",
                          [("Proxy-Authenticate",
                            'Basic realm="aria2 proxy"')])
            return False
        return True

    def _respond(self, code, body, extra_headers=None):
        self.send_response(code)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        if extra_headers:
            for k, v in extra_headers:
                self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def _is_health_check(self):
        """Handle direct GET /health (not a proxy request)."""
        if self.path == "/health":
            self._respond(200, b"ok")
            return True
        return False

    def _forward_request(self, method):
        """Forward an HTTP proxy request to the upstream server."""
        if self._is_health_check():
            return
        if not self._check_auth():
            return

        self._log_request(method, self.path)

        parsed = urlparse(self.path)
        host = parsed.hostname
        port = parsed.port or 80
        path = parsed.path or "/"
        if parsed.query:
            path = path + "?" + parsed.query

        # Read request body if present
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length) if content_length else None

        try:
            upstream = HTTPConnection(host, port, timeout=30)
            # Build headers, stripping hop-by-hop and proxy headers
            fwd_headers = {}
            skip = {"proxy-authorization", "proxy-connection",
                    "connection", "keep-alive", "host"}
            for key in self.headers:
                if key.lower() not in skip:
                    fwd_headers[key] = self.headers[key]
            fwd_headers["Host"] = parsed.netloc
            fwd_headers["Connection"] = "close"

            upstream.request(method, path, body=body,
                             headers=fwd_headers)
            resp = upstream.getresponse()
            resp_body = resp.read()

            self.send_response(resp.status)
            skip_resp = {"transfer-encoding", "connection",
                         "keep-alive"}
            for key, val in resp.getheaders():
                if key.lower() not in skip_resp:
                    self.send_header(key, val)
            self.send_header("Content-Length", str(len(resp_body)))
            self.end_headers()
            self.wfile.write(resp_body)
            upstream.close()
        except Exception as exc:
            self._respond(502, f"Bad Gateway: {exc}".encode())

    def do_GET(self):
        self._forward_request("GET")

    def do_HEAD(self):
        self._forward_request("HEAD")

    def do_POST(self):
        self._forward_request("POST")

    def do_CONNECT(self):
        """Handle CONNECT method for HTTPS tunneling."""
        if not self._check_auth():
            return

        self._log_request("CONNECT", self.path)

        # Parse host:port from CONNECT target
        if ":" in self.path:
            host, port_str = self.path.rsplit(":", 1)
            try:
                port = int(port_str)
            except ValueError:
                self._respond(400, b"Bad port")
                return
        else:
            host = self.path
            port = 443

        try:
            upstream = socket.create_connection((host, port), timeout=30)
        except Exception as exc:
            self._respond(502, f"Bad Gateway: {exc}".encode())
            return

        # Send 200 Connection Established
        self.send_response(200, "Connection Established")
        self.end_headers()

        client_sock = self.connection

        # Replace the buffered rfile/wfile wrappers with dummies so
        # they don't interfere with raw socket I/O during the relay.
        # We can't close them since handle_one_request() calls
        # wfile.flush() after do_CONNECT() returns.
        import io
        self.wfile.flush()
        self.rfile = io.BytesIO()
        self.wfile = io.BytesIO()
        self.close_connection = True

        # Bidirectional relay using threads
        def relay(src, dst, name):
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

        t_client = threading.Thread(
            target=relay, args=(client_sock, upstream, "c->s"),
            daemon=True)
        t_upstream = threading.Thread(
            target=relay, args=(upstream, client_sock, "s->c"),
            daemon=True)
        t_client.start()
        t_upstream.start()
        t_client.join(timeout=120)
        t_upstream.join(timeout=120)

        try:
            upstream.close()
        except OSError:
            pass


def main():
    parser = argparse.ArgumentParser(
        description="aria2 e2e test HTTP forward/CONNECT proxy")
    parser.add_argument("--port", type=int, default=18120)
    parser.add_argument("--auth", default=None,
                        help="user:pass for proxy basic auth")
    parser.add_argument("--log-requests", default=None,
                        help="Log each request as JSONL to FILE")
    args = parser.parse_args()

    ProxyHandler.auth_credentials = args.auth
    ProxyHandler.log_requests_file = args.log_requests

    server = ThreadingHTTPServer(("127.0.0.1", args.port), ProxyHandler)

    def shutdown_handler(signum, frame):
        threading.Thread(target=server.shutdown, daemon=True).start()

    signal.signal(signal.SIGTERM, shutdown_handler)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()


if __name__ == "__main__":
    main()
