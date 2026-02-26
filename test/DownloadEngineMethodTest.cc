#include "DownloadEngine.h"

#include <cppunit/extensions/HelperMacros.h>

#ifndef __MINGW32__
#  include <sys/socket.h>
#  include <unistd.h>
#endif

#include "TestEngineHelper.h"
#include "SocketCore.h"

namespace aria2 {

class DownloadEngineMethodTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DownloadEngineMethodTest);
  CPPUNIT_TEST(testCacheIPAddress);
  CPPUNIT_TEST(testCacheIPAddress_overwrite);
  CPPUNIT_TEST(testMarkBadIPAddress);
  CPPUNIT_TEST(testRemoveCachedIPAddress);
  CPPUNIT_TEST(testFindAllCachedIPAddresses);
#ifndef __MINGW32__
  CPPUNIT_TEST(testSocketPool_basic);
  CPPUNIT_TEST(testSocketPool_withAuth);
  CPPUNIT_TEST(testSocketPool_multipleAddrs);
#endif
  CPPUNIT_TEST(testSessionId);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;

public:
  void setUp()
  {
    e_ = createTestEngine(option_, "aria2_DownloadEngineMethodTest");
  }

  void tearDown() {}

  void testCacheIPAddress()
  {
    e_->cacheIPAddress("example.com", "1.2.3.4", 80);
    CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"),
                         e_->findCachedIPAddress("example.com", 80));
    // Different port = different cache entry
    CPPUNIT_ASSERT_EQUAL(std::string(""),
                         e_->findCachedIPAddress("example.com", 443));
    // Different host = not found
    CPPUNIT_ASSERT_EQUAL(std::string(""),
                         e_->findCachedIPAddress("other.com", 80));
  }

  void testCacheIPAddress_overwrite()
  {
    e_->cacheIPAddress("example.com", "1.2.3.4", 80);
    e_->cacheIPAddress("example.com", "5.6.7.8", 80);
    // Both cached; findCachedIPAddress returns the first good one
    CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"),
                         e_->findCachedIPAddress("example.com", 80));
  }

  void testMarkBadIPAddress()
  {
    e_->cacheIPAddress("example.com", "1.2.3.4", 80);
    e_->cacheIPAddress("example.com", "5.6.7.8", 80);
    // Mark the first one as bad
    e_->markBadIPAddress("example.com", "1.2.3.4", 80);
    // Should now find the second one
    std::string found = e_->findCachedIPAddress("example.com", 80);
    CPPUNIT_ASSERT_EQUAL(std::string("5.6.7.8"), found);
  }

  void testRemoveCachedIPAddress()
  {
    e_->cacheIPAddress("example.com", "1.2.3.4", 80);
    CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"),
                         e_->findCachedIPAddress("example.com", 80));
    e_->removeCachedIPAddress("example.com", 80);
    CPPUNIT_ASSERT_EQUAL(std::string(""),
                         e_->findCachedIPAddress("example.com", 80));
  }

  void testFindAllCachedIPAddresses()
  {
    e_->cacheIPAddress("example.com", "1.2.3.4", 80);
    e_->cacheIPAddress("example.com", "5.6.7.8", 80);
    std::vector<std::string> addrs;
    e_->findAllCachedIPAddresses(std::back_inserter(addrs), "example.com", 80);
    CPPUNIT_ASSERT_EQUAL((size_t)2, addrs.size());
  }

#ifndef __MINGW32__
  // RAII wrapper for file descriptors to prevent leaks on assertion failure.
  struct FdGuard {
    int fd;
    FdGuard(int fd) : fd(fd) {}
    ~FdGuard()
    {
      if (fd >= 0) {
        close(fd);
      }
    }
  };

  void testSocketPool_basic()
  {
    int fds[2];
    CPPUNIT_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    FdGuard guard(fds[1]);
    auto sock = std::make_shared<SocketCore>(fds[0]);

    e_->poolSocket("127.0.0.1", 80, "", 0, sock);

    auto retrieved = e_->popPooledSocket("127.0.0.1", 80, "", 0);
    CPPUNIT_ASSERT(retrieved);

    // Pool is now empty
    auto empty = e_->popPooledSocket("127.0.0.1", 80, "", 0);
    CPPUNIT_ASSERT(!empty);
  }

  void testSocketPool_withAuth()
  {
    int fds[2];
    CPPUNIT_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    FdGuard guard(fds[1]);
    auto sock = std::make_shared<SocketCore>(fds[0]);

    std::string options;
    e_->poolSocket("127.0.0.1", 80, "user", "", 0, sock, "");

    auto retrieved =
        e_->popPooledSocket(options, "127.0.0.1", 80, "user", "", 0);
    CPPUNIT_ASSERT(retrieved);
    CPPUNIT_ASSERT_EQUAL(std::string(""), options);
  }

  void testSocketPool_multipleAddrs()
  {
    int fds1[2], fds2[2];
    CPPUNIT_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds1));
    CPPUNIT_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds2));
    FdGuard guard1(fds1[1]);
    FdGuard guard2(fds2[1]);

    auto sock1 = std::make_shared<SocketCore>(fds1[0]);
    auto sock2 = std::make_shared<SocketCore>(fds2[0]);

    e_->poolSocket("10.0.0.1", 80, "", 0, sock1);
    e_->poolSocket("10.0.0.2", 80, "", 0, sock2);

    std::vector<std::string> addrs{"10.0.0.1", "10.0.0.2", "10.0.0.3"};
    auto retrieved = e_->popPooledSocket(addrs, 80);
    CPPUNIT_ASSERT(retrieved);
  }
#endif // !__MINGW32__

  void testSessionId()
  {
    std::string sid = e_->getSessionId();
    CPPUNIT_ASSERT(!sid.empty());
    // Session ID should be consistent within same engine
    CPPUNIT_ASSERT_EQUAL(sid, e_->getSessionId());
    // Different engine should have different session ID
    std::shared_ptr<Option> opt2;
    auto e2 = createTestEngine(opt2, "aria2_DownloadEngineMethodTest_sid");
    CPPUNIT_ASSERT(sid != e2->getSessionId());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DownloadEngineMethodTest);

} // namespace aria2
