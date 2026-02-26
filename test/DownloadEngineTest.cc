#include "DownloadEngine.h"

#include <cppunit/extensions/HelperMacros.h>

#include "TestEngineHelper.h"

namespace aria2 {

namespace {

// A Command that increments an external counter each time execute() is
// called, then exits after a configurable number of calls.  The counter
// is external so that we can inspect it after the Command has been
// destroyed by the engine.
class CountingCommand : public Command {
public:
  int& counter;
  int maxExecutions;
  DownloadEngine* e;

  CountingCommand(cuid_t cuid, DownloadEngine* engine, int maxExec,
                  int& externalCounter)
      : Command(cuid),
        counter(externalCounter),
        maxExecutions(maxExec),
        e(engine)
  {
  }

  bool execute() override
  {
    ++counter;
    if (counter >= maxExecutions) {
      return true; // done, delete this command
    }
    // Re-add self — engine takes ownership, does not delete when
    // execute() returns false.
    e->addCommand(std::unique_ptr<Command>(this));
    return false;
  }
};

// A Command that records itself as executed, for verifying scheduling
// order.
class RecordingCommand : public Command {
public:
  std::vector<cuid_t>& log;

  RecordingCommand(cuid_t cuid, std::vector<cuid_t>& executionLog)
      : Command(cuid), log(executionLog)
  {
  }

  bool execute() override
  {
    log.push_back(getCuid());
    return true;
  }
};

} // namespace

class DownloadEngineTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DownloadEngineTest);
  CPPUNIT_TEST(testRun_emptyCommands);
  CPPUNIT_TEST(testRun_singleCommand);
  CPPUNIT_TEST(testRun_commandRequeues);
  CPPUNIT_TEST(testRun_halt);
  CPPUNIT_TEST(testRun_forceHalt);
  CPPUNIT_TEST(testAddCommand);
  CPPUNIT_TEST(testAddRoutineCommand);
  CPPUNIT_TEST(testNewCUID);
  CPPUNIT_TEST(testHaltState);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;

public:
  void setUp() { e_ = createTestEngine(option_, "aria2_DownloadEngineTest"); }

  void tearDown() {}

  void testRun_emptyCommands()
  {
    // run() with no commands should return immediately
    int rv = e_->run(true);
    CPPUNIT_ASSERT_EQUAL(0, rv);
  }

  void testRun_singleCommand()
  {
    int count = 0;
    e_->setNoWait(true);
    e_->addCommand(std::unique_ptr<Command>(
        new CountingCommand(e_->newCUID(), e_.get(), 1, count)));
    e_->run(false);
    CPPUNIT_ASSERT_EQUAL(1, count);
  }

  void testRun_commandRequeues()
  {
    int count = 0;
    e_->setNoWait(true);
    e_->addCommand(std::unique_ptr<Command>(
        new CountingCommand(e_->newCUID(), e_.get(), 3, count)));
    e_->run(false);
    CPPUNIT_ASSERT_EQUAL(3, count);
  }

  void testRun_halt()
  {
    // HaltCommand requests halt, then engine should stop
    e_->addCommand(
        make_unique<TestHaltCommand>(e_->newCUID(), e_.get(), false));
    e_->run(false);
    CPPUNIT_ASSERT(e_->isHaltRequested());
    CPPUNIT_ASSERT(!e_->isForceHaltRequested());
  }

  void testRun_forceHalt()
  {
    e_->addCommand(make_unique<TestHaltCommand>(e_->newCUID(), e_.get(), true));
    e_->run(false);
    CPPUNIT_ASSERT(e_->isHaltRequested());
    CPPUNIT_ASSERT(e_->isForceHaltRequested());
  }

  void testAddCommand()
  {
    std::vector<cuid_t> log;
    e_->addCommand(make_unique<RecordingCommand>(1, log));
    e_->addCommand(make_unique<RecordingCommand>(2, log));
    e_->addCommand(make_unique<RecordingCommand>(3, log));
    e_->run(false);
    CPPUNIT_ASSERT_EQUAL((size_t)3, log.size());
    // Commands execute in FIFO order (deque front)
    CPPUNIT_ASSERT_EQUAL((cuid_t)1, log[0]);
    CPPUNIT_ASSERT_EQUAL((cuid_t)2, log[1]);
    CPPUNIT_ASSERT_EQUAL((cuid_t)3, log[2]);
  }

  void testAddRoutineCommand()
  {
    std::vector<cuid_t> log;
    // Routine commands are separate from regular commands
    // Add a regular command that halts (so run() eventually stops)
    e_->addCommand(
        make_unique<TestHaltCommand>(e_->newCUID(), e_.get(), false));
    e_->addRoutineCommand(make_unique<RecordingCommand>(100, log));
    e_->run(false);
    // The routine command should have executed
    CPPUNIT_ASSERT(!log.empty());
    CPPUNIT_ASSERT_EQUAL((cuid_t)100, log[0]);
  }

  void testNewCUID()
  {
    cuid_t c1 = e_->newCUID();
    cuid_t c2 = e_->newCUID();
    cuid_t c3 = e_->newCUID();
    // CUIDs should be unique and incrementing
    CPPUNIT_ASSERT(c1 < c2);
    CPPUNIT_ASSERT(c2 < c3);
  }

  void testHaltState()
  {
    CPPUNIT_ASSERT(!e_->isHaltRequested());
    CPPUNIT_ASSERT(!e_->isForceHaltRequested());

    e_->requestHalt();
    CPPUNIT_ASSERT(e_->isHaltRequested());
    CPPUNIT_ASSERT(!e_->isForceHaltRequested());

    e_->requestForceHalt();
    CPPUNIT_ASSERT(e_->isHaltRequested());
    CPPUNIT_ASSERT(e_->isForceHaltRequested());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DownloadEngineTest);

} // namespace aria2
