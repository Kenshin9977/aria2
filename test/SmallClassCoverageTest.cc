#include "ContextAttribute.h"
#include "DefaultBtInteractive.h"
#include "Notifier.h"
#include "FatalException.h"
#include "TruncFileAllocationIterator.h"
#include "UnknownOptionException.h"
#include "ByteArrayDiskWriter.h"
#include "RequestGroup.h"
#include "BtLeecherStateChoke.h"
#include "BtSeederStateChoke.h"
#include "Peer.h"
#include "paramed_string.h"
#include "MockBtMessageDispatcher.h"
#include "UnknownLengthPieceStorage.h"
#include "DownloadContext.h"
#include "Piece.h"
#include "SegList.h"

#include <cppunit/extensions/HelperMacros.h>

#include <string>
#include <vector>

#include "util.h"

namespace aria2 {

namespace {

class TestDownloadEventListener : public DownloadEventListener {
public:
  std::vector<DownloadEvent> events;
  void onEvent(DownloadEvent event, const RequestGroup* group) override
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
  CPPUNIT_TEST(testLeecherChokeEmpty);
  CPPUNIT_TEST(testLeecherChokeWithPeers);
  CPPUNIT_TEST(testLeecherChokeRoundCycle);
  CPPUNIT_TEST(testSeederChokeEmpty);
  CPPUNIT_TEST(testSeederChokeWithPeers);
  CPPUNIT_TEST(testSeederChokeRoundCycle);
  CPPUNIT_TEST(testParamedStringExpand);
  CPPUNIT_TEST(testParamedStringExpandLoop);
  CPPUNIT_TEST(testParamedStringExpandChoice);
  CPPUNIT_TEST(testUnknownLengthPieceStorage_basic);
  CPPUNIT_TEST(testUnknownLengthPieceStorage_markTotalLengthSet);
  CPPUNIT_TEST(testUnknownLengthPieceStorage_hasMissingPiece);
  CPPUNIT_TEST(testSegListBasic);
  CPPUNIT_TEST(testSegListSingleRange);
  CPPUNIT_TEST(testSegListMultiRange);
  CPPUNIT_TEST(testSegListEmpty);
  CPPUNIT_TEST(testSegListClear);
  CPPUNIT_TEST(testSegListNormalize);
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
  void testLeecherChokeEmpty()
  {
    BtLeecherStateChoke choke;
    PeerSet peerSet;
    // No peers — should not crash
    choke.executeChoke(peerSet);
    // Round advances
    choke.executeChoke(peerSet);
    choke.executeChoke(peerSet);
    // After 3 rounds, wraps back to 0
    choke.executeChoke(peerSet);
  }

  void testLeecherChokeWithPeers()
  {
    BtLeecherStateChoke choke;
    PeerSet peerSet;
    // Create 5 active peers
    for (int i = 0; i < 5; ++i) {
      auto peer =
          std::make_shared<Peer>("192.168.0." + util::uitos(i + 1), 6881 + i);
      peer->allocateSessionResource(1_k, 1_m);
      peer->peerInterested(true);
      peerSet.insert(peer);
    }
    // Run round 0 (planned optimistic unchoke)
    choke.executeChoke(peerSet);
    // All peers should have chokingRequired set
    // At least one peer should be unchoked
    int unchoked = 0;
    for (const auto& p : peerSet) {
      if (!p->chokingRequired()) {
        ++unchoked;
      }
    }
    CPPUNIT_ASSERT(unchoked >= 1);
    CPPUNIT_ASSERT(unchoked <= 4);
  }

  void testLeecherChokeRoundCycle()
  {
    BtLeecherStateChoke choke;
    PeerSet peerSet;
    auto peer = std::make_shared<Peer>("127.0.0.1", 6881);
    peer->allocateSessionResource(1_k, 1_m);
    peer->peerInterested(true);
    peerSet.insert(peer);
    // Round 0: planned optimistic unchoke
    choke.executeChoke(peerSet);
    // Round 1: regular unchoke
    choke.executeChoke(peerSet);
    // Round 2: regular unchoke
    choke.executeChoke(peerSet);
    // Round 3 wraps to 0: planned optimistic unchoke again
    choke.executeChoke(peerSet);
    // Verify lastRound is updated
    CPPUNIT_ASSERT(choke.getLastRound().getTime() != Timer::zero().getTime());
  }

  void testSeederChokeEmpty()
  {
    BtSeederStateChoke choke;
    PeerSet peerSet;
    choke.executeChoke(peerSet);
    choke.executeChoke(peerSet);
    choke.executeChoke(peerSet);
    // After 3 rounds, wraps back to 0
    choke.executeChoke(peerSet);
    CPPUNIT_ASSERT(choke.getLastRound().getTime() != Timer::zero().getTime());
  }

  void testSeederChokeWithPeers()
  {
    BtSeederStateChoke choke;
    PeerSet peerSet;
    // Need a dispatcher for countOutstandingUpload()
    MockBtMessageDispatcher dispatcher;
    for (int i = 0; i < 5; ++i) {
      auto peer =
          std::make_shared<Peer>("10.0.0." + util::uitos(i + 1), 7000 + i);
      peer->allocateSessionResource(1_k, 1_m);
      peer->setBtMessageDispatcher(&dispatcher);
      peer->peerInterested(true);
      peerSet.insert(peer);
    }
    choke.executeChoke(peerSet);
    // At least some peers should be unchoked
    int unchoked = 0;
    for (const auto& p : peerSet) {
      if (!p->chokingRequired()) {
        ++unchoked;
      }
    }
    CPPUNIT_ASSERT(unchoked >= 1);
  }

  void testSeederChokeRoundCycle()
  {
    BtSeederStateChoke choke;
    PeerSet peerSet;
    MockBtMessageDispatcher dispatcher;
    auto peer = std::make_shared<Peer>("127.0.0.1", 6881);
    peer->allocateSessionResource(1_k, 1_m);
    peer->setBtMessageDispatcher(&dispatcher);
    peer->peerInterested(true);
    peerSet.insert(peer);
    // Rounds 0-2
    choke.executeChoke(peerSet);
    choke.executeChoke(peerSet);
    // Round 2: unchokes 4 instead of 3
    choke.executeChoke(peerSet);
    // After round 2, peer should not be choked (only 1 peer)
    CPPUNIT_ASSERT(!peer->chokingRequired());
  }

  void testParamedStringExpand()
  {
    std::vector<std::string> result;
    std::string src = "http://host/file";
    paramed_string::expand(src.begin(), src.end(), std::back_inserter(result));
    CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
    CPPUNIT_ASSERT_EQUAL(std::string("http://host/file"), result[0]);
  }

  void testParamedStringExpandLoop()
  {
    // Numeric loop
    {
      std::vector<std::string> result;
      std::string src = "file[1-3]";
      paramed_string::expand(src.begin(), src.end(),
                             std::back_inserter(result));
      CPPUNIT_ASSERT_EQUAL((size_t)3, result.size());
      CPPUNIT_ASSERT_EQUAL(std::string("file1"), result[0]);
      CPPUNIT_ASSERT_EQUAL(std::string("file2"), result[1]);
      CPPUNIT_ASSERT_EQUAL(std::string("file3"), result[2]);
    }
    // Numeric loop with step
    {
      std::vector<std::string> result;
      std::string src = "[1-10:5]";
      paramed_string::expand(src.begin(), src.end(),
                             std::back_inserter(result));
      CPPUNIT_ASSERT_EQUAL((size_t)2, result.size());
      CPPUNIT_ASSERT_EQUAL(std::string("1"), result[0]);
      CPPUNIT_ASSERT_EQUAL(std::string("6"), result[1]);
    }
    // Zero-padded numeric loop
    {
      std::vector<std::string> result;
      std::string src = "[01-03]";
      paramed_string::expand(src.begin(), src.end(),
                             std::back_inserter(result));
      CPPUNIT_ASSERT_EQUAL((size_t)3, result.size());
      CPPUNIT_ASSERT_EQUAL(std::string("01"), result[0]);
      CPPUNIT_ASSERT_EQUAL(std::string("02"), result[1]);
      CPPUNIT_ASSERT_EQUAL(std::string("03"), result[2]);
    }
    // Alpha loop
    {
      std::vector<std::string> result;
      std::string src = "[a-c]";
      paramed_string::expand(src.begin(), src.end(),
                             std::back_inserter(result));
      CPPUNIT_ASSERT_EQUAL((size_t)3, result.size());
      CPPUNIT_ASSERT_EQUAL(std::string("a"), result[0]);
      CPPUNIT_ASSERT_EQUAL(std::string("b"), result[1]);
      CPPUNIT_ASSERT_EQUAL(std::string("c"), result[2]);
    }
    // Uppercase alpha loop
    {
      std::vector<std::string> result;
      std::string src = "[A-C]";
      paramed_string::expand(src.begin(), src.end(),
                             std::back_inserter(result));
      CPPUNIT_ASSERT_EQUAL((size_t)3, result.size());
      CPPUNIT_ASSERT_EQUAL(std::string("A"), result[0]);
      CPPUNIT_ASSERT_EQUAL(std::string("B"), result[1]);
      CPPUNIT_ASSERT_EQUAL(std::string("C"), result[2]);
    }
  }

  void testParamedStringExpandChoice()
  {
    std::vector<std::string> result;
    std::string src = "pre{foo,bar}post";
    paramed_string::expand(src.begin(), src.end(), std::back_inserter(result));
    CPPUNIT_ASSERT_EQUAL((size_t)2, result.size());
    CPPUNIT_ASSERT_EQUAL(std::string("prefoopost"), result[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("prebarpost"), result[1]);
  }

  void testUnknownLengthPieceStorage_basic()
  {
    std::shared_ptr<DownloadContext> dctx(
        new DownloadContext(0, 0, "unknown.dat"));
    UnknownLengthPieceStorage ps(dctx);

    // Initial state: totalLength is 0
    CPPUNIT_ASSERT_EQUAL((int64_t)0, ps.getTotalLength());
    // Not finished
    CPPUNIT_ASSERT(!ps.downloadFinished());
    // No bitfield
    CPPUNIT_ASSERT_EQUAL((size_t)0, ps.getBitfieldLength());
    CPPUNIT_ASSERT(!ps.getBitfield());
    // filteredTotalLength delegates to totalLength
    CPPUNIT_ASSERT_EQUAL((int64_t)0, ps.getFilteredTotalLength());
    // Not selective, not end game
    CPPUNIT_ASSERT(!ps.isSelectiveDownloadingMode());
    CPPUNIT_ASSERT(!ps.isEndGame());
    // countInFlightPiece is 0
    CPPUNIT_ASSERT_EQUAL((size_t)0, ps.countInFlightPiece());
    // getInFlightPieces returns empty vector
    std::vector<std::shared_ptr<Piece>> pieces;
    ps.getInFlightPieces(pieces);
    CPPUNIT_ASSERT(pieces.empty());
    // hasPiece for index 0 is false (not finished yet)
    CPPUNIT_ASSERT(!ps.hasPiece(0));
    CPPUNIT_ASSERT(!ps.hasPiece(1));
    // isPieceUsed for index 0 is false (no piece checked out)
    CPPUNIT_ASSERT(!ps.isPieceUsed(0));
    // completedLength is 0 (no piece)
    CPPUNIT_ASSERT_EQUAL((int64_t)0, ps.getCompletedLength());
  }

  void testUnknownLengthPieceStorage_markTotalLengthSet()
  {
    std::shared_ptr<DownloadContext> dctx(
        new DownloadContext(0, 0, "unknown.dat"));
    UnknownLengthPieceStorage ps(dctx);

    // Get the single piece
    std::shared_ptr<Piece> piece = ps.getMissingPiece(0, nullptr, 0, 1);
    CPPUNIT_ASSERT(piece);
    // isPieceUsed should now be true for index 0
    CPPUNIT_ASSERT(ps.isPieceUsed(0));
    // Second call returns nullptr (only one piece)
    CPPUNIT_ASSERT(!ps.getMissingPiece(0, nullptr, 0, 2));

    // markAllPiecesDone sets downloadFinished
    ps.markAllPiecesDone();
    CPPUNIT_ASSERT(ps.downloadFinished());
    CPPUNIT_ASSERT(ps.allDownloadFinished());
    // After markAllPiecesDone, hasPiece(0) is true
    CPPUNIT_ASSERT(ps.hasPiece(0));
    // isPieceUsed(0) is false since piece_ was reset
    CPPUNIT_ASSERT(!ps.isPieceUsed(0));
    // getMissingPiece returns nullptr when finished
    CPPUNIT_ASSERT(!ps.getMissingPiece(0, nullptr, 0, 1));
  }

  void testUnknownLengthPieceStorage_hasMissingPiece()
  {
    std::shared_ptr<DownloadContext> dctx(
        new DownloadContext(0, 0, "unknown.dat"));
    UnknownLengthPieceStorage ps(dctx);

    // getMissingPiece(index, cuid) with index 0 returns piece
    std::shared_ptr<Piece> piece = ps.getMissingPiece(0, 1);
    CPPUNIT_ASSERT(piece);
    // getMissingPiece(index, cuid) with index != 0 returns nullptr
    CPPUNIT_ASSERT(!ps.getMissingPiece(1, 1));

    // getPiece(0) returns the checked-out piece
    std::shared_ptr<Piece> got = ps.getPiece(0);
    CPPUNIT_ASSERT(got);
    // getPiece(1) returns nullptr
    CPPUNIT_ASSERT(!ps.getPiece(1));

    // cancelPiece resets piece_
    ps.cancelPiece(piece, 1);
    CPPUNIT_ASSERT(!ps.isPieceUsed(0));

    // After cancel, can get piece again
    std::shared_ptr<Piece> piece2 = ps.getMissingPiece(0, 2);
    CPPUNIT_ASSERT(piece2);

    // getPiece(0) when piece_ is null returns a new empty piece
    ps.cancelPiece(piece2, 2);
    std::shared_ptr<Piece> fresh = ps.getPiece(0);
    CPPUNIT_ASSERT(fresh);
  }

  void testSegListBasic()
  {
    // Basic usage: add, hasNext, next, peek
    SegList<int> sl;
    sl.add(1, 4);
    sl.normalize();
    CPPUNIT_ASSERT(sl.hasNext());
    CPPUNIT_ASSERT_EQUAL(1, sl.peek());
    CPPUNIT_ASSERT_EQUAL(1, sl.next());
    CPPUNIT_ASSERT_EQUAL(2, sl.peek());
    CPPUNIT_ASSERT_EQUAL(2, sl.next());
    CPPUNIT_ASSERT_EQUAL(3, sl.next());
    CPPUNIT_ASSERT(!sl.hasNext());
    // When hasNext() is false, next() returns 0
    CPPUNIT_ASSERT_EQUAL(0, sl.next());
    // When hasNext() is false, peek() returns 0
    CPPUNIT_ASSERT_EQUAL(0, sl.peek());
  }

  void testSegListSingleRange()
  {
    SegList<int> sl;
    sl.add(1, 5);
    sl.normalize();
    std::vector<int> result;
    while (sl.hasNext()) {
      result.push_back(sl.next());
    }
    CPPUNIT_ASSERT_EQUAL((size_t)4, result.size());
    CPPUNIT_ASSERT_EQUAL(1, result[0]);
    CPPUNIT_ASSERT_EQUAL(2, result[1]);
    CPPUNIT_ASSERT_EQUAL(3, result[2]);
    CPPUNIT_ASSERT_EQUAL(4, result[3]);
  }

  void testSegListMultiRange()
  {
    SegList<int> sl;
    sl.add(1, 3);
    sl.add(10, 12);
    sl.normalize();
    std::vector<int> result;
    while (sl.hasNext()) {
      result.push_back(sl.next());
    }
    CPPUNIT_ASSERT_EQUAL((size_t)4, result.size());
    CPPUNIT_ASSERT_EQUAL(1, result[0]);
    CPPUNIT_ASSERT_EQUAL(2, result[1]);
    CPPUNIT_ASSERT_EQUAL(10, result[2]);
    CPPUNIT_ASSERT_EQUAL(11, result[3]);
  }

  void testSegListEmpty()
  {
    SegList<int> sl;
    // No ranges added
    CPPUNIT_ASSERT(!sl.hasNext());
    CPPUNIT_ASSERT_EQUAL(0, sl.next());
    CPPUNIT_ASSERT_EQUAL(0, sl.peek());

    // normalize on empty list is safe
    sl.normalize();
    CPPUNIT_ASSERT(!sl.hasNext());
  }

  void testSegListClear()
  {
    SegList<int> sl;
    sl.add(5, 10);
    sl.normalize();
    CPPUNIT_ASSERT(sl.hasNext());
    sl.clear();
    CPPUNIT_ASSERT(!sl.hasNext());
    CPPUNIT_ASSERT_EQUAL(0, sl.next());
    CPPUNIT_ASSERT_EQUAL(0, sl.peek());

    // After clear, can add new ranges
    sl.add(20, 22);
    sl.normalize();
    CPPUNIT_ASSERT(sl.hasNext());
    CPPUNIT_ASSERT_EQUAL(20, sl.next());
    CPPUNIT_ASSERT_EQUAL(21, sl.next());
    CPPUNIT_ASSERT(!sl.hasNext());
  }

  void testSegListNormalize()
  {
    // Overlapping ranges are merged
    {
      SegList<int> sl;
      sl.add(1, 5);
      sl.add(3, 8);
      sl.normalize();
      std::vector<int> result;
      while (sl.hasNext()) {
        result.push_back(sl.next());
      }
      CPPUNIT_ASSERT_EQUAL((size_t)7, result.size());
      CPPUNIT_ASSERT_EQUAL(1, result[0]);
      CPPUNIT_ASSERT_EQUAL(7, result[6]);
    }
    // Touching ranges are merged
    {
      SegList<int> sl;
      sl.add(1, 3);
      sl.add(3, 5);
      sl.normalize();
      std::vector<int> result;
      while (sl.hasNext()) {
        result.push_back(sl.next());
      }
      CPPUNIT_ASSERT_EQUAL((size_t)4, result.size());
      CPPUNIT_ASSERT_EQUAL(1, result[0]);
      CPPUNIT_ASSERT_EQUAL(4, result[3]);
    }
    // Unsorted ranges are sorted
    {
      SegList<int> sl;
      sl.add(10, 12);
      sl.add(1, 3);
      sl.normalize();
      std::vector<int> result;
      while (sl.hasNext()) {
        result.push_back(sl.next());
      }
      CPPUNIT_ASSERT_EQUAL((size_t)4, result.size());
      CPPUNIT_ASSERT_EQUAL(1, result[0]);
      CPPUNIT_ASSERT_EQUAL(2, result[1]);
      CPPUNIT_ASSERT_EQUAL(10, result[2]);
      CPPUNIT_ASSERT_EQUAL(11, result[3]);
    }
    // Invalid range (a >= b) is ignored by add
    {
      SegList<int> sl;
      sl.add(5, 5); // empty range
      sl.add(5, 3); // reversed range
      CPPUNIT_ASSERT(!sl.hasNext());
    }
    // Contained range is absorbed
    {
      SegList<int> sl;
      sl.add(1, 10);
      sl.add(3, 5);
      sl.normalize();
      std::vector<int> result;
      while (sl.hasNext()) {
        result.push_back(sl.next());
      }
      CPPUNIT_ASSERT_EQUAL((size_t)9, result.size());
      CPPUNIT_ASSERT_EQUAL(1, result[0]);
      CPPUNIT_ASSERT_EQUAL(9, result[8]);
    }
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(SmallClassCoverageTest);

} // namespace aria2
