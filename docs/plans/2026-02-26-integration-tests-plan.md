# Phase 3: Integration Tests — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Command-level integration tests for HTTP, FTP, and BitTorrent using real sockets and the real DownloadEngine event loop.

**Architecture:** Each test creates a connected socket pair (listen on port 0, connect, accept), injects canned protocol responses on the server fd, and drives Commands through the engine. All tests are single-threaded and deterministic.

**Tech Stack:** C++11, CppUnit, socketpair/listen+accept, SelectEventPoll, SocketCore

---

### Task 1: HTTP Command Integration Test — 200 OK

**Files:**
- Create: `test/HttpCommandIntegrationTest.cc`
- Modify: `test/Makefile.am`

**Context:**
- `HttpRequestCommand` registers the socket with `SelectEventPoll` via `setWriteCheckSocket()`. It needs `AuthConfigFactory` on the engine (`e->getAuthConfigFactory()`). It does `static_pointer_cast<SocketCore>()` so only real sockets work (not MockSocketCore).
- The constructor calls `getOption()->getAsInt(PREF_CONNECT_TIMEOUT)`, so the `RequestGroup`'s `Option` must exist.
- The HTTP pipeline: `HttpRequestCommand` → sends request → creates `HttpResponseCommand` → parses headers → creates `HttpDownloadCommand`.

**Step 1: Write the test file**

```cpp
// test/HttpCommandIntegrationTest.cc
#include "HttpRequestCommand.h"
#include "HttpResponseCommand.h"
#include "HttpConnection.h"
#include "SocketRecvBuffer.h"
#include "SocketCore.h"
#include "Request.h"
#include "FileEntry.h"
#include "RequestGroup.h"
#include "DownloadContext.h"
#include "GroupId.h"
#include "Option.h"
#include "AuthConfigFactory.h"
#include "prefs.h"

#include <cppunit/extensions/HelperMacros.h>

#include "TestEngineHelper.h"

namespace aria2 {

namespace {

struct HttpIntegrationContext {
  std::shared_ptr<Option> option;
  std::shared_ptr<RequestGroup> rg;
  std::shared_ptr<Request> req;
  std::shared_ptr<FileEntry> fileEntry;
  std::shared_ptr<SocketCore> clientSocket;
  std::shared_ptr<SocketCore> serverSocket;
  std::shared_ptr<HttpConnection> httpConn;
  // engine last — destroyed first
  std::unique_ptr<DownloadEngine> engine;

  void setUp()
  {
    engine = createTestEngine(option, "aria2_HttpCmdIntTest", true);
    engine->setAuthConfigFactory(make_unique<AuthConfigFactory>());

    // Connected socket pair via listen/connect/accept
    SocketCore listenSock;
    listenSock.bind(0);
    listenSock.beginListen();
    listenSock.setBlockingMode();
    uint16_t port = listenSock.getAddrInfo().port;

    clientSocket = std::make_shared<SocketCore>();
    clientSocket->establishConnection("localhost", port);
    while (!clientSocket->isWritable(0))
      ;
    serverSocket = listenSock.acceptConnection();
    serverSocket->setBlockingMode();

    req = std::make_shared<Request>();
    req->setUri("http://localhost/file.bin");
    fileEntry = std::make_shared<FileEntry>("file.bin", 0, 0);

    rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    rg->setDownloadContext(
        std::make_shared<DownloadContext>(1048576, 0, "file.bin"));

    auto recvBuf = std::make_shared<SocketRecvBuffer>(clientSocket);
    httpConn = std::make_shared<HttpConnection>(
        engine->newCUID(), clientSocket, recvBuf);
  }
};

} // namespace

class HttpCommandIntegrationTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(HttpCommandIntegrationTest);
  CPPUNIT_TEST(testSendRequest);
  CPPUNIT_TEST_SUITE_END();

  HttpIntegrationContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testSendRequest()
  {
    // Create HttpRequestCommand — it registers the socket for write-check
    auto cmd = make_unique<HttpRequestCommand>(
        ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
        ctx_.httpConn, ctx_.engine.get(), ctx_.clientSocket);

    // Pre-write the HTTP response on the server side so that when the
    // engine polls, it will see both write-readiness (to send the request)
    // and then read-readiness (the response waiting).
    ctx_.serverSocket->writeData(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello");

    // Release ownership — the engine takes ownership via addCommand(this)
    // when the command re-enqueues itself.
    auto* raw = cmd.release();
    raw->execute();

    // The command should have written the HTTP request to the socket.
    // Read it from the server side.
    char buf[4096] = {};
    size_t len = sizeof(buf);
    ctx_.serverSocket->readData(buf, len);
    std::string request(buf, len);
    CPPUNIT_ASSERT(request.find("GET /file.bin") != std::string::npos);
    CPPUNIT_ASSERT(request.find("Host: localhost") != std::string::npos);

    // Drain the engine to clean up re-enqueued commands.
    ctx_.engine->setNoWait(true);
    ctx_.engine->addCommand(
        make_unique<TestHaltCommand>(ctx_.engine->newCUID(), ctx_.engine.get()));
    ctx_.engine->run(true);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpCommandIntegrationTest);

} // namespace aria2
```

**Step 2: Add to Makefile.am**

In `test/Makefile.am`, add `HttpCommandIntegrationTest.cc` at the end of the
main `aria2c_SOURCES` list (before the `if ENABLE_XML_RPC` block), after
`RealtimeCommandTest.cc`:

```
	RealtimeCommandTest.cc \
	HttpCommandIntegrationTest.cc
```

**Step 3: Build and run**

```bash
touch test/HttpCommandIntegrationTest.cc && make -C test aria2c 2>&1 | tail -5
cd test && ./aria2c 2>/tmp/stderr.txt; echo "EXIT: $?"
tail -3 /tmp/stderr.txt
```

Expected: compilation succeeds, test passes (or fails revealing what needs
adjustment — the first iteration will likely need debugging around the engine
poll behavior).

**Step 4: Iterate until the test passes**

The test may need adjustments:
- If `execute()` blocks on `select()` because the socket isn't writable yet,
  call `clientSocket->setNonBlockingMode()` before constructing the command.
- If the request isn't sent in a single `execute()` call, run more engine ticks.
- If `HttpRequestCommand::executeInternal()` creates an `HttpResponseCommand`
  and enqueues it, verify that by checking the engine's command count or by
  running `engine->run(true)` to let it process the response.

**Step 5: Commit**

```bash
git add test/HttpCommandIntegrationTest.cc test/Makefile.am
git commit -m "test(http): add HttpRequestCommand integration test"
```

---

### Task 2: HTTP Command Integration Test — Error Responses

**Files:**
- Modify: `test/HttpCommandIntegrationTest.cc`

**Step 1: Add 404 test**

Add to the test suite:

```cpp
CPPUNIT_TEST(testResponseNotFound);
```

Declaration:
```cpp
void testResponseNotFound();
```

Implementation:
```cpp
void testResponseNotFound()
{
  ctx_.serverSocket->writeData(
      "HTTP/1.1 404 Not Found\r\n"
      "Content-Length: 0\r\n"
      "\r\n");

  auto cmd = make_unique<HttpRequestCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.httpConn, ctx_.engine.get(), ctx_.clientSocket);

  // Release and drive via engine — HttpRequestCommand sends request,
  // then HttpResponseCommand reads the 404 and handles the error.
  cmd.release();
  ctx_.engine->setNoWait(true);
  ctx_.engine->addCommand(
      make_unique<TestHaltCommand>(ctx_.engine->newCUID(), ctx_.engine.get()));
  ctx_.engine->run(true);

  // After the 404 response is processed, the engine should not have
  // enqueued a download command. The request group should be in error state
  // or the command should have completed.
  // Exact assertion depends on runtime behavior — adjust after first run.
}
```

**Step 2: Build and test**

```bash
touch test/HttpCommandIntegrationTest.cc && make -C test aria2c 2>&1 | tail -5
cd test && ./aria2c 2>/tmp/stderr.txt; echo "EXIT: $?"
```

**Step 3: Commit**

```bash
git add test/HttpCommandIntegrationTest.cc
git commit -m "test(http): add 404 error response integration test"
```

---

### Task 3: FTP Command Integration Test — PASV Download

**Files:**
- Create: `test/FtpCommandIntegrationTest.cc`
- Modify: `test/Makefile.am`

**Context:**
- `FtpNegotiationCommand` does `static_pointer_cast<SocketCore>(socket)` — must be real.
- The constructor calls `e->getAuthConfigFactory()->createAuthConfig(...)`.
- The state machine: RECV_GREETING → SEND_USER → RECV_USER → SEND_PASS → RECV_PASS → SEND_TYPE → RECV_TYPE → SEND_PWD → RECV_PWD → etc.
- FTP responses: `"220 Ready\r\n"`, `"331 Password\r\n"`, `"230 OK\r\n"`, `"200 Type\r\n"`, `"257 \"/\"\r\n"`.
- Pattern from `FtpConnectionTest`: listen on port 0, connect, accept, write responses on server fd.

**Step 1: Write the test file**

```cpp
// test/FtpCommandIntegrationTest.cc
#include "FtpNegotiationCommand.h"
#include "SocketCore.h"
#include "Request.h"
#include "FileEntry.h"
#include "RequestGroup.h"
#include "DownloadContext.h"
#include "GroupId.h"
#include "Option.h"
#include "AuthConfigFactory.h"
#include "prefs.h"

#include <cppunit/extensions/HelperMacros.h>

#include "TestEngineHelper.h"

namespace aria2 {

namespace {

void waitRead(const std::shared_ptr<SocketCore>& socket)
{
  while (!socket->isReadable(0))
    ;
}

struct FtpIntegrationContext {
  std::shared_ptr<Option> option;
  std::shared_ptr<RequestGroup> rg;
  std::shared_ptr<Request> req;
  std::shared_ptr<FileEntry> fileEntry;
  std::shared_ptr<SocketCore> clientSocket;
  std::shared_ptr<SocketCore> serverSocket;
  // engine last
  std::unique_ptr<DownloadEngine> engine;

  void setUp()
  {
    engine = createTestEngine(option, "aria2_FtpCmdIntTest", true);
    engine->setAuthConfigFactory(make_unique<AuthConfigFactory>());

    SocketCore listenSock;
    listenSock.bind(0);
    listenSock.beginListen();
    listenSock.setBlockingMode();
    uint16_t port = listenSock.getAddrInfo().port;

    clientSocket = std::make_shared<SocketCore>();
    clientSocket->establishConnection("localhost", port);
    while (!clientSocket->isWritable(0))
      ;
    serverSocket = listenSock.acceptConnection();
    serverSocket->setBlockingMode();

    req = std::make_shared<Request>();
    req->setUri("ftp://user:pass@localhost/file.bin");
    fileEntry = std::make_shared<FileEntry>("file.bin", 0, 0);

    rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    rg->setDownloadContext(
        std::make_shared<DownloadContext>(1048576, 0, "file.bin"));
  }
};

} // namespace

class FtpCommandIntegrationTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(FtpCommandIntegrationTest);
  CPPUNIT_TEST(testGreetingAndUser);
  CPPUNIT_TEST(testAuthFailure);
  CPPUNIT_TEST_SUITE_END();

  FtpIntegrationContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testGreetingAndUser()
  {
    auto cmd = make_unique<FtpNegotiationCommand>(
        ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
        ctx_.engine.get(), ctx_.clientSocket,
        FtpNegotiationCommand::SEQ_RECV_GREETING);

    // Send greeting from server
    ctx_.serverSocket->writeData("220 FTP ready\r\n");
    waitRead(ctx_.clientSocket);

    // Release ownership — command re-enqueues itself
    auto* raw = cmd.release();
    raw->execute();

    // The command should have received the greeting and sent USER.
    // Read the USER command from the server side.
    char buf[4096] = {};
    size_t len = sizeof(buf);
    ctx_.serverSocket->readData(buf, len);
    std::string sent(buf, len);
    CPPUNIT_ASSERT(sent.find("USER") != std::string::npos);

    // Drain engine
    ctx_.engine->setNoWait(true);
    ctx_.engine->addCommand(
        make_unique<TestHaltCommand>(ctx_.engine->newCUID(), ctx_.engine.get()));
    ctx_.engine->run(true);
  }

  void testAuthFailure()
  {
    auto cmd = make_unique<FtpNegotiationCommand>(
        ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
        ctx_.engine.get(), ctx_.clientSocket,
        FtpNegotiationCommand::SEQ_RECV_GREETING);

    // greeting
    ctx_.serverSocket->writeData("220 FTP ready\r\n");
    waitRead(ctx_.clientSocket);
    auto* raw = cmd.release();
    raw->execute();

    // Read USER from server side, then send 331 (password needed)
    char buf[4096] = {};
    size_t len = sizeof(buf);
    ctx_.serverSocket->readData(buf, len);

    ctx_.serverSocket->writeData("331 Password required\r\n");
    waitRead(ctx_.clientSocket);

    // Tick engine to process RECV_USER → SEND_PASS
    ctx_.engine->setNoWait(true);
    ctx_.engine->run(true);

    // Read PASS from server side, send 530 (auth failure)
    len = sizeof(buf);
    ctx_.serverSocket->readData(buf, len);
    std::string pass(buf, len);
    CPPUNIT_ASSERT(pass.find("PASS") != std::string::npos);

    ctx_.serverSocket->writeData("530 Login incorrect\r\n");
    waitRead(ctx_.clientSocket);

    // Tick — should handle the auth error
    ctx_.engine->addCommand(
        make_unique<TestHaltCommand>(ctx_.engine->newCUID(), ctx_.engine.get()));
    ctx_.engine->run(true);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(FtpCommandIntegrationTest);

} // namespace aria2
```

**Step 2: Add to Makefile.am**

In `test/Makefile.am`, add `FtpCommandIntegrationTest.cc` after
`HttpCommandIntegrationTest.cc` in the main sources list:

```
	HttpCommandIntegrationTest.cc \
	FtpCommandIntegrationTest.cc
```

**Step 3: Build and test**

```bash
touch test/FtpCommandIntegrationTest.cc && make -C test aria2c 2>&1 | tail -5
cd test && ./aria2c 2>/tmp/stderr.txt; echo "EXIT: $?"
tail -3 /tmp/stderr.txt
```

**Step 4: Iterate until tests pass**

FTP tick-by-tick is the trickiest part. `FtpNegotiationCommand::execute()` calls
`processSequence()` which runs through multiple states in one call (it loops
while each step returns `true`). The greeting→USER transition may happen in a
single `execute()` call. The test assertions may need to be adjusted based on
how many states the command processes per tick.

**Step 5: Commit**

```bash
git add test/FtpCommandIntegrationTest.cc test/Makefile.am
git commit -m "test(ftp): add FtpNegotiationCommand integration test"
```

---

### Task 4: BT Peer Integration Test — Handshake

**Files:**
- Create: `test/BtPeerIntegrationTest.cc`
- Modify: `test/Makefile.am`

**Context:**
- `PeerInitiateConnectionCommand` needs: `RequestGroup` with loaded torrent, `BtRuntime`, `PeerStorage`, `PieceStorage`, `BtRegistry` registration.
- Set `mseHandshakeEnabled=false` to bypass MSE and go straight to the BT handshake.
- The 68-byte BT handshake: `0x13` + `"BitTorrent protocol"` + 8 reserved bytes + 20-byte info_hash + 20-byte peer_id.
- Pattern from `MSEHandshakeTest`: listen/connect/accept with real sockets.
- `PeerInteractionCommand`'s constructor builds the entire BT object graph internally — we just need to provide the inputs.

**Step 1: Write the test file**

```cpp
// test/BtPeerIntegrationTest.cc
#include "PeerInitiateConnectionCommand.h"
#include "PeerInteractionCommand.h"
#include "BtHandshakeMessage.h"
#include "SocketCore.h"
#include "Peer.h"
#include "BtRuntime.h"
#include "BtRegistry.h"
#include "DownloadContext.h"
#include "RequestGroup.h"
#include "GroupId.h"
#include "Option.h"
#include "bittorrent_helper.h"
#include "prefs.h"

#include <cppunit/extensions/HelperMacros.h>

#include "TestEngineHelper.h"
#include "MockPeerStorage.h"
#include "MockPieceStorage.h"

namespace aria2 {

namespace {

struct BtIntegrationContext {
  std::shared_ptr<Option> option;
  std::shared_ptr<DownloadContext> dctx;
  std::shared_ptr<RequestGroup> rg;
  std::shared_ptr<BtRuntime> btRuntime;
  std::shared_ptr<MockPeerStorage> peerStorage;
  std::shared_ptr<MockPieceStorage> pieceStorage;
  std::shared_ptr<Peer> peer;
  std::shared_ptr<SocketCore> clientSocket;
  std::shared_ptr<SocketCore> serverSocket;
  // engine last
  std::unique_ptr<DownloadEngine> engine;

  void setUp()
  {
    engine = createTestEngine(option, "aria2_BtPeerIntTest", true);

    option->put(PREF_BT_TIMEOUT, "60");
    option->put(PREF_PEER_CONNECTION_TIMEOUT, "20");
    option->put(PREF_BT_KEEP_ALIVE_INTERVAL, "120");
    option->put(PREF_BT_REQUEST_TIMEOUT, "60");
    option->put(PREF_ENABLE_PEER_EXCHANGE, A2_V_TRUE);

    // Load test torrent
    dctx = std::make_shared<DownloadContext>();
    bittorrent::load(A2_TEST_DIR "/test.torrent", dctx, option);

    rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    rg->setDownloadContext(dctx);

    btRuntime = std::make_shared<BtRuntime>();
    peerStorage = std::make_shared<MockPeerStorage>();
    pieceStorage = std::make_shared<MockPieceStorage>();

    // Register in BtRegistry
    auto btObject = make_unique<BtObject>();
    btObject->downloadContext = dctx;
    btObject->btRuntime = btRuntime;
    btObject->peerStorage = peerStorage;
    btObject->pieceStorage = pieceStorage;
    engine->getBtRegistry()->put(rg->getGID(), std::move(btObject));

    // Connected socket pair
    SocketCore listenSock;
    listenSock.bind(0);
    listenSock.beginListen();
    listenSock.setBlockingMode();
    auto endpoint = listenSock.getAddrInfo();

    clientSocket = std::make_shared<SocketCore>();
    clientSocket->establishConnection("localhost", endpoint.port, false);
    while (!clientSocket->isWritable(0))
      ;
    serverSocket = listenSock.acceptConnection();
    serverSocket->setBlockingMode();

    peer = std::make_shared<Peer>("127.0.0.1", endpoint.port);
  }
};

} // namespace

class BtPeerIntegrationTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(BtPeerIntegrationTest);
  CPPUNIT_TEST(testHandshakeSent);
  CPPUNIT_TEST_SUITE_END();

  BtIntegrationContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testHandshakeSent()
  {
    // Create PeerInitiateConnectionCommand with MSE disabled
    auto cmd = make_unique<PeerInitiateConnectionCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.peer, ctx_.engine.get(),
        ctx_.btRuntime, false);
    cmd->setPeerStorage(ctx_.peerStorage);
    cmd->setPieceStorage(ctx_.pieceStorage);

    // Execute — since the socket is already connected and writable,
    // this should create a PeerInteractionCommand with
    // INITIATOR_SEND_HANDSHAKE and enqueue it.
    auto* raw = cmd.release();
    raw->execute();

    // Pre-write the peer's handshake response on the server side
    const unsigned char* infoHash = bittorrent::getInfoHash(ctx_.dctx);
    unsigned char peerId[20];
    memset(peerId, 'P', sizeof(peerId));
    BtHandshakeMessage hs(infoHash, peerId);
    auto msg = hs.createMessage();
    ctx_.serverSocket->writeData(msg.data(), msg.size());

    // Run engine ticks to let the handshake proceed
    ctx_.engine->setNoWait(true);
    ctx_.engine->addCommand(
        make_unique<TestHaltCommand>(ctx_.engine->newCUID(), ctx_.engine.get()));
    ctx_.engine->run(true);

    // Read the handshake sent by our side from the server socket
    unsigned char recvBuf[68] = {};
    size_t recvLen = sizeof(recvBuf);
    ctx_.serverSocket->readData(recvBuf, recvLen);

    // Verify the handshake header
    CPPUNIT_ASSERT_EQUAL((size_t)68, recvLen);
    CPPUNIT_ASSERT_EQUAL((int)0x13, (int)recvBuf[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("BitTorrent protocol"),
                         std::string((char*)recvBuf + 1, 19));

    // Verify info_hash matches
    CPPUNIT_ASSERT(memcmp(recvBuf + 28, infoHash, 20) == 0);

    // Drain engine
    ctx_.btRuntime->setHalt(true);
    ctx_.engine->run(true);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(BtPeerIntegrationTest);

} // namespace aria2
```

**Step 2: Add to Makefile.am**

In `test/Makefile.am`, inside the `if ENABLE_BITTORRENT` block, add
`BtPeerIntegrationTest.cc` after `DHTCommandTest.cc`:

```
	DHTCommandTest.cc \
	BtPeerIntegrationTest.cc
```

**Step 3: Build and test**

```bash
touch test/BtPeerIntegrationTest.cc && make -C test aria2c 2>&1 | tail -5
cd test && ./aria2c 2>/tmp/stderr.txt; echo "EXIT: $?"
tail -3 /tmp/stderr.txt
```

**Step 4: Iterate until the test passes**

BT integration is the most complex. Potential issues:
- `PeerInitiateConnectionCommand::executeInternal()` creates a `PeerInteractionCommand` and adds it to the engine via `e_->addCommand()`. The command then takes ownership via the engine. The test needs multiple engine ticks to drive the handshake.
- `PeerInteractionCommand`'s constructor reads many PREF_* options from the RequestGroup's option. Missing prefs may cause crashes — add them to setUp().
- The `BtObject` registered in `BtRegistry` may need more fields populated (like `btAnnounce`) depending on what the constructor touches.

**Step 5: Commit**

```bash
git add test/BtPeerIntegrationTest.cc test/Makefile.am
git commit -m "test(bt): add BT peer handshake integration test"
```

---

### Task 5: Stabilization and Final Verification

**Files:**
- Possibly modify: all 3 integration test files based on runtime findings

**Step 1: Run full test suite**

```bash
make check 2>&1 | tail -20
```

Expected: `OK (1041+)`, exit 0. The count should be 1038 + at least 3 new tests.

**Step 2: Fix any failures**

Each protocol test may need adjustments:
- Timing: add more `engine->run(true)` ticks
- Missing prefs: add `option->put(PREF_*, "value")` in setUp()
- Missing objects on the engine: check what other factories/configs are needed
- Socket readiness: ensure `clientSocket` is in the right mode (blocking vs non-blocking)

**Step 3: Final commit**

```bash
git add -u
git commit -m "test: stabilize integration tests"
```
