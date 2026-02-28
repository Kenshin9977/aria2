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

  std::shared_ptr<SocketCore> sock_;
  std::unique_ptr<SocketBuffer> buf_;

public:
  void setUp()
  {
    sock_ = std::make_shared<SocketCore>();
    buf_ = std::make_unique<SocketBuffer>(sock_);
  }
  void tearDown() {}

  void testPushStr()
  {
    CPPUNIT_ASSERT(buf_->sendBufferIsEmpty());
    buf_->pushStr("hello");
    CPPUNIT_ASSERT(!buf_->sendBufferIsEmpty());
  }

  void testPushBytes()
  {
    std::vector<unsigned char> data{0x01, 0x02, 0x03};
    buf_->pushBytes(std::move(data));
    CPPUNIT_ASSERT(!buf_->sendBufferIsEmpty());
    CPPUNIT_ASSERT_EQUAL((size_t)1, buf_->getBufferEntrySize());
  }

  void testSendBufferIsEmpty()
  {
    CPPUNIT_ASSERT(buf_->sendBufferIsEmpty());
    CPPUNIT_ASSERT_EQUAL((size_t)0, buf_->getBufferEntrySize());
    buf_->pushStr("test");
    CPPUNIT_ASSERT(!buf_->sendBufferIsEmpty());
    CPPUNIT_ASSERT_EQUAL((size_t)1, buf_->getBufferEntrySize());
  }

  void testGetBufferEntrySize()
  {
    buf_->pushStr("aaa");
    buf_->pushStr("bbb");
    buf_->pushStr("ccc");
    CPPUNIT_ASSERT_EQUAL((size_t)3, buf_->getBufferEntrySize());
  }

  void testMultiplePush()
  {
    buf_->pushStr("first");
    std::vector<unsigned char> data{0x0A, 0x0B};
    buf_->pushBytes(std::move(data));
    buf_->pushStr("third");
    CPPUNIT_ASSERT_EQUAL((size_t)3, buf_->getBufferEntrySize());
    CPPUNIT_ASSERT(!buf_->sendBufferIsEmpty());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(SocketBufferTest);

class SocketRecvBufferTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SocketRecvBufferTest);
  CPPUNIT_TEST(testBufferEmpty);
  CPPUNIT_TEST(testTruncateBuffer_initial);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<SocketCore> sock_;
  std::unique_ptr<SocketRecvBuffer> buf_;

public:
  void setUp()
  {
    sock_ = std::make_shared<SocketCore>();
    buf_ = std::make_unique<SocketRecvBuffer>(sock_);
  }
  void tearDown() {}

  void testBufferEmpty()
  {
    CPPUNIT_ASSERT(buf_->bufferEmpty());
    CPPUNIT_ASSERT_EQUAL((size_t)0, buf_->getBufferLength());
  }

  void testTruncateBuffer_initial()
  {
    // Truncate on an empty buffer should be safe
    buf_->truncateBuffer();
    CPPUNIT_ASSERT(buf_->bufferEmpty());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(SocketRecvBufferTest);

} // namespace aria2
