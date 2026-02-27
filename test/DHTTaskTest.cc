#include "DHTPingTask.h"
#include "DHTBucketRefreshTask.h"
#include "DHTNodeLookupTask.h"
#include "DHTNode.h"
#include "DHTBucket.h"
#include "DHTRoutingTable.h"
#include "DHTConstants.h"
#include "MockDHTMessage.h"
#include "MockDHTMessageDispatcher.h"
#include "MockDHTMessageFactory.h"
#include "MockDHTTaskQueue.h"
#include "a2functional.h"

#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

namespace {

// A mock factory that actually returns a MockDHTMessage for
// createPingMessage, so DHTPingTask::addMessage() works.
class PingTestFactory : public MockDHTMessageFactory {
public:
  virtual std::unique_ptr<DHTPingMessage>
  createPingMessage(const std::shared_ptr<DHTNode>& remoteNode,
                    const std::string& transactionID = "") override
  {
    // DHTPingMessage constructor needs localNode, remoteNode, transactionID
    return make_unique<DHTPingMessage>(localNode_, remoteNode, transactionID);
  }
};

} // namespace

class DHTPingTaskTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHTPingTaskTest);
  CPPUNIT_TEST(testStartup);
  CPPUNIT_TEST(testOnReceived);
  CPPUNIT_TEST(testOnTimeoutNoRetry);
  CPPUNIT_TEST(testOnTimeoutWithRetry);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<DHTNode> localNode_;
  std::shared_ptr<DHTNode> remoteNode_;
  MockDHTMessageDispatcher dispatcher_;
  PingTestFactory factory_;

public:
  void setUp()
  {
    localNode_ = std::make_shared<DHTNode>();
    remoteNode_ = std::make_shared<DHTNode>();
    factory_.setLocalNode(localNode_);
  }

  void testStartup()
  {
    DHTPingTask task(remoteNode_, 0);
    task.setMessageDispatcher(&dispatcher_);
    task.setMessageFactory(&factory_);
    task.setLocalNode(localNode_);

    CPPUNIT_ASSERT(!task.finished());
    CPPUNIT_ASSERT(!task.isPingSuccessful());

    task.startup();
    // startup() queues a ping message to the dispatcher
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1),
                         dispatcher_.messageQueue_.size());
  }

  void testOnReceived()
  {
    DHTPingTask task(remoteNode_, 0);
    task.setMessageDispatcher(&dispatcher_);
    task.setMessageFactory(&factory_);
    task.setLocalNode(localNode_);

    task.startup();
    CPPUNIT_ASSERT(!task.finished());

    // Simulate receiving a reply
    task.onReceived(nullptr);
    CPPUNIT_ASSERT(task.finished());
    CPPUNIT_ASSERT(task.isPingSuccessful());
  }

  void testOnTimeoutNoRetry()
  {
    // numMaxRetry = 0 means no retries
    DHTPingTask task(remoteNode_, 0);
    task.setMessageDispatcher(&dispatcher_);
    task.setMessageFactory(&factory_);
    task.setLocalNode(localNode_);

    task.startup();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1),
                         dispatcher_.messageQueue_.size());

    // Simulate timeout — should finish since numMaxRetry=0
    task.onTimeout(remoteNode_);
    CPPUNIT_ASSERT(task.finished());
    CPPUNIT_ASSERT(!task.isPingSuccessful());
  }

  void testOnTimeoutWithRetry()
  {
    // numMaxRetry = 2 means retry up to 2 times
    DHTPingTask task(remoteNode_, 2);
    task.setMessageDispatcher(&dispatcher_);
    task.setMessageFactory(&factory_);
    task.setLocalNode(localNode_);

    task.startup();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1),
                         dispatcher_.messageQueue_.size());

    // First timeout — should retry
    task.onTimeout(remoteNode_);
    CPPUNIT_ASSERT(!task.finished());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2),
                         dispatcher_.messageQueue_.size());

    // Second timeout — should finish (numRetry=2 >= numMaxRetry=2)
    task.onTimeout(remoteNode_);
    CPPUNIT_ASSERT(task.finished());
    CPPUNIT_ASSERT(!task.isPingSuccessful());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHTPingTaskTest);

class DHTBucketRefreshTaskTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHTBucketRefreshTaskTest);
  CPPUNIT_TEST(testStartupNoStale);
  CPPUNIT_TEST(testStartupForceRefresh);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<DHTNode> localNode_;
  MockDHTMessageDispatcher dispatcher_;
  MockDHTMessageFactory factory_;
  MockDHTTaskQueue taskQueue_;

public:
  void setUp()
  {
    localNode_ = std::make_shared<DHTNode>();
    factory_.setLocalNode(localNode_);
  }

  void testStartupNoStale()
  {
    DHTRoutingTable routingTable(localNode_);

    // Fill the root bucket with K=8 good nodes so needsRefresh()
    // returns false (nodes_.size() >= K and time elapsed is small).
    for (int i = 0; i < 8; ++i) {
      auto node = std::make_shared<DHTNode>();
      // Each DHTNode() gets a random ID, so all should be unique
      routingTable.addGoodNode(node);
    }

    DHTBucketRefreshTask task;
    task.setRoutingTable(&routingTable);
    task.setMessageDispatcher(&dispatcher_);
    task.setMessageFactory(&factory_);
    task.setTaskQueue(&taskQueue_);
    task.setLocalNode(localNode_);

    task.startup();
    // Bucket is full and fresh — needsRefresh() is false.
    // With forceRefresh=false, no node lookup tasks are created.
    CPPUNIT_ASSERT(task.finished());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
                         taskQueue_.periodicTaskQueue1_.size());
  }

  void testStartupForceRefresh()
  {
    DHTRoutingTable routingTable(localNode_);

    DHTBucketRefreshTask task;
    task.setForceRefresh(true);
    task.setRoutingTable(&routingTable);
    task.setMessageDispatcher(&dispatcher_);
    task.setMessageFactory(&factory_);
    task.setTaskQueue(&taskQueue_);
    task.setLocalNode(localNode_);

    task.startup();
    // With forceRefresh=true, a node lookup task should be created
    // for the root bucket.
    CPPUNIT_ASSERT(task.finished());
    CPPUNIT_ASSERT(taskQueue_.periodicTaskQueue1_.size() >= 1);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHTBucketRefreshTaskTest);

} // namespace aria2
