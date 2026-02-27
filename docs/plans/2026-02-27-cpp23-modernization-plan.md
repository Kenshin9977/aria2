# C++23 Modernization — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Modernize aria2 from C++11 to C++23, bottom-up, improving performance and maintainability.

**Architecture:** Five layers — foundations, utils/types, I/O, protocols, engine. Each layer is a feature branch merged into `develop`. Tests must pass between every task.

**Tech Stack:** C++23 (GCC 15+ / Clang 19+), autotools, CppUnit tests.

**Design doc:** `docs/plans/2026-02-27-cpp23-modernization-design.md`

---

## Task 1: Update build system to C++23

**Files:**
- Modify: `configure.ac:144`
- Modify: `.clang-format:113`

**Step 1: Create feature branch**

```bash
git checkout -b feature/cpp23-layer1-foundations develop
```

**Step 2: Update configure.ac**

Change line 144 from:
```m4
AX_CXX_COMPILE_STDCXX([11], [], [mandatory])
```
to:
```m4
AX_CXX_COMPILE_STDCXX([23], [], [mandatory])
```

Also update the comment on line 140 from `C++0x/C++11` to `C++23`.

**Step 3: Update .clang-format**

Change line 113 from:
```yaml
Standard:        Cpp11
```
to:
```yaml
Standard:        c++23
```

**Step 4: Rebuild and verify**

```bash
make distclean
autoreconf -if
./configure --with-libcares --with-sqlite3 --with-libssh2 \
  --without-gnutls --without-libgcrypt --without-libnettle \
  PKG_CONFIG_PATH="/opt/homebrew/opt/openssl@3/lib/pkgconfig:/opt/homebrew/opt/sqlite/lib/pkgconfig:/opt/homebrew/opt/c-ares/lib/pkgconfig:/opt/homebrew/opt/libssh2/lib/pkgconfig:/opt/homebrew/opt/libxml2/lib/pkgconfig:/opt/homebrew/opt/cppunit/lib/pkgconfig"
make -j$(sysctl -n hw.ncpu)
make -C test aria2c
cd test && ./aria2c > /tmp/t.out 2>&1 && grep "^OK" /tmp/t.out
```

Expected: `OK (1252)` — C++23 is backward-compatible, all existing code compiles.

**Step 5: Commit**

```bash
git add configure.ac .clang-format
git commit -m "chore: require C++23 standard (GCC 15+ / Clang 19+)"
```

---

## Task 2: Add `override` to all virtual overrides

This is the highest-value mechanical change — the compiler will catch accidental
signature mismatches in 58+ virtual overrides across 20 files.

**Files (src/):**
- Modify: `src/DiskAdaptor.h` (6 methods)
- Modify: `src/DiskWriter.h` (5 methods)
- Modify: `src/DHTAbstractNodeLookupTask.h` (3 methods)
- Modify: `src/TimeBasedCommand.h` (2 methods)
- Modify: `src/PeerAbstractCommand.h` (2 methods)
- Modify: `src/Event.h` (2 methods)
- Modify: `src/BinaryStream.h` (2 methods)
- Modify: remaining src/ files with missing overrides (~19 methods across ~13 files)

**Files (test/):**
- Modify: `test/MockDHTTaskFactory.h` (3 methods)
- Modify: `test/MockDHTMessage.h` (3 methods)
- Modify: remaining test mock headers (~11 methods across ~13 files)

**Step 1: Run clang-tidy to find all missing overrides**

```bash
# Generate compilation database
make clean && bear -- make -j$(sysctl -n hw.ncpu)
# Or if bear is not available, use:
# compiledb make -j$(sysctl -n hw.ncpu)

clang-tidy -checks='-*,modernize-use-override' \
  -fix -fix-errors \
  src/*.cc src/*.h test/*.h \
  -- -std=c++23
```

If clang-tidy is not available or too noisy, do manual search-and-fix:
- Search for `virtual` methods in classes that inherit from another class
- Add `override` to every method that overrides a parent
- Remove redundant `virtual` keyword on overrides (keep `virtual` only on base declarations)

**Step 2: Build and verify**

```bash
make -j$(sysctl -n hw.ncpu) && make -C test aria2c
cd test && ./aria2c > /tmp/t.out 2>&1 && grep "^OK" /tmp/t.out
```

Expected: `OK (1252)`

**Step 3: Commit**

```bash
git add -A
git commit -m "refactor: add override to all virtual overrides"
```

---

## Task 3: Convert `static const` to `static constexpr`

**Files (top priority — protocol constants):**
- Modify: `src/BtUnchokeMessage.h`, `src/BtChokeMessage.h`, `src/BtInterestedMessage.h`,
  `src/BtNotInterestedMessage.h`, `src/BtHaveMessage.h`, `src/BtBitfieldMessage.h`,
  `src/BtRequestMessage.h`, `src/BtPieceMessage.h`, `src/BtCancelMessage.h`,
  `src/BtPortMessage.h`, `src/BtSuggestPieceMessage.h`, `src/BtKeepAliveMessage.h`,
  `src/BtHaveAllMessage.h`, `src/BtHaveNoneMessage.h`, `src/BtRejectMessage.h`,
  `src/BtAllowedFastMessage.h`, `src/BtExtendedMessage.h`
- Modify: `src/MSEHandshake.h`
- Modify: `src/DHTBucket.h`, `src/Piece.h`, `src/Request.h`, `src/BtRuntime.h`
- Modify: EventPoll headers (`src/EpollEventPoll.h`, `src/KqueueEventPoll.h`,
  `src/PollEventPoll.h`, `src/SelectEventPoll.h`, `src/PortEventPoll.h`,
  `src/LibuvEventPoll.h`)

**Step 1: Replace in all BT message headers**

Pattern: `static const uint8_t ID = N;` → `static constexpr uint8_t ID = N;`
Pattern: `static const size_t MESSAGE_LENGTH = N;` → `static constexpr size_t MESSAGE_LENGTH = N;`

**Step 2: Replace in infrastructure headers**

Pattern: `static const int K = 8;` → `static constexpr int K = 8;` (DHTBucket.h)
Pattern: `static const int IEV_READ = POLLIN;` → `static constexpr int IEV_READ = POLLIN;` (EventPoll headers)

**Step 3: Build and verify**

```bash
make -j$(sysctl -n hw.ncpu) && make -C test aria2c
cd test && ./aria2c > /tmp/t.out 2>&1 && grep "^OK" /tmp/t.out
```

Expected: `OK (1252)`

**Step 4: Commit**

```bash
git add -A
git commit -m "refactor: convert static const to constexpr where applicable"
```

---

## Task 4: Replace `NULL` with `nullptr`

**Files:**
- Modify: `src/util.h` (3), `src/util.cc` (1), `src/uri_split.h` (1),
  `src/RequestGroupMan.cc` (1), `src/Option.h` (1), `src/GZipEncoder.h` (1),
  `src/FeatureConfig.h` (1), `src/BitfieldMan.h` (1), `src/WinConsoleFile.cc` (1),
  `src/LibsslTLSSession.cc` (1), `src/RpcMethodImpl.h` (9 — `= 0` pointer assigns)
- Plus remaining files (~38 more occurrences)

**Step 1: Search and replace**

```bash
# Find all NULL usages in src/
grep -rn '\bNULL\b' src/ --include='*.cc' --include='*.h'
# Find = 0 null pointer patterns
grep -rn '= 0;' src/ --include='*.h' | grep -i 'ptr\|pointer\|const.*\*'
```

Replace `NULL` → `nullptr` in all contexts.
Replace `= 0` → `= nullptr` only when the variable is a pointer type.

**Step 2: Build and verify**

```bash
make -j$(sysctl -n hw.ncpu) && make -C test aria2c
cd test && ./aria2c > /tmp/t.out 2>&1 && grep "^OK" /tmp/t.out
```

Expected: `OK (1252)`

**Step 3: Commit**

```bash
git add -A
git commit -m "refactor: replace NULL with nullptr"
```

---

## Task 5: Add `= delete("reason")` messages

**Files:**
- Modify: `src/ValueBase.h` (10), `src/Piece.h` (2), `src/SocketBuffer.h` (2),
  `src/DownloadResult.h` (2), `src/MessageDigest.h` (2), `src/MessageDigestImpl.h` (2),
  `src/SSHSession.h` (2), `src/TorrentAttribute.h` (2), `src/WinConsoleFile.h` (2),
  `src/MetalinkResource.h` (2), `src/Metalinker.h` (2), `src/AnnounceTier.h` (2),
  `src/AnnounceList.h` (2), `src/SegList.h` (2), `src/crypto_hash.h` (2),
  plus EventPoll inner classes (~5)

**Step 1: Add reason strings**

Pattern:
```cpp
// Before:
ClassName(const ClassName&) = delete;
ClassName& operator=(const ClassName&) = delete;

// After:
ClassName(const ClassName&) = delete("non-copyable");
ClassName& operator=(const ClassName&) = delete("non-copyable");
```

Use consistent reason strings:
- `"non-copyable"` — for types that own resources (most cases)
- `"move-only"` — for Command and similar types that are explicitly move-only

**Step 2: Build and verify**

```bash
make -j$(sysctl -n hw.ncpu) && make -C test aria2c
cd test && ./aria2c > /tmp/t.out 2>&1 && grep "^OK" /tmp/t.out
```

Expected: `OK (1252)`

**Step 3: Commit**

```bash
git add -A
git commit -m "refactor: add diagnostic messages to deleted functions (C++26)"
```

---

## Task 6: Merge Layer 1, start Layer 2

**Step 1: Merge Layer 1**

```bash
git checkout develop
git merge feature/cpp23-layer1-foundations
git branch -d feature/cpp23-layer1-foundations
```

**Step 2: Create Layer 2 branch**

```bash
git checkout -b feature/cpp23-layer2-types develop
```

---

## Task 7: Convert `util.h` string functions to `string_view`

This is the highest-impact change — these functions are called from hundreds
of sites across the codebase.

**Files:**
- Modify: `src/util.h` — change parameter types in declarations
- Modify: `src/util.cc` — change parameter types in definitions
- Test: existing tests in `test/UtilTest1.cc`, `test/UtilTest2.cc`

**Step 1: Add `#include <string_view>` to `util.h`**

**Step 2: Convert read-only string parameters**

Functions to convert (all in `util::` namespace):

```cpp
// String comparison functions — change const std::string& to std::string_view
bool startsWith(std::string_view a, const char* b);
bool startsWith(std::string_view a, std::string_view b);
bool istartsWith(std::string_view a, const char* b);
bool istartsWith(std::string_view a, std::string_view b);
bool endsWith(std::string_view a, const char* b);
bool endsWith(std::string_view a, std::string_view b);
bool iendsWith(std::string_view a, const char* b);
bool iendsWith(std::string_view a, std::string_view b);
bool strieq(std::string_view a, const char* b);
bool strieq(std::string_view a, std::string_view b);

// Inspection functions
bool isUtf8(std::string_view str);
bool isNumericHost(std::string_view name);
bool isHexDigit(std::string_view s);
bool inPrivateAddress(std::string_view ipv4addr);
```

**Important:** Do NOT convert functions that store the string into a member
or insert it into a container (e.g., `Option::put()`).

**Step 3: Update implementations in `util.cc`**

Most of these functions delegate to iterator-based templates, e.g.:
```cpp
bool startsWith(std::string_view a, std::string_view b) {
  return a.size() >= b.size() && a.substr(0, b.size()) == b;
}
```

**Step 4: Build and verify**

```bash
make -j$(sysctl -n hw.ncpu) && make -C test aria2c
cd test && ./aria2c > /tmp/t.out 2>&1 && grep "^OK" /tmp/t.out
```

Expected: `OK (1252)` — `std::string` converts implicitly to `std::string_view`.

**Step 5: Commit**

```bash
git add src/util.h src/util.cc
git commit -m "refactor(util): convert string comparison functions to string_view"
```

---

## Task 8: Convert parsing functions to `std::expected`

Replace `bool + out-param` parsing functions with `std::expected`.
These have the best cost/benefit — they already signal errors via bool return.

**Files:**
- Modify: `src/util.h` — declarations
- Modify: `src/util.cc` — implementations
- Modify: callers of these functions (grep for function name)
- Test: `test/UtilTest1.cc`, `test/UtilTest2.cc`

**Step 1: Add `#include <expected>` to `util.h`**

**Step 2: Convert the three `NoThrow` parsing functions**

```cpp
// Before (util.h):
bool parseIntNoThrow(int32_t& res, const std::string& s, int base = 10);
bool parseLLIntNoThrow(int64_t& res, const std::string& s, int base = 10);
bool parseDoubleNoThrow(double& res, const std::string& s);

// After:
std::expected<int32_t, std::string> parseInt(std::string_view s, int base = 10);
std::expected<int64_t, std::string> parseLLInt(std::string_view s, int base = 10);
std::expected<double, std::string> parseDouble(std::string_view s);
```

**Step 3: Keep old functions as deprecated wrappers (temporary)**

To avoid breaking all callers at once:
```cpp
[[deprecated("use parseInt()")]]
inline bool parseIntNoThrow(int32_t& res, const std::string& s, int base = 10) {
  auto r = parseInt(s, base);
  if (r) { res = *r; return true; }
  return false;
}
```

**Step 4: Build, fix deprecation warnings one by one, verify tests**

```bash
make -j$(sysctl -n hw.ncpu) 2>&1 | grep "deprecated"
# Fix callers, then:
make -C test aria2c && cd test && ./aria2c > /tmp/t.out 2>&1 && grep "^OK" /tmp/t.out
```

**Step 5: Once all callers migrated, remove deprecated wrappers**

**Step 6: Commit**

```bash
git add -A
git commit -m "refactor(util): replace parse*NoThrow with std::expected"
```

---

## Task 9: Add `std::optional` to lookup functions

**Files:**
- Modify: `src/HttpHeader.h`, `src/HttpHeader.cc`
- Modify: callers of `HttpHeader::find()` (grep for `->find(` and `.find(`)
- Test: `test/HttpHeaderTest.cc`

**Step 1: Change `HttpHeader::find()` signature**

```cpp
// Before:
const std::string& find(int hdKey) const;

// After:
std::optional<std::string_view> find(int hdKey) const;
```

**Step 2: Update implementation**

```cpp
std::optional<std::string_view> HttpHeader::find(int hdKey) const {
  auto it = table_.find(hdKey);
  if (it == table_.end()) return std::nullopt;
  return it->second;
}
```

**Step 3: Update all callers**

Pattern change:
```cpp
// Before:
const std::string& val = header->find(HD_CONTENT_LENGTH);
if (!val.empty()) { ... }

// After:
auto val = header->find(HD_CONTENT_LENGTH);
if (val) { ... use *val ... }
```

**Step 4: Build and verify**

```bash
make -j$(sysctl -n hw.ncpu) && make -C test aria2c
cd test && ./aria2c > /tmp/t.out 2>&1 && grep "^OK" /tmp/t.out
```

**Step 5: Commit**

```bash
git add -A
git commit -m "refactor(http): use std::optional for HttpHeader::find()"
```

---

## Task 10: Add structured bindings to map iterations

**Files:**
- Modify: `src/ValueBase.cc`, `src/json.cc`, `src/RpcMethodImpl.cc`,
  `src/CookieStorage.cc`, `src/BtRegistry.cc`, `src/DownloadEngine.cc`,
  `src/HttpServer.cc`, `src/RpcResponse.cc`, `src/MetalinkParserController.cc`,
  `src/option_processing.cc`

**Step 1: Search for `.first` / `.second` patterns**

```bash
grep -rn '\.first\b\|\.second\b\|->first\b\|->second\b' src/ \
  --include='*.cc' | grep -i 'for\|iter\|auto'
```

**Step 2: Convert map iterations**

Example from `ValueBase.cc`:
```cpp
// Before:
auto r = dict_.insert(std::move(p));
if (!r.second) {
  (*r.first).second = std::move(p.second);
}

// After:
auto [it, inserted] = dict_.insert(std::move(p));
if (!inserted) {
  it->second = std::move(vlb);
}
```

Example from range-for over maps:
```cpp
// Before:
for (const auto& p : headers_) {
  if (p.first == key) return p.second;
}

// After:
for (const auto& [k, v] : headers_) {
  if (k == key) return v;
}
```

**Step 3: Build and verify**

```bash
make -j$(sysctl -n hw.ncpu) && make -C test aria2c
cd test && ./aria2c > /tmp/t.out 2>&1 && grep "^OK" /tmp/t.out
```

**Step 4: Commit**

```bash
git add -A
git commit -m "refactor: use structured bindings for map/pair iteration"
```

---

## Task 11: Merge Layer 2, start Layer 3

**Step 1: Merge Layer 2**

```bash
git checkout develop
git merge feature/cpp23-layer2-types
git branch -d feature/cpp23-layer2-types
```

**Step 2: Create Layer 3 branch**

```bash
git checkout -b feature/cpp23-layer3-io develop
```

---

## Task 12: Convert binary buffer APIs to `std::span`

**Files:**
- Modify: `src/ISocketCore.h` — interface
- Modify: `src/SocketCore.h`, `src/SocketCore.cc` — implementation
- Modify: `test/MockSocketCore.h` — test mock
- Modify: `src/SocketBuffer.h`, `src/SocketBuffer.cc`
- Modify: callers of `writeData(const void*, size_t)`

**Step 1: Add span overloads (non-breaking)**

Add new overloads alongside existing ones:
```cpp
// ISocketCore.h — add:
virtual ssize_t writeData(std::span<const uint8_t> data) {
  return writeData(data.data(), data.size());
}
```

**Step 2: Build and verify**

```bash
make -j$(sysctl -n hw.ncpu) && make -C test aria2c
cd test && ./aria2c > /tmp/t.out 2>&1 && grep "^OK" /tmp/t.out
```

**Step 3: Migrate callers incrementally**

Convert callsites one file at a time, verifying tests between each.

**Step 4: Commit**

```bash
git add -A
git commit -m "refactor(io): add std::span overloads for binary buffer APIs"
```

---

## Task 13: Introduce `std::format` logging macro

**Files:**
- Modify: `src/LogFactory.h` or `src/a2functional.h` — add A2_FMT macro
- Test: manual verification with existing log output

**Step 1: Add A2_FMT macro**

```cpp
#include <format>

#define A2_FMT(...) std::format(__VA_ARGS__)
```

**Step 2: Convert a small set of fmt() calls as proof-of-concept**

Pick 5-10 callsites in a single file (e.g., `src/DownloadEngine.cc`),
convert from `fmt("...", args)` to `A2_FMT("...", args)`.

**Step 3: Build and verify**

**Step 4: Commit**

```bash
git add -A
git commit -m "refactor: introduce A2_FMT macro using std::format"
```

---

## Task 14: Convert `std::find_if` / `std::sort` to ranges

**Files:**
- Modify: files using `std::find_if`, `std::sort`, `std::any_of`, `std::all_of`,
  `std::none_of`, `std::count_if`, `std::remove_if`, `std::for_each`
  with `.begin()/.end()` patterns

**Step 1: Search for begin/end algorithm patterns**

```bash
grep -rn 'std::find_if\|std::sort\|std::any_of\|std::all_of\|std::count_if' src/ \
  --include='*.cc' --include='*.h' | wc -l
```

**Step 2: Convert to ranges**

```cpp
// Before:
auto it = std::find_if(v.begin(), v.end(), pred);

// After:
auto it = std::ranges::find_if(v, pred);
```

Only use algorithms confirmed available in both GCC 15 and Clang 19 libc++:
`ranges::find`, `ranges::find_if`, `ranges::sort`, `ranges::any_of`,
`ranges::all_of`, `ranges::none_of`, `ranges::count_if`, `ranges::for_each`,
`ranges::remove_if`, `ranges::contains`.

Do NOT use: `views::zip`, `views::chunk`, `views::stride`, `views::enumerate`,
`ranges::fold` (not in libc++ 19).

**Step 3: Build and verify**

**Step 4: Commit**

```bash
git add -A
git commit -m "refactor: migrate STL algorithms to std::ranges"
```

---

## Task 15: Merge Layer 3

```bash
git checkout develop
git merge feature/cpp23-layer3-io
git branch -d feature/cpp23-layer3-io
```

---

## Tasks 16-20: Layers 4 & 5 (Protocols & Engine)

These are larger, riskier changes. Each should be planned in detail after
Layers 1-3 are stable. High-level outline:

### Task 16: HTTP parser string_view (Layer 4)
- `HttpHeader::find()` already converted in Task 9
- Convert `HttpRequest`, `HttpResponse` header building to use `string_view`
- Convert `HttpConnection` request parsing

### Task 17: FTP response parsing with `std::expected` (Layer 4)
- `FtpConnection::receiveResponse()` → `std::expected<int, error_code>`
- EPSV/PASV/PWD parsers use `string_view`

### Task 18: BT message factories with `std::span` (Layer 4)
- `BtHandshakeMessage::create(span<const uint8_t>)`
- Message factories return `std::expected` for parse errors

### Task 19: URI parsing with `std::expected` (Layer 4)
- `uri::parse()` → `std::expected<UriStruct, std::string>`

### Task 20: Deducing this + std::print (Layer 5)
- Simplify CRTP patterns in AbstractCommand hierarchy
- Replace `fprintf(stdout, ...)` with `std::print`
- Move semantics audit

---

## Verification Checklist (after each task)

```bash
# Build
make -j$(sysctl -n hw.ncpu)

# Build tests
make -C test aria2c

# Run tests
cd test && ./aria2c > /tmp/t.out 2>&1
grep "^OK" /tmp/t.out    # Expected: OK (1252)
echo $?                   # Expected: 0
```

## Branch Strategy

```
develop
  ├── feature/cpp23-layer1-foundations  (Tasks 1-5)
  ├── feature/cpp23-layer2-types       (Tasks 7-10)
  ├── feature/cpp23-layer3-io          (Tasks 12-14)
  ├── feature/cpp23-layer4-protocols   (Tasks 16-19)
  └── feature/cpp23-layer5-engine      (Task 20)
```
