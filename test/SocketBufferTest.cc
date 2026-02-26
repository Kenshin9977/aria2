#include "SocketBuffer.h"
#include "SocketRecvBuffer.h"

#include <cppunit/extensions/HelperMacros.h>

#include "SocketCore.h"
#include "a2functional.h"

namespace aria2 {

class SocketBufferTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SocketBufferTest);
  CPPUNIT_TEST(testPushStr);
  CPPUNIT_TEST(testPushBytes);
  CPPUNIT_TEST(testSendBufferIsEmpty);
  CPPUNIT_TEST(testGetBufferEntrySize);
  CPPUNIT_TEST(testMultiplePush);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void testPushStr()
  {
    // SocketBuffer needs a SocketCore but we only test queue operations
    // here (no send). Create a dummy unconnected socket.
    auto sock = std::make_shared<SocketCore>();
    SocketBuffer buf(sock);
    CPPUNIT_ASSERT(buf.sendBufferIsEmpty());
    buf.pushStr("hello");
    CPPUNIT_ASSERT(!buf.sendBufferIsEmpty());
  }

  void testPushBytes()
  {
    auto sock = std::make_shared<SocketCore>();
    SocketBuffer buf(sock);
    std::vector<unsigned char> data{0x01, 0x02, 0x03};
    buf.pushBytes(std::move(data));
    CPPUNIT_ASSERT(!buf.sendBufferIsEmpty());
    CPPUNIT_ASSERT_EQUAL((size_t)1, buf.getBufferEntrySize());
  }

  void testSendBufferIsEmpty()
  {
    auto sock = std::make_shared<SocketCore>();
    SocketBuffer buf(sock);
    CPPUNIT_ASSERT(buf.sendBufferIsEmpty());
    CPPUNIT_ASSERT_EQUAL((size_t)0, buf.getBufferEntrySize());
    buf.pushStr("test");
    CPPUNIT_ASSERT(!buf.sendBufferIsEmpty());
    CPPUNIT_ASSERT_EQUAL((size_t)1, buf.getBufferEntrySize());
  }

  void testGetBufferEntrySize()
  {
    auto sock = std::make_shared<SocketCore>();
    SocketBuffer buf(sock);
    buf.pushStr("aaa");
    buf.pushStr("bbb");
    buf.pushStr("ccc");
    CPPUNIT_ASSERT_EQUAL((size_t)3, buf.getBufferEntrySize());
  }

  void testMultiplePush()
  {
    auto sock = std::make_shared<SocketCore>();
    SocketBuffer buf(sock);
    buf.pushStr("first");
    std::vector<unsigned char> data{0x0A, 0x0B};
    buf.pushBytes(std::move(data));
    buf.pushStr("third");
    CPPUNIT_ASSERT_EQUAL((size_t)3, buf.getBufferEntrySize());
    CPPUNIT_ASSERT(!buf.sendBufferIsEmpty());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(SocketBufferTest);

class SocketRecvBufferTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SocketRecvBufferTest);
  CPPUNIT_TEST(testBufferEmpty);
  CPPUNIT_TEST(testTruncateBuffer_initial);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void testBufferEmpty()
  {
    auto sock = std::make_shared<SocketCore>();
    SocketRecvBuffer buf(sock);
    CPPUNIT_ASSERT(buf.bufferEmpty());
    CPPUNIT_ASSERT_EQUAL((size_t)0, buf.getBufferLength());
  }

  void testTruncateBuffer_initial()
  {
    auto sock = std::make_shared<SocketCore>();
    SocketRecvBuffer buf(sock);
    // Truncate on an empty buffer should be safe
    buf.truncateBuffer();
    CPPUNIT_ASSERT(buf.bufferEmpty());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(SocketRecvBufferTest);

} // namespace aria2
