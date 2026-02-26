# Phase 2: ISocketCore Interface Extraction

## Goal

Extract a pure virtual `ISocketCore` interface from `SocketCore` to enable mocking
all 36 network-dependent Command classes in tests.

## Approach

**Thin I/O interface** with minimal production code changes. Only the methods
Commands actually use go into the interface. TLS, SSH, multicast, and static
methods stay on the concrete class.

## Interface

New file `src/ISocketCore.h`:

```cpp
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
```

21 methods. `SocketCore` inherits from `ISocketCore` with no implementation change
(all methods already exist).

## Production Code Changes

### src/SocketCore.h

```cpp
// Before
class SocketCore {
// After
class SocketCore : public ISocketCore {
```

### src/AbstractCommand.h

Change 3 member types and all method signatures using them:
- `shared_ptr<SocketCore>` -> `shared_ptr<ISocketCore>` for `socket_`, `readCheckTarget_`, `writeCheckTarget_`
- All getSocket/setSocket/setReadCheckSocket/setWriteCheckSocket signatures

### src/PeerAbstractCommand.h

Same pattern: change `socket_`, `readCheckTarget_`, `writeCheckTarget_`.

### src/DownloadEngine.h

Change 4 event poll registration methods (they only call `getSockfd()`):
- `addSocketForReadCheck(shared_ptr<SocketCore>&)` -> `shared_ptr<ISocketCore>&`
- `deleteSocketForReadCheck(shared_ptr<SocketCore>&)` -> `shared_ptr<ISocketCore>&`
- `addSocketForWriteCheck(shared_ptr<SocketCore>&)` -> `shared_ptr<ISocketCore>&`
- `deleteSocketForWriteCheck(shared_ptr<SocketCore>&)` -> `shared_ptr<ISocketCore>&`

Socket pool methods stay as `shared_ptr<SocketCore>` (pooling needs concrete type
for TLS state, not needed in tests).

### Subclass callsites needing TLS/SSH

Commands that call TLS or SSH methods (HttpRequestCommand, SftpNegotiationCommand,
etc.) access the concrete type via `static_pointer_cast<SocketCore>`. This is safe
because in production the socket is always a SocketCore.

## Test Infrastructure

New file `test/MockSocketCore.h`:

```cpp
class MockSocketCore : public ISocketCore {
public:
  bool readable = false;
  bool writable = false;
  bool open = true;
  bool wantRead_ = false;
  bool wantWrite_ = false;
  std::string readBuffer;
  size_t readPos = 0;
  std::vector<std::string> writtenData;
  Endpoint addrInfo;
  Endpoint peerInfo;
  // ... all 21 methods return controllable values
};
```

## Files Touched

**New (2):**
- `src/ISocketCore.h`
- `test/MockSocketCore.h`

**Modified (5):**
- `src/SocketCore.h` (add base class)
- `src/AbstractCommand.h` (change member types)
- `src/AbstractCommand.cc` (adjust any casts if needed)
- `src/PeerAbstractCommand.h` (change member types)
- `src/DownloadEngine.h` + `.cc` (change 4 method signatures)

**Estimated diff:** ~150 lines production, ~200 lines test mock.

## Testing Unlocked

Priority Command classes to test with MockSocketCore:
1. ConnectCommand (connection establishment)
2. HttpRequestCommand / HttpResponseCommand (HTTP flow)
3. FtpNegotiationCommand (FTP state machine)
4. DownloadCommand / HttpDownloadCommand (data transfer)
5. PeerInteractionCommand (BT wire protocol)

## Risk

- **Runtime:** One vtable lookup per socket call, negligible vs actual I/O.
- **Binary compat:** N/A, not a library.
- **Regression:** Adding a base class changes no behavior.
- **Rollback:** Remove inheritance, revert 5 files.
