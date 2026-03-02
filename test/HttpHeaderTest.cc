#include "HttpHeader.h"

#include <cppunit/extensions/HelperMacros.h>

#include "Range.h"
#include "DlAbortEx.h"

namespace aria2 {

class HttpHeaderTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(HttpHeaderTest);
  CPPUNIT_TEST(testGetRange);
  CPPUNIT_TEST(testFindAll);
  CPPUNIT_TEST(testClearField);
  CPPUNIT_TEST(testFieldContains);
  CPPUNIT_TEST(testRemove);
  CPPUNIT_TEST(testFindEmptyButPresent);
  CPPUNIT_TEST(testGetRangeEmptyHeaders);
  CPPUNIT_TEST(testIsKeepAliveEmptyConnection);
  CPPUNIT_TEST_SUITE_END();

public:
  void testGetRange();
  void testFindAll();
  void testClearField();
  void testFieldContains();
  void testRemove();
  void testFindEmptyButPresent();
  void testGetRangeEmptyHeaders();
  void testIsKeepAliveEmptyConnection();
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpHeaderTest);

void HttpHeaderTest::testGetRange()
{
  {
    HttpHeader httpHeader;
    httpHeader.put(
        HttpHeader::CONTENT_RANGE,
        "9223372036854775800-9223372036854775801/9223372036854775807");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775800LL, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775801LL, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775807LL, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(
        HttpHeader::CONTENT_RANGE,
        "9223372036854775800-9223372036854775801/9223372036854775807");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775800LL, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775801LL, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775807LL, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes */1024");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes 0-9/*");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes */*");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes 0");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes 0/");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
  {
    // Support for non-compliant server
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes=0-1023/1024");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)1023, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)1024, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes 0-/3");
    try {
      httpHeader.getRange();
      CPPUNIT_FAIL("Exception must be thrown");
    }
    catch (const DlAbortEx& e) {
      // success
    }
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes -0/3");
    try {
      httpHeader.getRange();
      CPPUNIT_FAIL("Exception must be thrown");
    }
    catch (const DlAbortEx& e) {
      // success
    }
  }
}

void HttpHeaderTest::testFindAll()
{
  HttpHeader h;
  h.put(HttpHeader::LINK, "100");
  h.put(HttpHeader::LINK, "101");
  h.put(HttpHeader::CONNECTION, "200");

  std::vector<std::string> r(h.findAll(HttpHeader::LINK));
  CPPUNIT_ASSERT_EQUAL((size_t)2, r.size());
  CPPUNIT_ASSERT_EQUAL(std::string("100"), r[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("101"), r[1]);
}

void HttpHeaderTest::testClearField()
{
  HttpHeader h;
  h.setStatusCode(200);
  h.setVersion("HTTP/1.1");
  h.put(HttpHeader::LINK, "Bar");

  CPPUNIT_ASSERT(h.find(HttpHeader::LINK).has_value());
  CPPUNIT_ASSERT_EQUAL(std::string("Bar"),
                       std::string(*h.find(HttpHeader::LINK)));

  h.clearField();

  CPPUNIT_ASSERT(!h.find(HttpHeader::LINK).has_value());
  CPPUNIT_ASSERT_EQUAL(200, h.getStatusCode());
  CPPUNIT_ASSERT_EQUAL(std::string("HTTP/1.1"), h.getVersion());
}

void HttpHeaderTest::testFieldContains()
{
  HttpHeader h;
  h.put(HttpHeader::CONNECTION, "Keep-Alive, Upgrade");
  h.put(HttpHeader::UPGRADE, "WebSocket");
  h.put(HttpHeader::SEC_WEBSOCKET_VERSION, "13");
  h.put(HttpHeader::SEC_WEBSOCKET_VERSION, "8, 7");
  CPPUNIT_ASSERT(h.fieldContains(HttpHeader::CONNECTION, "upgrade"));
  CPPUNIT_ASSERT(h.fieldContains(HttpHeader::CONNECTION, "keep-alive"));
  CPPUNIT_ASSERT(!h.fieldContains(HttpHeader::CONNECTION, "close"));
  CPPUNIT_ASSERT(h.fieldContains(HttpHeader::UPGRADE, "websocket"));
  CPPUNIT_ASSERT(!h.fieldContains(HttpHeader::UPGRADE, "spdy"));
  CPPUNIT_ASSERT(h.fieldContains(HttpHeader::SEC_WEBSOCKET_VERSION, "13"));
  CPPUNIT_ASSERT(h.fieldContains(HttpHeader::SEC_WEBSOCKET_VERSION, "8"));
  CPPUNIT_ASSERT(!h.fieldContains(HttpHeader::SEC_WEBSOCKET_VERSION, "6"));
}

void HttpHeaderTest::testRemove()
{
  HttpHeader h;
  h.put(HttpHeader::CONNECTION, "close");
  h.put(HttpHeader::TRANSFER_ENCODING, "chunked");
  h.put(HttpHeader::TRANSFER_ENCODING, "gzip");

  h.remove(HttpHeader::TRANSFER_ENCODING);

  CPPUNIT_ASSERT(!h.defined(HttpHeader::TRANSFER_ENCODING));
  CPPUNIT_ASSERT(h.defined(HttpHeader::CONNECTION));
}

void HttpHeaderTest::testFindEmptyButPresent()
{
  HttpHeader h;
  h.put(HttpHeader::CONTENT_LENGTH, "");

  // Empty-but-present header: find() returns optional with empty view
  auto result = h.find(HttpHeader::CONTENT_LENGTH);
  CPPUNIT_ASSERT(result.has_value());
  CPPUNIT_ASSERT(result->empty());

  // Absent header: find() returns nullopt
  auto absent = h.find(HttpHeader::CONTENT_RANGE);
  CPPUNIT_ASSERT(!absent.has_value());

  // defined() returns true for empty-but-present
  CPPUNIT_ASSERT(h.defined(HttpHeader::CONTENT_LENGTH));
}

void HttpHeaderTest::testGetRangeEmptyHeaders()
{
  // Empty Content-Range should return empty Range (not throw)
  {
    HttpHeader h;
    h.put(HttpHeader::CONTENT_RANGE, "");
    Range range = h.getRange();
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
  // Empty Content-Length should return empty Range (not throw)
  {
    HttpHeader h;
    h.put(HttpHeader::CONTENT_LENGTH, "");
    Range range = h.getRange();
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
}

void HttpHeaderTest::testIsKeepAliveEmptyConnection()
{
  // Empty Connection header with HTTP/1.1 should behave like absent
  {
    HttpHeader h;
    h.setVersion("HTTP/1.1");
    h.put(HttpHeader::CONNECTION, "");
    // Empty Connection: old code returned empty string from find(),
    // strieq("", "close") is false, and version is 1.1, so keep-alive
    CPPUNIT_ASSERT(h.isKeepAlive());
  }
  // Empty Connection with HTTP/1.0: strieq("", "keep-alive") is false
  {
    HttpHeader h;
    h.setVersion("HTTP/1.0");
    h.put(HttpHeader::CONNECTION, "");
    CPPUNIT_ASSERT(!h.isKeepAlive());
  }
}

} // namespace aria2
