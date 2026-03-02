#include "DownloadContext.h"

#include <cppunit/extensions/HelperMacros.h>

#include "ContextAttribute.h"
#include "FileEntry.h"
#include "Signature.h"
#include "array_fun.h"

namespace aria2 {

class DownloadContextTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DownloadContextTest);
  CPPUNIT_TEST(testFindFileEntryByOffset);
  CPPUNIT_TEST(testGetPieceHash);
  CPPUNIT_TEST(testGetNumPieces);
  CPPUNIT_TEST(testGetBasePath);
  CPPUNIT_TEST(testSetFileFilter);
  CPPUNIT_TEST(testKnowsTotalLength);
  CPPUNIT_TEST(testGetTotalLength);
  CPPUNIT_TEST(testGetFirstRequestedFileEntry);
  CPPUNIT_TEST(testCountRequestedFileEntry);
  CPPUNIT_TEST(testChecksumVerification);
  CPPUNIT_TEST(testPieceHashVerification);
  CPPUNIT_TEST(testSignature);
  CPPUNIT_TEST(testDigest);
  CPPUNIT_TEST(testAcceptMetalink);
  CPPUNIT_TEST(testSessionTime);
  CPPUNIT_TEST(testSetFilePathWithIndex);
  CPPUNIT_TEST_SUITE_END();

public:
  void testFindFileEntryByOffset();
  void testGetPieceHash();
  void testGetNumPieces();
  void testGetBasePath();
  void testSetFileFilter();
  void testKnowsTotalLength();
  void testGetTotalLength();
  void testGetFirstRequestedFileEntry();
  void testCountRequestedFileEntry();
  void testChecksumVerification();
  void testPieceHashVerification();
  void testSignature();
  void testDigest();
  void testAcceptMetalink();
  void testSessionTime();
  void testSetFilePathWithIndex();
};

CPPUNIT_TEST_SUITE_REGISTRATION(DownloadContextTest);

void DownloadContextTest::testFindFileEntryByOffset()
{
  DownloadContext ctx;

  CPPUNIT_ASSERT(!ctx.findFileEntryByOffset(0));

  const std::shared_ptr<FileEntry> fileEntries[] = {
      std::shared_ptr<FileEntry>(new FileEntry("file1", 1000, 0)),
      std::shared_ptr<FileEntry>(new FileEntry("file2", 0, 1000)),
      std::shared_ptr<FileEntry>(new FileEntry("file3", 0, 1000)),
      std::shared_ptr<FileEntry>(new FileEntry("file4", 2000, 1000)),
      std::shared_ptr<FileEntry>(new FileEntry("file5", 3000, 3000)),
      std::shared_ptr<FileEntry>(new FileEntry("file6", 0, 6000))};
  ctx.setFileEntries(std::begin(fileEntries), std::end(fileEntries));

  CPPUNIT_ASSERT_EQUAL(std::string("file1"),
                       ctx.findFileEntryByOffset(0)->getPath());
  CPPUNIT_ASSERT_EQUAL(std::string("file4"),
                       ctx.findFileEntryByOffset(1500)->getPath());
  CPPUNIT_ASSERT_EQUAL(std::string("file5"),
                       ctx.findFileEntryByOffset(5999)->getPath());
  CPPUNIT_ASSERT(!ctx.findFileEntryByOffset(6000));
}

void DownloadContextTest::testGetPieceHash()
{
  DownloadContext ctx;
  const std::string pieceHashes[] = {"hash1", "hash2", "shash3"};
  ctx.setPieceHashes("sha-1", &pieceHashes[0], &pieceHashes[3]);
  CPPUNIT_ASSERT_EQUAL(std::string("hash1"), ctx.getPieceHash(0));
  CPPUNIT_ASSERT_EQUAL(std::string(""), ctx.getPieceHash(3));
}

void DownloadContextTest::testGetNumPieces()
{
  DownloadContext ctx(345, 9889, "");
  CPPUNIT_ASSERT_EQUAL((size_t)29, ctx.getNumPieces());
}

void DownloadContextTest::testGetBasePath()
{
  DownloadContext ctx(0, 0, "");
  CPPUNIT_ASSERT_EQUAL(std::string(""), ctx.getBasePath());
  ctx.getFirstFileEntry()->setPath("aria2.tar.bz2");
  CPPUNIT_ASSERT_EQUAL(std::string("aria2.tar.bz2"), ctx.getBasePath());
}

void DownloadContextTest::testSetFileFilter()
{
  DownloadContext ctx;
  std::vector<std::shared_ptr<FileEntry>> files;
  for (int i = 0; i < 10; ++i) {
    files.push_back(std::shared_ptr<FileEntry>(new FileEntry("file", 1, i)));
  }
  ctx.setFileEntries(files.begin(), files.end());
  auto sgl = util::parseIntSegments("6-8,2-4");
  sgl.normalize();
  ctx.setFileFilter(std::move(sgl));
  const std::vector<std::shared_ptr<FileEntry>>& res = ctx.getFileEntries();
  CPPUNIT_ASSERT(!res[0]->isRequested());
  CPPUNIT_ASSERT(res[1]->isRequested());
  CPPUNIT_ASSERT(res[2]->isRequested());
  CPPUNIT_ASSERT(res[3]->isRequested());
  CPPUNIT_ASSERT(!res[4]->isRequested());
  CPPUNIT_ASSERT(res[5]->isRequested());
  CPPUNIT_ASSERT(res[6]->isRequested());
  CPPUNIT_ASSERT(res[7]->isRequested());
  CPPUNIT_ASSERT(!res[8]->isRequested());
  CPPUNIT_ASSERT(!res[9]->isRequested());
}

void DownloadContextTest::testKnowsTotalLength()
{
  DownloadContext ctx(1_k, 10_k, "file");
  // Default: knows total length
  CPPUNIT_ASSERT(ctx.knowsTotalLength());

  ctx.markTotalLengthIsUnknown();
  CPPUNIT_ASSERT(!ctx.knowsTotalLength());

  ctx.markTotalLengthIsKnown();
  CPPUNIT_ASSERT(ctx.knowsTotalLength());
}

void DownloadContextTest::testGetTotalLength()
{
  DownloadContext ctx(1_k, 10_k, "file");
  CPPUNIT_ASSERT_EQUAL((int64_t)10_k, ctx.getTotalLength());

  // 0-length context
  DownloadContext ctx2(0, 0, "empty");
  CPPUNIT_ASSERT_EQUAL((int64_t)0, ctx2.getTotalLength());
}

void DownloadContextTest::testGetFirstRequestedFileEntry()
{
  DownloadContext ctx;
  std::vector<std::shared_ptr<FileEntry>> files;
  files.push_back(std::make_shared<FileEntry>("file1", 100, 0));
  files.push_back(std::make_shared<FileEntry>("file2", 200, 100));
  files.push_back(std::make_shared<FileEntry>("file3", 300, 300));
  ctx.setFileEntries(files.begin(), files.end());

  // By default all files are requested
  CPPUNIT_ASSERT_EQUAL(std::string("file1"),
                       ctx.getFirstRequestedFileEntry()->getPath());

  // Filter to only file3
  auto sgl = util::parseIntSegments("3");
  sgl.normalize();
  ctx.setFileFilter(std::move(sgl));
  CPPUNIT_ASSERT_EQUAL(std::string("file3"),
                       ctx.getFirstRequestedFileEntry()->getPath());
}

void DownloadContextTest::testCountRequestedFileEntry()
{
  DownloadContext ctx;
  std::vector<std::shared_ptr<FileEntry>> files;
  for (int i = 0; i < 5; ++i) {
    files.push_back(std::make_shared<FileEntry>("file", 100, i * 100));
  }
  ctx.setFileEntries(files.begin(), files.end());

  // All requested by default
  CPPUNIT_ASSERT_EQUAL((size_t)5, ctx.countRequestedFileEntry());

  // Filter to 2
  auto sgl = util::parseIntSegments("1,3");
  sgl.normalize();
  ctx.setFileFilter(std::move(sgl));
  CPPUNIT_ASSERT_EQUAL((size_t)2, ctx.countRequestedFileEntry());
}

void DownloadContextTest::testChecksumVerification()
{
  DownloadContext ctx(1_k, 10_k, "file");
  // No checksum set → not available
  CPPUNIT_ASSERT(!ctx.isChecksumVerificationAvailable());
  CPPUNIT_ASSERT(!ctx.isChecksumVerificationNeeded());

  // Set digest → available and needed
  ctx.setDigest("sha-1", "abc123");
  CPPUNIT_ASSERT(ctx.isChecksumVerificationAvailable());
  CPPUNIT_ASSERT(ctx.isChecksumVerificationNeeded());

  // Mark verified → no longer needed
  ctx.setChecksumVerified(true);
  CPPUNIT_ASSERT(ctx.isChecksumVerificationAvailable());
  CPPUNIT_ASSERT(!ctx.isChecksumVerificationNeeded());
}

void DownloadContextTest::testPieceHashVerification()
{
  DownloadContext ctx(1_k, 3_k, "file");
  CPPUNIT_ASSERT(!ctx.isPieceHashVerificationAvailable());

  const std::string hashes[] = {"h1", "h2", "h3"};
  ctx.setPieceHashes("sha-1", &hashes[0], &hashes[3]);
  CPPUNIT_ASSERT(ctx.isPieceHashVerificationAvailable());
  CPPUNIT_ASSERT_EQUAL(std::string("sha-1"), ctx.getPieceHashType());
}

void DownloadContextTest::testSignature()
{
  DownloadContext ctx(1_k, 1_k, "file");
  CPPUNIT_ASSERT(!ctx.getSignature());

  auto sig = std::make_unique<Signature>();
  sig->setBody("signature-data");
  ctx.setSignature(std::move(sig));
  CPPUNIT_ASSERT(ctx.getSignature());
  CPPUNIT_ASSERT_EQUAL(std::string("signature-data"),
                       ctx.getSignature()->getBody());
}

void DownloadContextTest::testDigest()
{
  DownloadContext ctx(1_k, 1_k, "file");
  CPPUNIT_ASSERT_EQUAL(std::string(""), ctx.getDigest());
  CPPUNIT_ASSERT_EQUAL(std::string(""), ctx.getHashType());

  ctx.setDigest("sha-256", "abcdef1234567890");
  CPPUNIT_ASSERT_EQUAL(std::string("abcdef1234567890"), ctx.getDigest());
  CPPUNIT_ASSERT_EQUAL(std::string("sha-256"), ctx.getHashType());
}

void DownloadContextTest::testAcceptMetalink()
{
  DownloadContext ctx(1_k, 1_k, "file");
  // Default is true
  CPPUNIT_ASSERT(ctx.getAcceptMetalink());

  ctx.setAcceptMetalink(false);
  CPPUNIT_ASSERT(!ctx.getAcceptMetalink());

  ctx.setAcceptMetalink(true);
  CPPUNIT_ASSERT(ctx.getAcceptMetalink());
}

void DownloadContextTest::testSessionTime()
{
  DownloadContext ctx(1_k, 1_k, "file");
  ctx.resetDownloadStartTime();
  // Session time should be >= 0
  auto st = ctx.calculateSessionTime();
  CPPUNIT_ASSERT(st.count() >= 0);
}

void DownloadContextTest::testSetFilePathWithIndex()
{
  DownloadContext ctx;
  std::vector<std::shared_ptr<FileEntry>> files;
  files.push_back(std::make_shared<FileEntry>("orig1", 100, 0));
  files.push_back(std::make_shared<FileEntry>("orig2", 200, 100));
  ctx.setFileEntries(files.begin(), files.end());

  ctx.setFilePathWithIndex(1, "/new/path1");
  CPPUNIT_ASSERT_EQUAL(std::string("/new/path1"),
                       ctx.getFileEntries()[0]->getPath());

  ctx.setFilePathWithIndex(2, "/new/path2");
  CPPUNIT_ASSERT_EQUAL(std::string("/new/path2"),
                       ctx.getFileEntries()[1]->getPath());
}

} // namespace aria2
