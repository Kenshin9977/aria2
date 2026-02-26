# aria2 Test Suite Reinforcement

Date: 2026-02-26

## Problem

64% of aria2 source modules have zero test coverage. The entire runtime
core (DownloadEngine, 67+ Command classes, TLS, event loops, sockets) is
untested. This blocks safe C++ standard migration and feature work.

## Current State

- 187 test suites, 991 test methods, ~36% module coverage
- Well-tested: parsers, serialization, data structures, DHT messages
- Untested: DownloadEngine, all Commands, TLS, event polls, socket buffers

## Strategy: 3 Phases

### Phase 1 — Safety net without modifying production code

Test everything testable today using existing interfaces, mocks, and
socketpair() for real fd-based I/O.

**Unit tests (new test files):**

| Target | Technique | Files |
|--------|-----------|-------|
| DownloadEngine event loop | Real SelectEventPoll + MockCommand | DownloadEngineTest.cc |
| TimeBasedCommand (8 subclasses) | Minimal real engine + mock domain objects | TimeBasedCommandTest.cc |
| RealtimeCommand (4 subclasses) | Mock entries + stub engine | RealtimeCommandTest.cc |
| DHT Commands (7 files) | Setter injection with existing DHT mocks | DHTCommandTest.cc |
| BT Orchestration (4 files) | Existing BT mocks via setters | BtOrchestrationTest.cc |
| SocketBuffer / SocketRecvBuffer | socketpair() for real fds | SocketBufferTest.cc |
| DNS cache + socket pool | Direct unit tests on DownloadEngine methods | DownloadEngineMethodTest.cc |
| RequestGroupMan | Real engine + mock groups | RequestGroupManTest.cc |

**CI expansion (new workflow jobs):**

| Platform | Method | What it covers |
|----------|--------|----------------|
| Windows | GitHub Actions + MSYS2 runner | WinTLS, winsock2, WinConsoleFile, SelectEventPoll |
| FreeBSD | vmactions/freebsd-vm on Ubuntu runner | kqueue, BSD sockets |
| Solaris | Compile-only with stub headers | PortEventPoll syntax/types |
| Linux epoll | Already covered by Ubuntu runner | EpollEventPoll runtime |

**Estimated coverage after Phase 1: ~60%**

### Phase 2 — Extract ISocketCore interface

Single surgical modification to production code that unlocks mockability
for all Command classes that do network I/O.

- Create `ISocketCore` pure virtual interface with readData(), writeData(),
  wantRead(), wantWrite(), isReadable(), etc.
- `SocketCore` implements `ISocketCore`
- Change `shared_ptr<SocketCore>` to `shared_ptr<ISocketCore>` where needed
- Create `MockSocketCore` implementing `ISocketCore` with canned responses
- Write tests for HTTP, FTP, BT peer Commands using MockSocketCore

**Estimated coverage after Phase 2: ~85%**

### Phase 3 — Integration tests with local mock servers

End-to-end protocol tests with local servers:

- HTTP: local server socket feeding canned responses
- FTP: local server simulating FTP negotiation (35 states)
- BitTorrent: simulated peer for wire protocol
- SFTP: local libssh2 server (if feasible)

**Estimated coverage after Phase 3: ~95%**

## Cross-Platform CI Matrix

```yaml
# Target build.yml matrix:
#   ubuntu-22.04:  gcc-12/clang-15 x openssl/gnutls x with/without-bt (existing)
#   macos-14:      appleTLS (existing)
#   windows-latest: MSYS2/mingw64 + openssl + WinTLS (NEW)
#   freebsd:       vmactions/freebsd-vm + openssl (NEW)
#   solaris:       compile-only stub headers (NEW, separate workflow)
```

## Constraints

- CppUnit framework (existing, not migrating yet)
- No production code changes in Phase 1
- Tests must be deterministic (no network flakiness)
- All new tests must pass `make check` locally and in CI
- Follow existing test conventions: `test/<ClassName>Test.cc`

## Success Criteria

- Phase 1: all new tests green on macOS + Linux + Windows CI
- Phase 2: MockSocketCore enables testing all Command classes
- Phase 3: full protocol round-trips verified locally
- Final: confident enough to begin C++ standard migration
