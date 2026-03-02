#ifndef D_MOCK_PIECE_STORAGE_H
#define D_MOCK_PIECE_STORAGE_H

#include "PieceStorage.h"

#include <algorithm>
#include <span>

#include "BitfieldMan.h"
#include "FatalException.h"
#include "Piece.h"
#include "DiskAdaptor.h"

namespace aria2 {

class MockPieceStorage : public PieceStorage {
private:
  int64_t totalLength;
  int64_t filteredTotalLength;
  int64_t completedLength;
  int64_t filteredCompletedLength;
  BitfieldMan* bitfieldMan;
  bool selectiveDownloadingMode;
  bool endGame;
  std::shared_ptr<DiskAdaptor> diskAdaptor;
  std::deque<int32_t> pieceLengthList;
  std::deque<std::shared_ptr<Piece>> inFlightPieces;
  bool downloadFinished_;
  bool allDownloadFinished_;

public:
  MockPieceStorage()
      : totalLength(0),
        filteredTotalLength(0),
        completedLength(0),
        filteredCompletedLength(0),
        bitfieldMan(0),
        selectiveDownloadingMode(false),
        endGame(false),
        downloadFinished_(false),
        allDownloadFinished_(false)
  {
  }

  ~MockPieceStorage() override {}

#ifdef ENABLE_BITTORRENT

  bool hasMissingPiece(const std::shared_ptr<Peer>& peer) override
  {
    return false;
  }

  void getMissingPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                       size_t minMissingBlocks,
                       const std::shared_ptr<Peer>& peer, cuid_t cuid) override
  {
  }

  void getMissingPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                       size_t minMissingBlocks,
                       const std::shared_ptr<Peer>& peer,
                       const std::vector<size_t>& excludedIndexes,
                       cuid_t cuid) override
  {
  }

  void getMissingFastPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                           size_t minMissingBlocks,
                           const std::shared_ptr<Peer>& peer,
                           cuid_t cuid) override
  {
  }

  void getMissingFastPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                           size_t minMissingBlocks,
                           const std::shared_ptr<Peer>& peer,
                           const std::vector<size_t>& excludedIndexes,
                           cuid_t cuid) override
  {
  }

  std::shared_ptr<Piece> getMissingPiece(const std::shared_ptr<Peer>& peer,
                                         cuid_t cuid) override
  {
    return std::shared_ptr<Piece>(new Piece());
  }

  std::shared_ptr<Piece>
  getMissingPiece(const std::shared_ptr<Peer>& peer,
                  const std::vector<size_t>& excludedIndexes,
                  cuid_t cuid) override
  {
    return std::shared_ptr<Piece>(new Piece());
  }

#endif // ENABLE_BITTORRENT

  bool hasMissingUnusedPiece() override { return false; }

  std::shared_ptr<Piece> getMissingPiece(size_t minSplitSize,
                                         const unsigned char* ignoreBitfield,
                                         size_t length, cuid_t cuid) override
  {
    return std::shared_ptr<Piece>(new Piece());
  }

  std::shared_ptr<Piece> getMissingPiece(size_t index, cuid_t cuid) override
  {
    return std::shared_ptr<Piece>(new Piece());
  }

  bool isPieceUsed(size_t index) override { return false; }

  void markPieceMissing(size_t index) override {}

  void markPiecesDone(int64_t) override {}

  std::shared_ptr<Piece> getPiece(size_t index) override
  {
    return std::shared_ptr<Piece>(new Piece());
  }

  void completePiece(const std::shared_ptr<Piece>& piece) override {}

  void cancelPiece(const std::shared_ptr<Piece>& piece, cuid_t cuid) override {}

  bool hasPiece(size_t index) override { return false; }

  int64_t getTotalLength() override { return totalLength; }

  void setTotalLength(int64_t totalLength) { this->totalLength = totalLength; }

  int64_t getFilteredTotalLength() override { return filteredTotalLength; }

  void setFilteredTotalLength(int64_t totalLength)
  {
    this->filteredTotalLength = totalLength;
  }

  int64_t getCompletedLength() override { return completedLength; }

  void setCompletedLength(int64_t completedLength)
  {
    this->completedLength = completedLength;
  }

  int64_t getFilteredCompletedLength() override
  {
    return filteredCompletedLength;
  }

  void setFilteredCompletedLength(int64_t completedLength)
  {
    this->filteredCompletedLength = completedLength;
  }

  void setupFileFilter() override {}

  void clearFileFilter() override {}

  bool downloadFinished() override { return downloadFinished_; }

  void setDownloadFinished(bool f) { downloadFinished_ = f; }

  bool allDownloadFinished() override { return allDownloadFinished_; }

  void setAllDownloadFinished(bool f) { allDownloadFinished_ = f; }

  void initStorage() override {}

  const unsigned char* getBitfield() override
  {
    return bitfieldMan->getBitfield();
  }

  void setBitfield(std::span<const unsigned char> bitfield) override
  {
    bitfieldMan->setBitfield(bitfield);
  }

  size_t getBitfieldLength() override
  {
    return bitfieldMan->getBitfieldLength();
  }

  void setBitfield(BitfieldMan* bitfieldMan)
  {
    this->bitfieldMan = bitfieldMan;
  }

  bool isSelectiveDownloadingMode() override
  {
    return selectiveDownloadingMode;
  }

  void setSelectiveDownloadingMode(bool flag)
  {
    this->selectiveDownloadingMode = flag;
  }

  bool isEndGame() override { return endGame; }

  void setEndGamePieceNum(size_t num) override {}

  void enterEndGame() override { this->endGame = true; }

  std::shared_ptr<DiskAdaptor> getDiskAdaptor() override { return diskAdaptor; }

  WrDiskCache* getWrDiskCache() override { return 0; }

  void flushWrDiskCacheEntry(bool releaseEntries) override {}

  void setDiskAdaptor(const std::shared_ptr<DiskAdaptor>& adaptor)
  {
    this->diskAdaptor = adaptor;
  }

  int32_t getPieceLength(size_t index) override
  {
    return pieceLengthList.at(index);
  }

  void addPieceLengthList(int32_t length) { pieceLengthList.push_back(length); }

  void advertisePiece(cuid_t cuid, size_t index, Timer registeredTime) override
  {
  }

  uint64_t getAdvertisedPieceIndexes(std::vector<size_t>& indexes,
                                     cuid_t myCuid,
                                     uint64_t lastHaveIndex) override
  {
    throw FATAL_EXCEPTION("Not Implemented!");
  }

  void removeAdvertisedPiece(const Timer& expiry) override {}

  void markAllPiecesDone() override {}

  void
  addInFlightPiece(const std::vector<std::shared_ptr<Piece>>& pieces) override
  {
    std::copy(pieces.begin(), pieces.end(), back_inserter(inFlightPieces));
  }

  size_t countInFlightPiece() override { return inFlightPieces.size(); }

  void getInFlightPieces(std::vector<std::shared_ptr<Piece>>& pieces) override
  {
    pieces.insert(pieces.end(), inFlightPieces.begin(), inFlightPieces.end());
  }

  void addPieceStats(size_t index) override {}

  void addPieceStats(std::span<const unsigned char> bitfield) override {}

  void subtractPieceStats(std::span<const unsigned char> bitfield) override {}

  void updatePieceStats(std::span<const unsigned char> newBitfield,
                        std::span<const unsigned char> oldBitfield) override
  {
  }

  size_t getNextUsedIndex(size_t index) override { return 0; }

  void onDownloadIncomplete() override {}
};

} // namespace aria2

#endif // D_MOCK_PIECE_STORAGE_H
