#include "BitfieldMan.h"

#include <cstring>
#include <vector>

#include <cppunit/extensions/HelperMacros.h>

#include "bitfield.h"
#include "array_fun.h"

namespace aria2 {

class BitfieldManTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(BitfieldManTest);
  CPPUNIT_TEST(testGetBlockSize);
  CPPUNIT_TEST(testGetFirstMissingUnusedIndex);
  CPPUNIT_TEST(testGetFirstMissingIndex);
  CPPUNIT_TEST(testIsAllBitSet);
  CPPUNIT_TEST(testFilter);
  CPPUNIT_TEST(testIsFilterBitSet);
  CPPUNIT_TEST(testAddFilter_zeroLength);
  CPPUNIT_TEST(testAddNotFilter);
  CPPUNIT_TEST(testAddNotFilter_zeroLength);
  CPPUNIT_TEST(testAddNotFilter_overflow);
  CPPUNIT_TEST(testGetSparseMissingUnusedIndex);
  CPPUNIT_TEST(testGetSparseMissingUnusedIndex_setBit);
  CPPUNIT_TEST(testGetSparseMissingUnusedIndex_withMinSplitSize);
  CPPUNIT_TEST(testIsBitSetOffsetRange);
  CPPUNIT_TEST(testGetOffsetCompletedLength);
  CPPUNIT_TEST(testGetOffsetCompletedLength_largeFile);
  CPPUNIT_TEST(testGetMissingUnusedLength);
  CPPUNIT_TEST(testSetBitRange);
  CPPUNIT_TEST(testGetAllMissingIndexes);
  CPPUNIT_TEST(testGetAllMissingIndexes_noarg);
  CPPUNIT_TEST(testGetAllMissingIndexes_checkLastByte);
  CPPUNIT_TEST(testGetAllMissingUnusedIndexes);
  CPPUNIT_TEST(testCountFilteredBlock);
  CPPUNIT_TEST(testCountMissingBlock);
  CPPUNIT_TEST(testZeroLengthFilter);
  CPPUNIT_TEST(testGetFirstNMissingUnusedIndex);
  CPPUNIT_TEST(testGetInorderMissingUnusedIndex);
  CPPUNIT_TEST(testGetGeomMissingUnusedIndex);
  CPPUNIT_TEST(testGetLastBlockLength);
  CPPUNIT_TEST(testSetAllUseBit);
  CPPUNIT_TEST(testUnsetBitRange);
  CPPUNIT_TEST(testIsBitRangeSet);
  CPPUNIT_TEST(testIsAllFilterBitSet);
  CPPUNIT_TEST(testHasMissingPiece);
  CPPUNIT_TEST(testCountMissingBlock_filter);
  CPPUNIT_TEST_SUITE_END();

public:
  void testGetBlockSize();
  void testGetFirstMissingUnusedIndex();
  void testGetFirstMissingIndex();
  void testGetAllMissingIndexes();
  void testGetAllMissingIndexes_noarg();
  void testGetAllMissingIndexes_checkLastByte();
  void testGetAllMissingUnusedIndexes();

  void testIsAllBitSet();
  void testFilter();
  void testIsFilterBitSet();
  void testAddFilter_zeroLength();
  void testAddNotFilter();
  void testAddNotFilter_zeroLength();
  void testAddNotFilter_overflow();
  void testGetSparseMissingUnusedIndex();
  void testGetSparseMissingUnusedIndex_setBit();
  void testGetSparseMissingUnusedIndex_withMinSplitSize();
  void testIsBitSetOffsetRange();
  void testGetOffsetCompletedLength();
  void testGetOffsetCompletedLength_largeFile();
  void testGetMissingUnusedLength();
  void testSetBitRange();
  void testCountFilteredBlock();
  void testCountMissingBlock();
  void testZeroLengthFilter();
  void testGetFirstNMissingUnusedIndex();
  void testGetInorderMissingUnusedIndex();
  void testGetGeomMissingUnusedIndex();
  void testGetLastBlockLength();
  void testSetAllUseBit();
  void testUnsetBitRange();
  void testIsBitRangeSet();
  void testIsAllFilterBitSet();
  void testHasMissingPiece();
  void testCountMissingBlock_filter();
};

CPPUNIT_TEST_SUITE_REGISTRATION(BitfieldManTest);

void BitfieldManTest::testGetBlockSize()
{
  BitfieldMan bt1(1_k, 10_k);
  CPPUNIT_ASSERT_EQUAL((int32_t)1_k, bt1.getBlockLength(9));

  BitfieldMan bt2(1_k, 10_k + 1);
  CPPUNIT_ASSERT_EQUAL((int32_t)1_k, bt2.getBlockLength(9));
  CPPUNIT_ASSERT_EQUAL((int32_t)1, bt2.getBlockLength(10));
  CPPUNIT_ASSERT_EQUAL((int32_t)0, bt2.getBlockLength(11));
}

void BitfieldManTest::testGetFirstMissingUnusedIndex()
{
  {
    BitfieldMan bt1(1_k, 10_k);
    {
      auto r = bt1.getFirstMissingUnusedIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)0, *r);
    }
    bt1.setUseBit(0);
    {
      auto r = bt1.getFirstMissingUnusedIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)1, *r);
    }
    bt1.unsetUseBit(0);
    bt1.setBit(0);
    {
      auto r = bt1.getFirstMissingUnusedIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)1, *r);
    }
    bt1.setAllBit();
    {
      CPPUNIT_ASSERT(!bt1.getFirstMissingUnusedIndex().has_value());
    }
  }
  {
    BitfieldMan bt1(1_k, 10_k);

    bt1.addFilter(1_k, 10_k);
    bt1.enableFilter();
    {
      auto r = bt1.getFirstMissingUnusedIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)1, *r);
    }
    bt1.setUseBit(1);
    {
      auto r = bt1.getFirstMissingUnusedIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)2, *r);
    }
    bt1.setBit(2);
    {
      auto r = bt1.getFirstMissingUnusedIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)3, *r);
    }
  }
}

void BitfieldManTest::testGetFirstMissingIndex()
{
  {
    BitfieldMan bt1(1_k, 10_k);
    {
      auto r = bt1.getFirstMissingIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)0, *r);
    }
    bt1.setUseBit(0);
    {
      auto r = bt1.getFirstMissingIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)0, *r);
    }
    bt1.unsetUseBit(0);
    bt1.setBit(0);
    {
      auto r = bt1.getFirstMissingIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)1, *r);
    }
    bt1.setAllBit();
    {
      CPPUNIT_ASSERT(!bt1.getFirstMissingIndex().has_value());
    }
  }
  {
    BitfieldMan bt1(1_k, 10_k);

    bt1.addFilter(1_k, 10_k);
    bt1.enableFilter();
    {
      auto r = bt1.getFirstMissingIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)1, *r);
    }
    bt1.setUseBit(1);
    {
      auto r = bt1.getFirstMissingIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)1, *r);
    }
    bt1.setBit(1);
    {
      auto r = bt1.getFirstMissingIndex();
      CPPUNIT_ASSERT(r.has_value());
      CPPUNIT_ASSERT_EQUAL((size_t)2, *r);
    }
  }
}

void BitfieldManTest::testIsAllBitSet()
{
  BitfieldMan bt1(1_k, 10_k);
  CPPUNIT_ASSERT(!bt1.isAllBitSet());
  bt1.setBit(1);
  CPPUNIT_ASSERT(!bt1.isAllBitSet());

  for (size_t i = 0; i < 8; i++) {
    CPPUNIT_ASSERT(bt1.setBit(i));
  }
  CPPUNIT_ASSERT(!bt1.isAllBitSet());

  for (size_t i = 0; i < bt1.countBlock(); i++) {
    CPPUNIT_ASSERT(bt1.setBit(i));
  }
  CPPUNIT_ASSERT(bt1.isAllBitSet());

  BitfieldMan btzero(1_k, 0);
  CPPUNIT_ASSERT(btzero.isAllBitSet());
}

void BitfieldManTest::testFilter()
{
  BitfieldMan btman(2, 32);
  // test offset=4, length=12
  btman.addFilter(4, 12);
  btman.enableFilter();
  std::vector<size_t> out;
  CPPUNIT_ASSERT_EQUAL((size_t)6, btman.getFirstNMissingUnusedIndex(out, 32));
  const size_t ans[] = {2, 3, 4, 5, 6, 7};
  for (size_t i = 0; i < arraySize(ans); ++i) {
    CPPUNIT_ASSERT_EQUAL(ans[i], out[i]);
  }
  CPPUNIT_ASSERT_EQUAL((int64_t)12ULL, btman.getFilteredTotalLength());

  // test offset=5, length=2
  out.clear();
  btman.clearAllBit();
  btman.clearAllUseBit();
  btman.clearFilter();
  btman.addFilter(5, 2);
  btman.enableFilter();
  CPPUNIT_ASSERT_EQUAL((size_t)2, btman.getFirstNMissingUnusedIndex(out, 32));
  CPPUNIT_ASSERT_EQUAL((size_t)2, out[0]);
  CPPUNIT_ASSERT_EQUAL((size_t)3, out[1]);
  btman.setBit(2);
  btman.setBit(3);
  CPPUNIT_ASSERT_EQUAL((int64_t)4ULL, btman.getFilteredTotalLength());
  CPPUNIT_ASSERT(btman.isFilteredAllBitSet());

  BitfieldMan btman2(2, 31);
  btman2.addFilter(0, 31);
  btman2.enableFilter();
  CPPUNIT_ASSERT_EQUAL((int64_t)31ULL, btman2.getFilteredTotalLength());
}

void BitfieldManTest::testIsFilterBitSet()
{
  BitfieldMan btman(2, 32);
  CPPUNIT_ASSERT(!btman.isFilterBitSet(0));
  btman.addFilter(0, 2);
  CPPUNIT_ASSERT(btman.isFilterBitSet(0));
  CPPUNIT_ASSERT(!btman.isFilterBitSet(1));
  btman.addFilter(2, 4);
  CPPUNIT_ASSERT(btman.isFilterBitSet(1));
}

void BitfieldManTest::testAddFilter_zeroLength()
{
  BitfieldMan bits(1_k, 1_m);
  bits.addFilter(2_k, 0);
  bits.enableFilter();
  CPPUNIT_ASSERT_EQUAL((size_t)0, bits.countMissingBlock());
  CPPUNIT_ASSERT(bits.isFilteredAllBitSet());
}

void BitfieldManTest::testAddNotFilter()
{
  BitfieldMan btman(2, 32);

  btman.addNotFilter(3, 6);
  CPPUNIT_ASSERT(bitfield::test(btman.getFilterBitfield(), 16, 0));
  for (size_t i = 1; i < 5; ++i) {
    CPPUNIT_ASSERT(!bitfield::test(btman.getFilterBitfield(), 16, i));
  }
  for (size_t i = 5; i < 16; ++i) {
    CPPUNIT_ASSERT(bitfield::test(btman.getFilterBitfield(), 16, i));
  }
}

void BitfieldManTest::testAddNotFilter_zeroLength()
{
  BitfieldMan btman(2, 6);
  btman.addNotFilter(2, 0);
  CPPUNIT_ASSERT(!bitfield::test(btman.getFilterBitfield(), 3, 0));
  CPPUNIT_ASSERT(!bitfield::test(btman.getFilterBitfield(), 3, 1));
  CPPUNIT_ASSERT(!bitfield::test(btman.getFilterBitfield(), 3, 2));
}

void BitfieldManTest::testAddNotFilter_overflow()
{
  BitfieldMan btman(2, 6);
  btman.addNotFilter(6, 100);
  CPPUNIT_ASSERT(bitfield::test(btman.getFilterBitfield(), 3, 0));
  CPPUNIT_ASSERT(bitfield::test(btman.getFilterBitfield(), 3, 1));
  CPPUNIT_ASSERT(bitfield::test(btman.getFilterBitfield(), 3, 2));
}

// TODO1.5 add test using ignoreBitfield
void BitfieldManTest::testGetSparseMissingUnusedIndex()
{
  BitfieldMan bitfield(1_m, 10_m);
  const size_t length = 2;
  unsigned char ignoreBitfield[length];
  memset(ignoreBitfield, 0, sizeof(ignoreBitfield));
  size_t minSplitSize = 1_m;
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)0, *r);
  }
  bitfield.setUseBit(0);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)5, *r);
  }
  bitfield.setUseBit(5);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)3, *r);
  }
  bitfield.setUseBit(3);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)8, *r);
  }
  bitfield.setUseBit(8);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)2, *r);
  }
  bitfield.setUseBit(2);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)1, *r);
  }
  bitfield.setUseBit(1);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)4, *r);
  }
  bitfield.setUseBit(4);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)7, *r);
  }
  bitfield.setUseBit(7);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)6, *r);
  }
  bitfield.setUseBit(6);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)9, *r);
  }
  bitfield.setUseBit(9);
  CPPUNIT_ASSERT(
      !bitfield
           .getSparseMissingUnusedIndex(minSplitSize, {ignoreBitfield, length})
           .has_value());
}

void BitfieldManTest::testGetSparseMissingUnusedIndex_setBit()
{
  BitfieldMan bitfield(1_m, 10_m);
  const size_t length = 2;
  unsigned char ignoreBitfield[length];
  memset(ignoreBitfield, 0, sizeof(ignoreBitfield));
  size_t minSplitSize = 1_m;
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)0, *r);
  }
  bitfield.setBit(0);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)1, *r);
  }
  bitfield.setBit(1);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)2, *r);
  }
  bitfield.setBit(2);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)3, *r);
  }
  bitfield.setBit(3);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)4, *r);
  }
  bitfield.setBit(4);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)5, *r);
  }
  bitfield.setBit(5);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)6, *r);
  }
  bitfield.setBit(6);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)7, *r);
  }
  bitfield.setBit(7);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)8, *r);
  }
  bitfield.setBit(8);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)9, *r);
  }
  bitfield.setBit(9);
  CPPUNIT_ASSERT(
      !bitfield
           .getSparseMissingUnusedIndex(minSplitSize, {ignoreBitfield, length})
           .has_value());
}

void BitfieldManTest::testGetSparseMissingUnusedIndex_withMinSplitSize()
{
  BitfieldMan bitfield(1_m, 10_m);
  const size_t length = 2;
  unsigned char ignoreBitfield[length];
  memset(ignoreBitfield, 0, sizeof(ignoreBitfield));
  size_t minSplitSize = 2_m;
  bitfield.setUseBit(1);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)6, *r);
  }
  bitfield.setBit(6);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)7, *r);
  }
  bitfield.setUseBit(7);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)4, *r);
  }
  bitfield.setBit(4);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)0, *r);
  }
  bitfield.setBit(0);
  {
    auto r = bitfield.getSparseMissingUnusedIndex(minSplitSize,
                                                  {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)5, *r);
  }
  bitfield.setBit(5);
  CPPUNIT_ASSERT(
      !bitfield
           .getSparseMissingUnusedIndex(minSplitSize, {ignoreBitfield, length})
           .has_value());
}

void BitfieldManTest::testIsBitSetOffsetRange()
{
  int64_t totalLength = 4_g;
  int32_t pieceLength = 4_m;
  BitfieldMan bitfield(pieceLength, totalLength);
  bitfield.setAllBit();

  CPPUNIT_ASSERT(!bitfield.isBitSetOffsetRange(0, 0));
  CPPUNIT_ASSERT(!bitfield.isBitSetOffsetRange(totalLength, 100));
  CPPUNIT_ASSERT(!bitfield.isBitSetOffsetRange(totalLength + 1, 100));

  CPPUNIT_ASSERT(bitfield.isBitSetOffsetRange(0, totalLength));
  CPPUNIT_ASSERT(bitfield.isBitSetOffsetRange(0, totalLength + 1));

  bitfield.clearAllBit();

  bitfield.setBit(100);
  bitfield.setBit(101);

  CPPUNIT_ASSERT(
      bitfield.isBitSetOffsetRange(pieceLength * 100, pieceLength * 2));
  CPPUNIT_ASSERT(
      !bitfield.isBitSetOffsetRange(pieceLength * 100 - 10, pieceLength * 2));
  CPPUNIT_ASSERT(
      !bitfield.isBitSetOffsetRange(pieceLength * 100, pieceLength * 2 + 1));

  bitfield.clearAllBit();

  bitfield.setBit(100);
  bitfield.setBit(102);

  CPPUNIT_ASSERT(
      !bitfield.isBitSetOffsetRange(pieceLength * 100, pieceLength * 3));
}

void BitfieldManTest::testGetOffsetCompletedLength()
{
  BitfieldMan bt(1_k, 20_k);
  // 00000|00000|00000|00000
  CPPUNIT_ASSERT_EQUAL((int64_t)0, bt.getOffsetCompletedLength(0, 1_k));
  CPPUNIT_ASSERT_EQUAL((int64_t)0, bt.getOffsetCompletedLength(0, 0));
  for (size_t i = 2; i <= 4; ++i) {
    bt.setBit(i);
  }
  // 00111|00000|00000|00000
  CPPUNIT_ASSERT_EQUAL((int64_t)3072, bt.getOffsetCompletedLength(2048, 3072));
  CPPUNIT_ASSERT_EQUAL((int64_t)3071, bt.getOffsetCompletedLength(2047, 3072));
  CPPUNIT_ASSERT_EQUAL((int64_t)3071, bt.getOffsetCompletedLength(2049, 3072));
  CPPUNIT_ASSERT_EQUAL((int64_t)0, bt.getOffsetCompletedLength(2048, 0));
  CPPUNIT_ASSERT_EQUAL((int64_t)1, bt.getOffsetCompletedLength(2048, 1));
  CPPUNIT_ASSERT_EQUAL((int64_t)0, bt.getOffsetCompletedLength(2047, 1));
  CPPUNIT_ASSERT_EQUAL((int64_t)3072, bt.getOffsetCompletedLength(0, 20_k));
  CPPUNIT_ASSERT_EQUAL((int64_t)3072,
                       bt.getOffsetCompletedLength(0, 20_k + 10));
  CPPUNIT_ASSERT_EQUAL((int64_t)0, bt.getOffsetCompletedLength(20_k, 1));
}

void BitfieldManTest::testGetOffsetCompletedLength_largeFile()
{
  // Test for overflow on 32-bit systems.

  // Total 4TiB, 4MiB block
  BitfieldMan bt(1 << 22, 1LL << 40);
  bt.setBit(1 << 11);
  bt.setBit((1 << 11) + 1);
  bt.setBit((1 << 11) + 2);

  // The last piece is missing:
  CPPUNIT_ASSERT_EQUAL((int64_t)bt.getBlockLength() * 3,
                       bt.getOffsetCompletedLength(1LL << 33, 1 << 24));

  // The first piece is missing:
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)bt.getBlockLength() * 3,
      bt.getOffsetCompletedLength((1LL << 33) - bt.getBlockLength(), 1 << 24));
}

void BitfieldManTest::testGetMissingUnusedLength()
{
  int64_t totalLength = 10_k + 10;
  size_t blockLength = 1_k;

  BitfieldMan bf(blockLength, totalLength);

  // from index 0 and all blocks are unused and not acquired.
  CPPUNIT_ASSERT_EQUAL(totalLength, bf.getMissingUnusedLength(0));

  // from index 10 and all blocks are unused and not acquired.
  CPPUNIT_ASSERT_EQUAL((int64_t)10ULL, bf.getMissingUnusedLength(10));

  // from index 11
  CPPUNIT_ASSERT_EQUAL((int64_t)0ULL, bf.getMissingUnusedLength(11));

  // from index 12
  CPPUNIT_ASSERT_EQUAL((int64_t)0ULL, bf.getMissingUnusedLength(12));

  // from index 0 and 5th block is used.
  bf.setUseBit(5);
  CPPUNIT_ASSERT_EQUAL((int64_t)(5LL * blockLength),
                       bf.getMissingUnusedLength(0));

  // from index 0 and 4th block is acquired.
  bf.setBit(4);
  CPPUNIT_ASSERT_EQUAL((int64_t)(4LL * blockLength),
                       bf.getMissingUnusedLength(0));

  // from index 1
  CPPUNIT_ASSERT_EQUAL((int64_t)(3LL * blockLength),
                       bf.getMissingUnusedLength(1));
}

void BitfieldManTest::testSetBitRange()
{
  size_t blockLength = 1_m;
  int64_t totalLength = 10 * blockLength;

  BitfieldMan bf(blockLength, totalLength);

  bf.setBitRange(0, 4);

  for (size_t i = 0; i < 5; ++i) {
    CPPUNIT_ASSERT(bf.isBitSet(i));
  }
  for (size_t i = 5; i < 10; ++i) {
    CPPUNIT_ASSERT(!bf.isBitSet(i));
  }
  CPPUNIT_ASSERT_EQUAL((int64_t)(5LL * blockLength), bf.getCompletedLength());
}

void BitfieldManTest::testGetAllMissingIndexes_noarg()
{
  size_t blockLength = 16_k;
  int64_t totalLength = 1_m;
  size_t nbits = (totalLength + blockLength - 1) / blockLength;
  BitfieldMan bf(blockLength, totalLength);
  unsigned char misbitfield[8];
  CPPUNIT_ASSERT(bf.getAllMissingIndexes(misbitfield, sizeof(misbitfield)));
  CPPUNIT_ASSERT_EQUAL((size_t)64, bitfield::countSetBit(misbitfield, nbits));

  for (size_t i = 0; i < 63; ++i) {
    bf.setBit(i);
  }
  CPPUNIT_ASSERT(bf.getAllMissingIndexes(misbitfield, sizeof(misbitfield)));
  CPPUNIT_ASSERT_EQUAL((size_t)1, bitfield::countSetBit(misbitfield, nbits));
  CPPUNIT_ASSERT(bitfield::test(misbitfield, nbits, 63));
}

// See garbage bits of last byte are 0
void BitfieldManTest::testGetAllMissingIndexes_checkLastByte()
{
  size_t blockLength = 16_k;
  int64_t totalLength = blockLength * 2;
  size_t nbits = (totalLength + blockLength - 1) / blockLength;
  BitfieldMan bf(blockLength, totalLength);
  unsigned char misbitfield[1];
  CPPUNIT_ASSERT(bf.getAllMissingIndexes(misbitfield, sizeof(misbitfield)));
  CPPUNIT_ASSERT_EQUAL((size_t)2, bitfield::countSetBit(misbitfield, nbits));
  CPPUNIT_ASSERT(bitfield::test(misbitfield, nbits, 0));
  CPPUNIT_ASSERT(bitfield::test(misbitfield, nbits, 1));
}

void BitfieldManTest::testGetAllMissingIndexes()
{
  size_t blockLength = 16_k;
  int64_t totalLength = 1_m;
  size_t nbits = (totalLength + blockLength - 1) / blockLength;
  BitfieldMan bf(blockLength, totalLength);
  BitfieldMan peerBf(blockLength, totalLength);
  peerBf.setAllBit();
  unsigned char misbitfield[8];

  CPPUNIT_ASSERT(bf.getAllMissingIndexes(
      misbitfield, sizeof(misbitfield),
      {peerBf.getBitfield(), peerBf.getBitfieldLength()}));
  CPPUNIT_ASSERT_EQUAL((size_t)64, bitfield::countSetBit(misbitfield, nbits));
  for (size_t i = 0; i < 62; ++i) {
    bf.setBit(i);
  }
  peerBf.unsetBit(62);

  CPPUNIT_ASSERT(bf.getAllMissingIndexes(
      misbitfield, sizeof(misbitfield),
      {peerBf.getBitfield(), peerBf.getBitfieldLength()}));
  CPPUNIT_ASSERT_EQUAL((size_t)1, bitfield::countSetBit(misbitfield, nbits));
  CPPUNIT_ASSERT(bitfield::test(misbitfield, nbits, 63));
}

void BitfieldManTest::testGetAllMissingUnusedIndexes()
{
  size_t blockLength = 16_k;
  int64_t totalLength = 1_m;
  size_t nbits = (totalLength + blockLength - 1) / blockLength;
  BitfieldMan bf(blockLength, totalLength);
  BitfieldMan peerBf(blockLength, totalLength);
  peerBf.setAllBit();
  unsigned char misbitfield[8];

  CPPUNIT_ASSERT(bf.getAllMissingUnusedIndexes(
      misbitfield, sizeof(misbitfield),
      {peerBf.getBitfield(), peerBf.getBitfieldLength()}));
  CPPUNIT_ASSERT_EQUAL((size_t)64, bitfield::countSetBit(misbitfield, nbits));

  for (size_t i = 0; i < 61; ++i) {
    bf.setBit(i);
  }
  bf.setUseBit(61);
  peerBf.unsetBit(62);
  CPPUNIT_ASSERT(bf.getAllMissingUnusedIndexes(
      misbitfield, sizeof(misbitfield),
      {peerBf.getBitfield(), peerBf.getBitfieldLength()}));
  CPPUNIT_ASSERT_EQUAL((size_t)1, bitfield::countSetBit(misbitfield, nbits));
  CPPUNIT_ASSERT(bitfield::test(misbitfield, nbits, 63));
}

void BitfieldManTest::testCountFilteredBlock()
{
  BitfieldMan bt(1_k, 256_k);
  CPPUNIT_ASSERT_EQUAL((size_t)256, bt.countBlock());
  CPPUNIT_ASSERT_EQUAL((size_t)0, bt.countFilteredBlock());
  bt.addFilter(1_k, 256_k);
  bt.enableFilter();
  CPPUNIT_ASSERT_EQUAL((size_t)256, bt.countBlock());
  CPPUNIT_ASSERT_EQUAL((size_t)255, bt.countFilteredBlock());
  bt.disableFilter();
  CPPUNIT_ASSERT_EQUAL((size_t)256, bt.countBlock());
  CPPUNIT_ASSERT_EQUAL((size_t)0, bt.countFilteredBlock());
}

void BitfieldManTest::testCountMissingBlock()
{
  BitfieldMan bt(1_k, 10_k);
  CPPUNIT_ASSERT_EQUAL((size_t)10, bt.countMissingBlock());
  bt.setBit(1);
  CPPUNIT_ASSERT_EQUAL((size_t)9, bt.countMissingBlock());
  bt.setAllBit();
  CPPUNIT_ASSERT_EQUAL((size_t)0, bt.countMissingBlock());
}

void BitfieldManTest::testZeroLengthFilter()
{
  BitfieldMan bt(1_k, 10_k);
  bt.enableFilter();
  CPPUNIT_ASSERT_EQUAL((size_t)0, bt.countMissingBlock());
}

void BitfieldManTest::testGetFirstNMissingUnusedIndex()
{
  BitfieldMan bt(1_k, 10_k);
  bt.setUseBit(1);
  bt.setBit(5);
  std::vector<size_t> out;
  CPPUNIT_ASSERT_EQUAL((size_t)8, bt.getFirstNMissingUnusedIndex(out, 256));
  CPPUNIT_ASSERT_EQUAL((size_t)8, out.size());
  const size_t ans[] = {0, 2, 3, 4, 6, 7, 8, 9};
  for (size_t i = 0; i < out.size(); ++i) {
    CPPUNIT_ASSERT_EQUAL(ans[i], out[i]);
  }
  out.clear();
  CPPUNIT_ASSERT_EQUAL((size_t)3, bt.getFirstNMissingUnusedIndex(out, 3));
  CPPUNIT_ASSERT_EQUAL((size_t)3, out.size());
  for (size_t i = 0; i < out.size(); ++i) {
    CPPUNIT_ASSERT_EQUAL(ans[i], out[i]);
  }
  CPPUNIT_ASSERT_EQUAL((size_t)0, bt.getFirstNMissingUnusedIndex(out, 0));
  bt.setAllBit();
  CPPUNIT_ASSERT_EQUAL((size_t)0, bt.getFirstNMissingUnusedIndex(out, 10));
  bt.clearAllBit();
  out.clear();
  bt.addFilter(9_k, 1_k);
  bt.enableFilter();
  CPPUNIT_ASSERT_EQUAL((size_t)1, bt.getFirstNMissingUnusedIndex(out, 256));
  CPPUNIT_ASSERT_EQUAL((size_t)1, out.size());
  CPPUNIT_ASSERT_EQUAL((size_t)9, out[0]);
}

void BitfieldManTest::testGetInorderMissingUnusedIndex()
{
  BitfieldMan bt(1_k, 20_k);
  const size_t length = 3;
  unsigned char ignoreBitfield[length];
  memset(ignoreBitfield, 0, sizeof(ignoreBitfield));
  size_t minSplitSize = 1_k;
  // 00000|00000|00000|00000
  {
    auto r =
        bt.getInorderMissingUnusedIndex(minSplitSize, {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)0, *r);
  }
  bt.setUseBit(0);
  // 10000|00000|00000|00000
  {
    auto r =
        bt.getInorderMissingUnusedIndex(minSplitSize, {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)1, *r);
  }
  minSplitSize = 2_k;
  {
    auto r =
        bt.getInorderMissingUnusedIndex(minSplitSize, {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)2, *r);
  }
  bt.unsetUseBit(0);
  bt.setBit(0);
  {
    auto r =
        bt.getInorderMissingUnusedIndex(minSplitSize, {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)1, *r);
  }
  bt.setAllBit();
  bt.unsetBit(10);
  // 11111|11111|01111|11111
  {
    auto r =
        bt.getInorderMissingUnusedIndex(minSplitSize, {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)10, *r);
  }
  bt.setUseBit(10);
  CPPUNIT_ASSERT(
      !bt.getInorderMissingUnusedIndex(minSplitSize, {ignoreBitfield, length})
           .has_value());
  bt.unsetUseBit(10);
  bt.setAllBit();
  // 11111|11111|11111|11111
  CPPUNIT_ASSERT(
      !bt.getInorderMissingUnusedIndex(minSplitSize, {ignoreBitfield, length})
           .has_value());
  bt.clearAllBit();
  // 00000|00000|00000|00000
  for (int i = 0; i <= 1; ++i) {
    bitfield::flipBit(ignoreBitfield, length, i);
  }
  {
    auto r =
        bt.getInorderMissingUnusedIndex(minSplitSize, {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)2, *r);
  }
  bt.addFilter(3_k, 3_k);
  bt.enableFilter();
  {
    auto r =
        bt.getInorderMissingUnusedIndex(minSplitSize, {ignoreBitfield, length});
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)3, *r);
  }
}

void BitfieldManTest::testGetGeomMissingUnusedIndex()
{
  BitfieldMan bt(1_k, 20_k);
  const size_t length = 3;
  unsigned char ignoreBitfield[length];
  memset(ignoreBitfield, 0, sizeof(ignoreBitfield));
  size_t minSplitSize = 1_k;
  // 00000|00000|00000|00000
  {
    auto r = bt.getGeomMissingUnusedIndex(minSplitSize,
                                          {ignoreBitfield, length}, 2, 0);
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)0, *r);
  }
  bt.setUseBit(0);
  // 10000|00000|00000|00000
  {
    auto r = bt.getGeomMissingUnusedIndex(minSplitSize,
                                          {ignoreBitfield, length}, 2, 0);
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)1, *r);
  }
  bt.setUseBit(1);
  // 11000|00000|00000|00000
  {
    auto r = bt.getGeomMissingUnusedIndex(minSplitSize,
                                          {ignoreBitfield, length}, 2, 0);
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)2, *r);
  }
  bt.setUseBit(2);
  // 11100|00000|00000|00000
  {
    auto r = bt.getGeomMissingUnusedIndex(minSplitSize,
                                          {ignoreBitfield, length}, 2, 0);
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)4, *r);
  }
  bt.setUseBit(4);
  // 11110|00000|00000|00000
  {
    auto r = bt.getGeomMissingUnusedIndex(minSplitSize,
                                          {ignoreBitfield, length}, 2, 0);
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)8, *r);
  }
  bt.setUseBit(8);
  // 11110|00010|00000|00000
  {
    auto r = bt.getGeomMissingUnusedIndex(minSplitSize,
                                          {ignoreBitfield, length}, 2, 0);
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)16, *r);
  }
  bt.setUseBit(16);
  // 11110|00010|00000|01000
  {
    auto r = bt.getGeomMissingUnusedIndex(minSplitSize,
                                          {ignoreBitfield, length}, 2, 0);
    CPPUNIT_ASSERT(r.has_value());
    CPPUNIT_ASSERT_EQUAL((size_t)12, *r);
  }
  bt.setUseBit(12);
}

void BitfieldManTest::testGetLastBlockLength()
{
  // Non-aligned: totalLength=256, blockLength=100
  // blocks = ceil(256/100) = 3
  // lastBlockLength = 256 - 100*(3-1) = 256-200 = 56
  BitfieldMan bt1(100, 256);
  CPPUNIT_ASSERT_EQUAL((int32_t)56, bt1.getLastBlockLength());

  // Aligned: totalLength=300, blockLength=100
  // blocks = 3, lastBlockLength = 300 - 100*2 = 100
  BitfieldMan bt2(100, 300);
  CPPUNIT_ASSERT_EQUAL((int32_t)100, bt2.getLastBlockLength());

  // Single block: totalLength=50, blockLength=100
  // blocks = 1, lastBlockLength = 50 - 100*0 = 50
  BitfieldMan bt3(100, 50);
  CPPUNIT_ASSERT_EQUAL((int32_t)50, bt3.getLastBlockLength());
}

void BitfieldManTest::testSetAllUseBit()
{
  BitfieldMan bt(1_k, 10_k);
  bt.setAllUseBit();
  for (size_t i = 0; i < bt.countBlock(); ++i) {
    CPPUNIT_ASSERT(bt.isUseBitSet(i));
  }
  // Verify no missing unused index exists when all use bits are set
  CPPUNIT_ASSERT(!bt.getFirstMissingUnusedIndex().has_value());
}

void BitfieldManTest::testUnsetBitRange()
{
  size_t blockLength = 1_m;
  int64_t totalLength = 10 * blockLength;

  BitfieldMan bf(blockLength, totalLength);

  bf.setBitRange(0, 4);
  // Verify bits 0-4 are set
  for (size_t i = 0; i < 5; ++i) {
    CPPUNIT_ASSERT(bf.isBitSet(i));
  }
  // Unset bits 1-3
  bf.unsetBitRange(1, 3);
  // Bit 0 and 4 should still be set
  CPPUNIT_ASSERT(bf.isBitSet(0));
  CPPUNIT_ASSERT(bf.isBitSet(4));
  // Bits 1-3 should be cleared
  for (size_t i = 1; i <= 3; ++i) {
    CPPUNIT_ASSERT(!bf.isBitSet(i));
  }
}

void BitfieldManTest::testIsBitRangeSet()
{
  size_t blockLength = 1_m;
  int64_t totalLength = 10 * blockLength;

  BitfieldMan bf(blockLength, totalLength);

  bf.setBitRange(0, 4);
  CPPUNIT_ASSERT(bf.isBitRangeSet(0, 4));
  CPPUNIT_ASSERT(bf.isBitRangeSet(0, 0));
  CPPUNIT_ASSERT(bf.isBitRangeSet(2, 3));

  // Unset bit 2
  bf.unsetBit(2);
  CPPUNIT_ASSERT(!bf.isBitRangeSet(0, 4));
  CPPUNIT_ASSERT(bf.isBitRangeSet(0, 1));
  CPPUNIT_ASSERT(bf.isBitRangeSet(3, 4));
  CPPUNIT_ASSERT(!bf.isBitRangeSet(1, 3));
}

void BitfieldManTest::testIsAllFilterBitSet()
{
  BitfieldMan bt(1_k, 10_k);
  // No filter bitfield allocated yet
  CPPUNIT_ASSERT(!bt.isAllFilterBitSet());

  // Add filter for entire range
  bt.addFilter(0, 10_k);
  bt.enableFilter();
  CPPUNIT_ASSERT(bt.isAllFilterBitSet());

  // Clear filter deallocates filterBitfield
  bt.clearFilter();
  CPPUNIT_ASSERT(!bt.isAllFilterBitSet());

  // Partial filter should not be all set
  bt.addFilter(0, 5_k);
  bt.enableFilter();
  CPPUNIT_ASSERT(!bt.isAllFilterBitSet());
}

void BitfieldManTest::testHasMissingPiece()
{
  BitfieldMan bt(1_k, 4_k);
  // bt has 4 blocks, all clear

  // Create peer bitfield with all bits set
  BitfieldMan peerBf(1_k, 4_k);
  peerBf.setAllBit();

  // All pieces are missing on our side, peer has them all
  CPPUNIT_ASSERT(
      bt.hasMissingPiece({peerBf.getBitfield(), peerBf.getBitfieldLength()}));

  // Set all our bits - no missing pieces
  bt.setAllBit();
  CPPUNIT_ASSERT(
      !bt.hasMissingPiece({peerBf.getBitfield(), peerBf.getBitfieldLength()}));

  // Clear our bits, clear peer bits - peer has nothing we need
  bt.clearAllBit();
  peerBf.clearAllBit();
  CPPUNIT_ASSERT(
      !bt.hasMissingPiece({peerBf.getBitfield(), peerBf.getBitfieldLength()}));

  // Peer has only bit 2, we are missing it
  peerBf.setBit(2);
  CPPUNIT_ASSERT(
      bt.hasMissingPiece({peerBf.getBitfield(), peerBf.getBitfieldLength()}));

  // We acquire bit 2 - no longer missing
  bt.setBit(2);
  CPPUNIT_ASSERT(
      !bt.hasMissingPiece({peerBf.getBitfield(), peerBf.getBitfieldLength()}));

  // Mismatched bitfield length should return false
  // 4_k with 1_k blocks = 4 blocks = 1 byte bitfield
  // 16_k with 1_k blocks = 16 blocks = 2 byte bitfield
  BitfieldMan otherBf(1_k, 16_k);
  otherBf.setAllBit();
  CPPUNIT_ASSERT(!bt.hasMissingPiece(
      {otherBf.getBitfield(), otherBf.getBitfieldLength()}));
}

void BitfieldManTest::testCountMissingBlock_filter()
{
  BitfieldMan bt(1_k, 10_k);
  CPPUNIT_ASSERT_EQUAL((size_t)10, bt.countMissingBlock());

  // Set some bits
  bt.setBit(0);
  bt.setBit(1);
  bt.setBit(2);
  CPPUNIT_ASSERT_EQUAL((size_t)7, bt.countMissingBlock());

  // Enable filter for last 5 blocks
  bt.addFilter(5_k, 5_k);
  bt.enableFilter();
  // Only blocks 5-9 are filtered, none are set
  CPPUNIT_ASSERT_EQUAL((size_t)5, bt.countMissingBlock());

  // Set a filtered block
  bt.setBit(7);
  CPPUNIT_ASSERT_EQUAL((size_t)4, bt.countMissingBlock());

  // Set all filtered blocks
  for (size_t i = 5; i < 10; ++i) {
    bt.setBit(i);
  }
  CPPUNIT_ASSERT_EQUAL((size_t)0, bt.countMissingBlock());
}

} // namespace aria2
