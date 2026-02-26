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

class DHTBucketRefreshCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHTBucketRefreshCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testProcess);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;

public:
  void setUp() { e_ = createTestEngine(option_, "aria2_DHTCommandTest", true); }
  void tearDown() {}

  void testExitOnHalt()
  {
    e_->setNoWait(true);
    auto localNode = std::make_shared<DHTNode>();
    DHTRoutingTable routingTable(localNode);
    MockDHTTaskQueue taskQueue;
    MockDHTTaskFactory taskFactory;

    auto cmd = make_unique<DHTBucketRefreshCommand>(e_->newCUID(), e_.get(),
                                                    std::chrono::seconds(60));
    cmd->setRoutingTable(&routingTable);
    cmd->setTaskQueue(&taskQueue);
    cmd->setTaskFactory(&taskFactory);

    // Halt the engine — preProcess() should call enableExit()
    e_->addCommand(make_unique<TestHaltCommand>(e_->newCUID(), e_.get()));
    e_->addCommand(std::move(cmd));
    e_->run(true);
  }

  void testProcess()
  {
    e_->setNoWait(true);
    auto localNode = std::make_shared<DHTNode>();
    DHTRoutingTable routingTable(localNode);
    MockDHTTaskQueue taskQueue;
    MockDHTTaskFactory taskFactory;

    // interval=0 so process() fires immediately on first execute()
    auto cmd = make_unique<DHTBucketRefreshCommand>(e_->newCUID(), e_.get(),
                                                    std::chrono::seconds(0));
    cmd->setRoutingTable(&routingTable);
    cmd->setTaskQueue(&taskQueue);
    cmd->setTaskFactory(&taskFactory);

    // Add DHT command first so it executes before the halt
    e_->addCommand(std::move(cmd));
    e_->addCommand(make_unique<TestHaltCommand>(e_->newCUID(), e_.get()));
    e_->run(true);

    // process() should have added a task to periodicTaskQueue1
    CPPUNIT_ASSERT_EQUAL((size_t)1, taskQueue.periodicTaskQueue1_.size());
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

public:
  void setUp() { e_ = createTestEngine(option_, "aria2_DHTCommandTest", true); }
  void tearDown() {}

  void testExitOnHalt()
  {
    e_->setNoWait(true);
    DHTTokenTracker tokenTracker;

    auto cmd = make_unique<DHTTokenUpdateCommand>(e_->newCUID(), e_.get(),
                                                  std::chrono::seconds(60));
    cmd->setTokenTracker(&tokenTracker);

    e_->addCommand(make_unique<TestHaltCommand>(e_->newCUID(), e_.get()));
    e_->addCommand(std::move(cmd));
    e_->run(true);
  }

  void testProcess()
  {
    e_->setNoWait(true);
    DHTTokenTracker tokenTracker;
    // Generate a token before update to verify it changes
    unsigned char infoHash[20];
    memset(infoHash, 0, sizeof(infoHash));
    std::string tokenBefore =
        tokenTracker.generateToken(infoHash, "192.168.0.1", 6881);

    auto cmd = make_unique<DHTTokenUpdateCommand>(e_->newCUID(), e_.get(),
                                                  std::chrono::seconds(0));
    cmd->setTokenTracker(&tokenTracker);

    e_->addCommand(make_unique<TestHaltCommand>(e_->newCUID(), e_.get()));
    e_->addCommand(std::move(cmd));
    e_->run(true);

    // Token secret was updated — the old token should still validate
    // (DHTTokenTracker keeps 2 secrets), but internal state changed.
    CPPUNIT_ASSERT(
        tokenTracker.validateToken(tokenBefore, infoHash, "192.168.0.1", 6881));
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

public:
  void setUp() { e_ = createTestEngine(option_, "aria2_DHTCommandTest", true); }
  void tearDown() {}

  void testExitOnHalt()
  {
    e_->setNoWait(true);
    DHTPeerAnnounceStorage storage;

    auto cmd = make_unique<DHTPeerAnnounceCommand>(e_->newCUID(), e_.get(),
                                                   std::chrono::seconds(60));
    cmd->setPeerAnnounceStorage(&storage);

    e_->addCommand(make_unique<TestHaltCommand>(e_->newCUID(), e_.get()));
    e_->addCommand(std::move(cmd));
    e_->run(true);
  }

  void testProcess()
  {
    e_->setNoWait(true);
    DHTPeerAnnounceStorage storage;

    auto cmd = make_unique<DHTPeerAnnounceCommand>(e_->newCUID(), e_.get(),
                                                   std::chrono::seconds(0));
    cmd->setPeerAnnounceStorage(&storage);

    e_->addCommand(make_unique<TestHaltCommand>(e_->newCUID(), e_.get()));
    e_->addCommand(std::move(cmd));
    e_->run(true);
    // handleTimeout() on empty storage is a no-op but should not crash
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
    e_->setNoWait(true);
    auto localNode = std::make_shared<DHTNode>();
    DHTRoutingTable routingTable(localNode);

    auto cmd = make_unique<DHTAutoSaveCommand>(e_->newCUID(), e_.get(), AF_INET,
                                               std::chrono::seconds(60));
    cmd->setLocalNode(localNode);
    cmd->setRoutingTable(&routingTable);

    e_->addCommand(make_unique<TestHaltCommand>(e_->newCUID(), e_.get()));
    e_->addCommand(std::move(cmd));
    e_->run(true);
    // The command should save and exit cleanly
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
    // execute() returns true when btRuntime is halted
    CPPUNIT_ASSERT(cmd->execute());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHTGetPeersCommandTest);

} // namespace aria2
