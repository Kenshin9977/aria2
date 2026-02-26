# Test Reinforcement — Implementation Plan

Date: 2026-02-26

## Phase 1: Unit Tests (no production code changes)

### Order of implementation

1. **DownloadEngineTest.cc** — engine lifecycle, command scheduling, run() loop
2. **DownloadEngineMethodTest.cc** — DNS cache, socket pool, CUID generation
3. **SocketBufferTest.cc** — SocketBuffer + SocketRecvBuffer via socketpair()
4. **TimeBasedCommandTest.cc** — TimeBasedCommand + AutoSaveCommand + TimedHaltCommand
5. **RealtimeCommandTest.cc** — CheckIntegrity + FileAllocation commands
6. **BtOrchestrationTest.cc** — TrackerWatcher, SeedCheck, PeerChoke, BtStop
7. **DHTCommandTest.cc** — all 7 DHT command files

### For each test file

1. Create `test/<Name>Test.cc` following existing conventions
2. Add to `test/Makefile.am` in the `aria2c_SOURCES` list
3. Run `make check` to verify

### Build system changes

- Add new test .cc files to `test/Makefile.am`
- Add new mock .h files if needed
- No changes to `configure.ac` or `src/Makefile.am`

## Phase 1b: CI Expansion

### Windows (MSYS2)

Add job to `.github/workflows/build.yml`:
- runs-on: windows-latest
- uses: msys2/setup-msys2@v2 with MINGW64
- packages: mingw-w64-x86_64-{gcc,cppunit,openssl,c-ares,sqlite3,libssh2,zlib,expat}
- build and test: autoreconf + configure + make check

### FreeBSD

Add job using vmactions/freebsd-vm@v1:
- runs-on: ubuntu-latest
- pkg install: autoconf automake libtool pkgconf cppunit openssl c-ares sqlite3 libssh2 expat
- build and test

### Solaris (compile-only)

Separate lightweight workflow:
- Create stub headers for port.h
- Compile PortEventPoll.cc with -fsyntax-only
