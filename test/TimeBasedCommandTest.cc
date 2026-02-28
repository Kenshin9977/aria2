#include "TimeBasedCommand.h"
#include "AutoSaveCommand.h"
#include "TimedHaltCommand.h"

#include <cppunit/extensions/HelperMacros.h>

#include "TestEngineHelper.h"

namespace aria2 {

namespace {

// Counters live on the stack so they survive command destruction.
struct TimedCommandCounters {
  int preProcess;
  int process;
  int postProcess;
  TimedCommandCounters() : preProcess(0), process(0), postProcess(0) {}
};

class MockTimedCommand : public TimeBasedCommand {
public:
  TimedCommandCounters& counters;
  bool exitOnProcess;

  MockTimedCommand(cuid_t cuid, DownloadEngine* e,
                   std::chrono::seconds interval, TimedCommandCounters& c,
                   bool routineCommand = false)
      : TimeBasedCommand(cuid, e, interval, routineCommand),
        counters(c),
        exitOnProcess(false)
  {
  }

  void preProcess() override { ++counters.preProcess; }

  void process() override
  {
    ++counters.process;
    if (exitOnProcess) {
      enableExit();
    }
  }

  void postProcess() override { ++counters.postProcess; }
};

} // namespace

class TimeBasedCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(TimeBasedCommandTest);
  CPPUNIT_TEST(testExecute_intervalNotElapsed);
  CPPUNIT_TEST(testExecute_intervalElapsed);
  CPPUNIT_TEST(testExecute_prePostProcessCalled);
  CPPUNIT_TEST(testAutoSaveCommand_exitOnHalt);
  CPPUNIT_TEST(testAutoSaveCommand_exitOnFinished);
  CPPUNIT_TEST(testTimedHaltCommand);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;

public:
  void setUp()
  {
    e_ = createTestEngine(option_, "aria2_TimeBasedCommandTest", true);
  }

  void tearDown() {}

  void testExecute_intervalNotElapsed()
  {
    TimedCommandCounters counters;
    auto* cmd = new MockTimedCommand(e_->newCUID(), e_.get(),
                                     std::chrono::seconds(60), counters);
    cmd->exitOnProcess = true;
    e_->setNoWait(true);
    e_->addCommand(std::make_unique<TestHaltCommand>(e_->newCUID(), e_.get()));
    e_->addCommand(std::unique_ptr<Command>(cmd));
    e_->run(true);
    // interval=60s didn't elapse, so process() was never called
    CPPUNIT_ASSERT_EQUAL(1, counters.preProcess);
    CPPUNIT_ASSERT_EQUAL(0, counters.process);
    CPPUNIT_ASSERT_EQUAL(1, counters.postProcess);
  }

  void testExecute_intervalElapsed()
  {
    TimedCommandCounters counters;
    auto* cmd = new MockTimedCommand(e_->newCUID(), e_.get(),
                                     std::chrono::seconds(0), counters);
    cmd->exitOnProcess = true;
    e_->setNoWait(true);
    e_->addCommand(std::unique_ptr<Command>(cmd));
    e_->run(false);
    // interval=0 elapsed immediately, process() called and triggered exit
    CPPUNIT_ASSERT_EQUAL(1, counters.process);
  }

  void testExecute_prePostProcessCalled()
  {
    TimedCommandCounters counters;
    auto* cmd = new MockTimedCommand(e_->newCUID(), e_.get(),
                                     std::chrono::seconds(60), counters);
    cmd->exitOnProcess = false;
    e_->setNoWait(true);
    e_->addCommand(std::make_unique<TestHaltCommand>(e_->newCUID(), e_.get()));
    e_->addCommand(std::unique_ptr<Command>(cmd));
    e_->run(true);
    CPPUNIT_ASSERT(counters.preProcess >= 1);
    CPPUNIT_ASSERT(counters.postProcess >= 1);
  }

  void testAutoSaveCommand_exitOnHalt()
  {
    e_->requestHalt();
    auto cmd = std::make_unique<AutoSaveCommand>(e_->newCUID(), e_.get(),
                                            std::chrono::seconds(60));
    e_->addCommand(std::move(cmd));
    e_->run(false);
    CPPUNIT_ASSERT(e_->isHaltRequested());
  }

  void testAutoSaveCommand_exitOnFinished()
  {
    // keepRunning=false so engine exits when downloads finish
    e_ = createTestEngine(option_, "aria2_TimeBasedCommandTest", false);
    auto cmd = std::make_unique<AutoSaveCommand>(e_->newCUID(), e_.get(),
                                            std::chrono::seconds(60));
    e_->addCommand(std::move(cmd));
    e_->run(false);
    // Engine stopped because downloadFinished() returned true
    CPPUNIT_ASSERT(!e_->isHaltRequested());
  }

  void testTimedHaltCommand()
  {
    e_->setNoWait(true);
    auto cmd = std::make_unique<TimedHaltCommand>(e_->newCUID(), e_.get(),
                                             std::chrono::seconds(0));
    e_->addCommand(std::move(cmd));
    e_->run(false);
    CPPUNIT_ASSERT(e_->isHaltRequested());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(TimeBasedCommandTest);

} // namespace aria2
