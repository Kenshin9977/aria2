# C++23 Modernization — Design

Date: 2026-02-27

## Problem

aria2 is pinned to C++11 (configure.ac: `AX_CXX_COMPILE_STDCXX([11])`).
The codebase is well-written modern C++11 but misses 12 years of language
and library improvements. With 1300+ open issues and no active maintainer,
modernizing the code makes it safer, faster, and more attractive to
contributors.

## Goals

- **Performance**: zero-copy parsing with `string_view`, `std::span`
- **Reliability**: `std::expected` for error handling, `override` everywhere
- **Readability**: structured bindings, `std::format`, `std::optional`
- **Safety**: compiler catches more bugs at compile time

## Compiler Requirements

- GCC 15+ / Clang 19+ (both support C++23 fully + select C++26 features)
- macOS: Clang 19 via Homebrew (`/opt/homebrew/opt/llvm/bin/clang++`)
- Linux: system GCC 15 or Clang 19
- Windows: MSYS2 GCC 15

## Strategy: Bottom-Up by Layer

Each layer is a separate branch. Tests must pass before moving to the next.

### Layer 1 — Foundations (mechanical, zero risk)

Pure mechanical changes, no behavior change.

**`override` on all virtual overrides (~300 sites)**
Automate with `clang-tidy -checks=modernize-use-override`. Remove redundant
`virtual` keyword on overrides.

**`constexpr` on constants**
`static const` literals in headers become `static constexpr`. Pure utility
functions in `util.h` become `constexpr` where possible.

**`= delete("reason")` on deleted APIs (C++26)**
Add diagnostic messages to deleted constructors/operators.
Example: `Command(const Command&) = delete("Commands are move-only")`.

**`nullptr` everywhere**
Replace `NULL` and `0`-as-pointer with `nullptr` (~100 sites).

Estimation: ~500 mechanical edits, 0 behavior change.

### Layer 2 — Utils/Types (high impact)

Core type changes that propagate through the codebase.

**`std::string_view` for read-only parameters (~200 signatures)**
`const std::string&` becomes `std::string_view` when the function does not
store the string. Targets: `util.h`, `Request.cc`, `Option.cc`, HTTP header
lookups, URI parsing.
Rule: if the function stores the value (member, map key), keep `const std::string&`.

**`std::optional<T>` for nullable results (~80 sites)**
Replace `return ""` / `return -1` / `bool + out-param` patterns.
Targets: `util::parseUInt()`, `Request::getHost()`, DNS results, cookie
lookups.

**`std::expected<T, E>` for parse/network errors (~50 functions)**
Replace `throw DL_ABORT_EX` in low-level parsing helpers with value-based
error returns. Commands continue to catch exceptions — only helper functions
change. Error type: existing `error_code` enum or new `enum class ParseError`.

**Structured bindings (~100 sites)**
`for (auto& p : map)` becomes `for (auto& [key, value] : map)`.
`std::pair` returns use direct destructuring.

### Layer 3 — I/O & Containers

**`std::span<const uint8_t>` for binary buffers**
Replace `(const unsigned char* data, size_t len)` pairs with `std::span`.
Targets: `SocketCore::writeData`, `SocketBuffer`, `MessageDigest`,
`DiskWriter`. Callers convert implicitly from vector/array.

**`std::format` for logging**
Current `fmt()` macro uses `snprintf`. Migrate to `std::format`.
Phase 1: new `A2_FMT(...)` macro wrapping `std::format`.
Phase 2: progressive migration of callsites.

**`std::ranges` algorithms**
`std::find_if(v.begin(), v.end(), pred)` becomes `std::ranges::find_if(v, pred)`.
Only use algorithms supported by both GCC 15 and Clang 19 libc++. No
advanced views (zip, chunk, stride — missing from libc++ 19).

### Layer 4 — Protocols

**HTTP parser zero-copy**
`HttpHeader::find()` and response parsing use `string_view` on the receive
buffer. Buffer lifetime exceeds view lifetime — safe by construction.

**FTP response parsing**
`FtpConnection::receiveResponse()` returns `std::expected<int, error_code>`
instead of throwing. EPSV/PASV/PWD parsers use `string_view`.

**BitTorrent messages**
`BtHandshakeMessage::create()` takes `std::span` instead of raw pointers.
Message factories return `std::expected` for parse errors.

**URI parsing**
`uri::parse()` returns `std::expected<UriStruct, ParseError>` instead of
throwing on error.

### Layer 5 — Engine & Commands

**Deducing this (C++23)**
Simplify CRTP patterns in `AbstractCommand` hierarchy where subclasses
call parent methods with casts.

**`std::print` / `std::println`**
Replace `fprintf(stdout, ...)` and `std::cout <<` for user-facing output.
Internal logging stays on `A2_LOG_*` macros.

**Move semantics audit**
Verify all Command transfers use `std::unique_ptr<Command>&&`. Eliminate
unnecessary copies found by `-Wextra` under C++23.

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| `string_view` dangling | Rule: never store as member. Review all conversions |
| `expected` + legacy catch | Progressive, layer by layer. Commands still catch |
| macOS build broken | CI verifies Clang 19 (Homebrew) from first commit |
| Too many changes at once | One branch per layer, tests green between each |
| `std::format` perf | Benchmark vs current `fmt()` before mass migration |

## Out of Scope

- **Reflection** — GCC 16 not released yet
- **Contracts** — no compiler supports P2900
- **`std::execution`** — not in any stdlib
- **`std::flat_map`** — not in libc++ 19
- **Advanced ranges views** — not in libc++ 19
- **C++20 modules** — build system change too invasive

## Features Used — Compiler Support Matrix

| Feature | Standard | GCC 15 | Clang 19 |
|---------|----------|--------|----------|
| `std::string_view` | C++17 | Yes | Yes |
| `std::optional` | C++17 | Yes | Yes |
| Structured bindings | C++17 | Yes | Yes |
| `std::span` | C++20 | Yes | Yes |
| `std::ranges` (basic) | C++20 | Yes | Yes |
| `std::format` | C++20 | Yes | Yes |
| `std::expected` | C++23 | Yes | Yes |
| `std::print/println` | C++23 | Yes | Yes |
| Deducing this | C++23 | Yes | Yes |
| `= delete("reason")` | C++26 | Yes | Yes |
| `constexpr` extensions | C++23 | Yes | Yes |

All features verified available in both compilers.

## Success Criteria

- `configure.ac` requires C++23: `AX_CXX_COMPILE_STDCXX([23])`
- All 1252+ tests pass on GCC 15 and Clang 19
- Zero `string_view` dangling (ASan clean)
- `clang-tidy modernize-*` checks pass with zero warnings
- Coverage does not decrease
