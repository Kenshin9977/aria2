# Phase 3: Command-Level Integration Tests

Date: 2026-02-26

## Problem

Phases 1-2 added unit tests for Commands using mocks. The actual protocol
I/O paths (HTTP request/response, FTP negotiation, BT handshake) remain
untested at the Command level with real sockets and the real DownloadEngine
event loop.

## Strategy

Test Commands with the real `DownloadEngine` + `SelectEventPoll` + real
sockets via `socketpair()`. A "server" fd injects canned protocol responses
while the Command reads from the "client" fd through the engine's event loop.

Two patterns:

- **Pre-write** (approach A): write the full response on the server fd before
  `engine->run()`. Works for simple request-response (HTTP).
- **Tick-by-tick** (approach A+C): alternate between writing one response on
  the server fd and calling `engine->run(true)` (oneshot). Works for
  multi-round-trip protocols (FTP, BT).

Both are single-threaded and deterministic. No threading, no flakiness.

## Test Files

### HttpCommandIntegrationTest.cc

Tests `HttpRequestCommand` → `HttpResponseCommand` → `HttpDownloadCommand`
pipeline with real sockets.

**Scenarios:**

| Test | Server response | Assertion |
|------|----------------|-----------|
| 200 OK | `HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello` | Request sent on wire, response parsed, DownloadCommand enqueued |
| 301 Redirect | `HTTP/1.1 301\r\nLocation: http://other/\r\n\r\n` | Redirect followed |
| 404 Not Found | `HTTP/1.1 404\r\n\r\n` | Error handled, command completes |

**Setup:** `socketpair()` → `SocketCore(fds[0])` as client socket,
`fds[1]` as raw server fd. Minimal `RequestGroup` + `DownloadContext` +
`FileEntry` + `Request` with fake URL.

### FtpCommandIntegrationTest.cc

Tests `FtpNegotiationCommand` state machine with tick-by-tick server
responses.

**Scenarios:**

| Test | States covered | Assertion |
|------|---------------|-----------|
| PASV download | greeting→USER→PASS→TYPE→PWD→SIZE→EPSV→REST→RETR | Reaches NEGOTIATION_COMPLETED |
| Auth failure | greeting→USER→PASS(530) | Command exits on auth error |

**Setup:** Same socketpair pattern. Write one FTP response per engine tick.
Data channel requires a second socketpair for the PASV connection.

### BtPeerIntegrationTest.cc

Tests `PeerInitiateConnectionCommand` handshake with pre-written BT
handshake response.

**Scenarios:**

| Test | Server response | Assertion |
|------|----------------|-----------|
| Handshake OK | 68-byte BT handshake with matching info_hash | Transition to WIRED state |
| Bitfield exchange | BT handshake + bitfield message | PeerStorage updated |

**Setup:** socketpair + `RequestGroup` with `test.torrent` loaded.
Full BT object graph: `BtRuntime`, `PieceStorage`, `PeerStorage`,
`BtRegistry`.

## Files Changed

| Action | File |
|--------|------|
| Create | `test/HttpCommandIntegrationTest.cc` |
| Create | `test/FtpCommandIntegrationTest.cc` |
| Create | `test/BtPeerIntegrationTest.cc` |
| Modify | `test/Makefile.am` |

No production code changes.

## Constraints

- CppUnit framework
- Single-threaded, deterministic tests
- `socketpair(AF_UNIX, SOCK_STREAM)` for connected socket pairs
- Real `SelectEventPoll` via `TestEngineHelper::createTestEngine()`
- All tests must pass `make check`
