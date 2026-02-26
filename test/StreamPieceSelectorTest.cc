#include "InorderStreamPieceSelector.h"
#include "RandomStreamPieceSelector.h"
#include "UnionSeedCriteria.h"
#include "TimeSeedCriteria.h"
#include "BitfieldMan.h"

#include <cppunit/extensions/HelperMacros.h>

#include <cstring>

namespace aria2 {

class StreamPieceSelectorTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(StreamPieceSelectorTest);
  CPPUNIT_TEST(testInorderSelect);
  CPPUNIT_TEST(testInorderSelectAllUsed);
  CPPUNIT_TEST(testRandomSelect);
  CPPUNIT_TEST(testRandomSelectAllUsed);
  CPPUNIT_TEST(testUnionSeedCriteriaAllFalse);
  CPPUNIT_TEST(testUnionSeedCriteriaOneTrue);
  CPPUNIT_TEST(testUnionSeedCriteriaEmpty);
  CPPUNIT_TEST_SUITE_END();

public:
  void testInorderSelect()
  {
    // 10 pieces of 1KB each = 10KB total
    BitfieldMan bf(1024, 10240);
    // Mark pieces 0-4 as completed
    bf.setBitRange(0, 4);
    InorderStreamPieceSelector sel(&bf);

    unsigned char igbf[2];
    memset(igbf, 0, sizeof(igbf));
    size_t index;
    CPPUNIT_ASSERT(sel.select(index, 1024, igbf, sizeof(igbf)));
    // First missing piece should be 5
    CPPUNIT_ASSERT_EQUAL((size_t)5, index);
  }

  void testInorderSelectAllUsed()
  {
    BitfieldMan bf(1024, 10240);
    bf.setBitRange(0, 9);
    InorderStreamPieceSelector sel(&bf);

    unsigned char igbf[2];
    memset(igbf, 0, sizeof(igbf));
    size_t index;
    // All pieces complete, nothing to select
    CPPUNIT_ASSERT(!sel.select(index, 1024, igbf, sizeof(igbf)));
  }

  void testRandomSelect()
  {
    // 20 pieces, mark first 10 as complete
    BitfieldMan bf(1024, 20480);
    bf.setBitRange(0, 9);
    RandomStreamPieceSelector sel(&bf);

    unsigned char igbf[3];
    memset(igbf, 0, sizeof(igbf));
    size_t index;
    CPPUNIT_ASSERT(sel.select(index, 1024, igbf, sizeof(igbf)));
    // Should select a piece in range [10, 19]
    CPPUNIT_ASSERT(index >= 10);
    CPPUNIT_ASSERT(index <= 19);
  }

  void testRandomSelectAllUsed()
  {
    BitfieldMan bf(1024, 10240);
    bf.setBitRange(0, 9);
    RandomStreamPieceSelector sel(&bf);

    unsigned char igbf[2];
    memset(igbf, 0, sizeof(igbf));
    size_t index;
    CPPUNIT_ASSERT(!sel.select(index, 1024, igbf, sizeof(igbf)));
  }

  void testUnionSeedCriteriaAllFalse()
  {
    UnionSeedCriteria uc;
    // TimeSeedCriteria with a very long duration won't evaluate to true
    auto tc1 = make_unique<TimeSeedCriteria>(std::chrono::seconds(9999));
    auto tc2 = make_unique<TimeSeedCriteria>(std::chrono::seconds(9999));
    uc.addSeedCriteria(std::move(tc1));
    uc.addSeedCriteria(std::move(tc2));
    // Neither should be satisfied
    CPPUNIT_ASSERT(!uc.evaluate());
  }

  void testUnionSeedCriteriaOneTrue()
  {
    UnionSeedCriteria uc;
    // Duration 0 means immediately satisfied
    auto tc1 = make_unique<TimeSeedCriteria>(std::chrono::seconds(0));
    auto tc2 = make_unique<TimeSeedCriteria>(std::chrono::seconds(9999));
    uc.addSeedCriteria(std::move(tc1));
    uc.addSeedCriteria(std::move(tc2));
    // First one should be satisfied → union returns true
    CPPUNIT_ASSERT(uc.evaluate());
  }

  void testUnionSeedCriteriaEmpty()
  {
    UnionSeedCriteria uc;
    // No criteria → evaluate should return false
    CPPUNIT_ASSERT(!uc.evaluate());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(StreamPieceSelectorTest);

} // namespace aria2
