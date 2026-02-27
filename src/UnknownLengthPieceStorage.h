/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#ifndef D_UNKNOWN_LENGTH_PIECE_STORAGE_H
#define D_UNKNOWN_LENGTH_PIECE_STORAGE_H

#include "FatalException.h"
#include "PieceStorage.h"

namespace aria2 {

class Option;
class DownloadContext;
class DiskWriterFactory;
class DirectDiskAdaptor;
class BitfieldMan;

class UnknownLengthPieceStorage : public PieceStorage {
private:
  std::shared_ptr<DownloadContext> downloadContext_;

  std::shared_ptr<DirectDiskAdaptor> diskAdaptor_;

  std::shared_ptr<DiskWriterFactory> diskWriterFactory_;

  int64_t totalLength_;

  std::unique_ptr<BitfieldMan> bitfield_;

  bool downloadFinished_;

  std::shared_ptr<Piece> piece_;

  void createBitfield();

public:
  UnknownLengthPieceStorage(
      const std::shared_ptr<DownloadContext>& downloadContext);

  ~UnknownLengthPieceStorage() override;

#ifdef ENABLE_BITTORRENT

  /**
   * Returns true if the peer has a piece that localhost doesn't have.
   * Otherwise returns false.
   */
  bool hasMissingPiece(const std::shared_ptr<Peer>& peer) override;

  void getMissingPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                       size_t minMissingBlocks,
                       const std::shared_ptr<Peer>& peer, cuid_t cuid) override;

  void getMissingPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                       size_t minMissingBlocks,
                       const std::shared_ptr<Peer>& peer,
                       const std::vector<size_t>& excludedIndexes,
                       cuid_t cuid) override;

  void getMissingFastPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                           size_t minMissingBlocks,
                           const std::shared_ptr<Peer>& peer,
                           cuid_t cuid) override;

  void getMissingFastPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                           size_t minMissingBlocks,
                           const std::shared_ptr<Peer>& peer,
                           const std::vector<size_t>& excludedIndexes,
                           cuid_t cuid) override;

  std::shared_ptr<Piece> getMissingPiece(const std::shared_ptr<Peer>& peer,
                                         cuid_t cuid) override;

  std::shared_ptr<Piece>
  getMissingPiece(const std::shared_ptr<Peer>& peer,
                  const std::vector<size_t>& excludedIndexes,
                  cuid_t cuid) override;
#endif // ENABLE_BITTORRENT

  bool hasMissingUnusedPiece() override;

  /**
   * Returns a missing piece if available. Otherwise returns 0;
   */
  std::shared_ptr<Piece> getMissingPiece(size_t minSplitSize,
                                         const unsigned char* ignoreBitfield,
                                         size_t length, cuid_t cuid) override;

  /**
   * Returns a missing piece whose index is index.
   * If a piece whose index is index is already acquired or currently used,
   * then returns 0.
   * Also returns 0 if any of missing piece is not available.
   */
  std::shared_ptr<Piece> getMissingPiece(size_t index, cuid_t cuid) override;

  /**
   * Returns the piece denoted by index.
   * No status of the piece is changed in this method.
   */
  std::shared_ptr<Piece> getPiece(size_t index) override;

  /**
   * Tells that the download of the specified piece completes.
   */
  void completePiece(const std::shared_ptr<Piece>& piece) override;

  /**
   * Tells that the download of the specified piece is canceled.
   */
  void cancelPiece(const std::shared_ptr<Piece>& piece, cuid_t cuid) override;

  /**
   * Returns true if the specified piece is already downloaded.
   * Otherwise returns false.
   */
  bool hasPiece(size_t index) override;

  bool isPieceUsed(size_t index) override;

  int64_t getTotalLength() override { return totalLength_; }

  int64_t getFilteredTotalLength() override { return totalLength_; }

  int64_t getCompletedLength() override;

  int64_t getFilteredCompletedLength() override { return getCompletedLength(); }

  void setupFileFilter() override {}

  void clearFileFilter() override {}

  /**
   * Returns true if download has completed.
   * If file filter is enabled, then returns true if those files have
   * downloaded.
   */
  bool downloadFinished() override { return downloadFinished_; }

  /**
   * Returns true if all files have downloaded.
   * The file filter is ignored.
   */
  bool allDownloadFinished() override { return downloadFinished(); }

  /**
   * Initializes DiskAdaptor.
   * TODO add better documentation here.
   */
  void initStorage() override;

  const unsigned char* getBitfield() override;

  void setBitfield(std::span<const unsigned char> bitfield) override {}

  size_t getBitfieldLength() override;

  bool isSelectiveDownloadingMode() override { return false; }

  bool isEndGame() override { return false; }

  void enterEndGame() override {}

  void setEndGamePieceNum(size_t num) override {}

  std::shared_ptr<DiskAdaptor> getDiskAdaptor() override;

  WrDiskCache* getWrDiskCache() override { return nullptr; }

  void flushWrDiskCacheEntry(bool releaseEntries) override {}

  int32_t getPieceLength(size_t index) override;

  void advertisePiece(cuid_t cuid, size_t index, Timer registeredTime) override
  {
  }

  /**
   * indexes is filled with piece index which is not advertised by the
   * caller command and newer than lastHaveIndex.
   */
  uint64_t getAdvertisedPieceIndexes(std::vector<size_t>& indexes,
                                     cuid_t myCuid,
                                     uint64_t lastHaveIndex) override
  {
    throw FATAL_EXCEPTION("Not Implemented!");
  }

  void removeAdvertisedPiece(const Timer& expiry) override {}

  /**
   * Sets all bits in bitfield to 1.
   */
  void markAllPiecesDone() override;

  void markPiecesDone(int64_t length) override;

  void markPieceMissing(size_t index) override;

  /**
   * Do nothing because loading in-flight piece is not supported for this
   * class.
   */
  void
  addInFlightPiece(const std::vector<std::shared_ptr<Piece>>& pieces) override
  {
  }

  size_t countInFlightPiece() override { return 0; }

  void getInFlightPieces(std::vector<std::shared_ptr<Piece>>& pieces) override;

  void addPieceStats(size_t index) override {}

  void addPieceStats(std::span<const unsigned char> bitfield) override {}

  void subtractPieceStats(std::span<const unsigned char> bitfield) override {}

  void updatePieceStats(std::span<const unsigned char> newBitfield,
                        std::span<const unsigned char> oldBitfield) override
  {
  }

  size_t getNextUsedIndex(size_t index) override { return 0; }

  void setDiskWriterFactory(
      const std::shared_ptr<DiskWriterFactory>& diskWriterFactory);

  void onDownloadIncomplete() override {}
};

} // namespace aria2

#endif // D_UNKNOWN_LENGTH_PIECE_STORAGE_H
