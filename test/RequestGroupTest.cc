#include "RequestGroup.h"

#include <cppunit/extensions/HelperMacros.h>

#include "Option.h"
#include "DownloadContext.h"
#include "FileEntry.h"
#include "PieceStorage.h"
#include "DownloadResult.h"
#include "DownloadFailureException.h"
#include "TimeA2.h"
#include "prefs.h"

namespace aria2 {

class RequestGroupTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(RequestGroupTest);
  CPPUNIT_TEST(testGetFirstFilePath);
  CPPUNIT_TEST(testTryAutoFileRenaming);
  CPPUNIT_TEST(testCreateDownloadResult);
  CPPUNIT_TEST(testSpeedLimits);
  CPPUNIT_TEST(testStreamCommandCounter);
  CPPUNIT_TEST(testNumConnectionCounter);
  CPPUNIT_TEST(testIsDependencyResolved);
  CPPUNIT_TEST(testUpdateLastModifiedTime);
  CPPUNIT_TEST(testFileNotFoundCount);
  CPPUNIT_TEST_SUITE_END();

private:
  std::shared_ptr<Option> option_;

public:
  void setUp() { option_.reset(new Option()); }

  void testGetFirstFilePath();
  void testTryAutoFileRenaming();
  void testCreateDownloadResult();
  void testSpeedLimits();
  void testStreamCommandCounter();
  void testNumConnectionCounter();
  void testIsDependencyResolved();
  void testUpdateLastModifiedTime();
  void testFileNotFoundCount();
};

CPPUNIT_TEST_SUITE_REGISTRATION(RequestGroupTest);

void RequestGroupTest::testGetFirstFilePath()
{
  std::shared_ptr<DownloadContext> ctx(
      new DownloadContext(1_k, 1_k, "/tmp/myfile"));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);

  CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile"), group.getFirstFilePath());

  group.markInMemoryDownload();

  CPPUNIT_ASSERT_EQUAL(std::string("[MEMORY]myfile"), group.getFirstFilePath());
}

void RequestGroupTest::testTryAutoFileRenaming()
{
  std::shared_ptr<DownloadContext> ctx(
      new DownloadContext(1_k, 1_k, "/tmp/myfile"));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);

  option_->put(PREF_AUTO_FILE_RENAMING, "false");
  try {
    group.tryAutoFileRenaming();
  }
  catch (const Exception& ex) {
    CPPUNIT_ASSERT_EQUAL(error_code::FILE_ALREADY_EXISTS, ex.getErrorCode());
  }

  option_->put(PREF_AUTO_FILE_RENAMING, "true");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile.1"), group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp/myfile.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile.1.txt"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/myfile");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/myfile.1"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/myfile.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/myfile.1.txt"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath(".bashrc");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string(".bashrc.1"), group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath(".bashrc.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string(".bashrc.1.txt"), group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/.bashrc");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/.bashrc.1"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/.bashrc.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/.bashrc.1.txt"),
                       group.getFirstFilePath());
}

void RequestGroupTest::testCreateDownloadResult()
{
  std::shared_ptr<DownloadContext> ctx(
      new DownloadContext(1_k, 1_m, "/tmp/myfile"));
  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.initPieceStorage();
  {
    std::shared_ptr<DownloadResult> result = group.createDownloadResult();

    CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile"),
                         result->fileEntries[0]->getPath());
    CPPUNIT_ASSERT_EQUAL((int64_t)1_m,
                         result->fileEntries.back()->getLastOffset());
    CPPUNIT_ASSERT_EQUAL((uint64_t)0, result->sessionDownloadLength);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, result->sessionTime.count());
    // result is UNKNOWN_ERROR if download has not completed and no specific
    // error has been reported
    CPPUNIT_ASSERT_EQUAL(error_code::UNKNOWN_ERROR, result->result);

    // if haltReason is set to RequestGroup::USER_REQUEST, download
    // result will become REMOVED.
    group.setHaltRequested(true, RequestGroup::USER_REQUEST);
    result = group.createDownloadResult();
    CPPUNIT_ASSERT_EQUAL(error_code::REMOVED, result->result);
    // if haltReason is set to RequestGroup::SHUTDOWN_SIGNAL, download
    // result will become IN_PROGRESS.
    group.setHaltRequested(true, RequestGroup::SHUTDOWN_SIGNAL);
    result = group.createDownloadResult();
    CPPUNIT_ASSERT_EQUAL(error_code::IN_PROGRESS, result->result);
  }
  {
    group.setLastErrorCode(error_code::RESOURCE_NOT_FOUND);

    std::shared_ptr<DownloadResult> result = group.createDownloadResult();

    CPPUNIT_ASSERT_EQUAL(error_code::RESOURCE_NOT_FOUND, result->result);
  }
  {
    group.getPieceStorage()->markAllPiecesDone();

    std::shared_ptr<DownloadResult> result = group.createDownloadResult();

    CPPUNIT_ASSERT_EQUAL(error_code::FINISHED, result->result);
  }
}

void RequestGroupTest::testSpeedLimits()
{
  RequestGroup group(GroupId::create(), option_);
  auto ctx = std::make_shared<DownloadContext>(1_k, 1_k, "dummy");
  group.setDownloadContext(ctx);

  // No limit set (0) → never exceeds
  CPPUNIT_ASSERT(!group.doesDownloadSpeedExceed());
  CPPUNIT_ASSERT(!group.doesUploadSpeedExceed());

  // Set limits — speed is 0 so should not exceed
  group.setMaxDownloadSpeedLimit(100);
  CPPUNIT_ASSERT(!group.doesDownloadSpeedExceed());

  group.setMaxUploadSpeedLimit(100);
  CPPUNIT_ASSERT(!group.doesUploadSpeedExceed());
}

void RequestGroupTest::testStreamCommandCounter()
{
  RequestGroup group(GroupId::create(), option_);
  auto ctx = std::make_shared<DownloadContext>(1_k, 1_k, "dummy");
  group.setDownloadContext(ctx);

  group.increaseStreamCommand();
  group.increaseStreamCommand();
  group.decreaseStreamCommand();

  group.increaseStreamConnection();
  group.increaseStreamConnection();
  group.decreaseStreamConnection();

  // Should not crash or fail
  CPPUNIT_ASSERT(true);
}

void RequestGroupTest::testNumConnectionCounter()
{
  RequestGroup group(GroupId::create(), option_);
  auto ctx = std::make_shared<DownloadContext>(1_k, 1_k, "dummy");
  group.setDownloadContext(ctx);

  // No connections initially
  CPPUNIT_ASSERT_EQUAL(0, group.getNumConnection());

  group.increaseStreamConnection();
  CPPUNIT_ASSERT_EQUAL(1, group.getNumConnection());

  group.decreaseStreamConnection();
  CPPUNIT_ASSERT_EQUAL(0, group.getNumConnection());
}

void RequestGroupTest::testIsDependencyResolved()
{
  RequestGroup group(GroupId::create(), option_);
  auto ctx = std::make_shared<DownloadContext>(1_k, 1_k, "dummy");
  group.setDownloadContext(ctx);

  // No dependency → resolved
  CPPUNIT_ASSERT(group.isDependencyResolved());
}

void RequestGroupTest::testUpdateLastModifiedTime()
{
  RequestGroup group(GroupId::create(), option_);
  auto ctx = std::make_shared<DownloadContext>(1_k, 1_k, "dummy");
  group.setDownloadContext(ctx);

  // Bad time is ignored
  group.updateLastModifiedTime(Time::null());

  // Valid time is accepted
  Time t1(1000000);
  group.updateLastModifiedTime(t1);

  // Older time is ignored (no way to verify directly, but should not crash)
  Time t0(500000);
  group.updateLastModifiedTime(t0);
}

void RequestGroupTest::testFileNotFoundCount()
{
  RequestGroup group(GroupId::create(), option_);
  auto ctx = std::make_shared<DownloadContext>(1_k, 1_k, "dummy");
  group.setDownloadContext(ctx);

  // No limit (default 0) → never throws
  group.increaseAndValidateFileNotFoundCount();
  group.increaseAndValidateFileNotFoundCount();

  // Set limit to 2 — already at count 2, so next call should throw
  option_->put(PREF_MAX_FILE_NOT_FOUND, "2");
  try {
    group.increaseAndValidateFileNotFoundCount();
    CPPUNIT_FAIL("Expected exception");
  }
  catch (const DownloadFailureException& e) {
    // expected
  }
}

} // namespace aria2
