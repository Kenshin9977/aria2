# aria2 Development Environment Setup

Date: 2026-02-25

## Goal

Set up a safe, guardrailed development environment to work on aria2
without risk of damaging the upstream project.

## What was done

### 1. Fork & Clone
- Forked `aria2/aria2` to `Kenshin9977/aria2` via GitHub CLI
- Cloned to `/Users/kenshin/CodeProjects/aria2`
- Remotes: `origin` = fork, `upstream` = aria2/aria2

### 2. Dependencies installed (via Homebrew)
- automake, libtool, cppunit, cppcheck, pre-commit, ccache
- Already present: autoconf, pkgconf, openssl@3, c-ares, libssh2, sqlite,
  libxml2, llvm (provides clang-format and clang-tidy)

### 3. Guardrails

**Layer 1 — Pre-commit hooks** (`.pre-commit-config.yaml`):
- Trailing whitespace / end-of-file fixer
- Merge conflict detection
- Large file blocker (500KB)
- clang-format enforcement on src/ and test/
- Debug statement detection

**Layer 2 — Static analysis** (`.clang-tidy`):
- bugprone, cert, clang-analyzer, misc, performance checks
- Configured for C++11 compatibility
- Header filter restricted to src/

### 4. Build verification
- `autoreconf -i` + `./configure` + `make -j` = success
- Binary: `aria2 version 1.37.0` with AppleTLS, BitTorrent, Metalink,
  WebSocket, SFTP, c-ares, libxml2, sqlite3, zlib
- `make check` = PASS (all tests green)

## Configuration summary

| Feature       | Provider    |
|---------------|-------------|
| SSL/TLS       | AppleTLS    |
| DNS            | c-ares      |
| XML            | libxml2     |
| Database       | sqlite3     |
| SFTP           | libssh2     |
| Compression    | zlib        |
| WebSocket      | wslay       |
| BitTorrent     | built-in    |
| Metalink       | built-in    |
| Test framework | CppUnit     |
