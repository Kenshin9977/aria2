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
#include "IteratableChunkChecksumValidator.h"

#include <array>
#include <cstring>
#include <cstdlib>
#include <future>
#include <thread>

#include "util.h"
#include "message.h"
#include "DiskAdaptor.h"
#include "FileEntry.h"
#include "RecoverableException.h"
#include "DownloadContext.h"
#include "PieceStorage.h"
#include "BitfieldMan.h"
#include "LogFactory.h"
#include "Logger.h"
#include "MessageDigest.h"
#include "fmt.h"
#include "DlAbortEx.h"

namespace aria2 {

IteratableChunkChecksumValidator::IteratableChunkChecksumValidator(
    const std::shared_ptr<DownloadContext>& dctx,
    const std::shared_ptr<PieceStorage>& pieceStorage)
    : dctx_(dctx),
      pieceStorage_(pieceStorage),
      bitfield_(std::make_unique<BitfieldMan>(dctx_->getPieceLength(),
                                         dctx_->getTotalLength())),
      currentIndex_(0),
      batchSize_(std::max(1u, std::thread::hardware_concurrency()))
{
}

IteratableChunkChecksumValidator::~IteratableChunkChecksumValidator() = default;

void IteratableChunkChecksumValidator::validateChunk()
{
  if (finished()) {
    return;
  }

  size_t numPieces = dctx_->getNumPieces();
  size_t batchEnd = std::min(currentIndex_ + batchSize_, numPieces);
  size_t batchCount = batchEnd - currentIndex_;

  // Pre-read piece data sequentially (disk I/O is not thread-safe)
  struct PieceWork {
    size_t index;
    std::vector<unsigned char> data;
    bool readOk;
  };
  std::vector<PieceWork> pieces(batchCount);
  for (size_t i = 0; i < batchCount; ++i) {
    size_t idx = currentIndex_ + i;
    pieces[i].index = idx;
    pieces[i].readOk = true;
    try {
      pieces[i].data = readPiece(idx);
    }
    catch (RecoverableException& ex) {
      A2_LOG_DEBUG_EX(
          fmt("Caught exception while reading piece index=%lu."
              " Some part of file may be missing."
              " Continue operation.",
              static_cast<unsigned long>(idx)),
          ex);
      pieces[i].readOk = false;
    }
  }

  // Hash pieces in parallel
  const std::string& hashType = dctx_->getPieceHashType();
  std::vector<std::future<std::string>> futures;
  futures.reserve(batchCount);
  for (auto& p : pieces) {
    if (p.readOk) {
      futures.push_back(std::async(
          std::launch::async,
          [&hashType, &p]() {
            return hashData(hashType, p.data.data(), p.data.size());
          }));
    }
    else {
      std::promise<std::string> prom;
      prom.set_value(std::string());
      futures.push_back(prom.get_future());
    }
  }

  // Collect results and update bitfield
  for (size_t i = 0; i < batchCount; ++i) {
    size_t idx = pieces[i].index;
    if (!pieces[i].readOk) {
      bitfield_->unsetBit(idx);
    }
    else {
      std::string actualChecksum = futures[i].get();
      if (actualChecksum == dctx_->getPieceHashes()[idx]) {
        bitfield_->setBit(idx);
      }
      else {
        A2_LOG_INFO(
            fmt(EX_INVALID_CHUNK_CHECKSUM,
                static_cast<unsigned long>(idx),
                static_cast<int64_t>(idx) * dctx_->getPieceLength(),
                util::toHex(dctx_->getPieceHashes()[idx]).c_str(),
                util::toHex(actualChecksum).c_str()));
        bitfield_->unsetBit(idx);
      }
    }
  }

  currentIndex_ = batchEnd;
  if (finished()) {
    pieceStorage_->setBitfield(
        {bitfield_->getBitfield(), bitfield_->getBitfieldLength()});
  }
}

std::vector<unsigned char>
IteratableChunkChecksumValidator::readPiece(size_t index)
{
  int64_t offset = static_cast<int64_t>(index) * dctx_->getPieceLength();
  size_t length;
  if (index + 1 == dctx_->getNumPieces()) {
    length = dctx_->getTotalLength() - offset;
  }
  else {
    length = dctx_->getPieceLength();
  }

  std::vector<unsigned char> buf(length);
  size_t pos = 0;
  while (pos < length) {
    size_t r = pieceStorage_->getDiskAdaptor()->readDataDropCache(
        buf.data() + pos, length - pos, offset + pos);
    if (r == 0) {
      throw DL_ABORT_EX(fmt(EX_FILE_READ, dctx_->getBasePath().c_str(),
                            "data is too short"));
    }
    pos += r;
  }
  return buf;
}

std::string IteratableChunkChecksumValidator::hashData(
    const std::string& hashType, const unsigned char* data, size_t length)
{
  auto md = MessageDigest::create(hashType);
  md->reset();
  // Process in 4KB chunks to match original behavior
  size_t pos = 0;
  while (pos < length) {
    size_t chunkLen = std::min(static_cast<size_t>(4_k), length - pos);
    md->update(data + pos, chunkLen);
    pos += chunkLen;
  }
  return md->digest();
}

std::string IteratableChunkChecksumValidator::calculateActualChecksum()
{
  int64_t offset = getCurrentOffset();
  size_t length;
  if (currentIndex_ + 1 == dctx_->getNumPieces()) {
    length = dctx_->getTotalLength() - offset;
  }
  else {
    length = dctx_->getPieceLength();
  }
  return digest(offset, length);
}

void IteratableChunkChecksumValidator::init()
{
  ctx_ = MessageDigest::create(dctx_->getPieceHashType());
  bitfield_->clearAllBit();
  currentIndex_ = 0;
}

std::string IteratableChunkChecksumValidator::digest(int64_t offset,
                                                     size_t length)
{
  std::array<unsigned char, 4_k> buf;
  ctx_->reset();
  int64_t max = offset + length;
  while (offset < max) {
    size_t r = pieceStorage_->getDiskAdaptor()->readDataDropCache(
        buf.data(), std::min(static_cast<int64_t>(buf.size()), max - offset),
        offset);
    if (r == 0) {
      throw DL_ABORT_EX(
          fmt(EX_FILE_READ, dctx_->getBasePath().c_str(), "data is too short"));
    }
    ctx_->update(buf.data(), r);
    offset += r;
  }
  return ctx_->digest();
}

bool IteratableChunkChecksumValidator::finished() const
{
  if (currentIndex_ >= dctx_->getNumPieces()) {
    return true;
  }
  else {
    return false;
  }
}

int64_t IteratableChunkChecksumValidator::getCurrentOffset() const
{
  return static_cast<int64_t>(currentIndex_) * dctx_->getPieceLength();
}

int64_t IteratableChunkChecksumValidator::getTotalLength() const
{
  return dctx_->getTotalLength();
}

} // namespace aria2
