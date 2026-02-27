#include "RequestGroupMan.h"

#include <fstream>

#include <cppunit/extensions/HelperMacros.h>

#include "TestUtil.h"
#include "prefs.h"
#include "DownloadContext.h"
#include "RequestGroup.h"
#include "Option.h"
#include "DownloadResult.h"
#include "FileEntry.h"
#include "ServerStatMan.h"
#include "ServerStat.h"
#include "File.h"
#include "array_fun.h"
#include "RecoverableException.h"
#include "util.h"
#include "DownloadEngine.h"
#include "SelectEventPoll.h"
#include "UriListParser.h"

namespace aria2 {

class RequestGroupManTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(RequestGroupManTest);
  CPPUNIT_TEST(testIsSameFileBeingDownloaded);
  CPPUNIT_TEST(testGetInitialCommands);
  CPPUNIT_TEST(testLoadServerStat);
  CPPUNIT_TEST(testSaveServerStat);
  CPPUNIT_TEST(testChangeReservedGroupPosition);
  CPPUNIT_TEST(testFillRequestGroupFromReserver);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_uriParser);
  CPPUNIT_TEST(testInsertReservedGroup);
  CPPUNIT_TEST(testAddDownloadResult);
  CPPUNIT_TEST(testCountRequestGroup);
  CPPUNIT_TEST(testDownloadFinished);
  CPPUNIT_TEST(testFindGroup);
  CPPUNIT_TEST(testRemoveReservedGroup);
  CPPUNIT_TEST(testFindDownloadResult);
  CPPUNIT_TEST(testPurgeDownloadResult);
  CPPUNIT_TEST(testRemoveDownloadResult);
  CPPUNIT_TEST(testGetDownloadStat);
  CPPUNIT_TEST(testOverallSpeedLimits);
  CPPUNIT_TEST(testHalt);
  CPPUNIT_TEST(testQueueCheck);
  CPPUNIT_TEST_SUITE_END();

private:
  std::unique_ptr<DownloadEngine> e_;
  std::shared_ptr<Option> option_;
  RequestGroupMan* rgman_;

public:
  void setUp()
  {
    option_ = std::make_shared<Option>();
    option_->put(PREF_PIECE_LENGTH, "1048576");
    // To enable paused RequestGroup
    option_->put(PREF_ENABLE_RPC, A2_V_TRUE);
    File(option_->get(PREF_DIR)).mkdirs();
    e_ = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
    e_->setOption(option_.get());
    auto rgman = make_unique<RequestGroupMan>(
        std::vector<std::shared_ptr<RequestGroup>>{}, 3, option_.get());
    rgman_ = rgman.get();
    e_->setRequestGroupMan(std::move(rgman));
  }

  void testIsSameFileBeingDownloaded();
  void testGetInitialCommands();
  void testLoadServerStat();
  void testSaveServerStat();
  void testChangeReservedGroupPosition();
  void testFillRequestGroupFromReserver();
  void testFillRequestGroupFromReserver_uriParser();
  void testInsertReservedGroup();
  void testAddDownloadResult();
  void testCountRequestGroup();
  void testDownloadFinished();
  void testFindGroup();
  void testRemoveReservedGroup();
  void testFindDownloadResult();
  void testPurgeDownloadResult();
  void testRemoveDownloadResult();
  void testGetDownloadStat();
  void testOverallSpeedLimits();
  void testHalt();
  void testQueueCheck();
};

CPPUNIT_TEST_SUITE_REGISTRATION(RequestGroupManTest);

void RequestGroupManTest::testIsSameFileBeingDownloaded()
{
  std::shared_ptr<RequestGroup> rg1(
      new RequestGroup(GroupId::create(), util::copy(option_)));
  std::shared_ptr<RequestGroup> rg2(
      new RequestGroup(GroupId::create(), util::copy(option_)));

  std::shared_ptr<DownloadContext> dctx1(
      new DownloadContext(0, 0, "aria2.tar.bz2"));
  std::shared_ptr<DownloadContext> dctx2(
      new DownloadContext(0, 0, "aria2.tar.bz2"));

  rg1->setDownloadContext(dctx1);
  rg2->setDownloadContext(dctx2);

  RequestGroupMan gm(std::vector<std::shared_ptr<RequestGroup>>(), 1,
                     option_.get());

  gm.addRequestGroup(rg1);
  gm.addRequestGroup(rg2);

  CPPUNIT_ASSERT(gm.isSameFileBeingDownloaded(rg1.get()));

  dctx2->getFirstFileEntry()->setPath("aria2.tar.gz");

  CPPUNIT_ASSERT(!gm.isSameFileBeingDownloaded(rg1.get()));
}

void RequestGroupManTest::testGetInitialCommands()
{
  // TODO implement later
}

void RequestGroupManTest::testSaveServerStat()
{
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 0,
                     option_.get());
  std::shared_ptr<ServerStat> ss_localhost(new ServerStat("localhost", "http"));
  rm.addServerStat(ss_localhost);
  File f(A2_TEST_OUT_DIR "/aria2_RequestGroupManTest_testSaveServerStat");
  if (f.exists()) {
    f.remove();
  }
  CPPUNIT_ASSERT(rm.saveServerStat(f.getPath()));
  CPPUNIT_ASSERT(f.isFile());

  f.remove();
  CPPUNIT_ASSERT(f.mkdirs());
  CPPUNIT_ASSERT(!rm.saveServerStat(f.getPath()));
}

void RequestGroupManTest::testLoadServerStat()
{
  File f(A2_TEST_OUT_DIR "/aria2_RequestGroupManTest_testLoadServerStat");
  std::ofstream o(f.getPath().c_str(), std::ios::binary);
  o << "host=localhost, protocol=http, dl_speed=0, last_updated=1219505257,"
    << "status=OK";
  o.close();

  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 0,
                     option_.get());
  std::cerr << "testLoadServerStat" << std::endl;
  CPPUNIT_ASSERT(rm.loadServerStat(f.getPath()));
  std::shared_ptr<ServerStat> ss_localhost =
      rm.findServerStat("localhost", "http");
  CPPUNIT_ASSERT(ss_localhost);
  CPPUNIT_ASSERT_EQUAL(std::string("localhost"), ss_localhost->getHostname());
}

void RequestGroupManTest::testChangeReservedGroupPosition()
{
  std::vector<std::shared_ptr<RequestGroup>> gs{
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)),
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)),
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)),
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_))};
  RequestGroupMan rm(gs, 0, option_.get());

  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), 0, OFFSET_MODE_SET));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), 1, OFFSET_MODE_SET));
  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), 10, OFFSET_MODE_SET));
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), -10, OFFSET_MODE_SET));

  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 0, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)2, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 1, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), -1, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), -10, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 1, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 10, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), -2, OFFSET_MODE_CUR));

  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), 0, OFFSET_MODE_END));
  CPPUNIT_ASSERT_EQUAL((size_t)2, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), -1, OFFSET_MODE_END));
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), -10, OFFSET_MODE_END));
  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), 10, OFFSET_MODE_END));

  CPPUNIT_ASSERT_EQUAL((size_t)4, rm.getReservedGroups().size());

  try {
    rm.changeReservedGroupPosition(GroupId::create()->getNumericId(), 0,
                                   OFFSET_MODE_CUR);
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (RecoverableException& e) {
    // success
  }
}

void RequestGroupManTest::testFillRequestGroupFromReserver()
{
  std::shared_ptr<RequestGroup> rgs[] = {
      createRequestGroup(0, 0, "foo1", "http://host/foo1", util::copy(option_)),
      createRequestGroup(0, 0, "foo2", "http://host/foo2", util::copy(option_)),
      createRequestGroup(0, 0, "foo3", "http://host/foo3", util::copy(option_)),
      // Intentionally same path/URI for first RequestGroup and set
      // length explicitly to do duplicate filename check.
      createRequestGroup(0, 10, "foo1", "http://host/foo1",
                         util::copy(option_)),
      createRequestGroup(0, 0, "foo4", "http://host/foo4", util::copy(option_)),
      createRequestGroup(0, 0, "foo5", "http://host/foo5",
                         util::copy(option_))};
  rgs[1]->setPauseRequested(true);
  for (const auto& i : rgs) {
    rgman_->addReservedGroup(i);
  }
  rgman_->fillRequestGroupFromReserver(e_.get());

  CPPUNIT_ASSERT_EQUAL((size_t)2, rgman_->getReservedGroups().size());
}

void RequestGroupManTest::testFillRequestGroupFromReserver_uriParser()
{
  std::shared_ptr<RequestGroup> rgs[] = {
      createRequestGroup(0, 0, "mem1", "http://mem1", util::copy(option_)),
      createRequestGroup(0, 0, "mem2", "http://mem2", util::copy(option_)),
  };
  rgs[0]->setPauseRequested(true);
  for (const auto& i : rgs) {
    rgman_->addReservedGroup(i);
  }

  std::shared_ptr<UriListParser> flp(
      new UriListParser(A2_TEST_DIR "/filelist2.txt"));
  rgman_->setUriListParser(flp);

  rgman_->fillRequestGroupFromReserver(e_.get());

  RequestGroupList::const_iterator itr;
  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman_->getReservedGroups().size());
  itr = rgman_->getReservedGroups().begin();
  CPPUNIT_ASSERT_EQUAL(rgs[0]->getGID(), (*itr)->getGID());
  CPPUNIT_ASSERT_EQUAL((size_t)3, rgman_->getRequestGroups().size());
}

void RequestGroupManTest::testInsertReservedGroup()
{
  std::vector<std::shared_ptr<RequestGroup>> rgs1{
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_))),
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_)))};
  std::vector<std::shared_ptr<RequestGroup>> rgs2{
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_))),
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_)))};
  rgman_->insertReservedGroup(0, rgs1);
  CPPUNIT_ASSERT_EQUAL((size_t)2, rgman_->getReservedGroups().size());
  RequestGroupList::const_iterator itr;
  itr = rgman_->getReservedGroups().begin();
  CPPUNIT_ASSERT_EQUAL(rgs1[0]->getGID(), (*itr++)->getGID());
  CPPUNIT_ASSERT_EQUAL(rgs1[1]->getGID(), (*itr++)->getGID());

  rgman_->insertReservedGroup(1, rgs2);
  CPPUNIT_ASSERT_EQUAL((size_t)4, rgman_->getReservedGroups().size());
  itr = rgman_->getReservedGroups().begin();
  ++itr;
  CPPUNIT_ASSERT_EQUAL(rgs2[0]->getGID(), (*itr++)->getGID());
  CPPUNIT_ASSERT_EQUAL(rgs2[1]->getGID(), (*itr++)->getGID());
}

void RequestGroupManTest::testAddDownloadResult()
{
  std::string uri = "http://example.org";
  rgman_->setMaxDownloadResult(3);
  rgman_->addDownloadResult(createDownloadResult(error_code::TIME_OUT, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  CPPUNIT_ASSERT_EQUAL(error_code::TIME_OUT,
                       rgman_->getDownloadStat().getLastErrorResult());
}

void RequestGroupManTest::testCountRequestGroup()
{
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 3,
                     option_.get());
  // countRequestGroup() counts only active (requestGroups_), not reserved
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.countRequestGroup());

  auto rg1 =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  rm.addRequestGroup(rg1);
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.countRequestGroup());

  auto rg2 =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  rm.addRequestGroup(rg2);
  CPPUNIT_ASSERT_EQUAL((size_t)2, rm.countRequestGroup());
}

void RequestGroupManTest::testDownloadFinished()
{
  // Use a separate option without ENABLE_RPC so keepRunning_ is false
  auto op = std::make_shared<Option>();
  op->put(PREF_PIECE_LENGTH, "1048576");
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 3, op.get());
  // No groups, no reserved, no keepRunning → finished
  CPPUNIT_ASSERT(rm.downloadFinished());

  // Add a reserved group → not finished
  auto rg = std::make_shared<RequestGroup>(GroupId::create(), util::copy(op));
  rm.addReservedGroup(rg);
  CPPUNIT_ASSERT(!rm.downloadFinished());
}

void RequestGroupManTest::testFindGroup()
{
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 3,
                     option_.get());
  auto rg1 =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  auto rg2 =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));

  rm.addRequestGroup(rg1);
  rm.addReservedGroup(rg2);

  // Find in active groups
  CPPUNIT_ASSERT(rm.findGroup(rg1->getGID()));
  CPPUNIT_ASSERT_EQUAL(rg1->getGID(), rm.findGroup(rg1->getGID())->getGID());

  // Find in reserved groups
  CPPUNIT_ASSERT(rm.findGroup(rg2->getGID()));
  CPPUNIT_ASSERT_EQUAL(rg2->getGID(), rm.findGroup(rg2->getGID())->getGID());

  // Not found
  CPPUNIT_ASSERT(!rm.findGroup(GroupId::create()->getNumericId()));
}

void RequestGroupManTest::testRemoveReservedGroup()
{
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 3,
                     option_.get());
  auto rg =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  rm.addReservedGroup(rg);
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.getReservedGroups().size());

  CPPUNIT_ASSERT(rm.removeReservedGroup(rg->getGID()));
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.getReservedGroups().size());

  // Remove non-existent returns false
  CPPUNIT_ASSERT(!rm.removeReservedGroup(GroupId::create()->getNumericId()));
}

void RequestGroupManTest::testFindDownloadResult()
{
  std::string uri = "http://example.org";
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 3,
                     option_.get());
  rm.setMaxDownloadResult(1000);
  auto dr = createDownloadResult(error_code::FINISHED, uri);
  a2_gid_t gid = dr->gid->getNumericId();
  rm.addDownloadResult(dr);

  // Found
  auto found = rm.findDownloadResult(gid);
  CPPUNIT_ASSERT(found);
  CPPUNIT_ASSERT_EQUAL(error_code::FINISHED, found->result);

  // Not found
  CPPUNIT_ASSERT(!rm.findDownloadResult(GroupId::create()->getNumericId()));
}

void RequestGroupManTest::testPurgeDownloadResult()
{
  std::string uri = "http://example.org";
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 3,
                     option_.get());
  rm.setMaxDownloadResult(1000);
  rm.addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  rm.addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  CPPUNIT_ASSERT_EQUAL((size_t)2, rm.getDownloadResults().size());

  rm.purgeDownloadResult();
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.getDownloadResults().size());
}

void RequestGroupManTest::testRemoveDownloadResult()
{
  std::string uri = "http://example.org";
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 3,
                     option_.get());
  rm.setMaxDownloadResult(1000);
  auto dr = createDownloadResult(error_code::FINISHED, uri);
  a2_gid_t gid = dr->gid->getNumericId();
  rm.addDownloadResult(dr);
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.getDownloadResults().size());

  // Remove existing → true
  CPPUNIT_ASSERT(rm.removeDownloadResult(gid));
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.getDownloadResults().size());

  // Remove non-existent → false
  CPPUNIT_ASSERT(!rm.removeDownloadResult(GroupId::create()->getNumericId()));
}

void RequestGroupManTest::testGetDownloadStat()
{
  std::string uri = "http://example.org";
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 3,
                     option_.get());

  // No results → allCompleted true
  {
    RequestGroupMan::DownloadStat stat = rm.getDownloadStat();
    CPPUNIT_ASSERT(stat.allCompleted());
    CPPUNIT_ASSERT_EQUAL(0, stat.getInProgress());
    CPPUNIT_ASSERT_EQUAL(error_code::FINISHED, stat.getLastErrorResult());
  }

  // Add an error result
  rm.addDownloadResult(createDownloadResult(error_code::TIME_OUT, uri));
  {
    RequestGroupMan::DownloadStat stat = rm.getDownloadStat();
    CPPUNIT_ASSERT(!stat.allCompleted());
    CPPUNIT_ASSERT_EQUAL(error_code::TIME_OUT, stat.getLastErrorResult());
  }
}

void RequestGroupManTest::testOverallSpeedLimits()
{
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 3,
                     option_.get());

  // No limit (0) → never exceeds
  CPPUNIT_ASSERT(!rm.doesOverallDownloadSpeedExceed());
  CPPUNIT_ASSERT(!rm.doesOverallUploadSpeedExceed());

  // Set limits — speed is 0, so should not exceed
  rm.setMaxOverallDownloadSpeedLimit(1000);
  CPPUNIT_ASSERT_EQUAL(1000, rm.getMaxOverallDownloadSpeedLimit());
  CPPUNIT_ASSERT(!rm.doesOverallDownloadSpeedExceed());

  rm.setMaxOverallUploadSpeedLimit(500);
  CPPUNIT_ASSERT_EQUAL(500, rm.getMaxOverallUploadSpeedLimit());
  CPPUNIT_ASSERT(!rm.doesOverallUploadSpeedExceed());
}

void RequestGroupManTest::testHalt()
{
  auto rg1 =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  auto rg2 =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  auto ctx1 = std::make_shared<DownloadContext>(0, 0, "foo1");
  auto ctx2 = std::make_shared<DownloadContext>(0, 0, "foo2");
  rg1->setDownloadContext(ctx1);
  rg2->setDownloadContext(ctx2);

  rgman_->addRequestGroup(rg1);
  rgman_->addRequestGroup(rg2);

  rgman_->halt();
  CPPUNIT_ASSERT(rg1->isHaltRequested());
  CPPUNIT_ASSERT(rg2->isHaltRequested());

  // forceHalt sets force flag
  rgman_->forceHalt();
  CPPUNIT_ASSERT(rg1->isForceHaltRequested());
  CPPUNIT_ASSERT(rg2->isForceHaltRequested());
}

void RequestGroupManTest::testQueueCheck()
{
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 3,
                     option_.get());

  // Default is true
  CPPUNIT_ASSERT(rm.queueCheckRequested());
  rm.clearQueueCheck();
  CPPUNIT_ASSERT(!rm.queueCheckRequested());
  rm.requestQueueCheck();
  CPPUNIT_ASSERT(rm.queueCheckRequested());
}

} // namespace aria2
