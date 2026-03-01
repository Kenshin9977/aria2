#!/usr/bin/env python3
"""Minimal FTP test server for aria2 e2e tests.

Features:
  - Serves files from --dir
  - Anonymous access (default) or --user/--passwd authentication
  - PASV data transfers with configurable port range
  - REST (resume) support for RETR
  - SIZE and MDTM commands
  - LIST directory listing
  - FEAT capability advertisement
  - EPSV stubbed (returns 500 to force PASV fallback)
  - SIGTERM handler for clean shutdown
  - Threading for concurrent connections
"""

import argparse
import os
import signal
import socket
import socketserver
import sys
import threading
import time


class FTPHandler(socketserver.StreamRequestHandler):
    """Handles a single FTP control connection."""

    serve_dir = "."
    required_user = None
    required_passwd = None
    pasv_range_start = 18101
    pasv_range_end = 18110
    _pasv_port_lock = threading.Lock()
    _next_pasv_port = 18101

    def setup(self):
        super().setup()
        self.cwd = "/"
        self.authenticated = False
        self.supplied_user = None
        self.transfer_type = "A"
        self.rest_offset = 0
        self.data_socket = None
        self.data_listener = None

    def handle(self):
        try:
            self._send("220 aria2 test FTP server ready")
            while True:
                try:
                    line = self.rfile.readline()
                except (ConnectionResetError, BrokenPipeError):
                    break
                if not line:
                    break
                line = line.decode("utf-8", errors="replace").rstrip("\r\n")
                if not line:
                    continue
                parts = line.split(None, 1)
                cmd = parts[0].upper()
                arg = parts[1] if len(parts) > 1 else ""
                handler = getattr(self, f"cmd_{cmd}", None)
                if handler:
                    handler(arg)
                else:
                    self._send(f"502 Command {cmd} not implemented")
        except (ConnectionResetError, BrokenPipeError, OSError):
            pass
        finally:
            self._close_data()

    def finish(self):
        self._close_data()
        try:
            super().finish()
        except OSError:
            pass

    # ------------------------------------------------------------------
    # FTP command handlers
    # ------------------------------------------------------------------

    def cmd_USER(self, arg):
        self.supplied_user = arg
        if self.required_user is None:
            # Anonymous mode: accept any user
            self.authenticated = True
            self._send("230 User logged in")
        elif arg == self.required_user:
            self._send("331 Password required")
        else:
            self._send("530 Invalid user")

    def cmd_PASS(self, arg):
        if self.required_user is None:
            # Anonymous mode
            self.authenticated = True
            self._send("230 User logged in")
            return
        if self.supplied_user != self.required_user:
            self._send("530 Login incorrect")
            return
        if self.required_passwd is None or arg == self.required_passwd:
            self.authenticated = True
            self._send("230 User logged in")
        else:
            self._send("530 Login incorrect")

    def cmd_SYST(self, arg):
        self._send("215 UNIX Type: L8")

    def cmd_TYPE(self, arg):
        t = arg.strip().upper()
        if t in ("A", "I"):
            self.transfer_type = t
            self._send(f"200 Type set to {t}")
        else:
            self._send("504 Type not supported")

    def cmd_PWD(self, arg):
        self._send(f'257 "{self.cwd}" is current directory')

    def cmd_CWD(self, arg):
        if not self._check_auth():
            return
        target = self._resolve_path(arg)
        real = self._real_path(target)
        if os.path.isdir(real):
            self.cwd = target
            self._send(f'250 CWD successful. "{self.cwd}"')
        else:
            self._send("550 Directory not found")

    def cmd_SIZE(self, arg):
        if not self._check_auth():
            return
        path = self._resolve_path(arg)
        real = self._real_path(path)
        if os.path.isfile(real):
            self._send(f"213 {os.path.getsize(real)}")
        else:
            self._send("550 File not found")

    def cmd_MDTM(self, arg):
        if not self._check_auth():
            return
        path = self._resolve_path(arg)
        real = self._real_path(path)
        if os.path.isfile(real):
            mtime = os.path.getmtime(real)
            ts = time.strftime("%Y%m%d%H%M%S", time.gmtime(mtime))
            self._send(f"213 {ts}")
        else:
            self._send("550 File not found")

    def cmd_PASV(self, arg):
        if not self._check_auth():
            return
        self._close_data()
        port = self._alloc_pasv_port()
        if port is None:
            self._send("425 No PASV ports available")
            return
        try:
            listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listener.bind(("127.0.0.1", port))
            listener.listen(1)
            listener.settimeout(30)
            self.data_listener = listener
            p1 = port >> 8
            p2 = port & 0xFF
            self._send(f"227 Entering Passive Mode (127,0,0,1,{p1},{p2})")
        except OSError as e:
            self._send(f"425 Cannot open data connection: {e}")

    def cmd_EPSV(self, arg):
        # Stub: force aria2 to fall back to PASV
        self._send("500 EPSV not supported, use PASV")

    def cmd_REST(self, arg):
        if not self._check_auth():
            return
        try:
            self.rest_offset = int(arg)
            self._send(f"350 Restarting at {self.rest_offset}")
        except ValueError:
            self._send("501 Invalid REST parameter")

    def cmd_RETR(self, arg):
        if not self._check_auth():
            return
        path = self._resolve_path(arg)
        real = self._real_path(path)
        if not os.path.isfile(real):
            self._send("550 File not found")
            return
        conn = self._accept_data()
        if conn is None:
            return
        try:
            self._send("150 Opening data connection")
            with open(real, "rb") as f:
                if self.rest_offset > 0:
                    f.seek(self.rest_offset)
                    self.rest_offset = 0
                while True:
                    chunk = f.read(65536)
                    if not chunk:
                        break
                    conn.sendall(chunk)
            self._send("226 Transfer complete")
        except (BrokenPipeError, ConnectionResetError, OSError):
            self._send("426 Transfer aborted")
        finally:
            conn.close()
            self._close_data()

    def cmd_LIST(self, arg):
        if not self._check_auth():
            return
        # Determine target directory (ignore -a/-l flags)
        target = self.cwd
        if arg and not arg.startswith("-"):
            target = self._resolve_path(arg)
        real = self._real_path(target)
        if not os.path.isdir(real):
            self._send("550 Directory not found")
            return
        conn = self._accept_data()
        if conn is None:
            return
        try:
            self._send("150 Opening data connection for LIST")
            entries = []
            for name in sorted(os.listdir(real)):
                full = os.path.join(real, name)
                try:
                    st = os.stat(full)
                except OSError:
                    continue
                if os.path.isdir(full):
                    perm = "drwxr-xr-x"
                else:
                    perm = "-rw-r--r--"
                mtime = time.strftime(
                    "%b %d %H:%M", time.gmtime(st.st_mtime))
                line = (f"{perm}   1 owner group "
                        f"{st.st_size:>12} {mtime} {name}")
                entries.append(line)
            listing = "\r\n".join(entries) + "\r\n"
            conn.sendall(listing.encode("utf-8"))
            self._send("226 Transfer complete")
        except (BrokenPipeError, ConnectionResetError, OSError):
            self._send("426 Transfer aborted")
        finally:
            conn.close()
            self._close_data()

    def cmd_FEAT(self, arg):
        self._send("211-Features:")
        self._send(" SIZE")
        self._send(" MDTM")
        self._send(" REST STREAM")
        self._send(" PASV")
        self._send(" UTF8")
        self._send("211 End")

    def cmd_QUIT(self, arg):
        self._send("221 Goodbye")

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _send(self, msg):
        try:
            self.wfile.write((msg + "\r\n").encode("utf-8"))
            self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass

    def _check_auth(self):
        if not self.authenticated:
            self._send("530 Please login first")
            return False
        return True

    def _resolve_path(self, arg):
        """Resolve an FTP path argument against cwd."""
        if arg.startswith("/"):
            return os.path.normpath(arg)
        return os.path.normpath(os.path.join(self.cwd, arg))

    def _real_path(self, ftp_path):
        """Map an FTP virtual path to a real filesystem path."""
        rel = ftp_path.lstrip("/")
        real = os.path.normpath(os.path.join(self.serve_dir, rel))
        # Prevent directory traversal
        if not real.startswith(self.serve_dir):
            return self.serve_dir
        return real

    def _alloc_pasv_port(self):
        """Allocate the next PASV port from the range (round-robin)."""
        with self._pasv_port_lock:
            start = self._next_pasv_port
            total = self.pasv_range_end - self.pasv_range_start + 1
            for i in range(total):
                port = self.pasv_range_start + (
                    (start - self.pasv_range_start + i) % total)
                FTPHandler._next_pasv_port = (
                    self.pasv_range_start
                    + (port - self.pasv_range_start + 1) % total)
                return port
        return None

    def _accept_data(self):
        """Accept a data connection from the PASV listener."""
        if self.data_listener is None:
            self._send("425 No data connection established")
            return None
        try:
            conn, _ = self.data_listener.accept()
            return conn
        except socket.timeout:
            self._send("425 Data connection timed out")
            return None
        except OSError as e:
            self._send(f"425 Data connection error: {e}")
            return None

    def _close_data(self):
        """Close any open data listener socket."""
        if self.data_listener:
            try:
                self.data_listener.close()
            except OSError:
                pass
            self.data_listener = None


class ThreadingFTPServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    parser = argparse.ArgumentParser(
        description="aria2 e2e test FTP server")
    parser.add_argument("--port", type=int, default=18100)
    parser.add_argument("--dir", default=".")
    parser.add_argument("--user", default=None,
                        help="Required username (anonymous if omitted)")
    parser.add_argument("--passwd", default=None,
                        help="Required password")
    parser.add_argument("--pasv-range", default="18101-18110",
                        help="PASV port range (e.g. 18101-18110)")
    args = parser.parse_args()

    pasv_start, pasv_end = args.pasv_range.split("-", 1)
    FTPHandler.serve_dir = os.path.abspath(args.dir)
    FTPHandler.required_user = args.user
    FTPHandler.required_passwd = args.passwd
    FTPHandler.pasv_range_start = int(pasv_start)
    FTPHandler.pasv_range_end = int(pasv_end)
    FTPHandler._next_pasv_port = int(pasv_start)

    server = ThreadingFTPServer(("127.0.0.1", args.port), FTPHandler)

    def shutdown_handler(*_):
        threading.Thread(target=server.shutdown).start()

    signal.signal(signal.SIGTERM, shutdown_handler)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()


if __name__ == "__main__":
    main()
