#include "DNSCache.h"

#include <cppunit/extensions/HelperMacros.h>

#include <thread>

namespace aria2 {

class DNSCacheTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DNSCacheTest);
  CPPUNIT_TEST(testFind);
  CPPUNIT_TEST(testMarkBad);
  CPPUNIT_TEST(testPutBadAddr);
  CPPUNIT_TEST(testRemove);
  CPPUNIT_TEST(testTTLExpiry);
  CPPUNIT_TEST(testTTLRefreshedOnPut);
  CPPUNIT_TEST(testTTLDisabled);
  CPPUNIT_TEST_SUITE_END();

  DNSCache cache_;

public:
  void setUp()
  {
    cache_ = DNSCache();
    cache_.put("www", "192.168.0.1", 80);
    cache_.put("www", "::1", 80);
    cache_.put("ftp", "192.168.0.1", 21);
    cache_.put("proxy", "192.168.1.2", 8080);
  }

  void testFind();
  void testMarkBad();
  void testPutBadAddr();
  void testRemove();
  void testTTLExpiry();
  void testTTLRefreshedOnPut();
  void testTTLDisabled();
};

CPPUNIT_TEST_SUITE_REGISTRATION(DNSCacheTest);

void DNSCacheTest::testFind()
{
  CPPUNIT_ASSERT_EQUAL(std::string("192.168.0.1"), cache_.find("www", 80));
  CPPUNIT_ASSERT_EQUAL(std::string("192.168.0.1"), cache_.find("ftp", 21));
  CPPUNIT_ASSERT_EQUAL(std::string("192.168.1.2"), cache_.find("proxy", 8080));
  CPPUNIT_ASSERT_EQUAL(std::string(""), cache_.find("www", 8080));
  CPPUNIT_ASSERT_EQUAL(std::string(""), cache_.find("another", 80));
}

void DNSCacheTest::testMarkBad()
{
  cache_.markBad("www", "192.168.0.1", 80);
  CPPUNIT_ASSERT_EQUAL(std::string("::1"), cache_.find("www", 80));
}

void DNSCacheTest::testPutBadAddr()
{
  cache_.markBad("www", "192.168.0.1", 80);
  cache_.put("www", "192.168.0.1", 80);
  CPPUNIT_ASSERT_EQUAL(std::string("::1"), cache_.find("www", 80));
}

void DNSCacheTest::testRemove()
{
  cache_.remove("www", 80);
  CPPUNIT_ASSERT_EQUAL(std::string(""), cache_.find("www", 80));
}

void DNSCacheTest::testTTLExpiry()
{
  DNSCache c(std::chrono::seconds(1));
  c.put("host", "10.0.0.1", 80);
  CPPUNIT_ASSERT_EQUAL(std::string("10.0.0.1"), c.find("host", 80));
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  CPPUNIT_ASSERT_EQUAL(std::string(""), c.find("host", 80));
}

void DNSCacheTest::testTTLRefreshedOnPut()
{
  DNSCache c(std::chrono::seconds(1));
  c.put("host", "10.0.0.1", 80);
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  // Re-put resets the TTL
  c.put("host", "10.0.0.1", 80);
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  // Only 600ms since re-put, should still be alive
  CPPUNIT_ASSERT_EQUAL(std::string("10.0.0.1"), c.find("host", 80));
}

void DNSCacheTest::testTTLDisabled()
{
  DNSCache c(std::chrono::seconds(0));
  c.put("host", "10.0.0.1", 80);
  CPPUNIT_ASSERT_EQUAL(std::string("10.0.0.1"), c.find("host", 80));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // With TTL=0, entries never expire
  CPPUNIT_ASSERT_EQUAL(std::string("10.0.0.1"), c.find("host", 80));
}

} // namespace aria2
