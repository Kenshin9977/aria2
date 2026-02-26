# ISocketCore Interface Extraction — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Extract a pure virtual `ISocketCore` interface from `SocketCore` to enable mocking network I/O in Command tests.

**Architecture:** Add `ISocketCore` as a base class for `SocketCore` with 21 methods. Change `shared_ptr<SocketCore>` to `shared_ptr<ISocketCore>` in `AbstractCommand`, `PeerAbstractCommand`, and `DownloadEngine` event poll methods. Create `MockSocketCore` for tests.

**Tech Stack:** C++11, CppUnit, CXX11_OVERRIDE macro, shared_ptr ownership

---

### Task 1: Create ISocketCore interface header

**Files:**
- Create: `src/ISocketCore.h`

**Step 1: Write the interface header**

```cpp
#ifndef D_ISOCKET_CORE_H
#define D_ISOCKET_CORE_H

#include "common.h"

#include <string>

#include "a2netcompat.h"
#include "a2io.h"

namespace aria2 {

class ISocketCore {
public:
  virtual ~ISocketCore() = default;

  // I/O operations
  virtual ssize_t writeData(const void* data, size_t len) = 0;
  virtual ssize_t writeData(const void* data, size_t len,
                            const std::string& host, uint16_t port) = 0;
  virtual void readData(void* data, size_t& len) = 0;
  virtual ssize_t readDataFrom(void* data, size_t len, Endpoint& sender) = 0;
  virtual ssize_t writeVector(a2iovec* iov, size_t iovcnt) = 0;

  // I/O readiness
  virtual bool isReadable(time_t timeout) = 0;
  virtual bool isWritable(time_t timeout) = 0;
  virtual bool wantRead() const = 0;
  virtual bool wantWrite() const = 0;

  // Socket state
  virtual sock_t getSockfd() const = 0;
  virtual bool isOpen() const = 0;
  virtual Endpoint getAddrInfo() const = 0;
  virtual Endpoint getPeerInfo() const = 0;
  virtual std::string getSocketError() const = 0;
  virtual size_t getRecvBufferedLength() const = 0;
  virtual int getAddressFamily() const = 0;

  // Connection control
  virtual void closeConnection() = 0;
  virtual void setBlockingMode() = 0;
  virtual void setNonBlockingMode() = 0;
  virtual void setTcpNodelay(bool f) = 0;
};

} // namespace aria2

#endif // D_ISOCKET_CORE_H
```

**Step 2: Verify it compiles standalone**

Run: `cd test && make aria2c 2>&1 | tail -5`
Expected: successful compilation (no code includes it yet)

**Step 3: Commit**

```
git add src/ISocketCore.h
git commit -m "refactor(net): add ISocketCore pure virtual interface"
```

---

### Task 2: Make SocketCore inherit from ISocketCore

**Files:**
- Modify: `src/SocketCore.h:62` — add base class

**Step 1: Add inheritance**

In `src/SocketCore.h`, change line 62:
```cpp
// Before:
class SocketCore {
// After:
class SocketCore : public ISocketCore {
```

Add include at `src/SocketCore.h:38` (after `#include "common.h"`):
```cpp
#include "ISocketCore.h"
```

**Step 2: Build the project**

Run: `cd test && make aria2c 2>&1 | tail -10`
Expected: compiles cleanly. SocketCore already implements all 21 ISocketCore methods.

**Step 3: Run tests**

Run: `cd test && ./aria2c 2>&1 | grep -c "^F"` (should be 0)

**Step 4: Commit**

```
git add src/SocketCore.h
git commit -m "refactor(net): make SocketCore inherit ISocketCore"
```

---

### Task 3: Change AbstractCommand to use ISocketCore

**Files:**
- Modify: `src/AbstractCommand.h:57,69,71,72,115,117,119,143,145,155,161,166,182,230`
- Modify: `src/AbstractCommand.cc:618,835-836`

**Step 1: Update AbstractCommand.h**

Replace forward declaration (line 57):
```cpp
// Before:
class SocketCore;
// After:
class ISocketCore;
class SocketCore;
```

Note: keep `class SocketCore;` because `createSocket()` still returns concrete type internally.

Replace member types (lines 69,71,72):
```cpp
  std::shared_ptr<ISocketCore> socket_;
  std::shared_ptr<SocketCore> socketRecvBuffer_;  // unchanged
  std::shared_ptr<ISocketCore> readCheckTarget_;
  std::shared_ptr<ISocketCore> writeCheckTarget_;
```

Replace method signatures:
- Line 115: `const std::shared_ptr<ISocketCore>& getSocket() const`
- Line 117: `std::shared_ptr<ISocketCore>& getSocket()`
- Line 119: `void setSocket(const std::shared_ptr<ISocketCore>& s);`
- Line 143: `void setReadCheckSocket(const std::shared_ptr<ISocketCore>& socket);`
- Line 145: `void setWriteCheckSocket(const std::shared_ptr<ISocketCore>& socket);`
- Line 155: `void setReadCheckSocketIf(const std::shared_ptr<ISocketCore>& socket, bool pred);`
- Line 161: `void setWriteCheckSocketIf(const std::shared_ptr<ISocketCore>& socket, bool pred);`
- Line 166: `void swapSocket(std::shared_ptr<ISocketCore>& socket);`
- Line 182: `bool checkIfConnectionEstablished(const std::shared_ptr<ISocketCore>& socket, ...);`
- Line 230: constructor param `const std::shared_ptr<ISocketCore>& s = nullptr`

**Step 2: Update AbstractCommand.cc**

- Line 618 (`swapSocket`): change param to `shared_ptr<ISocketCore>&`
- Line 835 (`checkIfConnectionEstablished`): change param to `shared_ptr<ISocketCore>&`
- All other method bodies work without change (they call ISocketCore methods)

**Step 3: Build**

Run: `cd test && make aria2c 2>&1 | tail -20`
Expected: may show errors in subclasses that pass `shared_ptr<SocketCore>` to changed signatures. Fix in next tasks.

**Step 4: Do NOT commit yet** — wait until all dependent changes compile.

---

### Task 4: Change DownloadEngine event poll methods

**Files:**
- Modify: `src/DownloadEngine.h:185-192`
- Modify: `src/DownloadEngine.cc:202-228`

**Step 1: Update DownloadEngine.h**

Lines 185-192: change all 4 methods:
```cpp
  bool addSocketForReadCheck(const std::shared_ptr<ISocketCore>& socket,
                             Command* command);
  bool deleteSocketForReadCheck(const std::shared_ptr<ISocketCore>& socket,
                                Command* command);
  bool addSocketForWriteCheck(const std::shared_ptr<ISocketCore>& socket,
                              Command* command);
  bool deleteSocketForWriteCheck(const std::shared_ptr<ISocketCore>& socket,
                                 Command* command);
```

Add forward declaration near top (after existing `class SocketCore;`):
```cpp
class ISocketCore;
```

**Step 2: Update DownloadEngine.cc**

Lines 202-228: change all 4 method implementations to match. Bodies unchanged —
they only call `socket->getSockfd()` which is on ISocketCore.

**Step 3: Build to check progress**

Run: `cd test && make aria2c 2>&1 | head -30`

---

### Task 5: Change PeerAbstractCommand to use ISocketCore

**Files:**
- Modify: `src/PeerAbstractCommand.h:49,56,61,62,68,86,87,97`
- Modify: `src/PeerAbstractCommand.cc` — corresponding method implementations

**Step 1: Update PeerAbstractCommand.h**

Replace forward declaration (line 49):
```cpp
class ISocketCore;
class SocketCore;
```

Replace member types:
- Line 56: `std::shared_ptr<ISocketCore> socket_;`
- Line 61: `std::shared_ptr<ISocketCore> readCheckTarget_;`
- Line 62: `std::shared_ptr<ISocketCore> writeCheckTarget_;`

Replace method signatures:
- Line 68: `const std::shared_ptr<ISocketCore>& getSocket() const`
- Line 86: `void setReadCheckSocket(const std::shared_ptr<ISocketCore>& socket);`
- Line 87: `void setWriteCheckSocket(const std::shared_ptr<ISocketCore>& socket);`
- Line 97: constructor param `const std::shared_ptr<ISocketCore>& s = std::shared_ptr<ISocketCore>()`

**Step 2: Update PeerAbstractCommand.cc**

Update method signatures to match. Bodies unchanged.

**Step 3: Build to check progress**

---

### Task 6: Fix BackupConnectInfo and TLS/SSH callsites

**Files:**
- Modify: `src/BackupIPv4ConnectCommand.h:58` — `shared_ptr<ISocketCore> socket;`
- Modify: `src/HttpRequestCommand.cc:129` — cast for tlsConnect
- Modify: `src/HttpServerCommand.cc:186` — cast for tlsAccept
- Modify: `src/SftpNegotiationCommand.cc:100,107` — cast for SSH methods

**Step 1: Fix BackupConnectInfo**

In `src/BackupIPv4ConnectCommand.h`, line 58:
```cpp
// Before:
  std::shared_ptr<SocketCore> socket;
// After:
  std::shared_ptr<ISocketCore> socket;
```

Add forward declaration of `ISocketCore` and remove or keep `SocketCore` as needed.

**Step 2: Fix TLS callsites**

In `src/HttpRequestCommand.cc`, around line 129:
```cpp
// Before:
      if (!getSocket()->tlsConnect(getRequest()->getHost())) {
// After:
      if (!std::static_pointer_cast<SocketCore>(getSocket())->tlsConnect(getRequest()->getHost())) {
```

In `src/HttpServerCommand.cc`, around line 186:
```cpp
// Before:
        if (!socket_->tlsAccept()) {
// After:
        if (!std::static_pointer_cast<SocketCore>(socket_)->tlsAccept()) {
```

In `src/SftpNegotiationCommand.cc`, around lines 100 and 107:
```cpp
// Before:
      if (!getSocket()->sshHandshake(hashType_, digest_)) {
// After:
      if (!std::static_pointer_cast<SocketCore>(getSocket())->sshHandshake(hashType_, digest_)) {
```
Same pattern for `sshAuthPassword` on line 107.

Add `#include "SocketCore.h"` in each file if not already present.

**Step 3: Full build and test**

Run: `cd test && make aria2c 2>&1 | tail -10`
Expected: compiles cleanly.

Run: `cd test && ./aria2c 2>&1 | grep -c "^F"` (should be 0)

**Step 4: Commit all production changes**

```
git add src/ISocketCore.h src/SocketCore.h \
        src/AbstractCommand.h src/AbstractCommand.cc \
        src/PeerAbstractCommand.h src/PeerAbstractCommand.cc \
        src/DownloadEngine.h src/DownloadEngine.cc \
        src/BackupIPv4ConnectCommand.h \
        src/HttpRequestCommand.cc src/HttpServerCommand.cc \
        src/SftpNegotiationCommand.cc
git commit -m "refactor(net): extract ISocketCore interface from SocketCore"
```

---

### Task 7: Create MockSocketCore test infrastructure

**Files:**
- Create: `test/MockSocketCore.h`

**Step 1: Write MockSocketCore**

```cpp
#ifndef D_MOCK_SOCKET_CORE_H
#define D_MOCK_SOCKET_CORE_H

#include "ISocketCore.h"

#include <string>
#include <vector>
#include <cstring>

#include "a2functional.h"

namespace aria2 {

class MockSocketCore : public ISocketCore {
public:
  // Controllable state
  bool readable = false;
  bool writable = false;
  bool open = true;
  bool wantRead_ = false;
  bool wantWrite_ = false;
  sock_t fd = 0;
  Endpoint addrInfo;
  Endpoint peerInfo;
  std::string socketError;
  int addressFamily = AF_INET;

  // Read buffer: data to return from readData()
  std::string readBuffer;
  size_t readPos = 0;

  // Write log: data written via writeData()
  std::vector<std::string> writtenData;

  // I/O operations
  ssize_t writeData(const void* data, size_t len) override
  {
    writtenData.emplace_back(static_cast<const char*>(data), len);
    return len;
  }

  ssize_t writeData(const void* data, size_t len, const std::string& host,
                    uint16_t port) override
  {
    return writeData(data, len);
  }

  void readData(void* data, size_t& len) override
  {
    size_t avail = readBuffer.size() - readPos;
    size_t n = std::min(len, avail);
    std::memcpy(data, readBuffer.data() + readPos, n);
    readPos += n;
    len = n;
  }

  ssize_t readDataFrom(void* data, size_t len, Endpoint& sender) override
  {
    size_t n = len;
    readData(data, n);
    sender = peerInfo;
    return n;
  }

  ssize_t writeVector(a2iovec* iov, size_t iovcnt) override
  {
    ssize_t total = 0;
    for (size_t i = 0; i < iovcnt; ++i) {
      total += writeData(iov[i].iov_base, iov[i].iov_len);
    }
    return total;
  }

  // I/O readiness
  bool isReadable(time_t timeout) override { return readable; }
  bool isWritable(time_t timeout) override { return writable; }
  bool wantRead() const override { return wantRead_; }
  bool wantWrite() const override { return wantWrite_; }

  // Socket state
  sock_t getSockfd() const override { return fd; }
  bool isOpen() const override { return open; }
  Endpoint getAddrInfo() const override { return addrInfo; }
  Endpoint getPeerInfo() const override { return peerInfo; }
  std::string getSocketError() const override { return socketError; }
  size_t getRecvBufferedLength() const override { return 0; }
  int getAddressFamily() const override { return addressFamily; }

  // Connection control
  void closeConnection() override { open = false; }
  void setBlockingMode() override {}
  void setNonBlockingMode() override {}
  void setTcpNodelay(bool f) override {}
};

} // namespace aria2

#endif // D_MOCK_SOCKET_CORE_H
```

**Step 2: Verify it compiles**

Write a trivial inclusion test — just `#include "MockSocketCore.h"` in any
existing test .cc file temporarily, then build. Or wait for Task 8.

**Step 3: Commit**

```
git add test/MockSocketCore.h
git commit -m "test(net): add MockSocketCore for ISocketCore testing"
```

---

### Task 8: Write first Command test using MockSocketCore

**Files:**
- Create: `test/AbstractCommandTest.cc`
- Modify: `test/Makefile.am` — add to aria2c_SOURCES

**Step 1: Identify test targets**

Test `AbstractCommand::execute()` flow:
- Socket readability check with mock
- Socket writability check with mock
- Timeout behavior

Subclass `AbstractCommand` with a mock `executeInternal()` and test the
base class socket-check logic.

**Step 2: Write the test**

Create `test/AbstractCommandTest.cc` with:
- A `StubCommand` subclass of `AbstractCommand` that records calls
- Tests for socket readable/writable detection using `MockSocketCore`
- Wire into Makefile.am

**Step 3: Build and run**

Run: `cd test && make aria2c && ./aria2c 2>&1 | grep -c "^F"`

**Step 4: Commit**

```
git add test/AbstractCommandTest.cc test/Makefile.am
git commit -m "test(engine): add AbstractCommand socket mock tests"
```

---

## Implementation Order

1. **Task 1** — Create `ISocketCore.h` (standalone, zero risk)
2. **Task 2** — Make `SocketCore` inherit (additive, zero behavior change)
3. **Tasks 3-6** — Production code changes (do as one batch, commit together)
4. **Task 7** — Create `MockSocketCore.h` (test infra)
5. **Task 8** — First test using the mock (validates the whole chain works)

## Potential Compilation Issues

During Tasks 3-6, expect cascading compiler errors as `shared_ptr<SocketCore>` /
`shared_ptr<ISocketCore>` mismatches surface. The plan accounts for the known
callsites, but there may be a few more. Fix each by:
1. If the callee only uses ISocketCore methods → change signature to ISocketCore
2. If the callee needs SocketCore-specific methods → use `static_pointer_cast`

## Verification

After all tasks, the full test suite must pass:
```
cd test && make aria2c && ./aria2c 2>&1 | grep "^F"
```
Expected: no output (no failures). Exit 139 is the pre-existing segfault, harmless.
