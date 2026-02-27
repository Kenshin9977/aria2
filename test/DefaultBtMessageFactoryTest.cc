#include "DefaultBtMessageFactory.h"

#include <cstring>
#include <span>

#include <iostream>

#include <cppunit/extensions/HelperMacros.h>

#include "Peer.h"
#include "bittorrent_helper.h"
#include "DownloadContext.h"
#include "MockExtensionMessageFactory.h"
#include "BtExtendedMessage.h"
#include "BtPortMessage.h"
#include "BtChokeMessage.h"
#include "BtUnchokeMessage.h"
#include "BtInterestedMessage.h"
#include "BtNotInterestedMessage.h"
#include "BtBitfieldMessage.h"
#include "BtKeepAliveMessage.h"
#include "BtHaveAllMessage.h"
#include "BtHaveNoneMessage.h"
#include "BtHaveMessage.h"
#include "BtCancelMessage.h"
#include "BtRequestMessage.h"
#include "BtPieceMessage.h"
#include "BtRejectMessage.h"
#include "BtAllowedFastMessage.h"
#include "BtHandshakeMessage.h"
#include "Exception.h"
#include "FileEntry.h"
#include "Piece.h"
#include "MockPieceStorage.h"

namespace aria2 {

class DefaultBtMessageFactoryTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DefaultBtMessageFactoryTest);
  CPPUNIT_TEST(testCreateBtMessage_BtExtendedMessage);
  CPPUNIT_TEST(testCreatePortMessage);
  CPPUNIT_TEST(testCreateSimpleMessages);
  CPPUNIT_TEST(testCreateParameterizedMessages);
  CPPUNIT_TEST(testCreateHandshakeMessage);
  CPPUNIT_TEST(testCreateBtMessage_KeepAlive);
  CPPUNIT_TEST(testCreateBtMessage_SimpleTypes);
  CPPUNIT_TEST_SUITE_END();

private:
  std::unique_ptr<DownloadContext> dctx_;
  std::shared_ptr<Peer> peer_;
  std::shared_ptr<MockExtensionMessageFactory> exmsgFactory_;
  std::unique_ptr<DefaultBtMessageFactory> factory_;

public:
  void setUp()
  {
    dctx_ = make_unique<DownloadContext>();

    peer_ = std::make_shared<Peer>("192.168.0.1", 6969);
    peer_->allocateSessionResource(1_k, 1_m);
    peer_->setExtendedMessagingEnabled(true);

    exmsgFactory_ = std::make_shared<MockExtensionMessageFactory>();

    factory_ = make_unique<DefaultBtMessageFactory>();
    factory_->setDownloadContext(dctx_.get());
    factory_->setPeer(peer_);
    factory_->setExtensionMessageFactory(exmsgFactory_.get());
  }

  void testCreateBtMessage_BtExtendedMessage();
  void testCreatePortMessage();
  void testCreateSimpleMessages();
  void testCreateParameterizedMessages();
  void testCreateHandshakeMessage();
  void testCreateBtMessage_KeepAlive();
  void testCreateBtMessage_SimpleTypes();
};

CPPUNIT_TEST_SUITE_REGISTRATION(DefaultBtMessageFactoryTest);

void DefaultBtMessageFactoryTest::testCreateBtMessage_BtExtendedMessage()
{
  // payload:{4:name3:foo}->11bytes
  std::string payload = "4:name3:foo";
  char msg[17]; // 6+11bytes
  bittorrent::createPeerMessageString((unsigned char*)msg, sizeof(msg), 13, 20);
  msg[5] = 1; // Set dummy extended message ID 1
  memcpy(msg + 6, payload.c_str(), payload.size());

  auto m = factory_->createBtMessage(
      {(const unsigned char*)msg + 4, sizeof(msg) - 4});
  CPPUNIT_ASSERT(BtExtendedMessage::ID == m->getId());
  try {
    // disable extended messaging
    peer_->setExtendedMessagingEnabled(false);
    factory_->createBtMessage({(const unsigned char*)msg + 4, sizeof(msg) - 4});
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
}

void DefaultBtMessageFactoryTest::testCreatePortMessage()
{
  {
    unsigned char data[7];
    bittorrent::createPeerMessageString(data, sizeof(data), 3, 9);
    bittorrent::setShortIntParam(&data[5], 6881);
    try {
      auto r = factory_->createBtMessage({&data[4], sizeof(data) - 4});
      CPPUNIT_ASSERT(BtPortMessage::ID == r->getId());
      auto m = static_cast<const BtPortMessage*>(r.get());
      CPPUNIT_ASSERT_EQUAL((uint16_t)6881, m->getPort());
    }
    catch (Exception& e) {
      CPPUNIT_FAIL(e.stackTrace());
    }
  }
  {
    auto m = factory_->createPortMessage(6881);
    CPPUNIT_ASSERT_EQUAL((uint16_t)6881, m->getPort());
  }
}

void DefaultBtMessageFactoryTest::testCreateSimpleMessages()
{
  // Test all simple message creation methods
  auto choke = factory_->createChokeMessage();
  CPPUNIT_ASSERT(choke);
  CPPUNIT_ASSERT(BtChokeMessage::ID == choke->getId());

  auto unchoke = factory_->createUnchokeMessage();
  CPPUNIT_ASSERT(unchoke);
  CPPUNIT_ASSERT(BtUnchokeMessage::ID == unchoke->getId());

  auto interested = factory_->createInterestedMessage();
  CPPUNIT_ASSERT(interested);
  CPPUNIT_ASSERT(BtInterestedMessage::ID == interested->getId());

  auto notInterested = factory_->createNotInterestedMessage();
  CPPUNIT_ASSERT(notInterested);
  CPPUNIT_ASSERT(BtNotInterestedMessage::ID == notInterested->getId());

  auto keepAlive = factory_->createKeepAliveMessage();
  CPPUNIT_ASSERT(keepAlive);

  auto haveAll = factory_->createHaveAllMessage();
  CPPUNIT_ASSERT(haveAll);
  CPPUNIT_ASSERT(BtHaveAllMessage::ID == haveAll->getId());

  auto haveNone = factory_->createHaveNoneMessage();
  CPPUNIT_ASSERT(haveNone);
  CPPUNIT_ASSERT(BtHaveNoneMessage::ID == haveNone->getId());
}

void DefaultBtMessageFactoryTest::testCreateParameterizedMessages()
{
  // Have message
  auto have = factory_->createHaveMessage(5);
  CPPUNIT_ASSERT(have);
  CPPUNIT_ASSERT(BtHaveMessage::ID == have->getId());
  CPPUNIT_ASSERT_EQUAL((size_t)5, have->getIndex());

  // Cancel message
  auto cancel = factory_->createCancelMessage(1, 0, 16_k);
  CPPUNIT_ASSERT(cancel);
  CPPUNIT_ASSERT(BtCancelMessage::ID == cancel->getId());

  // Reject message
  auto reject = factory_->createRejectMessage(2, 0, 16_k);
  CPPUNIT_ASSERT(reject);
  CPPUNIT_ASSERT(BtRejectMessage::ID == reject->getId());

  // AllowedFast message
  auto fast = factory_->createAllowedFastMessage(10);
  CPPUNIT_ASSERT(fast);
  CPPUNIT_ASSERT(BtAllowedFastMessage::ID == fast->getId());

  // Port message
  auto port = factory_->createPortMessage(6881);
  CPPUNIT_ASSERT(port);
  CPPUNIT_ASSERT(BtPortMessage::ID == port->getId());
  CPPUNIT_ASSERT_EQUAL((uint16_t)6881, port->getPort());
}

void DefaultBtMessageFactoryTest::testCreateHandshakeMessage()
{
  unsigned char infoHash[20];
  memset(infoHash, 'A', sizeof(infoHash));
  unsigned char peerId[20];
  memset(peerId, 'B', sizeof(peerId));

  auto msg = factory_->createHandshakeMessage(infoHash, peerId);
  CPPUNIT_ASSERT(msg);
  CPPUNIT_ASSERT(BtHandshakeMessage::ID == msg->getId());
}

void DefaultBtMessageFactoryTest::testCreateBtMessage_KeepAlive()
{
  // Keep alive: length=0 (just the 4-byte length prefix of 0)
  unsigned char data[4] = {0, 0, 0, 0};
  auto msg = factory_->createBtMessage({data, 0});
  CPPUNIT_ASSERT(msg);
}

void DefaultBtMessageFactoryTest::testCreateBtMessage_SimpleTypes()
{
  // Choke: id=0
  {
    unsigned char data[5];
    bittorrent::createPeerMessageString(data, sizeof(data), 1, 0);
    auto msg = factory_->createBtMessage({&data[4], 1});
    CPPUNIT_ASSERT(msg);
    CPPUNIT_ASSERT(BtChokeMessage::ID == msg->getId());
  }
  // Unchoke: id=1
  {
    unsigned char data[5];
    bittorrent::createPeerMessageString(data, sizeof(data), 1, 1);
    auto msg = factory_->createBtMessage({&data[4], 1});
    CPPUNIT_ASSERT(msg);
    CPPUNIT_ASSERT(BtUnchokeMessage::ID == msg->getId());
  }
  // Interested: id=2
  {
    unsigned char data[5];
    bittorrent::createPeerMessageString(data, sizeof(data), 1, 2);
    auto msg = factory_->createBtMessage({&data[4], 1});
    CPPUNIT_ASSERT(msg);
    CPPUNIT_ASSERT(BtInterestedMessage::ID == msg->getId());
  }
  // Not Interested: id=3
  {
    unsigned char data[5];
    bittorrent::createPeerMessageString(data, sizeof(data), 1, 3);
    auto msg = factory_->createBtMessage({&data[4], 1});
    CPPUNIT_ASSERT(msg);
    CPPUNIT_ASSERT(BtNotInterestedMessage::ID == msg->getId());
  }
  // Have All: id=14
  {
    unsigned char data[5];
    bittorrent::createPeerMessageString(data, sizeof(data), 1, 14);
    auto msg = factory_->createBtMessage({&data[4], 1});
    CPPUNIT_ASSERT(msg);
    CPPUNIT_ASSERT(BtHaveAllMessage::ID == msg->getId());
  }
  // Have None: id=15
  {
    unsigned char data[5];
    bittorrent::createPeerMessageString(data, sizeof(data), 1, 15);
    auto msg = factory_->createBtMessage({&data[4], 1});
    CPPUNIT_ASSERT(msg);
    CPPUNIT_ASSERT(BtHaveNoneMessage::ID == msg->getId());
  }
}

} // namespace aria2
