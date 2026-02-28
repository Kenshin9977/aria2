// test/BtPeerIntegrationTest.cc
// Integration test for PeerInitiateConnectionCommand using real sockets
// and the DownloadEngine event loop.
#include "PeerInitiateConnectionCommand.h"
#include "PeerInteractionCommand.h"
#include "SocketCore.h"
#include "Peer.h"
#include "RequestGroup.h"
#include "DownloadContext.h"
#include "GroupId.h"
#include "Option.h"
#include "BtRuntime.h"
#include "BtRegistry.h"
#include "BtHandshakeMessage.h"
#include "bittorrent_helper.h"
#include "prefs.h"

#include <cppunit/extensions/HelperMacros.h>

#include "TestEngineHelper.h"
#include "MockPeerStorage.h"
#include "MockPieceStorage.h"
#include "MockBtAnnounce.h"
#include "MockBtProgressInfoFile.h"

namespace aria2 {

namespace {

struct BtPeerIntegrationContext {
  std::shared_ptr<Option> option;
  std::shared_ptr<DownloadContext> dctx;
  std::shared_ptr<RequestGroup> rg;
  std::shared_ptr<BtRuntime> btRuntime;
  std::shared_ptr<MockPeerStorage> peerStorage;
  std::shared_ptr<MockPieceStorage> pieceStorage;
  std::shared_ptr<SocketCore> listenSocket;
  uint16_t port;
  // engine last -- destroyed first so commands (holding raw ptrs) are freed
  std::unique_ptr<DownloadEngine> engine;

  void setUp()
  {
    engine = createTestEngine(option, "aria2_BtPeerIntTest", true);

    // BT-specific preferences
    option->put(PREF_BT_TIMEOUT, "30");
    option->put(PREF_PEER_CONNECTION_TIMEOUT, "10");
    option->put(PREF_BT_REQUEST_TIMEOUT, "60");
    option->put(PREF_BT_KEEP_ALIVE_INTERVAL, "120");
    option->put(PREF_ENABLE_PEER_EXCHANGE, "false");
    option->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
    option->put(PREF_MAX_UPLOAD_LIMIT, "0");

    // Load torrent into DownloadContext
    dctx = std::make_shared<DownloadContext>();
    bittorrent::load(A2_TEST_DIR "/test.torrent", dctx, option);

    // RequestGroup must use the same option so PeerInteractionCommand
    // can look up prefs via requestGroup->getOption()
    rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    rg->setDownloadContext(dctx);

    btRuntime = std::make_shared<BtRuntime>();
    peerStorage = std::make_shared<MockPeerStorage>();
    pieceStorage = std::make_shared<MockPieceStorage>();

    // Register BtObject in the engine's BtRegistry so that
    // PeerInteractionCommand can look up tcpPort.
    auto btObj = std::make_unique<BtObject>(
        dctx, pieceStorage, peerStorage, std::make_shared<MockBtAnnounce>(),
        btRuntime, std::make_shared<MockBtProgressInfoFile>());
    engine->getBtRegistry()->put(rg->getGID(), std::move(btObj));

    // Listen socket for accepting the peer connection
    listenSocket = std::make_shared<SocketCore>();
    listenSocket->bind(0);
    listenSocket->beginListen();
    listenSocket->setBlockingMode();
    port = listenSocket->getAddrInfo().port;
  }
};

} // namespace

class BtPeerIntegrationTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(BtPeerIntegrationTest);
  CPPUNIT_TEST(testHandshakeSent);
  CPPUNIT_TEST_SUITE_END();

  BtPeerIntegrationContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testHandshakeSent()
  {
    auto peer = std::make_shared<Peer>("127.0.0.1", ctx_.port);

    // PeerInitiateConnectionCommand creates its own socket and calls
    // establishConnection() in executeInternal(), then hands off to
    // PeerInteractionCommand(INITIATOR_SEND_HANDSHAKE).
    auto cmd = std::make_unique<PeerInitiateConnectionCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), peer, ctx_.engine.get(),
        ctx_.btRuntime, /* mseHandshakeEnabled */ false);
    cmd->setPeerStorage(ctx_.peerStorage);
    cmd->setPieceStorage(ctx_.pieceStorage);

    ctx_.engine->addCommand(std::move(cmd));

    // First tick: PeerInitiateConnectionCommand::executeInternal()
    // creates a socket, calls establishConnection(), then creates
    // PeerInteractionCommand(INITIATOR_SEND_HANDSHAKE) and adds it
    // to the engine.  The command returns true (one-shot).
    ctx_.engine->run(true);

    // Accept the incoming connection on the server side.
    // The connection should be established by now since we're on
    // localhost.
    waitRead(ctx_.listenSocket);
    auto serverSocket = ctx_.listenSocket->acceptConnection();
    serverSocket->setBlockingMode();

    // Second tick: PeerInteractionCommand sees the socket is writable
    // (connection completed), sends the BT handshake via
    // btInteractive_->initiateHandshake(), transitions to
    // INITIATOR_WAIT_HANDSHAKE, and re-enqueues itself.
    ctx_.engine->run(true);

    // Read the 68-byte BT handshake from the server side.
    waitRead(serverSocket);
    unsigned char buf[BtHandshakeMessage::MESSAGE_LENGTH] = {};
    size_t len = sizeof(buf);
    serverSocket->readData(buf, len);

    CPPUNIT_ASSERT_EQUAL(
        static_cast<size_t>(BtHandshakeMessage::MESSAGE_LENGTH), len);

    // Byte 0: pstrlen = 0x13 (19)
    CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(0x13), buf[0]);

    // Bytes 1-19: pstr = "BitTorrent protocol"
    CPPUNIT_ASSERT(memcmp(buf + 1, "BitTorrent protocol", 19) == 0);

    // Bytes 28-47: info_hash from the loaded torrent
    const unsigned char* expectedHash = bittorrent::getInfoHash(ctx_.dctx);
    CPPUNIT_ASSERT(memcmp(buf + 28, expectedHash, 20) == 0);

    // Clean up: halt the engine to destroy the re-enqueued
    // PeerInteractionCommand.
    ctx_.btRuntime->setHalt(true);
    ctx_.engine->setNoWait(true);
    ctx_.engine->addCommand(std::make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->run(true);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(BtPeerIntegrationTest);

} // namespace aria2
