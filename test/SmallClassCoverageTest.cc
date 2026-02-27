#include "ContextAttribute.h"
#include "DefaultBtInteractive.h"
#include "Notifier.h"
#include "FatalException.h"
#include "TruncFileAllocationIterator.h"
#include "UnknownOptionException.h"
#include "ByteArrayDiskWriter.h"
#include "RequestGroup.h"

#include <cppunit/extensions/HelperMacros.h>

#include <string>
#include <vector>

namespace aria2 {

namespace {

class TestDownloadEventListener : public DownloadEventListener {
public:
  std::vector<DownloadEvent> events;
  void onEvent(DownloadEvent event, const RequestGroup* group) CXX11_OVERRIDE
  {
    events.push_back(event);
  }
};

} // namespace

class SmallClassCoverageTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SmallClassCoverageTest);
  CPPUNIT_TEST(testContextAttributeType);
  CPPUNIT_TEST(testNotifierSingleListener);
  CPPUNIT_TEST(testNotifierMultipleListeners);
  CPPUNIT_TEST(testFatalException);
  CPPUNIT_TEST(testFatalExceptionWithCause);
  CPPUNIT_TEST(testTruncFileAllocationIterator);
  CPPUNIT_TEST(testTruncFileAllocationIteratorAlreadyFinished);
  CPPUNIT_TEST(testUnknownOptionException);
  CPPUNIT_TEST(testFloodingStatBasic);
  CPPUNIT_TEST(testFloodingStatReset);
  CPPUNIT_TEST(testFloodingStatOverflow);
  CPPUNIT_TEST_SUITE_END();

public:
  void testContextAttributeType()
  {
    CPPUNIT_ASSERT_EQUAL(std::string("BitTorrent"),
                         std::string(strContextAttributeType(CTX_ATTR_BT)));
  }

  void testNotifierSingleListener()
  {
    Notifier notifier;
    TestDownloadEventListener listener;
    notifier.addDownloadEventListener(&listener);
    notifier.notifyDownloadEvent(EVENT_ON_DOWNLOAD_START, nullptr);
    notifier.notifyDownloadEvent(EVENT_ON_DOWNLOAD_COMPLETE, nullptr);
    CPPUNIT_ASSERT_EQUAL((size_t)2, listener.events.size());
    CPPUNIT_ASSERT_EQUAL((int)EVENT_ON_DOWNLOAD_START, (int)listener.events[0]);
    CPPUNIT_ASSERT_EQUAL((int)EVENT_ON_DOWNLOAD_COMPLETE,
                         (int)listener.events[1]);
  }

  void testNotifierMultipleListeners()
  {
    Notifier notifier;
    TestDownloadEventListener listener1;
    TestDownloadEventListener listener2;
    notifier.addDownloadEventListener(&listener1);
    notifier.addDownloadEventListener(&listener2);
    notifier.notifyDownloadEvent(EVENT_ON_DOWNLOAD_ERROR, nullptr);
    CPPUNIT_ASSERT_EQUAL((size_t)1, listener1.events.size());
    CPPUNIT_ASSERT_EQUAL((size_t)1, listener2.events.size());
  }

  void testFatalException()
  {
    FatalException ex(__FILE__, __LINE__, "test fatal error");
    std::string msg(ex.what());
    CPPUNIT_ASSERT(msg.find("test fatal error") != std::string::npos);
  }

  void testFatalExceptionWithCause()
  {
    FatalException cause(__FILE__, __LINE__, "root cause");
    FatalException ex(__FILE__, __LINE__, "wrapper", cause);
    std::string msg(ex.what());
    CPPUNIT_ASSERT(msg.find("wrapper") != std::string::npos);
  }

  void testTruncFileAllocationIterator()
  {
    ByteArrayDiskWriter writer;
    TruncFileAllocationIterator iter(&writer, 0, 1024);
    CPPUNIT_ASSERT(!iter.finished());
    CPPUNIT_ASSERT_EQUAL((int64_t)0, iter.getCurrentLength());
    CPPUNIT_ASSERT_EQUAL((int64_t)1024, iter.getTotalLength());
    iter.allocateChunk();
    CPPUNIT_ASSERT(iter.finished());
    CPPUNIT_ASSERT_EQUAL((int64_t)1024, iter.getCurrentLength());
  }

  void testTruncFileAllocationIteratorAlreadyFinished()
  {
    ByteArrayDiskWriter writer;
    // offset == totalLength means already finished
    TruncFileAllocationIterator iter(&writer, 512, 512);
    CPPUNIT_ASSERT(iter.finished());
  }

  void testUnknownOptionException()
  {
    UnknownOptionException ex(__FILE__, __LINE__, "--bad-option");
    CPPUNIT_ASSERT_EQUAL(std::string("--bad-option"), ex.getUnknownOption());
    std::string msg(ex.what());
    CPPUNIT_ASSERT(msg.find("--bad-option") != std::string::npos);
  }

  void testFloodingStatBasic()
  {
    FloodingStat stat;
    CPPUNIT_ASSERT_EQUAL(0, stat.getChokeUnchokeCount());
    CPPUNIT_ASSERT_EQUAL(0, stat.getKeepAliveCount());

    stat.incChokeUnchokeCount();
    stat.incChokeUnchokeCount();
    CPPUNIT_ASSERT_EQUAL(2, stat.getChokeUnchokeCount());

    stat.incKeepAliveCount();
    CPPUNIT_ASSERT_EQUAL(1, stat.getKeepAliveCount());
  }

  void testFloodingStatReset()
  {
    FloodingStat stat;
    stat.incChokeUnchokeCount();
    stat.incKeepAliveCount();
    stat.incKeepAliveCount();

    stat.reset();
    CPPUNIT_ASSERT_EQUAL(0, stat.getChokeUnchokeCount());
    CPPUNIT_ASSERT_EQUAL(0, stat.getKeepAliveCount());
  }

  void testFloodingStatOverflow()
  {
    // FloodingStat guards against INT_MAX overflow
    FloodingStat stat;
    // We can't iterate to INT_MAX in a test, but we can verify
    // the increment works and does not crash
    for (int i = 0; i < 100; ++i) {
      stat.incChokeUnchokeCount();
      stat.incKeepAliveCount();
    }
    CPPUNIT_ASSERT_EQUAL(100, stat.getChokeUnchokeCount());
    CPPUNIT_ASSERT_EQUAL(100, stat.getKeepAliveCount());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(SmallClassCoverageTest);

} // namespace aria2
