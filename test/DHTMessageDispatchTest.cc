#include "DHTMessageDispatcherImpl.h"
#include "DHTMessageTracker.h"
#include "DHTMessageCallback.h"
#include "DHTMessageTrackerEntry.h"
#include "DHTMessageEntry.h"
#include "DHTTaskQueueImpl.h"
#include "DHTNode.h"
#include "DHTConstants.h"
#include "MockDHTMessage.h"
#include "MockDHTTask.h"
#include "a2functional.h"

#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class DHTMessageDispatcherImplTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHTMessageDispatcherImplTest);
  CPPUNIT_TEST(testAddMessageToQueue);
  CPPUNIT_TEST(testAddMessageToQueueWithDefaultTimeout);
  CPPUNIT_TEST(testSendMessages);
  CPPUNIT_TEST(testSendReplyMessage);
  CPPUNIT_TEST(testCountMessageInQueue);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<DHTNode> localNode_;
  std::shared_ptr<DHTNode> remoteNode_;
  std::shared_ptr<DHTMessageTracker> tracker_;

public:
  void setUp()
  {
    localNode_ = std::make_shared<DHTNode>();
    remoteNode_ = std::make_shared<DHTNode>();
    tracker_ = std::make_shared<DHTMessageTracker>();
  }

  void testAddMessageToQueue()
  {
    DHTMessageDispatcherImpl dispatcher(tracker_);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
                         dispatcher.countMessageInQueue());

    auto msg = make_unique<MockDHTMessage>(localNode_, remoteNode_);
    dispatcher.addMessageToQueue(std::move(msg), std::chrono::seconds(10));

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1),
                         dispatcher.countMessageInQueue());
  }

  void testAddMessageToQueueWithDefaultTimeout()
  {
    DHTMessageDispatcherImpl dispatcher(tracker_);

    auto msg = make_unique<MockDHTMessage>(localNode_, remoteNode_);
    dispatcher.addMessageToQueue(std::move(msg));

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1),
                         dispatcher.countMessageInQueue());
  }

  void testSendMessages()
  {
    DHTMessageDispatcherImpl dispatcher(tracker_);

    // Add two non-reply messages
    auto msg1 = make_unique<MockDHTMessage>(localNode_, remoteNode_);
    auto msg2 = make_unique<MockDHTMessage>(localNode_, remoteNode_);
    dispatcher.addMessageToQueue(std::move(msg1), std::chrono::seconds(10));
    dispatcher.addMessageToQueue(std::move(msg2), std::chrono::seconds(10));
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2),
                         dispatcher.countMessageInQueue());

    dispatcher.sendMessages();

    // Queue should be empty after sending
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
                         dispatcher.countMessageInQueue());
    // Non-reply messages get tracked
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), tracker_->countEntry());
  }

  void testSendReplyMessage()
  {
    DHTMessageDispatcherImpl dispatcher(tracker_);

    // Add a reply message
    auto msg = make_unique<MockDHTMessage>(localNode_, remoteNode_);
    msg->setReply(true);
    dispatcher.addMessageToQueue(std::move(msg), std::chrono::seconds(10));

    dispatcher.sendMessages();

    // Queue should be empty
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
                         dispatcher.countMessageInQueue());
    // Reply messages are NOT tracked
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), tracker_->countEntry());
  }

  void testCountMessageInQueue()
  {
    DHTMessageDispatcherImpl dispatcher(tracker_);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
                         dispatcher.countMessageInQueue());

    for (int i = 0; i < 5; ++i) {
      auto msg = make_unique<MockDHTMessage>(localNode_, remoteNode_);
      dispatcher.addMessageToQueue(std::move(msg), std::chrono::seconds(10));
    }
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(5),
                         dispatcher.countMessageInQueue());

    dispatcher.sendMessages();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
                         dispatcher.countMessageInQueue());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHTMessageDispatcherImplTest);

namespace {

// A mock task that records whether startup() was called.
class TrackingMockDHTTask : public DHTTask {
public:
  bool startupCalled_;
  bool finished_;

  TrackingMockDHTTask() : startupCalled_(false), finished_(false) {}

  virtual void startup() CXX11_OVERRIDE { startupCalled_ = true; }

  virtual bool finished() CXX11_OVERRIDE { return finished_; }
};

} // namespace

class DHTTaskQueueImplTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHTTaskQueueImplTest);
  CPPUNIT_TEST(testAddPeriodicTask1);
  CPPUNIT_TEST(testAddImmediateTask);
  CPPUNIT_TEST(testExecuteTaskStartsQueued);
  CPPUNIT_TEST(testExecuteTaskRemovesFinished);
  CPPUNIT_TEST_SUITE_END();

public:
  void testAddPeriodicTask1()
  {
    DHTTaskQueueImpl taskQueue;
    auto task = std::make_shared<TrackingMockDHTTask>();
    // Should not throw
    taskQueue.addPeriodicTask1(task);
    CPPUNIT_ASSERT(!task->startupCalled_);
  }

  void testAddImmediateTask()
  {
    DHTTaskQueueImpl taskQueue;
    auto task = std::make_shared<TrackingMockDHTTask>();
    taskQueue.addImmediateTask(task);
    CPPUNIT_ASSERT(!task->startupCalled_);
  }

  void testExecuteTaskStartsQueued()
  {
    DHTTaskQueueImpl taskQueue;
    auto task1 = std::make_shared<TrackingMockDHTTask>();
    auto task2 = std::make_shared<TrackingMockDHTTask>();
    auto task3 = std::make_shared<TrackingMockDHTTask>();

    taskQueue.addPeriodicTask1(task1);
    taskQueue.addPeriodicTask2(task2);
    taskQueue.addImmediateTask(task3);

    // executeTask() calls update() on all three executors,
    // which starts queued tasks
    taskQueue.executeTask();

    CPPUNIT_ASSERT(task1->startupCalled_);
    CPPUNIT_ASSERT(task2->startupCalled_);
    CPPUNIT_ASSERT(task3->startupCalled_);
  }

  void testExecuteTaskRemovesFinished()
  {
    DHTTaskQueueImpl taskQueue;
    auto task = std::make_shared<TrackingMockDHTTask>();
    taskQueue.addPeriodicTask1(task);

    // First update: starts the task
    taskQueue.executeTask();
    CPPUNIT_ASSERT(task->startupCalled_);

    // Mark as finished, then update should remove it
    task->finished_ = true;
    taskQueue.executeTask();

    // Add a new task — if the old one was removed, this new one
    // should be started on the next update
    auto task2 = std::make_shared<TrackingMockDHTTask>();
    taskQueue.addPeriodicTask1(task2);
    taskQueue.executeTask();
    CPPUNIT_ASSERT(task2->startupCalled_);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHTTaskQueueImplTest);

} // namespace aria2
