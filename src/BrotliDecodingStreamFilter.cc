/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2024 Tatsuhiro Tsujikawa
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
#include "BrotliDecodingStreamFilter.h"

#include <cassert>

#include "DlAbortEx.h"

namespace aria2 {

constexpr const char BrotliDecodingStreamFilter::NAME[];

BrotliDecodingStreamFilter::BrotliDecodingStreamFilter(
    std::unique_ptr<StreamFilter> delegate)
    : StreamFilter{std::move(delegate)},
      decoder_{nullptr},
      finished_{false},
      bytesProcessed_{0}
{
}

BrotliDecodingStreamFilter::~BrotliDecodingStreamFilter() { release(); }

void BrotliDecodingStreamFilter::init()
{
  finished_ = false;
  release();
  decoder_ = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
  if (!decoder_) {
    throw DL_ABORT_EX("Initializing BrotliDecoderState failed.");
  }
}

void BrotliDecodingStreamFilter::release()
{
  if (decoder_) {
    BrotliDecoderDestroyInstance(decoder_);
    decoder_ = nullptr;
  }
}

ssize_t
BrotliDecodingStreamFilter::transform(const std::shared_ptr<BinaryStream>& out,
                                      const std::shared_ptr<Segment>& segment,
                                      const unsigned char* inbuf, size_t inlen)
{
  bytesProcessed_ = 0;
  ssize_t outlen = 0;
  if (inlen == 0) {
    return outlen;
  }

  size_t availIn = inlen;
  const uint8_t* nextIn = inbuf;

  unsigned char outbuf[OUTBUF_LENGTH];
  while (1) {
    size_t availOut = OUTBUF_LENGTH;
    uint8_t* nextOut = outbuf;

    auto result = BrotliDecoderDecompressStream(decoder_, &availIn, &nextIn,
                                                &availOut, &nextOut, nullptr);

    if (result == BROTLI_DECODER_RESULT_SUCCESS) {
      finished_ = true;
    }
    else if (result == BROTLI_DECODER_RESULT_ERROR) {
      throw DL_ABORT_EX("brotli decompression failed.");
    }

    size_t produced = OUTBUF_LENGTH - availOut;

    outlen += getDelegate()->transform(out, segment, outbuf, produced);
    if (availOut > 0) {
      break;
    }
  }
  assert(inlen >= availIn);
  bytesProcessed_ = inlen - availIn;
  return outlen;
}

bool BrotliDecodingStreamFilter::finished()
{
  return finished_ && getDelegate()->finished();
}

const char* BrotliDecodingStreamFilter::getName() const { return NAME; }

} // namespace aria2
