# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [2.0.0] - 2026-03-02

First major release of this modernised fork, based on upstream aria2
v1.37.0 with 160+ commits of improvements.

### Added — New Features

- **HTTP/2** support via libnghttp2
- **HTTP Digest authentication** (RFC 7616) with MD5 and SHA-256
- **Brotli** content-encoding decompression (`Content-Encoding: br`)
  via libbrotlidec
- **Zstd** content-encoding decompression (`Content-Encoding: zstd`)
  via libzstd
- **FTPS** support (explicit FTP over TLS) with AUTH TLS, PBSZ, and
  PROT P commands; TLS session reuse for data connections
- **SOCKS5 proxy** support (RFC 1928/1929) with username/password
  authentication; new options `--socks5-proxy`,
  `--socks5-proxy-user`, `--socks5-proxy-passwd`
- **HSTS** (HTTP Strict Transport Security, RFC 6797) with automatic
  HTTP-to-HTTPS upgrade and `includeSubDomains` support
- **Slow-start connection scaling** with AIMD (Additive Increase,
  Multiplicative Decrease) ramp-up; starts at 1 connection and
  doubles on throughput gains, halves on 403 or failure
- **`io_uring`** event poll backend for Linux (alongside epoll, kqueue,
  poll, select)
- **TTL-based DNS cache expiration** instead of indefinite caching
- **Cookie `SameSite` attribute** support
- **`NO_COLOR`** environment variable support (https://no-color.org/)
- **RPC port 0** for OS-assigned dynamic port allocation
  (`--rpc-listen-port=0`)
- **Max connections per server raised** from 16 to 10,000
- **Deflate raw mode fallback** for servers sending headerless deflate
  streams
- **HTTP/3 build system** detection for ngtcp2 + nghttp3 (libraries
  detected at configure time; transport not yet implemented)
- **CMake build system** as an alternative to Autotools
- **Thread pool for checksum verification** using `std::async`,
  scaling to available CPU cores
- **TLS 1.3 support** on Windows (WinTLS/Schannel)
- **WebSocket outbound queue limits** to prevent memory exhaustion
  under slow clients
- **Socket pool size limit** with lazy eviction

### Added — Testing & CI

- **310+ end-to-end tests** across 79 shell scripts
  (`test/e2e/run_all.sh`)
- **1,300+ unit tests** (CppUnit), up from ~1,100 upstream
- `ISocketCore` interface extraction with `MockSocketCore` for
  testable command pipelines
- Integration tests for HTTP, FTP, BitTorrent, and DHT command
  lifecycles
- Unit tests for: Http2Session, HstsStore, DNS TTL, DHT commands,
  DHT message dispatch/tasks, BT orchestration, BT peer handshake,
  SocketBuffer, TimeBasedCommand, DownloadEngine, AbstractCommand,
  SlowStartController, Socks5 commands, checksum validators, piece
  selectors, and more
- GitHub Actions CI: Linux (Ubuntu 24.04), macOS 26, Windows
  (MSYS2/MinGW64), with ASan, UBSan, and TSan sanitiser jobs
- Code coverage workflow (lcov)
- Static analysis workflow (cppcheck)
- **Release workflow** building static binaries for 5 platforms
  (linux-x86_64, linux-aarch64, macos-arm64, macos-x86_64,
  windows-x86_64) with SHA256SUMS
- **Dockerfile** for static Alpine-based builds
- **AppImage build script** (`scripts/build-appimage.sh`)

### Fixed — Bug Fixes

- **CPU spin on unauthorised RPC response** — socket left in
  write-check during 401 delay caused busy-loop (upstream #2209)
- **Memory leak in Piece/SinkStreamFilter** — allocated buffers
  leaked when exception thrown during disk cache update
  (upstream #1777)
- **HTTP 404 retry-wait not applied** — 404 errors now respect
  `--retry-wait` instead of failing immediately (upstream #1714)
- **Literal operator spacing** for C++ standard compliance
  (upstream #2323)
- **BitTorrent handshake bounds checking** — validate message
  lengths before buffer access
- **HTTP header injection** — reject headers containing CR/LF in
  `HttpRequest::addHeader()`
- **`fcntl()` error handling** — check return values of F_GETFL /
  F_SETFL calls
- **MSE Diffie-Hellman computation** — handle key exchange errors
  gracefully instead of silently continuing
- **`ares_init_options()` return value** now checked; throws on
  failure instead of using uninitialised channel
- **`SSHSession::closeConnection()` EAGAIN** — retry loops for
  `libssh2_sftp_close`, `libssh2_sftp_shutdown`, and
  `libssh2_session_disconnect`
- **`Transfer-Encoding` parsing** — handle comma-separated token
  lists per RFC 7230 instead of exact-match only
- **RPC `LogFactory::reconfigure()` exception** now logged instead
  of silently swallowed
- **WinTLS certificate revocation** — use
  `REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT` with offline fallback
  instead of hard-failing when OCSP is unreachable

### Changed — C++23 Modernisation

The entire codebase has been migrated from **C++11 to C++23**.
Minimum compiler: GCC 14+ or Clang 18+.

- `std::ranges` algorithms replacing raw loops and `<algorithm>` calls
- `operator<=>` (three-way comparison) replacing manual
  comparison operators
- Structured bindings for map/pair iteration
- `enum class` replacing plain enums (Protocol, HttpMethod, etc.)
- `constexpr` promoted from `const` where applicable
- `[[nodiscard]]` on key return values
- `[[maybe_unused]]` replacing `(void)var` suppression
- `starts_with()` / `ends_with()` replacing manual prefix/suffix checks
- `= default` virtual destructors
- `std::make_unique` replacing aria2's polyfill and raw `new`
- `std::unordered_map` where key ordering is not needed
- Lambdas replacing `std::bind`
- `using` aliases replacing `typedef`
- `emplace_back` replacing `push_back` with temporaries
- `static_cast` / `reinterpret_cast` replacing C-style casts
- `std::expected` for error handling in URI parsing, magnet/metalink
  parsers, and integer parsing (replacing `*NoThrow` + out-param
  patterns)
- `std::span<const unsigned char>` replacing `ptr + len` pairs
- `std::string_view` in HttpHeader, Netrc, Option, DownloadContext,
  FileEntry, Request, and utility functions
- `std::format` via `A2_FMT` macro (replacing `fmt()`)
- `if`-with-initializer replacing find-then-check patterns
- `using enum` to reduce verbose enum qualification
- Pass-by-value and move for sink parameters
- `std::unique_ptr` for RAII in zlib streams, TLS factories, and
  XmlRpc parser (replacing raw `new`/`delete`)
- `std::unique_ptr` for sole-owned pointers in RequestGroup
  (replacing `shared_ptr`)

### Changed — Performance Optimisations

- **`pwrite`/`pread`** replacing seek+write/read for all disk I/O
- **O(1) event poll socket lookup** — `unordered_map` in all event
  poll backends (epoll, kqueue, poll, select)
- **O(1) BitTorrent endgame request lookup** — indexed set of
  (piece, block) pairs instead of linear scan
- **O(1) BitTorrent peer lookups** — `unordered_set` with proper
  hash for peer/message dispatching
- **Eliminated heap allocations in set lookups** — DNSCache,
  DefaultPieceStorage, and ServerStatMan use direct key maps instead
  of `set<shared_ptr<T>>`
- **Template inlining** replacing `std::function` callbacks in
  SequentialPicker and bittorrent_helper
- **Hoisted vector allocation** outside BitTorrent endgame loop
- **Reserved string capacity** in HTTP request header construction
- **`unique_ptr` for SegmentEntry** — fewer allocator calls during
  segment checkout

### Changed — Architecture

- **`ISocketCore` interface** extracted from `SocketCore` with 21
  pure virtual methods; enables mock-based unit testing of all
  command classes without real sockets or TLS
- **`RequestGroupContext` interface** extracted for group-level
  state management
- **Socket transition helpers** extracted in command base classes
- **`util_string.h`** — string utilities extracted from monolithic
  `util.h`

### Changed — Miscellaneous

- README modernised with badges, fork highlights, protocol table,
  and quick start guide
- 100+ obsolete/unclear TODO comments cleaned up or rewritten
- Travis CI removed; GitHub Actions is the sole CI system

### Removed

- **FreeBSD CI job** — source code still compiles on FreeBSD but
  is no longer tested in CI
- **`std::print` usage** — reverted in favour of printf/iostream
  due to incomplete compiler support
- Verbose cross-compilation sections from README (see Dockerfiles)
- Upstream versioning/release schedule section from README
- Legacy `.lineno` build artifact

[2.0.0]: https://github.com/Kenshin9977/aria2/compare/release-1.37.0...v2.0.0
