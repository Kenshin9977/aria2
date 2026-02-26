#include "BtBitfieldMessageValidator.h"
#include "BtHandshakeMessageValidator.h"
#include "BtPieceMessageValidator.h"
#include "IndexBtMessageValidator.h"
#include "BtBitfieldMessage.h"
#include "BtHandshakeMessage.h"
#include "BtPieceMessage.h"
#include "BtHaveMessage.h"
#include "RecoverableException.h"

#include <cppunit/extensions/HelperMacros.h>

#include <cstring>

namespace aria2 {

class BtValidatorTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(BtValidatorTest);
  CPPUNIT_TEST(testBitfieldValid);
  CPPUNIT_TEST(testBitfieldInvalid);
  CPPUNIT_TEST(testHandshakeValid);
  CPPUNIT_TEST(testHandshakeInvalidHash);
  CPPUNIT_TEST(testPieceValid);
  CPPUNIT_TEST(testPieceInvalidIndex);
  CPPUNIT_TEST(testPieceInvalidBegin);
  CPPUNIT_TEST(testIndexValid);
  CPPUNIT_TEST(testIndexInvalidIndex);
  CPPUNIT_TEST_SUITE_END();

public:
  void testBitfieldValid()
  {
    // numPiece=8 means 1-byte bitfield
    BtBitfieldMessage msg;
    auto bf = std::vector<unsigned char>(1, 0xFF);
    msg.setBitfield(bf.data(), bf.size());
    BtBitfieldMessageValidator v(&msg, 8);
    v.validate();
  }

  void testBitfieldInvalid()
  {
    // numPiece=8 needs 1 byte, but we provide 2 bytes
    BtBitfieldMessage msg;
    auto bf = std::vector<unsigned char>(2, 0xFF);
    msg.setBitfield(bf.data(), bf.size());
    BtBitfieldMessageValidator v(&msg, 8);
    try {
      v.validate();
      CPPUNIT_FAIL("Expected exception for wrong bitfield length");
    }
    catch (const RecoverableException& e) {
      // expected
    }
  }

  void testHandshakeValid()
  {
    unsigned char infoHash[20];
    unsigned char peerId[20];
    memset(infoHash, 0xAA, 20);
    memset(peerId, 0xBB, 20);
    BtHandshakeMessage msg(infoHash, peerId);
    BtHandshakeMessageValidator v(&msg, infoHash);
    v.validate();
  }

  void testHandshakeInvalidHash()
  {
    unsigned char infoHash[20];
    unsigned char wrongHash[20];
    unsigned char peerId[20];
    memset(infoHash, 0xAA, 20);
    memset(wrongHash, 0xCC, 20);
    memset(peerId, 0xBB, 20);
    BtHandshakeMessage msg(infoHash, peerId);
    BtHandshakeMessageValidator v(&msg, wrongHash);
    try {
      v.validate();
      CPPUNIT_FAIL("Expected exception for wrong info hash");
    }
    catch (const RecoverableException& e) {
      // expected
    }
  }

  void testPieceValid()
  {
    BtPieceMessage msg;
    msg.setIndex(0);
    msg.setBegin(0);
    msg.setBlockLength(16384);
    // numPiece=10, pieceLength=1048576
    BtPieceMessageValidator v(&msg, 10, 1048576);
    v.validate();
  }

  void testPieceInvalidIndex()
  {
    BtPieceMessage msg;
    msg.setIndex(100);
    msg.setBegin(0);
    msg.setBlockLength(16384);
    BtPieceMessageValidator v(&msg, 10, 1048576);
    try {
      v.validate();
      CPPUNIT_FAIL("Expected exception for invalid index");
    }
    catch (const RecoverableException& e) {
      // expected
    }
  }

  void testPieceInvalidBegin()
  {
    BtPieceMessage msg;
    msg.setIndex(0);
    msg.setBegin(1048576);
    msg.setBlockLength(16384);
    BtPieceMessageValidator v(&msg, 10, 1048576);
    try {
      v.validate();
      CPPUNIT_FAIL("Expected exception for invalid begin");
    }
    catch (const RecoverableException& e) {
      // expected
    }
  }

  void testIndexValid()
  {
    BtHaveMessage msg(5);
    IndexBtMessageValidator v(&msg, 10);
    v.validate();
  }

  void testIndexInvalidIndex()
  {
    BtHaveMessage msg(100);
    IndexBtMessageValidator v(&msg, 10);
    try {
      v.validate();
      CPPUNIT_FAIL("Expected exception for invalid index");
    }
    catch (const RecoverableException& e) {
      // expected
    }
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(BtValidatorTest);

} // namespace aria2
