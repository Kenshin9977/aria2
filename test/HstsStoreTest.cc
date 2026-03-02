#include "HstsStore.h"

#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class HstsStoreTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(HstsStoreTest);
  CPPUNIT_TEST(testProcessHeader);
  CPPUNIT_TEST(testIncludeSubDomains);
  CPPUNIT_TEST(testRemove);
  CPPUNIT_TEST(testNoMaxAge);
  CPPUNIT_TEST(testShouldUpgradeNoMatch);
  CPPUNIT_TEST(testSubdomainWithoutInclude);
  CPPUNIT_TEST(testDeepSubdomain);
  CPPUNIT_TEST(testOverwrite);
  CPPUNIT_TEST_SUITE_END();

public:
  void testProcessHeader();
  void testIncludeSubDomains();
  void testRemove();
  void testNoMaxAge();
  void testShouldUpgradeNoMatch();
  void testSubdomainWithoutInclude();
  void testDeepSubdomain();
  void testOverwrite();
};

CPPUNIT_TEST_SUITE_REGISTRATION(HstsStoreTest);

void HstsStoreTest::testProcessHeader()
{
  HstsStore store;
  store.processHeader("example.com", "max-age=31536000");
  CPPUNIT_ASSERT(store.shouldUpgrade("example.com"));
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), store.size());
}

void HstsStoreTest::testIncludeSubDomains()
{
  HstsStore store;
  store.processHeader("example.com",
                      "max-age=31536000; includeSubDomains");
  CPPUNIT_ASSERT(store.shouldUpgrade("example.com"));
  CPPUNIT_ASSERT(store.shouldUpgrade("sub.example.com"));
}

void HstsStoreTest::testRemove()
{
  HstsStore store;
  store.processHeader("example.com", "max-age=31536000");
  CPPUNIT_ASSERT(store.shouldUpgrade("example.com"));
  store.processHeader("example.com", "max-age=0");
  CPPUNIT_ASSERT(!store.shouldUpgrade("example.com"));
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), store.size());
}

void HstsStoreTest::testNoMaxAge()
{
  HstsStore store;
  store.processHeader("example.com", "includeSubDomains");
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), store.size());
  CPPUNIT_ASSERT(!store.shouldUpgrade("example.com"));
}

void HstsStoreTest::testShouldUpgradeNoMatch()
{
  HstsStore store;
  store.processHeader("example.com", "max-age=31536000");
  CPPUNIT_ASSERT(!store.shouldUpgrade("other.com"));
}

void HstsStoreTest::testSubdomainWithoutInclude()
{
  HstsStore store;
  store.processHeader("example.com", "max-age=31536000");
  CPPUNIT_ASSERT(!store.shouldUpgrade("sub.example.com"));
}

void HstsStoreTest::testDeepSubdomain()
{
  HstsStore store;
  store.processHeader("example.com",
                      "max-age=31536000; includeSubDomains");
  CPPUNIT_ASSERT(store.shouldUpgrade("a.b.example.com"));
}

void HstsStoreTest::testOverwrite()
{
  HstsStore store;
  store.processHeader("example.com", "max-age=31536000");
  CPPUNIT_ASSERT(!store.shouldUpgrade("sub.example.com"));
  // Overwrite with includeSubDomains
  store.processHeader("example.com",
                      "max-age=31536000; includeSubDomains");
  CPPUNIT_ASSERT(store.shouldUpgrade("sub.example.com"));
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), store.size());
}

} // namespace aria2
