#include "Peer.h"

#include <cstring>

#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class PeerTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(PeerTest);
  CPPUNIT_TEST(testPeerAllowedIndexSet);
  CPPUNIT_TEST(testAmAllowedIndexSet);
  CPPUNIT_TEST(testCountSeeder);
  CPPUNIT_TEST(testConstruction);
  CPPUNIT_TEST(testUsedBy);
  CPPUNIT_TEST(testSessionResource);
  CPPUNIT_TEST(testPeerId);
  CPPUNIT_TEST(testChokingState);
  CPPUNIT_TEST(testInterestedState);
  CPPUNIT_TEST(testSnubbingAndOptUnchoking);
  CPPUNIT_TEST(testBitfieldOps);
  CPPUNIT_TEST(testExtensionFlags);
  CPPUNIT_TEST(testExtensionMessageID);
  CPPUNIT_TEST(testUploadDownloadTracking);
  CPPUNIT_TEST(testCompletedLength);
  CPPUNIT_TEST(testIncomingLocalDisconnected);
  CPPUNIT_TEST(testReconfigureSessionResource);
  CPPUNIT_TEST_SUITE_END();

private:
  std::shared_ptr<Peer> peer;

public:
  void setUp()
  {
    peer.reset(new Peer("localhost", 6969));
    peer->allocateSessionResource(1_k, 1_m);
  }

  void testPeerAllowedIndexSet();
  void testAmAllowedIndexSet();
  void testCountSeeder();
  void testConstruction();
  void testUsedBy();
  void testSessionResource();
  void testPeerId();
  void testChokingState();
  void testInterestedState();
  void testSnubbingAndOptUnchoking();
  void testBitfieldOps();
  void testExtensionFlags();
  void testExtensionMessageID();
  void testUploadDownloadTracking();
  void testCompletedLength();
  void testIncomingLocalDisconnected();
  void testReconfigureSessionResource();
};

CPPUNIT_TEST_SUITE_REGISTRATION(PeerTest);

void PeerTest::testPeerAllowedIndexSet()
{
  CPPUNIT_ASSERT(!peer->isInPeerAllowedIndexSet(0));
  peer->addPeerAllowedIndex(0);
  CPPUNIT_ASSERT(peer->isInPeerAllowedIndexSet(0));
}

void PeerTest::testAmAllowedIndexSet()
{
  CPPUNIT_ASSERT(!peer->isInAmAllowedIndexSet(0));
  peer->addAmAllowedIndex(0);
  CPPUNIT_ASSERT(peer->isInAmAllowedIndexSet(0));
}

void PeerTest::testCountSeeder()
{
  std::vector<std::shared_ptr<Peer>> peers(5);
  peers[0].reset(new Peer("192.168.0.1", 7000));
  peers[1].reset(new Peer("192.168.0.2", 7000));
  peers[2].reset(new Peer("192.168.0.3", 7000));
  peers[3].reset(new Peer("192.168.0.4", 7000));
  peers[4].reset(new Peer("192.168.0.5", 7000));
  for (std::vector<std::shared_ptr<Peer>>::iterator i = peers.begin();
       i != peers.end(); ++i) {
    (*i)->allocateSessionResource(1_k, 8_k);
  }
  unsigned char bitfield[] = {0xff};
  peers[1]->setBitfield({bitfield, 1});
  peers[3]->setBitfield({bitfield, 1});
  peers[4]->setBitfield({bitfield, 1});
  CPPUNIT_ASSERT_EQUAL((size_t)3, countSeeder(peers.begin(), peers.end()));
}

void PeerTest::testConstruction()
{
  Peer p1("192.168.0.1", 6881);
  CPPUNIT_ASSERT_EQUAL(std::string("192.168.0.1"), p1.getIPAddress());
  CPPUNIT_ASSERT_EQUAL((uint16_t)6881, p1.getPort());
  CPPUNIT_ASSERT_EQUAL((uint16_t)6881, p1.getOrigPort());
  CPPUNIT_ASSERT(!p1.isIncomingPeer());

  Peer p2("10.0.0.1", 7000, true);
  CPPUNIT_ASSERT(p2.isIncomingPeer());

  // setPort changes port but not origPort
  p1.setPort(8080);
  CPPUNIT_ASSERT_EQUAL((uint16_t)8080, p1.getPort());
  CPPUNIT_ASSERT_EQUAL((uint16_t)6881, p1.getOrigPort());
}

void PeerTest::testUsedBy()
{
  Peer p("192.168.0.1", 6881);
  CPPUNIT_ASSERT(p.unused());
  CPPUNIT_ASSERT_EQUAL((cuid_t)0, p.usedBy());

  p.usedBy(42);
  CPPUNIT_ASSERT(!p.unused());
  CPPUNIT_ASSERT_EQUAL((cuid_t)42, p.usedBy());
}

void PeerTest::testSessionResource()
{
  Peer p("192.168.0.1", 6881);
  CPPUNIT_ASSERT(!p.isActive());

  p.allocateSessionResource(1_k, 16_k);
  CPPUNIT_ASSERT(p.isActive());

  p.releaseSessionResource();
  CPPUNIT_ASSERT(!p.isActive());
}

void PeerTest::testPeerId()
{
  unsigned char id[PEER_ID_LENGTH];
  memset(id, 0xab, PEER_ID_LENGTH);
  peer->setPeerId(id);
  CPPUNIT_ASSERT(memcmp(id, peer->getPeerId(), PEER_ID_LENGTH) == 0);
}

void PeerTest::testChokingState()
{
  // Default: amChoking=true, peerChoking=true
  CPPUNIT_ASSERT(peer->amChoking());
  CPPUNIT_ASSERT(peer->peerChoking());

  peer->amChoking(false);
  CPPUNIT_ASSERT(!peer->amChoking());

  peer->peerChoking(false);
  CPPUNIT_ASSERT(!peer->peerChoking());

  // shouldBeChoking returns chokingRequired
  peer->chokingRequired(true);
  CPPUNIT_ASSERT(peer->shouldBeChoking());
  peer->chokingRequired(false);
  CPPUNIT_ASSERT(!peer->shouldBeChoking());
}

void PeerTest::testInterestedState()
{
  // Default: not interested
  CPPUNIT_ASSERT(!peer->amInterested());
  CPPUNIT_ASSERT(!peer->peerInterested());

  peer->amInterested(true);
  CPPUNIT_ASSERT(peer->amInterested());

  peer->peerInterested(true);
  CPPUNIT_ASSERT(peer->peerInterested());
}

void PeerTest::testSnubbingAndOptUnchoking()
{
  CPPUNIT_ASSERT(!peer->snubbing());
  peer->snubbing(true);
  CPPUNIT_ASSERT(peer->snubbing());
  peer->snubbing(false);
  CPPUNIT_ASSERT(!peer->snubbing());

  CPPUNIT_ASSERT(!peer->optUnchoking());
  peer->optUnchoking(true);
  CPPUNIT_ASSERT(peer->optUnchoking());
  peer->optUnchoking(false);
  CPPUNIT_ASSERT(!peer->optUnchoking());
}

void PeerTest::testBitfieldOps()
{
  // 16 pieces: pieceLength=1k, totalLength=16k
  // After allocateSessionResource, bitfield is all zeros
  CPPUNIT_ASSERT(!peer->hasPiece(0));
  CPPUNIT_ASSERT(!peer->isSeeder());

  // Set specific bit
  peer->updateBitfield(0, 1);
  CPPUNIT_ASSERT(peer->hasPiece(0));
  CPPUNIT_ASSERT(!peer->hasPiece(1));

  // Clear specific bit
  peer->updateBitfield(0, 0);
  CPPUNIT_ASSERT(!peer->hasPiece(0));

  // Set all bits (becomes seeder)
  peer->setAllBitfield();
  CPPUNIT_ASSERT(peer->hasPiece(0));
  CPPUNIT_ASSERT(peer->isSeeder());

  // setBitfield with custom data
  // 1M total / 1k piece = 1024 pieces = 128 bytes bitfield
  unsigned char bf[128];
  memset(bf, 0, sizeof(bf));
  bf[0] = 0x80; // only piece 0 set
  peer->setBitfield({bf, sizeof(bf)});
  CPPUNIT_ASSERT(peer->hasPiece(0));
  CPPUNIT_ASSERT(!peer->hasPiece(1));
  CPPUNIT_ASSERT_EQUAL((size_t)128, peer->getBitfieldLength());
}

void PeerTest::testExtensionFlags()
{
  // Fast extension
  CPPUNIT_ASSERT(!peer->isFastExtensionEnabled());
  peer->setFastExtensionEnabled(true);
  CPPUNIT_ASSERT(peer->isFastExtensionEnabled());

  // Extended messaging
  CPPUNIT_ASSERT(!peer->isExtendedMessagingEnabled());
  peer->setExtendedMessagingEnabled(true);
  CPPUNIT_ASSERT(peer->isExtendedMessagingEnabled());

  // DHT
  CPPUNIT_ASSERT(!peer->isDHTEnabled());
  peer->setDHTEnabled(true);
  CPPUNIT_ASSERT(peer->isDHTEnabled());
}

void PeerTest::testExtensionMessageID()
{
  // Use ExtensionMessageRegistry key constants
  peer->setExtension(1, 3); // key=1 → id=3
  CPPUNIT_ASSERT_EQUAL((uint8_t)3, peer->getExtensionMessageID(1));
}

void PeerTest::testUploadDownloadTracking()
{
  CPPUNIT_ASSERT_EQUAL((int64_t)0, peer->getSessionUploadLength());
  CPPUNIT_ASSERT_EQUAL((int64_t)0, peer->getSessionDownloadLength());

  peer->updateUploadLength(100);
  CPPUNIT_ASSERT_EQUAL((int64_t)100, peer->getSessionUploadLength());

  peer->updateDownload(200);
  CPPUNIT_ASSERT_EQUAL((int64_t)200, peer->getSessionDownloadLength());

  // Speed calculations should not crash
  peer->updateUploadSpeed(50);
  peer->calculateUploadSpeed();
  peer->calculateDownloadSpeed();
}

void PeerTest::testCompletedLength()
{
  // Initially 0
  CPPUNIT_ASSERT_EQUAL((int64_t)0, peer->getCompletedLength());

  // Set a piece, completed length increases
  peer->updateBitfield(0, 1);
  CPPUNIT_ASSERT(peer->getCompletedLength() > 0);
}

void PeerTest::testIncomingLocalDisconnected()
{
  Peer p("192.168.0.1", 6881);
  CPPUNIT_ASSERT(!p.isIncomingPeer());
  p.setIncomingPeer(true);
  CPPUNIT_ASSERT(p.isIncomingPeer());

  CPPUNIT_ASSERT(!p.isLocalPeer());
  p.setLocalPeer(true);
  CPPUNIT_ASSERT(p.isLocalPeer());

  CPPUNIT_ASSERT(!p.isDisconnectedGracefully());
  p.setDisconnectedGracefully(true);
  CPPUNIT_ASSERT(p.isDisconnectedGracefully());
}

void PeerTest::testReconfigureSessionResource()
{
  // Reconfigure changes piece length and total length
  peer->reconfigureSessionResource(2_k, 2_m);
  // Should still be active
  CPPUNIT_ASSERT(peer->isActive());
  // Bitfield should be reset
  CPPUNIT_ASSERT(!peer->hasPiece(0));
}

} // namespace aria2
