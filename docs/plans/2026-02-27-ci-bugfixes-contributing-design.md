# CI Improvements, Bug Fixes & CONTRIBUTING.md — Design

## Scope

Six workstreams on the `develop` branch:

1. **CI: TSan, coverage, static analysis, ccache** — new GitHub Actions workflows
2. **Fix: RPC port 0** — allow OS-assigned port for `--rpc-listen-port`
3. **Fix: WinTLS TLS 1.3** (#2332/#2327) — migrate to SCH_CREDENTIALS, add TLS 1.3
4. **Fix: RPC bottleneck** (#2340) — defer queue checks, document system.multicall
5. **CONTRIBUTING.md** — public contributor guide
6. **Cleanup: delete .travis.yml** — legacy CI config

## 1. CI Improvements

### Thread Sanitizer (TSan)

New job in `build.yml`. TSan is incompatible with ASan so it runs separately.
Linux-only, gcc-14, `-fsanitize=thread` in CPPFLAGS/LDFLAGS.

### Code Coverage

New workflow `coverage.yml`. Single config (Linux/gcc/OpenSSL/BT).
`--coverage` flags, lcov collection, Codecov upload. Badge in README.

### Static Analysis

New workflow `static-analysis.yml`:
- cppcheck: `--enable=warning,performance,portability` on `src/`
- clang-tidy: bugprone, performance, clang-analyzer checks
- clang-format: `--dry-run --Werror` to enforce style (blocking)

### ccache

Add `actions/cache@v4` for `~/.ccache` in existing `build.yml`.
Wrap compilers: `CC="ccache $CC" CXX="ccache $CXX"`.

### Cleanup

Delete `.travis.yml` (superseded by GitHub Actions).

## 2. RPC Port 0

**Problem:** `NumberOptionHandler` for `rpc-listen-port` has min=1024, rejecting port 0.

**Fix:**
- `OptionHandlerFactory.cc:769`: change min from 1024 to 0
- `HttpListenCommand::bindPort()`: after bind, if requested port was 0, log actual port via `getAddrInfo().port`
- `usage_text.h`: document port 0 = OS auto-assignment

`SocketCore::bind()` already supports port 0 (documented in header comment).
Pattern exists in `PeerListenCommand::getPort()`.

## 3. WinTLS TLS 1.3

**Target:** Windows 10+ only (drop Windows 7/8).

**Changes:**
- `WinTLSContext.cc`: migrate from `SCHANNEL_CRED` to `SCH_CREDENTIALS` + `TLS_PARAMETERS`
- Add `SP_PROT_TLS1_3_CLIENT` (0x00002000) and `SP_PROT_TLS1_3_SERVER` (0x00001000)
- Handle `TLS_PROTO_TLS13` in constructor switch
- Fix revocation: replace `SCH_CRED_REVOCATION_CHECK_CHAIN` with `SCH_CRED_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT`, add `SCH_CRED_IGNORE_REVOCATION_OFFLINE`
- `WinTLSSession.cc`: add case `0x0304` → `TLS_PROTO_TLS13` in protocol version switch

## 4. RPC Bottleneck

**Problem:** Rapid RPC addUri/addTorrent calls block the engine loop. `requestQueueCheck()` is called per-add, and `fillRequestGroupFromReserver()` only activates a limited batch per tick.

**Fix:**
- Defer `requestQueueCheck()` — batch it instead of per-add
- Document `system.multicall` as recommended for bulk additions
- Process all pending reserved groups in one tick (not just `maxConcurrent - active`)

## 5. CONTRIBUTING.md

Short public guide (~100 lines):
- Build setup (all platforms)
- Code style (reference .clang-format)
- Commit convention (commitizen)
- Git workflow (feature branches, rebase)
- Testing (make check, CppUnit)
- Link to issues

## 6. Risk Assessment

| # | Work | Risk | Effort |
|---|------|------|--------|
| 1 | CI workflows | Low | Medium |
| 2 | RPC port 0 | Low | Small |
| 3 | WinTLS TLS 1.3 | Medium | Medium |
| 4 | RPC bottleneck | High | Medium |
| 5 | CONTRIBUTING.md | None | Small |
| 6 | Delete .travis.yml | None | Trivial |
