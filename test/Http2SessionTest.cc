#include "Http2Session.h"

#include <cppunit/extensions/HelperMacros.h>

#include <memory>

#include "MockSocketCore.h"

namespace aria2 {

class Http2SessionTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(Http2SessionTest);
  CPPUNIT_TEST(testInit);
  CPPUNIT_TEST(testSubmitRequest);
  CPPUNIT_TEST(testGetStreamDataUnknown);
  CPPUNIT_TEST_SUITE_END();

public:
  void testInit();
  void testSubmitRequest();
  void testGetStreamDataUnknown();
};

CPPUNIT_TEST_SUITE_REGISTRATION(Http2SessionTest);

void Http2SessionTest::testInit()
{
  auto sock = std::make_shared<MockSocketCore>();
  sock->writable = true;
  Http2Session session(sock);
  CPPUNIT_ASSERT_EQUAL(0, session.init());
  // After init, connection preface is queued
  CPPUNIT_ASSERT(session.wantWrite());
}

void Http2SessionTest::testSubmitRequest()
{
  auto sock = std::make_shared<MockSocketCore>();
  sock->writable = true;
  Http2Session session(sock);
  session.init();
  int32_t streamId = session.submitRequest(
      "GET", "https", "example.com", "/",
      std::vector<std::pair<std::string, std::string>>());
  CPPUNIT_ASSERT(streamId > 0);
}

void Http2SessionTest::testGetStreamDataUnknown()
{
  auto sock = std::make_shared<MockSocketCore>();
  Http2Session session(sock);
  session.init();
  CPPUNIT_ASSERT(!session.getStreamData(999));
}

} // namespace aria2
