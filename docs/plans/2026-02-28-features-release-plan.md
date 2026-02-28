# Features & Release Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Cherry-pick quick-win PRs, implement slow-start connection scaling, add SOCKS5 proxy support, release.

**Architecture:** Command pipeline pattern for SOCKS5 (matching existing HTTP proxy). SlowStartController as a value object in FileEntry for connection ramp-up. Cherry-picks via `git cherry-pick` with conflict resolution.

**Tech Stack:** C++11, CppUnit, Autotools, SOCKS5 RFC 1928

---

### Task 1: Cherry-pick clean PRs

**Files:**
- No new files — cherry-pick only

**Step 1: Cherry-pick all 6 clean PRs**

```bash
# From develop branch
git cherry-pick <sha-for-2323>  # literal operator spacing
git cherry-pick <sha-for-1543>  # NO_COLOR support
git cherry-pick <sha-for-1696>  # remove max-conn 16 cap
git cherry-pick <sha-for-1714>  # retry-wait on 404
git cherry-pick <sha-for-1777>  # XML-RPC memory leak
git cherry-pick <sha-for-2209>  # waitData CPU spin fix
```

Fetch the actual SHAs from upstream/master for each PR's merge commit.

**Step 2: Cherry-pick #2268 with conflict resolution**

```bash
git cherry-pick <sha-for-2268>  # deflate raw mode
# Resolve any conflicts in src/HttpResponseCommand.cc
```

**Step 3: Build and test**

```bash
make -j$(sysctl -n hw.ncpu) check
```

**Step 4: Commit message for each cherry-pick**

Each cherry-pick preserves original commit message. Add `(cherry picked from commit <sha>)` trailer.

---

### Task 2: SlowStartController — unit tests first

**Files:**
- Create: `src/SlowStartController.h`
- Create: `test/SlowStartControllerTest.cc`
- Modify: `test/Makefile.am`

**Step 1: Write the test file**

```cpp
// test/SlowStartControllerTest.cc
#include "SlowStartController.h"
#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class SlowStartControllerTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(SlowStartControllerTest);
  CPPUNIT_TEST(testInitialLimit);
  CPPUNIT_TEST(testRampUp);
  CPPUNIT_TEST(testStable);
  CPPUNIT_TEST(testBackOff);
  CPPUNIT_TEST(testCeiling);
  CPPUNIT_TEST(testBackOffFloor);
  CPPUNIT_TEST_SUITE_END();

public:
  void testInitialLimit();
  void testRampUp();
  void testStable();
  void testBackOff();
  void testCeiling();
  void testBackOffFloor();
};

CPPUNIT_TEST_SUITE_REGISTRATION(SlowStartControllerTest);

void SlowStartControllerTest::testInitialLimit()
{
  SlowStartController ctrl(16); // ceiling = 16
  CPPUNIT_ASSERT_EQUAL(1, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testRampUp()
{
  SlowStartController ctrl(16);
  // Speed increased → should double
  ctrl.update(1000);  // 1 KB/s
  ctrl.update(2000);  // 2 KB/s — speed increased
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
  ctrl.update(4000);  // speed increased again
  CPPUNIT_ASSERT_EQUAL(4, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testStable()
{
  SlowStartController ctrl(16);
  ctrl.update(1000);
  ctrl.update(2000);  // ramp to 2
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
  ctrl.update(2050);  // < 10% increase → stable, no change
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testBackOff()
{
  SlowStartController ctrl(16);
  ctrl.update(1000);
  ctrl.update(2000);  // 2
  ctrl.update(4000);  // 4
  ctrl.update(8000);  // 8
  CPPUNIT_ASSERT_EQUAL(8, ctrl.getAllowedConnections());
  ctrl.backOff();     // halve → 4
  CPPUNIT_ASSERT_EQUAL(4, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testCeiling()
{
  SlowStartController ctrl(4); // ceiling = 4
  ctrl.update(1000);
  ctrl.update(2000);  // 2
  ctrl.update(4000);  // 4 (ceiling)
  ctrl.update(8000);  // still 4
  CPPUNIT_ASSERT_EQUAL(4, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testBackOffFloor()
{
  SlowStartController ctrl(16);
  ctrl.backOff();  // already at 1, stays at 1
  CPPUNIT_ASSERT_EQUAL(1, ctrl.getAllowedConnections());
}

} // namespace aria2
```

**Step 2: Write the header (minimal, make tests compile)**

```cpp
// src/SlowStartController.h
#ifndef D_SLOW_START_CONTROLLER_H
#define D_SLOW_START_CONTROLLER_H

#include "common.h"
#include <algorithm>

namespace aria2 {

class SlowStartController {
public:
  explicit SlowStartController(int ceiling = 16)
    : ceiling_(ceiling), allowed_(1), lastSpeed_(0)
  {
  }

  int getAllowedConnections() const { return allowed_; }

  // Call periodically with aggregate speed (bytes/sec).
  // Doubles allowed if speed increased by >= 10%.
  void update(int currentSpeed)
  {
    if (lastSpeed_ > 0) {
      double gain = static_cast<double>(currentSpeed - lastSpeed_) / lastSpeed_;
      if (gain >= 0.10) {
        allowed_ = std::min(allowed_ * 2, ceiling_);
      }
    }
    lastSpeed_ = currentSpeed;
  }

  // Halve on 403 or connection failure.
  void backOff()
  {
    allowed_ = std::max(1, allowed_ / 2);
  }

  void reset()
  {
    allowed_ = 1;
    lastSpeed_ = 0;
  }

private:
  int ceiling_;
  int allowed_;
  int lastSpeed_;
};

} // namespace aria2

#endif // D_SLOW_START_CONTROLLER_H
```

**Step 3: Register test in Makefile.am, build and run**

Add `SlowStartControllerTest.cc` to `test/Makefile.am`.

```bash
make -C test aria2c && cd test && ./aria2c
```

**Step 4: Commit**

```
test(core): add SlowStartController with AIMD ramp-up
```

---

### Task 3: Integrate SlowStartController into FileEntry

**Files:**
- Modify: `src/FileEntry.h`
- Modify: `src/FileEntry.cc`
- Modify: `test/FileEntryTest.cc`

**Step 1: Add SlowStartController to FileEntry**

In `FileEntry.h`:
- Add `#include "SlowStartController.h"`
- Add member: `SlowStartController slowStart_;`
- Add methods: `SlowStartController& getSlowStart()` and `const SlowStartController& getSlowStart() const`

In `FileEntry.cc` constructor:
- Initialize `slowStart_(maxConnectionPerServer_)`

**Step 2: Use slowStart_ in connection enforcement**

In `FileEntry.cc:147` (`getRequestWithInFlightHosts()`):
Replace:
```cpp
if (std::ranges::count(inFlightHosts, req->getHost()) >= maxConnectionPerServer_)
```
With:
```cpp
int effectiveLimit = std::min(maxConnectionPerServer_,
                              slowStart_.getAllowedConnections());
if (std::ranges::count(inFlightHosts, req->getHost()) >= effectiveLimit)
```

**Step 3: Add FileEntryTest for slow-start integration**

Test that FileEntry respects the slow-start limit when returning requests.

**Step 4: Build and test**

```bash
make -j$(sysctl -n hw.ncpu) check
```

**Step 5: Commit**

```
feat(core): integrate slow-start connection scaling into FileEntry
```

---

### Task 4: Wire slow-start speed updates

**Files:**
- Modify: `src/DownloadCommand.cc` or `src/AbstractCommand.cc`
- Modify: `src/HttpSkipResponseCommand.cc` (for 403 back-off)

**Step 1: Add speed sampling**

In the download loop (where PeerStat speed is already tracked), periodically
call `fileEntry->getSlowStart().update(aggregateSpeed)`.

Best location: `AbstractCommand::executeInternal()` or in
`DownloadCommand::executeInternal()` where speed is already calculated.

**Step 2: Add 403 back-off**

In `HttpSkipResponseCommand` when HTTP status == 403:
```cpp
getFileEntry()->getSlowStart().backOff();
```

**Step 3: Build and test**

```bash
make -j$(sysctl -n hw.ncpu) check
```

**Step 4: Commit**

```
feat(core): wire slow-start speed sampling and 403 back-off
```

---

### Task 5: SOCKS5 proxy options and preferences

**Files:**
- Modify: `src/prefs.h`
- Modify: `src/prefs.cc`
- Modify: `src/OptionHandlerFactory.cc`
- Modify: `src/usage_text.h`

**Step 1: Add preference declarations**

In `src/prefs.h` near other proxy prefs:
```cpp
extern PrefPtr PREF_SOCKS5_PROXY;
extern PrefPtr PREF_SOCKS5_PROXY_USER;
extern PrefPtr PREF_SOCKS5_PROXY_PASSWD;
```

In `src/prefs.cc`:
```cpp
PrefPtr PREF_SOCKS5_PROXY = makePref("socks5-proxy");
PrefPtr PREF_SOCKS5_PROXY_USER = makePref("socks5-proxy-user");
PrefPtr PREF_SOCKS5_PROXY_PASSWD = makePref("socks5-proxy-passwd");
```

**Step 2: Register option handlers**

In `src/OptionHandlerFactory.cc`, add entries following the HTTP proxy
pattern (using `HttpProxyOptionHandler` for URI validation).

**Step 3: Add usage text**

In `src/usage_text.h`:
```cpp
#define TEXT_SOCKS5_PROXY \
  _(" --socks5-proxy=PROXY         Use a SOCKS5 proxy. Specify as\n" \
    "                              host:port. This affects all protocols.")
```

**Step 4: Build (no test yet — options only)**

```bash
make -j$(sysctl -n hw.ncpu)
```

**Step 5: Commit**

```
feat(proxy): add SOCKS5 proxy option definitions
```

---

### Task 6: SOCKS5 handshake command

**Files:**
- Create: `src/Socks5HandshakeCommand.h`
- Create: `src/Socks5HandshakeCommand.cc`
- Modify: `src/Makefile.am`

**Step 1: Write Socks5HandshakeCommand**

Sends SOCKS5 greeting (version + auth methods), reads server's chosen method,
sends username/password if needed, reads auth response.

States: GREETING_SEND → GREETING_RECV → AUTH_SEND → AUTH_RECV → done.

When done, creates `Socks5ConnectCommand` as next command.

```cpp
class Socks5HandshakeCommand : public AbstractCommand {
public:
  Socks5HandshakeCommand(cuid_t cuid,
                         const std::shared_ptr<Request>& req,
                         const std::shared_ptr<FileEntry>& fileEntry,
                         RequestGroup* requestGroup,
                         DownloadEngine* e,
                         const std::shared_ptr<Request>& proxyRequest,
                         const std::shared_ptr<ISocketCore>& s);

protected:
  bool executeInternal() override;

private:
  enum State { GREETING_SEND, GREETING_RECV, AUTH_SEND, AUTH_RECV, DONE };
  State state_;
  std::shared_ptr<Request> proxyRequest_;
  std::string sendBuf_;
  std::string recvBuf_;
};
```

**Step 2: Build**

```bash
make -j$(sysctl -n hw.ncpu)
```

**Step 3: Commit**

```
feat(proxy): add SOCKS5 handshake command
```

---

### Task 7: SOCKS5 connect command

**Files:**
- Create: `src/Socks5ConnectCommand.h`
- Create: `src/Socks5ConnectCommand.cc`
- Modify: `src/Makefile.am`

**Step 1: Write Socks5ConnectCommand**

Sends SOCKS5 CONNECT request (version + CMD=0x01 + address type + dest),
reads response. On success, chains to the target protocol command
(HttpRequestCommand, FtpNegotiationCommand, etc.) — same socket, now
tunneled through SOCKS.

States: CONNECT_SEND → CONNECT_RECV → done.

```cpp
class Socks5ConnectCommand : public AbstractCommand {
public:
  Socks5ConnectCommand(cuid_t cuid,
                       const std::shared_ptr<Request>& req,
                       const std::shared_ptr<FileEntry>& fileEntry,
                       RequestGroup* requestGroup,
                       DownloadEngine* e,
                       const std::shared_ptr<Request>& proxyRequest,
                       const std::shared_ptr<ISocketCore>& s);

protected:
  bool executeInternal() override;

private:
  enum State { CONNECT_SEND, CONNECT_RECV };
  State state_;
  std::shared_ptr<Request> proxyRequest_;
  std::string sendBuf_;
};
```

The target command creation follows the same pattern as
`HttpProxyResponseCommand::getNextCommand()`.

**Step 2: Build**

```bash
make -j$(sysctl -n hw.ncpu)
```

**Step 3: Commit**

```
feat(proxy): add SOCKS5 connect command
```

---

### Task 8: SOCKS5 control chain and routing

**Files:**
- Create: `src/Socks5RequestConnectChain.h`
- Modify: `src/AbstractCommand.cc`
- Modify: `src/HttpInitiateConnectionCommand.cc`
- Modify: `src/FtpInitiateConnectionCommand.cc`

**Step 1: Create control chain**

```cpp
// src/Socks5RequestConnectChain.h
struct Socks5RequestConnectChain : public ControlChain<ConnectCommand*> {
  int run(ConnectCommand* t, DownloadEngine* e) override
  {
    auto c = make_unique<Socks5HandshakeCommand>(
        t->getCuid(), t->getRequest(), t->getFileEntry(),
        t->getRequestGroup(), e, t->getProxyRequest(), t->getSocket());
    c->setStatus(Command::STATUS_ONESHOT_REALTIME);
    e->setNoWait(true);
    e->addCommand(std::move(c));
    return 0;
  }
};
```

**Step 2: Modify getProxyUri() to check SOCKS5**

In `AbstractCommand.cc`, when the SOCKS5 proxy option is set, return it
(with priority: protocol-specific HTTP proxy > SOCKS5 > all-proxy).

**Step 3: Modify InitiateConnectionCommands**

In `HttpInitiateConnectionCommand.cc` and `FtpInitiateConnectionCommand.cc`,
detect SOCKS5 proxy and set `Socks5RequestConnectChain` instead of
`HttpProxyRequestConnectChain`.

**Step 4: Build and test**

```bash
make -j$(sysctl -n hw.ncpu) check
```

**Step 5: Commit**

```
feat(proxy): route connections through SOCKS5 when configured
```

---

### Task 9: SOCKS5 unit tests

**Files:**
- Create: `test/Socks5CommandTest.cc`
- Modify: `test/Makefile.am`

**Step 1: Write tests**

Test the byte-level protocol encoding:
- Greeting with no auth
- Greeting with user/pass auth
- Username/password auth request
- CONNECT request with domain name
- CONNECT request with IPv4
- Response parsing (success, failure codes)
- Back-off on SOCKS5 failure

Use `MockSocketCore` to inject/capture bytes.

**Step 2: Build and run**

```bash
make -C test aria2c && cd test && ./aria2c
```

**Step 3: Commit**

```
test(proxy): add SOCKS5 command unit tests
```

---

### Task 10: Final verification and release prep

**Step 1: Full build and test**

```bash
make distclean
autoreconf -i
./configure [flags]
make -j$(sysctl -n hw.ncpu) check
```

**Step 2: Verify all tests pass**

Expected: all tests pass, no regressions.

**Step 3: Update version if needed**

Check `configure.ac` for version string.

**Step 4: Commit any final touches**

```
chore: prepare release
```
