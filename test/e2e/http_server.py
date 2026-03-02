#!/usr/bin/env python3
"""Lightweight HTTP test server for aria2 e2e tests.

Features:
  - Serves files from --dir
  - Range request support (Accept-Ranges, 206 Partial Content)
  - Redirect mode (301/302)
  - Basic auth
  - Digest response header (sha-256)
  - SSL/TLS (--ssl --certfile --keyfile)
  - Fail-first-N requests per path (--fail-first-n)
  - Set-Cookie / Require-Cookie (--set-cookie, --require-cookie)
  - Last-Modified + 304 (--last-modified)
  - Response delay (--delay)
  - Request logging to JSONL (--log-requests)
  - Require-header (--require-header "Name: Value")
  - Content-Disposition (--content-disposition FILENAME)
  - Gzip response (--gzip)
  - Chunked transfer encoding (--chunked)
  - Custom Link header (--link-header)
  - Custom Digest header (--digest-header)
  - /health endpoint for readiness checks
  - /shutdown endpoint for clean shutdown
"""

import argparse
import base64
import gzip
import hashlib
import json
import os
import signal
import ssl
import sys
import threading
import time
from http.server import HTTPServer, BaseHTTPRequestHandler


class ReuseHTTPServer(HTTPServer):
    allow_reuse_address = True


class TestHandler(BaseHTTPRequestHandler):
    serve_dir = "."
    auth_credentials = None    # "user:pass"
    redirect_target = None     # URL to redirect to
    checksum_header = None     # "sha-256:<base64>"
    fail_first_n = 0           # Return 503 for first N requests per path
    set_cookie = None          # "NAME=VAL"
    require_cookie = None      # "NAME=VAL"
    last_modified = None       # Date string for Last-Modified header
    delay_secs = 0             # Seconds to sleep before response body
    log_requests_file = None   # Path to JSONL log file
    require_header = None      # "Name: Value"
    content_disposition = None # filename for Content-Disposition
    gzip_response = False      # Gzip-compress response body
    chunked_response = False   # Send chunked transfer encoding
    link_header = None         # Link header value
    digest_header = None       # Digest header value (overrides checksum)
    disconnect_after_bytes = 0 # Close connection after N body bytes (once)
    max_requests = 0           # Shut down server after N total requests
    _fail_counts = {}          # per-path request counters
    _fail_counts_lock = threading.Lock()
    _request_count = 0
    _request_count_lock = threading.Lock()
    _disconnect_fired = False
    _disconnect_lock = threading.Lock()

    def log_message(self, format, *args):
        pass  # suppress request logging

    def _log_request(self):
        if self.log_requests_file:
            entry = {
                "method": self.command,
                "path": self.path,
                "range": self.headers.get("Range", ""),
                "user_agent": self.headers.get("User-Agent", ""),
                "referer": self.headers.get("Referer", ""),
                "headers": dict(self.headers),
            }
            with open(self.log_requests_file, "a") as f:
                f.write(json.dumps(entry) + "\n")

    def do_GET(self):
        self._log_request()

        # Max-requests check: shut down server after N total requests
        if self.max_requests > 0:
            with self._request_count_lock:
                self._request_count += 1
                if self._request_count > self.max_requests:
                    self._respond(503, b"Server shutting down")
                    threading.Thread(target=self.server.shutdown).start()
                    return

        if self.path == "/health":
            self._respond(200, b"ok")
            return

        if self.path == "/shutdown":
            self._respond(200, b"shutting down")
            threading.Thread(target=self.server.shutdown).start()
            return

        # Fail-first-N check
        if self.fail_first_n > 0:
            with self._fail_counts_lock:
                count = self._fail_counts.get(self.path, 0)
                self._fail_counts[self.path] = count + 1
            if count < self.fail_first_n:
                self._respond(503, b"Service Unavailable")
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

        # Require-Cookie check
        if self.require_cookie:
            cookie_header = self.headers.get("Cookie", "")
            req_name, req_val = self.require_cookie.split("=", 1)
            found = False
            for part in cookie_header.split(";"):
                part = part.strip()
                if "=" in part:
                    n, v = part.split("=", 1)
                    if n.strip() == req_name and v.strip() == req_val:
                        found = True
                        break
            if not found:
                self._respond(403, b"Forbidden: missing required cookie")
                return

        # Require-header check
        if self.require_header:
            rh_name, rh_val = self.require_header.split(":", 1)
            rh_name = rh_name.strip()
            rh_val = rh_val.strip()
            actual = self.headers.get(rh_name, "")
            if actual.strip() != rh_val:
                self._respond(403, b"Forbidden: missing required header")
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

        # Conditional GET: If-Modified-Since
        if self.last_modified:
            ims = self.headers.get("If-Modified-Since")
            if ims:
                from email.utils import parsedate_to_datetime
                try:
                    ims_dt = parsedate_to_datetime(ims)
                    lm_dt = parsedate_to_datetime(self.last_modified)
                    if lm_dt <= ims_dt:
                        self.send_response(304)
                        self.send_header("Content-Length", "0")
                        self.end_headers()
                        return
                except (ValueError, TypeError):
                    pass

        with open(filepath, "rb") as f:
            data = f.read()

        total = len(data)
        range_header = self.headers.get("Range")

        extra = []
        if self.set_cookie:
            extra.append(("Set-Cookie", self.set_cookie))
        if self.last_modified:
            extra.append(("Last-Modified", self.last_modified))
        if self.content_disposition:
            extra.append((
                "Content-Disposition",
                f'attachment; filename="{self.content_disposition}"'))
        if self.link_header:
            extra.append(("Link", self.link_header))
        if self.digest_header:
            extra.append(("Digest", self.digest_header))

        # Gzip compress if requested and client accepts
        if self.gzip_response:
            ae = self.headers.get("Accept-Encoding", "")
            if "gzip" in ae:
                data = gzip.compress(data)
                extra.append(("Content-Encoding", "gzip"))
                total = len(data)

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
                for k, v in extra:
                    self.send_header(k, v)
                self.end_headers()
                if self.delay_secs > 0:
                    time.sleep(self.delay_secs)
                self._write_body(chunk)
                return
            except (ValueError, IndexError):
                pass

        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        if self.chunked_response:
            self.send_header("Transfer-Encoding", "chunked")
        else:
            self.send_header("Content-Length", str(total))
        self.send_header("Accept-Ranges", "bytes")
        if self.checksum_header:
            self._add_digest_header(data)
        for k, v in extra:
            self.send_header(k, v)
        self.end_headers()
        if self.delay_secs > 0:
            time.sleep(self.delay_secs)
        if self.chunked_response:
            self._write_chunked(data)
        else:
            self._write_body(data)

    def _write_body(self, data):
        """Write response body, honoring disconnect-after-bytes (once)."""
        if self.disconnect_after_bytes > 0:
            with self._disconnect_lock:
                if not self._disconnect_fired:
                    self._disconnect_fired = True
                    partial = data[:self.disconnect_after_bytes]
                    try:
                        self.wfile.write(partial)
                        self.wfile.flush()
                    except Exception:
                        pass
                    self.connection.close()
                    return
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def _write_chunked(self, data, chunk_size=8192):
        """Write response body using chunked transfer encoding."""
        for offset in range(0, len(data), chunk_size):
            chunk = data[offset:offset + chunk_size]
            self.wfile.write(f"{len(chunk):x}\r\n".encode("ascii"))
            self.wfile.write(chunk)
            self.wfile.write(b"\r\n")
        self.wfile.write(b"0\r\n\r\n")

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
        self._log_request()

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
        if self.last_modified:
            self.send_header("Last-Modified", self.last_modified)
        if self.link_header:
            self.send_header("Link", self.link_header)
        if self.digest_header:
            self.send_header("Digest", self.digest_header)
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
    parser.add_argument("--ssl", action="store_true",
                        help="Enable SSL/TLS")
    parser.add_argument("--certfile", default=None,
                        help="Path to SSL certificate file")
    parser.add_argument("--keyfile", default=None,
                        help="Path to SSL private key file")
    parser.add_argument("--fail-first-n", type=int, default=0,
                        help="Return 503 for first N requests per path")
    parser.add_argument("--set-cookie", default=None,
                        help="Add Set-Cookie header (NAME=VAL)")
    parser.add_argument("--require-cookie", default=None,
                        help="Require Cookie header (NAME=VAL)")
    parser.add_argument("--last-modified", default=None,
                        help="Send Last-Modified header, honor "
                             "If-Modified-Since with 304")
    parser.add_argument("--delay", type=float, default=0,
                        help="Sleep SECS before sending response body")
    parser.add_argument("--log-requests", default=None,
                        help="Log each request as JSONL to FILE")
    parser.add_argument("--require-header", default=None,
                        help="Require header 'Name: Value', else 403")
    parser.add_argument("--content-disposition", default=None,
                        help="Send Content-Disposition with FILENAME")
    parser.add_argument("--gzip", action="store_true",
                        help="Gzip-compress response body")
    parser.add_argument("--chunked", action="store_true",
                        help="Use chunked transfer encoding")
    parser.add_argument("--link-header", default=None,
                        help="Send Link header with every response")
    parser.add_argument("--digest-header", default=None,
                        help="Send Digest header with every response")
    parser.add_argument("--disconnect-after-bytes", type=int, default=0,
                        help="Close connection after N bytes of body "
                             "(once, then normal)")
    parser.add_argument("--max-requests", type=int, default=0,
                        help="Shut down server after N total requests")
    parser.add_argument("--require-client-cert", default=None,
                        help="CA file to verify client certificates "
                             "(enables mTLS)")
    args = parser.parse_args()

    TestHandler.serve_dir = os.path.abspath(args.dir)
    TestHandler.auth_credentials = args.auth
    TestHandler.redirect_target = args.redirect
    TestHandler.checksum_header = args.checksum
    TestHandler.fail_first_n = args.fail_first_n
    TestHandler.set_cookie = args.set_cookie
    TestHandler.require_cookie = args.require_cookie
    TestHandler.last_modified = args.last_modified
    TestHandler.delay_secs = args.delay
    TestHandler.log_requests_file = args.log_requests
    TestHandler.require_header = args.require_header
    TestHandler.content_disposition = args.content_disposition
    TestHandler.gzip_response = args.gzip
    TestHandler.chunked_response = args.chunked
    TestHandler.link_header = args.link_header
    TestHandler.digest_header = args.digest_header
    TestHandler.disconnect_after_bytes = args.disconnect_after_bytes
    TestHandler.max_requests = args.max_requests
    TestHandler._fail_counts = {}
    TestHandler._request_count = 0
    TestHandler._disconnect_fired = False

    server = ReuseHTTPServer(("127.0.0.1", args.port), TestHandler)

    if args.ssl:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(args.certfile, args.keyfile)
        if args.require_client_cert:
            ctx.verify_mode = ssl.CERT_REQUIRED
            ctx.load_verify_locations(args.require_client_cert)
        server.socket = ctx.wrap_socket(server.socket,
                                        server_side=True)

    signal.signal(signal.SIGTERM, lambda *_: server.shutdown())
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()


if __name__ == "__main__":
    main()
