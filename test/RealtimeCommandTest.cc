#include "CheckIntegrityCommand.h"
#include "FileAllocationCommand.h"

#include <cppunit/extensions/HelperMacros.h>

#include "TestEngineHelper.h"
#include "CheckIntegrityEntry.h"
#include "CheckIntegrityMan.h"
#include "FileAllocationEntry.h"
#include "FileAllocationMan.h"
#include "FileAllocationIterator.h"
#include "IteratableValidator.h"
#include "DiskAdaptor.h"
#include "DownloadContext.h"
#include "GroupId.h"
#include "MockPieceStorage.h"

namespace aria2 {

namespace {

// --- Mocks for CheckIntegrityCommand ---

class MockValidator : public IteratableValidator {
public:
  int validateCount = 0;
  bool finished_ = false;

  void init() override {}
  void validateChunk() override { ++validateCount; }
  bool finished() const override { return finished_; }
  int64_t getCurrentOffset() const override { return 0; }
  int64_t getTotalLength() const override { return 100; }
};

class MockCheckEntry : public CheckIntegrityEntry {
public:
  int onFinishedCount = 0;
  int onIncompleteCount = 0;

  MockCheckEntry(RequestGroup* rg) : CheckIntegrityEntry(rg) {}

  bool isValidationReady() override { return true; }
  void initValidator() override {}

  void onDownloadFinished(std::vector<std::unique_ptr<Command>>& commands,
                          DownloadEngine* e) override
  {
    ++onFinishedCount;
  }

  void onDownloadIncomplete(std::vector<std::unique_ptr<Command>>& commands,
                            DownloadEngine* e) override
  {
    ++onIncompleteCount;
  }

  void installValidator(std::unique_ptr<IteratableValidator> v)
  {
    setValidator(std::move(v));
  }
};

// --- Mocks for FileAllocationCommand ---

// Shared state for controlling the mock iterator from tests.
struct AllocIteratorState {
  int allocCount = 0;
  bool finished = false;
};

class MockAllocIterator : public FileAllocationIterator {
public:
  AllocIteratorState& state;

  explicit MockAllocIterator(AllocIteratorState& s) : state(s) {}

  void allocateChunk() override { ++state.allocCount; }
  bool finished() override { return state.finished; }
  int64_t getCurrentLength() override { return 0; }
  int64_t getTotalLength() override { return 100; }
};

// Minimal DiskAdaptor stub: only fileAllocationIterator() matters.
class StubDiskAdaptor : public DiskAdaptor {
public:
  AllocIteratorState& state;

  explicit StubDiskAdaptor(AllocIteratorState& s) : state(s) {}

  std::unique_ptr<FileAllocationIterator> fileAllocationIterator() override
  {
    return make_unique<MockAllocIterator>(state);
  }

  void openFile() override {}
  void closeFile() override {}
  void openExistingFile() override {}
  void initAndOpenFile() override {}
  bool fileExists() override { return false; }
  int64_t size() override { return 0; }
  void cutTrailingGarbage() override {}
  size_t utime(const Time& actime, const Time& modtime) override { return 0; }
  void writeData(const unsigned char*, size_t, int64_t) override {}
  ssize_t readData(unsigned char*, size_t, int64_t) override { return 0; }
  ssize_t readDataDropCache(unsigned char*, size_t, int64_t) override
  {
    return 0;
  }
  void writeCache(const WrDiskCacheEntry*) override {}
};

class MockAllocEntry : public FileAllocationEntry {
public:
  int prepareCount = 0;

  MockAllocEntry(RequestGroup* rg) : FileAllocationEntry(rg) {}

  void prepareForNextAction(std::vector<std::unique_ptr<Command>>& commands,
                            DownloadEngine* e) override
  {
    ++prepareCount;
  }
};

// Shared context for both fixtures.
struct RealtimeTestContext {
  std::shared_ptr<Option> option;
  std::shared_ptr<RequestGroup> rg;
  std::shared_ptr<MockPieceStorage> pieceStorage;
  // engine must be declared last: its commands hold raw pointers to rg,
  // so rg must outlive the engine.
  std::unique_ptr<DownloadEngine> engine;

  void setUp()
  {
    engine = createTestEngine(option, "aria2_RealtimeCommandTest", true);
    rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    rg->setDownloadContext(
        std::make_shared<DownloadContext>(1048576, 0, "dummy"));
    pieceStorage = std::make_shared<MockPieceStorage>();
    rg->setPieceStorage(pieceStorage);
  }
};

} // namespace

class CheckIntegrityCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(CheckIntegrityCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testFinished_downloadComplete);
  CPPUNIT_TEST(testFinished_downloadIncomplete);
  CPPUNIT_TEST(testReenqueue);
  CPPUNIT_TEST_SUITE_END();

  RealtimeTestContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testExitOnHalt()
  {
    auto entry = make_unique<MockCheckEntry>(ctx_.rg.get());
    auto validator = make_unique<MockValidator>();
    entry->installValidator(std::move(validator));

    auto& man = ctx_.engine->getCheckIntegrityMan();
    man->pushEntry(std::move(entry));
    man->pickNext();

    auto* rawEntry = static_cast<MockCheckEntry*>(man->getPickedEntry().get());

    ctx_.rg->setHaltRequested(true);
    auto cmd = make_unique<CheckIntegrityCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.engine.get(), rawEntry);
    CPPUNIT_ASSERT(cmd->execute());
  }

  void testFinished_downloadComplete()
  {
    auto entry = make_unique<MockCheckEntry>(ctx_.rg.get());
    auto validator = make_unique<MockValidator>();
    validator->finished_ = true;
    entry->installValidator(std::move(validator));

    auto& man = ctx_.engine->getCheckIntegrityMan();
    man->pushEntry(std::move(entry));
    auto* rawEntry = man->pickNext();

    ctx_.pieceStorage->setDownloadFinished(true);

    auto cmd = make_unique<CheckIntegrityCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.engine.get(), rawEntry);
    CPPUNIT_ASSERT(cmd->execute());
    CPPUNIT_ASSERT_EQUAL(
        1, static_cast<MockCheckEntry*>(rawEntry)->onFinishedCount);
    CPPUNIT_ASSERT_EQUAL(
        0, static_cast<MockCheckEntry*>(rawEntry)->onIncompleteCount);
  }

  void testFinished_downloadIncomplete()
  {
    auto entry = make_unique<MockCheckEntry>(ctx_.rg.get());
    auto validator = make_unique<MockValidator>();
    validator->finished_ = true;
    entry->installValidator(std::move(validator));

    auto& man = ctx_.engine->getCheckIntegrityMan();
    man->pushEntry(std::move(entry));
    auto* rawEntry = man->pickNext();

    ctx_.pieceStorage->setDownloadFinished(false);

    auto cmd = make_unique<CheckIntegrityCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.engine.get(), rawEntry);
    CPPUNIT_ASSERT(cmd->execute());
    CPPUNIT_ASSERT_EQUAL(
        0, static_cast<MockCheckEntry*>(rawEntry)->onFinishedCount);
    CPPUNIT_ASSERT_EQUAL(
        1, static_cast<MockCheckEntry*>(rawEntry)->onIncompleteCount);
  }

  void testReenqueue()
  {
    auto entry = make_unique<MockCheckEntry>(ctx_.rg.get());
    auto validator = make_unique<MockValidator>();
    validator->finished_ = false;
    auto* validatorPtr = validator.get();
    entry->installValidator(std::move(validator));

    auto& man = ctx_.engine->getCheckIntegrityMan();
    man->pushEntry(std::move(entry));
    man->pickNext();

    auto* rawEntry = static_cast<MockCheckEntry*>(man->getPickedEntry().get());

    // Create command on heap — engine takes ownership on re-enqueue.
    auto* cmd = new CheckIntegrityCommand(ctx_.engine->newCUID(), ctx_.rg.get(),
                                          ctx_.engine.get(), rawEntry);
    // Not finished → returns false and re-enqueues itself.
    CPPUNIT_ASSERT(!cmd->execute());

    // Now mark finished and run via engine to clean up.
    validatorPtr->finished_ = true;
    ctx_.pieceStorage->setDownloadFinished(true);
    ctx_.engine->setNoWait(true);
    ctx_.engine->addCommand(make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->run(true);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(CheckIntegrityCommandTest);

class FileAllocationCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(FileAllocationCommandTest);
  CPPUNIT_TEST(testExitOnHalt);
  CPPUNIT_TEST(testFinished);
  CPPUNIT_TEST(testReenqueue);
  CPPUNIT_TEST_SUITE_END();

  RealtimeTestContext ctx_;
  AllocIteratorState iteratorState_;

public:
  void setUp()
  {
    ctx_.setUp();
    iteratorState_.finished = false;
    iteratorState_.allocCount = 0;

    auto stubAdaptor = std::make_shared<StubDiskAdaptor>(iteratorState_);
    ctx_.pieceStorage->setDiskAdaptor(stubAdaptor);
  }
  void tearDown() {}

  void testExitOnHalt()
  {
    iteratorState_.finished = false;
    auto entry = make_unique<MockAllocEntry>(ctx_.rg.get());

    auto& man = ctx_.engine->getFileAllocationMan();
    man->pushEntry(std::move(entry));
    man->pickNext();

    auto* rawEntry = static_cast<MockAllocEntry*>(man->getPickedEntry().get());

    ctx_.rg->setHaltRequested(true);
    auto cmd = make_unique<FileAllocationCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.engine.get(), rawEntry);
    CPPUNIT_ASSERT(cmd->execute());
  }

  void testFinished()
  {
    iteratorState_.finished = true;
    auto entry = make_unique<MockAllocEntry>(ctx_.rg.get());

    auto& man = ctx_.engine->getFileAllocationMan();
    man->pushEntry(std::move(entry));
    auto* rawEntry = static_cast<MockAllocEntry*>(man->pickNext());

    auto cmd = make_unique<FileAllocationCommand>(
        ctx_.engine->newCUID(), ctx_.rg.get(), ctx_.engine.get(), rawEntry);
    CPPUNIT_ASSERT(cmd->execute());
    CPPUNIT_ASSERT_EQUAL(1, rawEntry->prepareCount);
  }

  void testReenqueue()
  {
    iteratorState_.finished = false;
    auto entry = make_unique<MockAllocEntry>(ctx_.rg.get());

    auto& man = ctx_.engine->getFileAllocationMan();
    man->pushEntry(std::move(entry));
    auto* rawEntry = static_cast<MockAllocEntry*>(man->pickNext());

    // Command on heap — engine takes ownership on re-enqueue.
    auto* cmd = new FileAllocationCommand(ctx_.engine->newCUID(), ctx_.rg.get(),
                                          ctx_.engine.get(), rawEntry);
    CPPUNIT_ASSERT(!cmd->execute());

    // Mark finished to let the command complete on next engine tick.
    iteratorState_.finished = true;
    ctx_.engine->setNoWait(true);
    ctx_.engine->addCommand(make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->run(true);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(FileAllocationCommandTest);

} // namespace aria2
