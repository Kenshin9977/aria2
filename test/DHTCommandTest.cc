#include "DHTBucketRefreshCommand.h"
#include "DHTAutoSaveCommand.h"
#include "DHTTokenUpdateCommand.h"
#include "DHTPeerAnnounceCommand.h"
#include "DHTGetPeersCommand.h"

#include <cppunit/extensions/HelperMacros.h>

#include "TestEngineHelper.h"
#include "DHTNode.h"
#include "DHTRoutingTable.h"
#include "DHTTokenTracker.h"
#include "DHTPeerAnnounceStorage.h"
#include "DownloadContext.h"
#include "GroupId.h"
#include "BtRuntime.h"
#include "MockDHTTaskQueue.h"
#include "MockDHTTaskFactory.h"
#include "MockPeerStorage.h"

namespace aria2 {

namespace {

// Runs a command inside the engine with a halt command to stop it.
// The target command is added first so it executes before the halt.
void runCmdThenHalt(DownloadEngine* e, std::unique_ptr<Command> cmd)
{
  e->setNoWait(true);
  e->addCommand(std::move(cmd));
  e->addCommand(make_unique<TestHaltCommand>(e->newCUID(), e));
  e->run(true);
}

// Runs a halt command first, then the target command.
// Use when testing that preProcess() exits on halt.
void runHaltThenCmd(DownloadEngine* e, std::unique_ptr<Command> cmd)
{
  e->setNoWait(true);
  e->addCommand(make_unique<TestHaltCommand>(e->newCUID(), e));
  e->addCommand(std::move(cmd));
  e->run(true);
}

} // namespace

class DHTBucketRefreshCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHTBucketRefreshCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testProcess);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;
  std::shared_ptr<DHTNode> localNode_;
  std::unique_ptr<DHTRoutingTable> routingTable_;
  MockDHTTaskQueue taskQueue_;
  MockDHTTaskFactory taskFactory_;

public:
  void setUp()
  {
    e_ = createTestEngine(option_, "aria2_DHTCommandTest", true);
    localNode_ = std::make_shared<DHTNode>();
    routingTable_ = make_unique<DHTRoutingTable>(localNode_);
  }

  void tearDown() {}

  std::unique_ptr<DHTBucketRefreshCommand>
  createCmd(std::chrono::seconds interval)
  {
    auto cmd =
        make_unique<DHTBucketRefreshCommand>(e_->newCUID(), e_.get(), interval);
    cmd->setRoutingTable(routingTable_.get());
    cmd->setTaskQueue(&taskQueue_);
    cmd->setTaskFactory(&taskFactory_);
    return cmd;
  }

  void testExitOnHalt()
  {
    runHaltThenCmd(e_.get(), createCmd(std::chrono::seconds(60)));
  }

  void testProcess()
  {
    runCmdThenHalt(e_.get(), createCmd(std::chrono::seconds(0)));
    CPPUNIT_ASSERT_EQUAL((size_t)1, taskQueue_.periodicTaskQueue1_.size());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHTBucketRefreshCommandTest);

class DHTTokenUpdateCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHTTokenUpdateCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testProcess);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;
  DHTTokenTracker tokenTracker_;

public:
  void setUp() { e_ = createTestEngine(option_, "aria2_DHTCommandTest", true); }
  void tearDown() {}

  std::unique_ptr<DHTTokenUpdateCommand>
  createCmd(std::chrono::seconds interval)
  {
    auto cmd =
        make_unique<DHTTokenUpdateCommand>(e_->newCUID(), e_.get(), interval);
    cmd->setTokenTracker(&tokenTracker_);
    return cmd;
  }

  void testExitOnHalt()
  {
    runHaltThenCmd(e_.get(), createCmd(std::chrono::seconds(60)));
  }

  void testProcess()
  {
    unsigned char infoHash[20];
    memset(infoHash, 0, sizeof(infoHash));
    std::string tokenBefore =
        tokenTracker_.generateToken(infoHash, "192.168.0.1", 6881);

    runCmdThenHalt(e_.get(), createCmd(std::chrono::seconds(0)));

    // Secret rotated — new token should differ
    std::string tokenAfter =
        tokenTracker_.generateToken(infoHash, "192.168.0.1", 6881);
    CPPUNIT_ASSERT(tokenBefore != tokenAfter);
    // Old token still validates (two-secret scheme)
    CPPUNIT_ASSERT(tokenTracker_.validateToken(tokenBefore, infoHash,
                                               "192.168.0.1", 6881));
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHTTokenUpdateCommandTest);

class DHTPeerAnnounceCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHTPeerAnnounceCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testProcess);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;
  DHTPeerAnnounceStorage storage_;

public:
  void setUp() { e_ = createTestEngine(option_, "aria2_DHTCommandTest", true); }
  void tearDown() {}

  std::unique_ptr<DHTPeerAnnounceCommand>
  createCmd(std::chrono::seconds interval)
  {
    auto cmd =
        make_unique<DHTPeerAnnounceCommand>(e_->newCUID(), e_.get(), interval);
    cmd->setPeerAnnounceStorage(&storage_);
    return cmd;
  }

  void testExitOnHalt()
  {
    runHaltThenCmd(e_.get(), createCmd(std::chrono::seconds(60)));
  }

  void testProcess()
  {
    // handleTimeout() on empty storage is a no-op; verify no crash
    runCmdThenHalt(e_.get(), createCmd(std::chrono::seconds(0)));
    CPPUNIT_ASSERT(true);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHTPeerAnnounceCommandTest);

class DHTAutoSaveCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHTAutoSaveCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;

public:
  void setUp()
  {
    e_ = createTestEngine(option_, "aria2_DHTCommandTest", true);
    option_->put(PREF_DHT_FILE_PATH,
                 A2_TEST_OUT_DIR "/aria2_DHTCommandTest/dht.dat");
  }
  void tearDown() {}

  void testExitOnHalt()
  {
    auto localNode = std::make_shared<DHTNode>();
    DHTRoutingTable routingTable(localNode);

    auto cmd = make_unique<DHTAutoSaveCommand>(e_->newCUID(), e_.get(), AF_INET,
                                               std::chrono::seconds(60));
    cmd->setLocalNode(localNode);
    cmd->setRoutingTable(&routingTable);

    runHaltThenCmd(e_.get(), std::move(cmd));
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHTAutoSaveCommandTest);

class DHTGetPeersCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHTGetPeersCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;
  std::shared_ptr<RequestGroup> rg_;
  std::shared_ptr<BtRuntime> btRuntime_;
  std::shared_ptr<MockPeerStorage> peerStorage_;

public:
  void setUp()
  {
    e_ = createTestEngine(option_, "aria2_DHTCommandTest", true);
    rg_ = std::make_shared<RequestGroup>(GroupId::create(), option_);
    rg_->setDownloadContext(
        std::make_shared<DownloadContext>(1048576, 0, "dummy"));
    btRuntime_ = std::make_shared<BtRuntime>();
    peerStorage_ = std::make_shared<MockPeerStorage>();
  }
  void tearDown() {}

  void testExitOnHalt()
  {
    btRuntime_->setHalt(true);
    MockDHTTaskQueue taskQueue;
    MockDHTTaskFactory taskFactory;

    auto cmd =
        make_unique<DHTGetPeersCommand>(e_->newCUID(), rg_.get(), e_.get());
    cmd->setBtRuntime(btRuntime_);
    cmd->setPeerStorage(peerStorage_);
    cmd->setTaskQueue(&taskQueue);
    cmd->setTaskFactory(&taskFactory);
    CPPUNIT_ASSERT(cmd->execute());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHTGetPeersCommandTest);

} // namespace aria2
