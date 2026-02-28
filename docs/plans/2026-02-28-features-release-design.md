# Features & Release Design

## Goal

Cherry-pick upstream quick-win PRs, implement slow-start connection scaling,
add SOCKS5 proxy support, and cut a release.

## 1. Cherry-Pick Quick-Win PRs

Six PRs apply cleanly to our `develop` branch:

| PR | Description | Risk |
|----|-------------|------|
| #2323 | Fix literal operator spacing (C++11) | None — whitespace |
| #1543 | Support `NO_COLOR` env variable | None — cosmetic |
| #1696 | Remove max-connection-per-server=16 cap | Low — raises cap to 16384 |
| #1714 | Add `--retry-wait` on HTTP 404 | Low — opt-in behavior |
| #1777 | Fix memory leak in XML-RPC list parse | None — bugfix |
| #2209 | Fix CPU spin in `waitData` with no sockets | None — bugfix |

One PR needs minor conflict resolution:

| PR | Description | Risk |
|----|-------------|------|
| #2268 | Deflate raw-inflate fallback | Low — gzip edge case |

Skipped: #2334 (RPC delete files) — needs full rewrite, too risky.

## 2. Slow-Start Connection Scaling

### Problem

aria2 creates all `--split` connections at once. Aggressive servers return
HTTP 403 or rate-limit. The old hard cap of 16 per server was a blunt guard.
PR #1696 removes it but offers no replacement safety mechanism.

### Design

Add a `SlowStartController` to `FileEntry`. It tracks per-host connection
ramp-up using TCP-style AIMD (additive increase, multiplicative decrease):

```
Phase       Connections   Trigger
STARTUP     1             download starts
RAMP_UP     1→2→4→8→…    speed increases between intervals
STABLE      current       speed plateaus (< 10% gain)
BACK_OFF    current / 2   HTTP 403 or connection refused
```

**Anchor points:**
- `FileEntry.h` — new `SlowStartController slowStart_` member
- `FileEntry.cc:147` — `getRequestWithInFlightHosts()` checks
  `slowStart_.getAllowedConnections(host)` instead of raw
  `maxConnectionPerServer_`
- `RequestGroup::createNextCommand()` — unchanged (it already delegates to
  FileEntry for per-host limits)

**Speed sampling:**
- `PeerStat::getAvgDownloadSpeed()` already tracks per-request speed
- `FileEntry::RequestFaster` comparator already sorts by speed
- Sample aggregate speed every 2 seconds; compare with previous sample

**New option:**
- Raise `--max-connection-per-server` default from 1 to 1 (unchanged)
- Raise max from 16 to 128 (via PR #1696 or our own change)
- The `maxConnectionPerServer_` becomes the *ceiling*; slow-start controls
  the *current effective limit*

**Fast-fail on 403:**
- In `HttpSkipResponseCommand` / `HttpResponseCommand`, when status == 403:
  signal `FileEntry::slowStart_.backOff(host)`
- Halves effective connections for that host

### What we do NOT build

- Work stealing (Gopeed feature) — YAGNI for v1
- Dynamic segment splitting — too invasive
- Per-URI slow-start (we do per-host, which is what matters)

## 3. SOCKS5 Proxy Support

### Problem

Issue #153 — most requested feature (106 +1). aria2 supports HTTP CONNECT
proxy but not SOCKS5. Users behind SOCKS proxies (Tor, corporate VPNs)
cannot use aria2.

### Design

Follow the existing proxy command pipeline pattern:

```
ConnectCommand → Socks5HandshakeCommand → Socks5ConnectCommand
                                          → target protocol command
```

**New files (6):**
- `src/Socks5HandshakeCommand.{h,cc}` — sends greeting + optional auth
- `src/Socks5ConnectCommand.{h,cc}` — sends CONNECT, reads response,
  chains to target protocol command
- `src/Socks5RequestConnectChain.h` — `ControlChain<ConnectCommand*>` that
  injects `Socks5HandshakeCommand`

**Modified files:**
- `src/prefs.{h,cc}` — add `PREF_SOCKS5_PROXY`, user, passwd
- `src/OptionHandlerFactory.cc` — register options
- `src/usage_text.h` — help text
- `src/AbstractCommand.cc` — `getProxyUri()` returns SOCKS5 URI when set;
  `resolveProxyMethod()` returns new method when SOCKS5 proxy detected
- `src/HttpInitiateConnectionCommand.cc` — add chain selection for SOCKS5
- `src/FtpInitiateConnectionCommand.cc` — same
- `src/Makefile.am` — register new files

**Protocol flow:**
1. Connect to SOCKS5 proxy (TCP)
2. Greeting: `05 01 00` (no auth) or `05 01 02` (user/pass)
3. Auth (if needed): `01 <ulen> <user> <plen> <pass>`
4. CONNECT: `05 01 00 03 <dlen> <domain> <port>` (domain name type)
5. Response: `05 00 ...` (success) → socket now tunnels to target
6. Proceed with HTTP/FTP/SFTP normally on the tunneled socket

**Auth support:**
- `--socks5-proxy-user` / `--socks5-proxy-passwd`
- If both empty: use `NO_AUTH` method (0x00)
- If set: use `USERNAME_PASSWORD` method (0x02)

### What we do NOT build

- SOCKS4/SOCKS4a (obsolete)
- UDP association (SOCKS5 CMD=0x03) — only TCP CONNECT
- BIND (CMD=0x02) — not needed for downloads

## 4. HTTP/2 — Deferred

HTTP/2 requires fundamental architectural changes:
- aria2's command scheduler is 1 connection = 1 segment (sequential)
- HTTP/2 multiplexes concurrent streams on 1 connection
- nghttp2 not bundled, would be new external dependency
- Estimated effort: 4-6 months

**Decision:** Defer to a future major release. The slow-start + SOCKS5
features provide more immediate user value.

## 5. Release

After all features land and tests pass:
- Tag `v1.37.1` (or whatever version is appropriate)
- Update `configure.ac` version
- Generate changelog from conventional commits

## Test Strategy

- **PRs:** Existing tests + verify `make check` passes
- **Slow-start:** Unit tests for `SlowStartController` (mock timer, mock
  speed samples), integration test in `FileEntryTest`
- **SOCKS5:** Unit tests for handshake/connect byte sequences,
  mock-based command tests following `HttpProxyRequestCommand` test pattern
