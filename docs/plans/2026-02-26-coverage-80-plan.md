# Coverage 80% — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Raise test line coverage from 62% to 80% (TLS/SSH excluded).

**Architecture:** 3-tier approach — quick-win unit tests for small classes, pipeline integration tests for HTTP/FTP/BT download flows, component integration tests for DHT/proxy/MSE. All integration tests use the Phase 3 pattern: real SocketCore pairs + real DownloadEngine + SelectEventPoll + engine->run(true).

**Tech Stack:** C++11, CppUnit, SelectEventPoll, SocketCore, lcov

---

### Task 1: BT Validator Unit Tests

**Files:**
- Create: `test/BtValidatorTest.cc`
- Modify: `test/Makefile.am`

**Covers:** BtBitfieldMessageValidator (9L), BtHandshakeMessageValidator (14L), BtPieceMessageValidator (9L), IndexBtMessageValidator (8L) — 40 lines total

**Step 1: Write test file**

```cpp
// test/BtValidatorTest.cc
#include "BtBitfieldMessageValidator.h"
#include "BtHandshakeMessageValidator.h"
#include "BtPieceMessageValidator.h"
#include "IndexBtMessageValidator.h"
#include "BtBitfieldMessage.h"
#include "BtHandshakeMessage.h"
#include "BtPieceMessage.h"
#include "BtHaveMessage.h"
#include "Exception.h"

#include <cppunit/extensions/HelperMacros.h>

#include <cstring>

namespace aria2 {

class BtValidatorTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(BtValidatorTest);
  CPPUNIT_TEST(testBitfieldValid);
  CPPUNIT_TEST(testHandshakeValid);
  CPPUNIT_TEST(testHandshakeInvalidHash);
  CPPUNIT_TEST(testPieceValid);
  CPPUNIT_TEST(testPieceInvalidIndex);
  CPPUNIT_TEST(testIndexValid);
  CPPUNIT_TEST(testIndexInvalidIndex);
  CPPUNIT_TEST_SUITE_END();

public:
  void testBitfieldValid()
  {
    // numPiece=8 means 1 byte bitfield
    BtBitfieldMessage msg;
    auto bf = std::vector<unsigned char>(1, 0xFF);
    msg.setBitfield(bf.data(), bf.size());
    BtBitfieldMessageValidator v(&msg, 8);
    // Should not throw
    v.validate();
  }

  void testHandshakeValid()
  {
    unsigned char infoHash[20];
    unsigned char peerId[20];
    memset(infoHash, 0xAA, 20);
    memset(peerId, 0xBB, 20);
    BtHandshakeMessage msg(infoHash, peerId);
    BtHandshakeMessageValidator v(&msg, infoHash);
    v.validate();
  }

  void testHandshakeInvalidHash()
  {
    unsigned char infoHash[20];
    unsigned char wrongHash[20];
    unsigned char peerId[20];
    memset(infoHash, 0xAA, 20);
    memset(wrongHash, 0xCC, 20);
    memset(peerId, 0xBB, 20);
    BtHandshakeMessage msg(infoHash, peerId);
    BtHandshakeMessageValidator v(&msg, wrongHash);
    try {
      v.validate();
      CPPUNIT_FAIL("Expected exception");
    }
    catch (const RecoverableException& e) {
      // expected
    }
  }

  void testPieceValid()
  {
    // Read BtPieceMessage header to find constructor.
    // BtPieceMessage needs index, begin, blockLength.
    // BtPieceMessageValidator checks index < numPiece and begin < pieceLength.
    BtPieceMessage msg;
    msg.setIndex(0);
    msg.setBegin(0);
    msg.setBlockLength(16384);
    BtPieceMessageValidator v(&msg, 10, 1048576);
    v.validate();
  }

  void testPieceInvalidIndex()
  {
    BtPieceMessage msg;
    msg.setIndex(100);
    msg.setBegin(0);
    msg.setBlockLength(16384);
    BtPieceMessageValidator v(&msg, 10, 1048576);
    try {
      v.validate();
      CPPUNIT_FAIL("Expected exception");
    }
    catch (const RecoverableException& e) {
      // expected
    }
  }

  void testIndexValid()
  {
    // IndexBtMessage is a base for Have, Cancel, etc.
    // BtHaveMessage inherits from IndexBtMessage.
    BtHaveMessage msg(5);
    IndexBtMessageValidator v(&msg, 10);
    v.validate();
  }

  void testIndexInvalidIndex()
  {
    BtHaveMessage msg(100);
    IndexBtMessageValidator v(&msg, 10);
    try {
      v.validate();
      CPPUNIT_FAIL("Expected exception");
    }
    catch (const RecoverableException& e) {
      // expected
    }
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(BtValidatorTest);

} // namespace aria2
```

**Note:** The exact constructor calls and setter methods may differ. Read the header files during implementation to confirm.

**Step 2: Add to Makefile.am**

In `test/Makefile.am`, inside the `if ENABLE_BITTORRENT` block, add `BtValidatorTest.cc` after `BtPeerIntegrationTest.cc`.

**Step 3: Build and test**

```bash
cd /Users/kenshin/CodeProjects/aria2
touch test/BtValidatorTest.cc && make -C test aria2c 2>&1 | tail -5
cd test && ./aria2c 2>/tmp/stderr.txt; echo "EXIT: $?"
tail -3 /tmp/stderr.txt
```

Expected: OK (1050+), EXIT 0

**Step 4: Commit**

```bash
git add test/BtValidatorTest.cc test/Makefile.am
git commit -m "test(bt): add BT message validator unit tests"
```

---

### Task 2: Piece Selector and Seed Criteria Unit Tests

**Files:**
- Create: `test/StreamPieceSelectorTest.cc`
- Modify: `test/Makefile.am`

**Covers:** InorderStreamPieceSelector (9L), RandomStreamPieceSelector (~10L), UnionSeedCriteria (15L) — 34 lines

**Context:**
- `InorderStreamPieceSelector(BitfieldMan*)` — `select()` calls `BitfieldMan::getInorderMissingUnusedIndex()`
- `RandomStreamPieceSelector(BitfieldMan*)` — `select()` picks random starting point
- `UnionSeedCriteria` — composite OR of `SeedCriteria` objects
- Reference: `GeomStreamPieceSelectorTest.cc`, `ShareRatioSeedCriteriaTest.cc`

**Step 1: Write test file**

Test InorderStreamPieceSelector by creating a BitfieldMan, marking some pieces as used/complete, and verifying select() returns the first missing piece in order. Test RandomStreamPieceSelector similarly (just verify it returns a valid piece). Test UnionSeedCriteria with mock criteria (from existing `TimeSeedCriteria` or a simple implementation).

**Step 2: Add to Makefile.am, build, test, commit**

```bash
git commit -m "test: add piece selector and seed criteria unit tests"
```

---

### Task 3: Small Class Unit Tests

**Files:**
- Create: `test/SmallClassCoverageTest.cc`
- Modify: `test/Makefile.am`

**Covers:** ContextAttribute (5L), Notifier (9L), FatalException (10L), TruncFileAllocationIterator (11L), UnknownOptionException (14L), DHTMessageEntry (6L) — 55 lines

**Context:**
- `ContextAttribute` — just `strContextAttributeType(CTX_ATTR_BT)` returns "BitTorrent"
- `Notifier` — observer pattern: `addDownloadEventListener()` + `notifyDownloadEvent()`
- `FatalException` — construct and check `what()`
- `TruncFileAllocationIterator(BinaryStream*, offset, totalLength)` — `allocateChunk()` then `finished()`
- `UnknownOptionException` — construct with option name
- `DHTMessageEntry` — struct with message and callback fields

**Step 1: Write test file**

For Notifier, create a test DownloadEventListener that records events, register it, fire events, verify. For TruncFileAllocationIterator, use a test file. For others, simple construct-and-assert tests.

**Step 2: Add to Makefile.am, build, test, commit**

```bash
git commit -m "test: add small class coverage unit tests"
```

---

### Task 4: HTTP Full Download Integration Test

**Files:**
- Modify: `test/HttpCommandIntegrationTest.cc`

**Covers:** HttpDownloadCommand (50L), DownloadCommand (213L), HttpSkipResponseCommand (112L), HttpResponseCommand branches — ~375 lines

**Context:**
- Current tests only cover HttpRequestCommand → HttpResponseCommand (send request, parse headers)
- Need to extend to cover the full download path: response parsed → HttpDownloadCommand created → body downloaded
- Also need: 301 redirect, HttpSkipResponseCommand for error bodies

**Step 1: Add testFullDownload**

Pre-write a 200 OK response with body. Drive the engine multiple ticks until the body is received. Verify the file was written to disk.

Key: DownloadCommand reads data via StreamFilter. Need a valid DownloadContext with correct totalLength so the segment tracking works. Need PieceStorage and DiskAdaptor set up on the RequestGroup.

```cpp
void testFullDownload()
{
  // Pre-write: HTTP 200 with 5-byte body
  ctx_.serverSocket->writeData(
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 5\r\n"
      "\r\n"
      "hello");

  // Need PieceStorage + DiskAdaptor for DownloadCommand to write data.
  // Use DefaultPieceStorage with ByteArrayDiskWriter.
  // Set DownloadContext totalLength to 5.
  // ... (research exact setup during implementation)

  auto cmd = make_unique<HttpRequestCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.httpConn, ctx_.engine.get(), ctx_.clientSocket);
  ctx_.engine->addCommand(std::move(cmd));

  // Multiple ticks to drive through:
  // HttpRequestCommand → HttpResponseCommand → HttpDownloadCommand
  for (int i = 0; i < 5; ++i) {
    ctx_.engine->run(true);
  }

  // Verify data was downloaded
  CPPUNIT_ASSERT(ctx_.rg->downloadFinished());
}
```

**Step 2: Add testRedirect**

Pre-write 301 with Location header. Verify the redirect is followed.

**Step 3: Build, test, commit**

```bash
git commit -m "test(http): add full download and redirect integration tests"
```

---

### Task 5: FTP Full PASV Download Integration Test

**Files:**
- Modify: `test/FtpCommandIntegrationTest.cc`

**Covers:** FtpInitiateConnectionCommand (105L), FtpDownloadCommand (23L), FtpFinishDownloadCommand (38L) — ~166 lines

**Context:**
- Current tests only cover greeting→USER→PASS flow
- Need to extend through: TYPE→PWD→SIZE→EPSV→REST→RETR→data download
- FTP PASV download requires a second socket pair for the data channel
- The EPSV response tells the client which port to connect to for data

**Step 1: Add testPasvDownload**

Drive the FTP state machine through all states:
1. 220 greeting → read USER
2. 331 password → read PASS
3. 230 login OK → read TYPE
4. 200 type → read PWD
5. 257 "/" → read SIZE
6. 213 5 (file size) → read EPSV (or PASV)
7. 229 (|||port|) → client connects data channel
8. read REST → 350 OK
9. read RETR → 150 data follows
10. Write data on data channel, close data channel
11. 226 Transfer complete

This is complex. The test needs:
- A data channel listen socket whose port is in the EPSV response
- The FtpNegotiationCommand creates FtpDownloadCommand which reads from the data socket

**Step 2: Build, test, iterate, commit**

```bash
git commit -m "test(ftp): add full PASV download integration test"
```

---

### Task 6: ConnectCommand Integration Test

**Files:**
- Create: `test/ConnectCommandIntegrationTest.cc`
- Modify: `test/Makefile.am`

**Covers:** ConnectCommand (47L), InitiateConnectionCommand partial, HttpInitiateConnectionCommand partial — ~120 lines

**Context:**
- ConnectCommand wraps socket establishment and delegates to a ControlChain
- Constructor: `ConnectCommand(cuid, req, proxyRequest, fileEntry, rg, engine, socket)`
- Need to set a ControlChain that creates the next command
- The simplest test: create a ConnectCommand with an already-connected socket and verify the chain runs

**Step 1: Write test file**

Use HttpRequestConnectChain (or create a minimal test chain) to verify ConnectCommand detects write-readiness and delegates.

**Step 2: Build, test, commit**

```bash
git commit -m "test: add ConnectCommand integration test"
```

---

### Task 7: BT Bitfield Exchange Integration Test

**Files:**
- Modify: `test/BtPeerIntegrationTest.cc`

**Covers:** PeerReceiveHandshakeCommand (63L), ActivePeerConnectionCommand (81L) partial, DefaultBtInteractive branches — ~100 lines

**Context:**
- Extend testHandshakeSent to complete the handshake: after sending our handshake, write back a peer handshake + bitfield message on the server socket
- Drive more engine ticks to exercise PeerInteractionCommand's bitfield processing

**Step 1: Add testBitfieldExchange**

After the BT handshake, write a bitfield message to the server socket and drive more engine ticks. Verify the peer's bitfield is recorded.

**Step 2: Build, test, commit**

```bash
git commit -m "test(bt): add bitfield exchange integration test"
```

---

### Task 8: DHT Message Dispatch/Receive Unit Tests

**Files:**
- Create: `test/DHTMessageDispatchTest.cc`
- Modify: `test/Makefile.am`

**Covers:** DHTMessageDispatcherImpl (39L), DHTMessageReceiver (62L), DHTMessageEntry (6L), DHTTaskQueueImpl (20L), DHTTaskFactoryImpl (61L) — ~188 lines

**Context:**
- These are the DHT infrastructure classes that don't need real UDP sockets
- DHTMessageDispatcherImpl: queue messages, call sendMessages(), verify tracker entries
- DHTMessageReceiver: decode bencode, dispatch to tracker
- DHTTaskQueueImpl: add tasks, execute, verify concurrent limits
- DHTTaskFactoryImpl: create tasks, verify wiring
- Reference: `test/DHTCommandTest.cc` uses MockDHTTaskQueue and MockDHTTaskFactory

**Step 1: Write test file**

Use existing mock classes (MockDHTTaskQueue, MockDHTTaskFactory) from DHTCommandTest pattern. Create DHTMessageDispatcherImpl with a real DHTMessageTracker, queue a message, call sendMessages().

**Step 2: Build, test, commit**

```bash
git commit -m "test(dht): add DHT message dispatch and receive unit tests"
```

---

### Task 9: DHT Task Unit Tests

**Files:**
- Create: `test/DHTTaskTest.cc`
- Modify: `test/Makefile.am`

**Covers:** DHTAbstractTask (24L), DHTPingTask (30L), DHTBucketRefreshTask (24L), DHTNodeLookupTask (12L), DHTPeerLookupTask (12L), callbacks (12L), DHTReplaceNodeTask — ~130 lines

**Context:**
- DHTPingTask: startup() queues a ping message, onReceived() marks success, onTimeout() retries
- DHTBucketRefreshTask: startup() finds stale buckets, creates node lookup tasks
- These can be tested with mock dispatcher/factory/routing table

**Step 1: Write test file**

Test DHTPingTask lifecycle: create, startup(), simulate onReceived(), verify isPingSuccessful(). Test onTimeout() + retry. Test DHTBucketRefreshTask with a routing table that has stale buckets.

**Step 2: Build, test, commit**

```bash
git commit -m "test(dht): add DHT task unit tests"
```

---

### Task 10: DHT Integration Test (full round-trip)

**Files:**
- Create: `test/DHTIntegrationTest.cc`
- Modify: `test/Makefile.am`

**Covers:** DHTInteractionCommand (90L), DHTSetup (170L) partial — ~130 lines

**Context:**
- DHTInteractionCommand is the main DHT loop: it calls taskQueue->executeTask(), receiver->receiveMessage(), dispatcher->sendMessages()
- Test with UDP socketpair: send a DHT ping query, verify response
- Use DHTSetup::setup() or manually wire the objects

**Step 1: Write test file**

Create a UDP socket pair (one for the DHT node, one for the "remote" node). Wire up DHTInteractionCommand with real DHTMessageReceiver/Dispatcher. Send a DHT ping query from the remote side, drive engine ticks, read the response.

**Step 2: Build, test, commit**

```bash
git commit -m "test(dht): add DHT interaction integration test"
```

---

### Task 11: Proxy Tunnel Integration Tests

**Files:**
- Create: `test/ProxyIntegrationTest.cc`
- Modify: `test/Makefile.am`

**Covers:** AbstractProxyRequestCommand (27L), AbstractProxyResponseCommand (17L), HttpProxyRequestCommand (10L), HttpProxyResponseCommand (10L), FtpTunnelRequestCommand (10L), FtpTunnelResponseCommand (15L) — ~89 lines

**Context:**
- Proxy commands implement HTTP CONNECT tunnel
- Flow: HttpProxyRequestCommand sends `CONNECT host:port HTTP/1.1\r\n`, HttpProxyResponseCommand reads `200 Connection Established\r\n\r\n`
- Same pattern as HTTP integration test: real socket pair, pre-write proxy response

**Step 1: Write test file**

Create socket pair. Create HttpProxyRequestCommand (needs proxyRequest with proxy URL). Pre-write "200 Connection Established" on server. Drive engine. Verify CONNECT request was sent. Then verify the chain creates the next protocol command.

**Step 2: Build, test, commit**

```bash
git commit -m "test: add proxy tunnel integration tests"
```

---

### Task 12: MSE Handshake Integration Test

**Files:**
- Create: `test/MSECommandIntegrationTest.cc`
- Modify: `test/Makefile.am`

**Covers:** InitiatorMSEHandshakeCommand (143L), ReceiverMSEHandshakeCommand (~100L) — ~243 lines

**Context:**
- MSEHandshakeTest already tests the MSEHandshake object directly (no commands)
- This test drives InitiatorMSEHandshakeCommand through the engine
- The initiator sends DH public key (96 bytes), receiver replies
- Need: BT object graph (BtRuntime, PeerStorage, PieceStorage, BtRegistry)
- Pattern: two socket pairs, one for initiator, one for receiver. Or test only the initiator with a canned receiver response.

**Step 1: Write test file**

Use BtPeerIntegrationTest pattern for BT setup. Create InitiatorMSEHandshakeCommand. Drive engine ticks. Read the DH public key from server side (96 bytes). Write back a canned response (from MSEHandshakeTest).

Simpler approach: test only that InitiatorMSEHandshakeCommand sends the initial DH key (96 bytes). Don't complete the full handshake.

**Step 2: Build, test, commit**

```bash
git commit -m "test(bt): add MSE handshake command integration test"
```

---

### Task 13: BT Setup and File Entry Tests

**Files:**
- Create: `test/BtSetupTest.cc`
- Modify: `test/Makefile.am`

**Covers:** BtSetup (154L), BtCheckIntegrityEntry (34L), BtFileAllocationEntry (33L), StreamCheckIntegrityEntry, StreamFileAllocationEntry — ~150 lines

**Context:**
- BtSetup::setup() creates all BT commands (TrackerWatcher, PeerChokeCommand, etc.)
- BtCheckIntegrityEntry/BtFileAllocationEntry are called by CheckIntegrityMan/FileAllocationMan
- Test BtSetup::setup() with a loaded torrent and verify it creates the expected commands
- Test BtCheckIntegrityEntry by calling onDownloadIncomplete() and verifying commands created

**Step 1: Write test file**

Load test.torrent into DownloadContext, create RequestGroup, register BtObject in BtRegistry, call BtSetup::setup(). Verify command vector is non-empty and contains expected command types.

**Step 2: Build, test, commit**

```bash
git commit -m "test(bt): add BtSetup and file entry tests"
```

---

### Task 14: AbstractCommand Branch Coverage

**Files:**
- Modify: `test/AbstractCommandTest.cc`

**Covers:** AbstractCommand.cc additional branches (33%→~55%) — ~115 lines

**Context:**
- AbstractCommand::execute() has many branches: timeout, DNS resolve, proxy check, read/write check, etc.
- Current tests cover basic socket mock paths
- Need to cover: timeout handling, proxy request creation, error paths

**Step 1: Add timeout and error tests**

Add tests for:
- Command timeout (set short timeout, don't provide data, verify timeout handling)
- Read check socket ready but data causes error
- Write check socket behavior

**Step 2: Build, test, commit**

```bash
git commit -m "test: extend AbstractCommand branch coverage"
```

---

### Task 15: RequestGroup and RequestGroupMan Branch Coverage

**Files:**
- Modify existing tests or create `test/RequestGroupBranchTest.cc`
- Modify: `test/Makefile.am`

**Covers:** RequestGroup.cc (38%→~60%) + RequestGroupMan.cc (36%→~60%) — ~300 lines

**Context:**
- RequestGroup has many methods already tested individually but many branches untouched
- RequestGroupMan manages the lifecycle of multiple RequestGroups
- Focus on: download completion flow, error handling, retry logic, URI management

**Step 1: Write targeted tests**

Test RequestGroup methods that are uncovered: completeSegment(), prepareForRetry(), getURIs(), seedCriteria handling. Test RequestGroupMan's processDownloadResult(), addReservedGroup(), changeReservedGroupPosition().

**Step 2: Build, test, commit**

```bash
git commit -m "test: extend RequestGroup and RequestGroupMan branch coverage"
```

---

### Task 16: Remaining Coverage Boost

**Files:**
- Various existing test files and new tests as needed

**Covers:** DefaultBtInteractive (17%→~50%), BtPieceMessage (31%→~60%), CheckIntegrityEntry (34%→~60%), AdaptiveURISelector (0%→~30%) — ~500 lines

**Context:**
- DefaultBtInteractive: extend BT integration tests to cover more interactive flows
- BtPieceMessage: test send/receive piece data
- CheckIntegrityEntry: test the integrity check lifecycle
- AdaptiveURISelector: test URI selection algorithms

**Step 1: Write targeted tests per component**

Prioritize by line count: DefaultBtInteractive has the most uncovered lines.

**Step 2: Build, test, commit per component**

```bash
git commit -m "test: boost coverage for BT interactive, piece messages, integrity checks"
```

---

### Task 17: Final Verification

**Step 1: Run coverage measurement**

```bash
./scripts/coverage.sh
```

Expected: ≥80% line coverage

**Step 2: If below 80%, identify remaining gaps**

```bash
./scripts/coverage.sh --html
open coverage-report/index.html
```

Find the largest uncovered files and write targeted tests.

**Step 3: Final commit**

```bash
git commit -m "test: achieve 80% line coverage target"
```
