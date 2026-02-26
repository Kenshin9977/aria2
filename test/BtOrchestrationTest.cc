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
  int resetCount;
  NeverSeedCriteria() : resetCount(0) {}
  bool evaluate() override { return false; }
  void reset() override { ++resetCount; }
};

// SeedCriteria that returns true after N evaluations.
class CountdownSeedCriteria : public SeedCriteria {
public:
  int remaining;
  CountdownSeedCriteria(int n) : remaining(n) {}
  bool evaluate() override { return --remaining <= 0; }
  void reset() override {}
};

} // namespace

class SeedCheckCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SeedCheckCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testNoCriteria);
  CPPUNIT_TEST(testCriteriaEvaluated);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;
  std::shared_ptr<RequestGroup> rg_;
  std::shared_ptr<BtRuntime> btRuntime_;
  std::shared_ptr<MockPieceStorage> pieceStorage_;

public:
  void setUp()
  {
    e_ = createTestEngine(option_, "aria2_BtOrchestrationTest", true);
    rg_ = std::make_shared<RequestGroup>(GroupId::create(), option_);
    btRuntime_ = std::make_shared<BtRuntime>();
    pieceStorage_ = std::make_shared<MockPieceStorage>();
  }
  void tearDown() {}

  void testExitOnHalt()
  {
    btRuntime_->setHalt(true);
    auto cmd = make_unique<SeedCheckCommand>(e_->newCUID(), rg_.get(), e_.get(),
                                             make_unique<NeverSeedCriteria>());
    cmd->setBtRuntime(btRuntime_);
    cmd->setPieceStorage(pieceStorage_);
    // execute() should return true immediately when halted
    CPPUNIT_ASSERT(cmd->execute());
  }

  void testNoCriteria()
  {
    // When constructed with nullptr seed criteria, execute() returns false
    // and re-enqueues itself (does nothing but loop).
    auto cmd = make_unique<SeedCheckCommand>(e_->newCUID(), rg_.get(), e_.get(),
                                             nullptr);
    cmd->setBtRuntime(btRuntime_);
    cmd->setPieceStorage(pieceStorage_);
    // execute() returns false (command keeps running)
    CPPUNIT_ASSERT(!cmd->execute());
    // Command re-added itself; retrieve it from engine to prevent leak
  }

  void testCriteriaEvaluated()
  {
    // Download is finished, seed criteria evaluates to true after 1 call
    pieceStorage_->setDownloadFinished(true);
    auto cmd =
        make_unique<SeedCheckCommand>(e_->newCUID(), rg_.get(), e_.get(),
                                      make_unique<CountdownSeedCriteria>(1));
    cmd->setBtRuntime(btRuntime_);
    cmd->setPieceStorage(pieceStorage_);

    // First execute: downloadFinished() → checkStarted=true → evaluate() →
    // countdown reaches 0 → btRuntime.setHalt(true)
    cmd->execute();
    CPPUNIT_ASSERT(btRuntime_->isHalt());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(SeedCheckCommandTest);

class PeerChokeCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(PeerChokeCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testChokeNotElapsed);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;
  std::shared_ptr<BtRuntime> btRuntime_;
  std::shared_ptr<MockPeerStorage> peerStorage_;

public:
  void setUp()
  {
    e_ = createTestEngine(option_, "aria2_BtOrchestrationTest", true);
    btRuntime_ = std::make_shared<BtRuntime>();
    peerStorage_ = std::make_shared<MockPeerStorage>();
  }
  void tearDown() {}

  void testExitOnHalt()
  {
    btRuntime_->setHalt(true);
    auto cmd = make_unique<PeerChokeCommand>(e_->newCUID(), e_.get());
    cmd->setBtRuntime(btRuntime_);
    cmd->setPeerStorage(peerStorage_);
    CPPUNIT_ASSERT(cmd->execute());
  }

  void testChokeNotElapsed()
  {
    // MockPeerStorage::chokeRoundIntervalElapsed() returns false by default,
    // so executeChoke() should NOT be called.
    auto cmd = make_unique<PeerChokeCommand>(e_->newCUID(), e_.get());
    cmd->setBtRuntime(btRuntime_);
    cmd->setPeerStorage(peerStorage_);
    cmd->execute();
    CPPUNIT_ASSERT_EQUAL(0, peerStorage_->getNumChokeExecuted());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(PeerChokeCommandTest);

class BtStopDownloadCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(BtStopDownloadCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testExitOnDownloadFinished);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;
  std::shared_ptr<RequestGroup> rg_;
  std::shared_ptr<BtRuntime> btRuntime_;
  std::shared_ptr<MockPieceStorage> pieceStorage_;

public:
  void setUp()
  {
    e_ = createTestEngine(option_, "aria2_BtOrchestrationTest", true);
    rg_ = std::make_shared<RequestGroup>(GroupId::create(), option_);
    rg_->setDownloadContext(
        std::make_shared<DownloadContext>(1048576, 0, "dummy"));
    btRuntime_ = std::make_shared<BtRuntime>();
    pieceStorage_ = std::make_shared<MockPieceStorage>();
  }
  void tearDown() {}

  void testExitOnHalt()
  {
    btRuntime_->setHalt(true);
    e_->setNoWait(true);
    auto cmd = make_unique<BtStopDownloadCommand>(
        e_->newCUID(), rg_.get(), e_.get(), std::chrono::seconds(60));
    cmd->setBtRuntime(btRuntime_);
    cmd->setPieceStorage(pieceStorage_);
    // Force halt to stop the engine
    e_->addCommand(make_unique<TestHaltCommand>(e_->newCUID(), e_.get()));
    e_->addCommand(std::move(cmd));
    e_->run(true);
    // BtStopDownloadCommand should have exited via preProcess()
  }

  void testExitOnDownloadFinished()
  {
    pieceStorage_->setDownloadFinished(true);
    e_->setNoWait(true);
    auto cmd = make_unique<BtStopDownloadCommand>(
        e_->newCUID(), rg_.get(), e_.get(), std::chrono::seconds(60));
    cmd->setBtRuntime(btRuntime_);
    cmd->setPieceStorage(pieceStorage_);
    e_->addCommand(make_unique<TestHaltCommand>(e_->newCUID(), e_.get()));
    e_->addCommand(std::move(cmd));
    e_->run(true);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(BtStopDownloadCommandTest);

class TrackerWatcherCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(TrackerWatcherCommandTest);
  CPPUNIT_TEST(testExitOnNoMoreAnnounce);
  CPPUNIT_TEST(testExitOnForceHalt);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;
  std::shared_ptr<RequestGroup> rg_;
  std::shared_ptr<BtRuntime> btRuntime_;
  std::shared_ptr<MockPieceStorage> pieceStorage_;
  std::shared_ptr<MockPeerStorage> peerStorage_;

public:
  void setUp()
  {
    e_ = createTestEngine(option_, "aria2_BtOrchestrationTest", true);
    rg_ = std::make_shared<RequestGroup>(GroupId::create(), option_);
    btRuntime_ = std::make_shared<BtRuntime>();
    pieceStorage_ = std::make_shared<MockPieceStorage>();
    peerStorage_ = std::make_shared<MockPeerStorage>();
  }
  void tearDown() {}

  void testExitOnNoMoreAnnounce()
  {
    // MockBtAnnounce with noMoreAnnounce overridden to return true
    class NoMoreAnnounce : public MockBtAnnounce {
    public:
      bool noMoreAnnounce() override { return true; }
    };
    auto btAnnounce = std::make_shared<NoMoreAnnounce>();
    auto cmd =
        make_unique<TrackerWatcherCommand>(e_->newCUID(), rg_.get(), e_.get());
    cmd->setBtRuntime(btRuntime_);
    cmd->setPieceStorage(pieceStorage_);
    cmd->setPeerStorage(peerStorage_);
    cmd->setBtAnnounce(btAnnounce);
    // execute() should return true because noMoreAnnounce() is true
    CPPUNIT_ASSERT(cmd->execute());
  }

  void testExitOnForceHalt()
  {
    auto btAnnounce = std::make_shared<MockBtAnnounce>();
    auto cmd =
        make_unique<TrackerWatcherCommand>(e_->newCUID(), rg_.get(), e_.get());
    cmd->setBtRuntime(btRuntime_);
    cmd->setPieceStorage(pieceStorage_);
    cmd->setPeerStorage(peerStorage_);
    cmd->setBtAnnounce(btAnnounce);

    // Force halt with no pending tracker request → should return true
    rg_->setForceHaltRequested(true);
    CPPUNIT_ASSERT(cmd->execute());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(TrackerWatcherCommandTest);

} // namespace aria2
