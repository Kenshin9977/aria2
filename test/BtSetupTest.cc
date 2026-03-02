#include "BtSetup.h"
#include "BtCheckIntegrityEntry.h"
#include "BtFileAllocationEntry.h"
#include "StreamCheckIntegrityEntry.h"
#include "StreamFileAllocationEntry.h"

#include <cppunit/extensions/HelperMacros.h>

#include "TestEngineHelper.h"
#include "BtRuntime.h"
#include "BtRegistry.h"
#include "DownloadContext.h"
#include "GroupId.h"
#include "MockPieceStorage.h"
#include "MockPeerStorage.h"
#include "MockBtAnnounce.h"
#include "MockBtProgressInfoFile.h"
#include "Option.h"
#include "RequestGroup.h"
#include "bittorrent_helper.h"
#include "prefs.h"

namespace aria2 {

namespace {

struct BtSetupTestContext {
  std::shared_ptr<Option> option;
  std::shared_ptr<RequestGroup> rg;
  std::shared_ptr<DownloadContext> dctx;
  std::shared_ptr<BtRuntime> btRuntime;
  std::shared_ptr<MockPieceStorage> pieceStorage;
  std::shared_ptr<MockPeerStorage> peerStorage;
  std::shared_ptr<MockBtAnnounce> btAnnounce;
  std::unique_ptr<DownloadEngine> engine;

  void setUp()
  {
    engine = createTestEngine(option, "aria2_BtSetupTest", true);

    // Load torrent to get a valid BT DownloadContext
    dctx = std::make_shared<DownloadContext>();
    bittorrent::load(A2_TEST_DIR "/test.torrent", dctx, option);

    rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    rg->setDownloadContext(dctx);

    btRuntime = std::make_shared<BtRuntime>();
    pieceStorage = std::make_shared<MockPieceStorage>();
    peerStorage = std::make_shared<MockPeerStorage>();
    btAnnounce = std::make_shared<MockBtAnnounce>();

    auto btObj = std::make_unique<BtObject>(
        dctx, pieceStorage, peerStorage, btAnnounce, btRuntime,
        std::make_shared<MockBtProgressInfoFile>());
    engine->getBtRegistry()->put(rg->getGID(), std::move(btObj));

    // Pre-set TCP port to skip PeerListenCommand binding
    engine->getBtRegistry()->setTcpPort(6881);
  }
};

} // namespace

class BtSetupTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(BtSetupTest);
  CPPUNIT_TEST(testSetupNoBtAttribute);
  CPPUNIT_TEST(testSetupBasicCommands);
  CPPUNIT_TEST(testSetupWithSeedTime);
  CPPUNIT_TEST(testSetupWithStopTimeout);
  CPPUNIT_TEST(testSetupSetsReady);
  CPPUNIT_TEST_SUITE_END();

  BtSetupTestContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testSetupNoBtAttribute()
  {
    // Create a non-BT DownloadContext
    auto plainDctx = std::make_shared<DownloadContext>(1048576, 0, "dummy");
    auto plainRg =
        std::make_shared<RequestGroup>(GroupId::create(), ctx_.option);
    plainRg->setDownloadContext(plainDctx);

    std::vector<std::unique_ptr<Command>> commands;
    BtSetup().setup(commands, plainRg.get(), ctx_.engine.get(),
                    ctx_.option.get());

    // No commands created for non-BT context
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), commands.size());
  }

  void testSetupBasicCommands()
  {
    std::vector<std::unique_ptr<Command>> commands;
    BtSetup().setup(commands, ctx_.rg.get(), ctx_.engine.get(),
                    ctx_.option.get());

    // Basic setup creates at least:
    // TrackerWatcherCommand + PeerChokeCommand + ActivePeerConnectionCommand
    CPPUNIT_ASSERT(commands.size() >= 3);
  }

  void testSetupWithSeedTime()
  {
    ctx_.option->put(PREF_SEED_TIME, "60");

    std::vector<std::unique_ptr<Command>> commands;
    BtSetup().setup(commands, ctx_.rg.get(), ctx_.engine.get(),
                    ctx_.option.get());

    // With seed time, a SeedCheckCommand is also created (4+ commands)
    CPPUNIT_ASSERT(commands.size() >= 4);
  }

  void testSetupWithStopTimeout()
  {
    ctx_.option->put(PREF_BT_STOP_TIMEOUT, "120");

    std::vector<std::unique_ptr<Command>> commands;
    BtSetup().setup(commands, ctx_.rg.get(), ctx_.engine.get(),
                    ctx_.option.get());

    // With stop timeout, a BtStopDownloadCommand is also created
    CPPUNIT_ASSERT(commands.size() >= 4);
  }

  void testSetupSetsReady()
  {
    CPPUNIT_ASSERT(!ctx_.btRuntime->ready());

    std::vector<std::unique_ptr<Command>> commands;
    BtSetup().setup(commands, ctx_.rg.get(), ctx_.engine.get(),
                    ctx_.option.get());

    // After setup, btRuntime is marked ready
    CPPUNIT_ASSERT(ctx_.btRuntime->ready());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(BtSetupTest);

class BtCheckIntegrityEntryTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(BtCheckIntegrityEntryTest);
  CPPUNIT_TEST(testConstruct);
  CPPUNIT_TEST_SUITE_END();

public:
  void testConstruct()
  {
    auto option = std::make_shared<Option>();
    auto rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    auto dctx = std::make_shared<DownloadContext>(1048576, 0, "dummy");
    rg->setDownloadContext(dctx);

    BtCheckIntegrityEntry entry(rg.get());
    // The entry should report total length from the download context
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), entry.getTotalLength());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(BtCheckIntegrityEntryTest);

class StreamCheckIntegrityEntryTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(StreamCheckIntegrityEntryTest);
  CPPUNIT_TEST(testConstruct);
  CPPUNIT_TEST_SUITE_END();

public:
  void testConstruct()
  {
    auto option = std::make_shared<Option>();
    auto rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    auto dctx = std::make_shared<DownloadContext>(1048576, 0, "dummy");
    rg->setDownloadContext(dctx);

    StreamCheckIntegrityEntry entry(rg.get());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), entry.getTotalLength());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(StreamCheckIntegrityEntryTest);

class BtFileAllocationEntryTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(BtFileAllocationEntryTest);
  CPPUNIT_TEST(testConstruct);
  CPPUNIT_TEST_SUITE_END();

public:
  void testConstruct()
  {
    auto option = std::make_shared<Option>();
    auto rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    auto dctx = std::make_shared<DownloadContext>(1048576, 0, "dummy");
    rg->setDownloadContext(dctx);

    BtFileAllocationEntry entry(rg.get());
    // File allocation entry starts with 0 current length
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), entry.getCurrentLength());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(BtFileAllocationEntryTest);

class StreamFileAllocationEntryTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(StreamFileAllocationEntryTest);
  CPPUNIT_TEST(testConstruct);
  CPPUNIT_TEST_SUITE_END();

public:
  void testConstruct()
  {
    auto option = std::make_shared<Option>();
    auto rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    auto dctx = std::make_shared<DownloadContext>(1048576, 0, "dummy");
    rg->setDownloadContext(dctx);

    StreamFileAllocationEntry entry(rg.get());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), entry.getCurrentLength());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(StreamFileAllocationEntryTest);

} // namespace aria2
