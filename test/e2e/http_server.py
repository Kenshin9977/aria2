#!/usr/bin/env python3
"""Lightweight HTTP test server for aria2 e2e tests.

Features:
  - Serves files from --dir
  - Range request support (Accept-Ranges, 206 Partial Content)
  - Redirect mode (301/302)
  - Basic auth
  - Digest response header (sha-256)
  - /health endpoint for readiness checks
  - /shutdown endpoint for clean shutdown
"""

import argparse
import base64
import hashlib
import os
import signal
import sys
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler


class ReuseHTTPServer(HTTPServer):
    allow_reuse_address = True


class TestHandler(BaseHTTPRequestHandler):
    serve_dir = "."
    auth_credentials = None    # "user:pass"
    redirect_target = None     # URL to redirect to
    checksum_header = None     # "sha-256:<base64>"

    def log_message(self, format, *args):
        pass  # suppress request logging

    def do_GET(self):
        if self.path == "/health":
            self._respond(200, b"ok")
            return

        if self.path == "/shutdown":
            self._respond(200, b"shutting down")
            threading.Thread(target=self.server.shutdown).start()
            return

        # Auth check
        if self.auth_credentials:
            auth_header = self.headers.get("Authorization", "")
            if not auth_header.startswith("Basic "):
                self._respond(401, b"Unauthorized",
                              [("WWW-Authenticate",
                                'Basic realm="aria2 test"')])
                return
            decoded = base64.b64decode(
                auth_header[6:]).decode("utf-8", errors="replace")
            if decoded != self.auth_credentials:
                self._respond(401, b"Unauthorized",
                              [("WWW-Authenticate",
                                'Basic realm="aria2 test"')])
                return

        # Redirect mode
        if self.redirect_target and self.path != "/redirected":
            status = 301
            self.send_response(status)
            self.send_header("Location", self.redirect_target)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return

        # Serve file
        path = self.path.lstrip("/")
        if not path or path == "redirected":
            path = "redirected"
        filepath = os.path.join(self.serve_dir, path)
        if not os.path.isfile(filepath):
            self._respond(404, b"Not Found")
            return

        with open(filepath, "rb") as f:
            data = f.read()

        total = len(data)
        range_header = self.headers.get("Range")

        if range_header and range_header.startswith("bytes="):
            try:
                range_spec = range_header[6:]
                start_str, end_str = range_spec.split("-", 1)
                start = int(start_str) if start_str else 0
                end = int(end_str) if end_str else total - 1
                end = min(end, total - 1)
                chunk = data[start:end + 1]
                self.send_response(206)
                self.send_header("Content-Type",
                                 "application/octet-stream")
                self.send_header("Content-Length", str(len(chunk)))
                self.send_header(
                    "Content-Range",
                    f"bytes {start}-{end}/{total}")
                self.send_header("Accept-Ranges", "bytes")
                if self.checksum_header:
                    self._add_digest_header(data)
                self.end_headers()
                self.wfile.write(chunk)
                return
            except (ValueError, IndexError):
                pass

        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(total))
        self.send_header("Accept-Ranges", "bytes")
        if self.checksum_header:
            self._add_digest_header(data)
        self.end_headers()
        self.wfile.write(data)

    def _add_digest_header(self, data):
        digest = hashlib.sha256(data).digest()
        b64 = base64.b64encode(digest).decode("ascii")
        self.send_header("Digest", f"sha-256={b64}")

    def _respond(self, code, body, extra_headers=None):
        self.send_response(code)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        if extra_headers:
            for k, v in extra_headers:
                self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def do_HEAD(self):
        # Needed for aria2 to probe file size
        path = self.path.lstrip("/")
        filepath = os.path.join(self.serve_dir, path)
        if not os.path.isfile(filepath):
            self._respond(404, b"Not Found")
            return
        total = os.path.getsize(filepath)
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(total))
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()


def main():
    parser = argparse.ArgumentParser(
        description="aria2 e2e test HTTP server")
    parser.add_argument("--port", type=int, default=18080)
    parser.add_argument("--dir", default=".")
    parser.add_argument("--auth", default=None,
                        help="user:pass for basic auth")
    parser.add_argument("--redirect", default=None,
                        help="URL to redirect to")
    parser.add_argument("--checksum", default=None,
                        help="Enable Digest header (sha-256)")
    args = parser.parse_args()

    TestHandler.serve_dir = os.path.abspath(args.dir)
    TestHandler.auth_credentials = args.auth
    TestHandler.redirect_target = args.redirect
    TestHandler.checksum_header = args.checksum

    server = ReuseHTTPServer(("127.0.0.1", args.port), TestHandler)
    signal.signal(signal.SIGTERM, lambda *_: server.shutdown())
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()


if __name__ == "__main__":
    main()
