#include "SeedCheckCommand.h"
#include "PeerChokeCommand.h"
#include "BtStopDownloadCommand.h"
#include "TrackerWatcherCommand.h"

#include <cppunit/extensions/HelperMacros.h>

#include "TestEngineHelper.h"
#include "BtRuntime.h"
#include "SeedCriteria.h"
#include "DownloadContext.h"
#include "GroupId.h"
#include "MockPieceStorage.h"
#include "MockPeerStorage.h"
#include "MockBtAnnounce.h"

namespace aria2 {

namespace {

// SeedCriteria that always returns false (seed forever).
class NeverSeedCriteria : public SeedCriteria {
public:
  bool evaluate() override { return false; }
  void reset() override {}
};

// SeedCriteria that returns true after N evaluations.
class CountdownSeedCriteria : public SeedCriteria {
public:
  int remaining;
  explicit CountdownSeedCriteria(int n) : remaining(n) {}
  bool evaluate() override { return --remaining <= 0; }
  void reset() override {}
};

// Shared setUp for BT orchestration test fixtures.
struct BtTestContext {
  std::shared_ptr<Option> option;
  std::shared_ptr<RequestGroup> rg;
  std::shared_ptr<BtRuntime> btRuntime;
  std::shared_ptr<MockPieceStorage> pieceStorage;
  std::shared_ptr<MockPeerStorage> peerStorage;
  // engine must be declared last: its commands hold raw pointers to rg,
  // so rg must outlive the engine.
  std::unique_ptr<DownloadEngine> engine;

  void setUp(bool needRequestGroup = true)
  {
    engine = createTestEngine(option, "aria2_BtOrchestrationTest", true);
    btRuntime = std::make_shared<BtRuntime>();
    pieceStorage = std::make_shared<MockPieceStorage>();
    peerStorage = std::make_shared<MockPeerStorage>();
    if (needRequestGroup) {
      rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    }
  }
};

} // namespace

class SeedCheckCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SeedCheckCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testNoCriteria);
  CPPUNIT_TEST(testCriteriaEvaluated);
  CPPUNIT_TEST_SUITE_END();

  BtTestContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testExitOnHalt()
  {
    ctx_.btRuntime->setHalt(true);
    auto cmd = make_unique<SeedCheckCommand>(ctx_.engine->newCUID(),
                                             ctx_.rg.get(), ctx_.engine.get(),
                                             make_unique<NeverSeedCriteria>());
    cmd->setBtRuntime(ctx_.btRuntime);
    cmd->setPieceStorage(ctx_.pieceStorage);
    CPPUNIT_ASSERT(cmd->execute());
  }

  void testNoCriteria()
  {
    // SeedCheckCommand::execute() always re-enqueues via addCommand(this),
    // so release ownership before calling execute() to avoid double-free.
    auto cmd = make_unique<SeedCheckCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.engine.get(), nullptr);
    cmd->setBtRuntime(ctx_.btRuntime);
    cmd->setPieceStorage(ctx_.pieceStorage);
    auto* rawCmd = cmd.release();
    CPPUNIT_ASSERT(!rawCmd->execute());

    ctx_.btRuntime->setHalt(true);
    ctx_.engine->setNoWait(true);
    ctx_.engine->addCommand(make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->run(true);
  }

  void testCriteriaEvaluated()
  {
    ctx_.pieceStorage->setDownloadFinished(true);
    auto cmd = make_unique<SeedCheckCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.engine.get(),
        make_unique<CountdownSeedCriteria>(1));
    cmd->setBtRuntime(ctx_.btRuntime);
    cmd->setPieceStorage(ctx_.pieceStorage);
    auto* rawCmd = cmd.release();
    rawCmd->execute();
    CPPUNIT_ASSERT(ctx_.btRuntime->isHalt());

    ctx_.engine->setNoWait(true);
    ctx_.engine->addCommand(make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->run(true);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(SeedCheckCommandTest);

class PeerChokeCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(PeerChokeCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testChokeNotElapsed);
  CPPUNIT_TEST_SUITE_END();

  BtTestContext ctx_;

public:
  void setUp() { ctx_.setUp(false); }
  void tearDown() {}

  void testExitOnHalt()
  {
    ctx_.btRuntime->setHalt(true);
    auto cmd = make_unique<PeerChokeCommand>(ctx_.engine->newCUID(),
                                             ctx_.engine.get());
    cmd->setBtRuntime(ctx_.btRuntime);
    cmd->setPeerStorage(ctx_.peerStorage);
    CPPUNIT_ASSERT(cmd->execute());
  }

  void testChokeNotElapsed()
  {
    // PeerChokeCommand::execute() always re-enqueues via addCommand(this),
    // so release ownership before calling execute() to avoid double-free.
    auto cmd = make_unique<PeerChokeCommand>(ctx_.engine->newCUID(),
                                             ctx_.engine.get());
    cmd->setBtRuntime(ctx_.btRuntime);
    cmd->setPeerStorage(ctx_.peerStorage);
    auto* rawCmd = cmd.release();
    rawCmd->execute();
    CPPUNIT_ASSERT_EQUAL(0, ctx_.peerStorage->getNumChokeExecuted());

    ctx_.btRuntime->setHalt(true);
    ctx_.engine->setNoWait(true);
    ctx_.engine->addCommand(make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->run(true);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(PeerChokeCommandTest);

class BtStopDownloadCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(BtStopDownloadCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testExitOnDownloadFinished);
  CPPUNIT_TEST_SUITE_END();

  BtTestContext ctx_;

public:
  void setUp()
  {
    ctx_.setUp();
    ctx_.rg->setDownloadContext(
        std::make_shared<DownloadContext>(1048576, 0, "dummy"));
  }
  void tearDown() {}

  void testExitOnHalt()
  {
    ctx_.btRuntime->setHalt(true);
    ctx_.engine->setNoWait(true);
    auto cmd = make_unique<BtStopDownloadCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.engine.get(),
        std::chrono::seconds(60));
    cmd->setBtRuntime(ctx_.btRuntime);
    cmd->setPieceStorage(ctx_.pieceStorage);
    ctx_.engine->addCommand(make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->addCommand(std::move(cmd));
    ctx_.engine->run(true);
    CPPUNIT_ASSERT(ctx_.btRuntime->isHalt());
  }

  void testExitOnDownloadFinished()
  {
    ctx_.pieceStorage->setDownloadFinished(true);
    ctx_.engine->setNoWait(true);
    auto cmd = make_unique<BtStopDownloadCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.engine.get(),
        std::chrono::seconds(60));
    cmd->setBtRuntime(ctx_.btRuntime);
    cmd->setPieceStorage(ctx_.pieceStorage);
    ctx_.engine->addCommand(make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->addCommand(std::move(cmd));
    ctx_.engine->run(true);
    CPPUNIT_ASSERT(ctx_.pieceStorage->downloadFinished());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(BtStopDownloadCommandTest);

class TrackerWatcherCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(TrackerWatcherCommandTest);
  CPPUNIT_TEST(testExitOnNoMoreAnnounce);
  CPPUNIT_TEST(testExitOnForceHalt);
  CPPUNIT_TEST_SUITE_END();

  BtTestContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testExitOnNoMoreAnnounce()
  {
    class NoMoreAnnounce : public MockBtAnnounce {
    public:
      bool noMoreAnnounce() override { return true; }
    };
    auto btAnnounce = std::make_shared<NoMoreAnnounce>();
    auto cmd = make_unique<TrackerWatcherCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.engine.get());
    cmd->setBtRuntime(ctx_.btRuntime);
    cmd->setPieceStorage(ctx_.pieceStorage);
    cmd->setPeerStorage(ctx_.peerStorage);
    cmd->setBtAnnounce(btAnnounce);
    CPPUNIT_ASSERT(cmd->execute());
  }

  void testExitOnForceHalt()
  {
    auto btAnnounce = std::make_shared<MockBtAnnounce>();
    auto cmd = make_unique<TrackerWatcherCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.engine.get());
    cmd->setBtRuntime(ctx_.btRuntime);
    cmd->setPieceStorage(ctx_.pieceStorage);
    cmd->setPeerStorage(ctx_.peerStorage);
    cmd->setBtAnnounce(btAnnounce);
    ctx_.rg->setForceHaltRequested(true);
    CPPUNIT_ASSERT(cmd->execute());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(TrackerWatcherCommandTest);

} // namespace aria2
