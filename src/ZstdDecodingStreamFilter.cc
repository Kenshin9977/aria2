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
#include "ZstdDecodingStreamFilter.h"

#include <cassert>

#include "fmt.h"
#include "DlAbortEx.h"

namespace aria2 {

constexpr const char ZstdDecodingStreamFilter::NAME[];

ZstdDecodingStreamFilter::ZstdDecodingStreamFilter(
    std::unique_ptr<StreamFilter> delegate)
    : StreamFilter{std::move(delegate)},
      dstream_{nullptr},
      finished_{false},
      bytesProcessed_{0}
{
}

ZstdDecodingStreamFilter::~ZstdDecodingStreamFilter() { release(); }

void ZstdDecodingStreamFilter::init()
{
  finished_ = false;
  release();
  dstream_ = ZSTD_createDStream();
  if (!dstream_) {
    throw DL_ABORT_EX("Initializing ZSTD_DStream failed.");
  }
  size_t ret = ZSTD_initDStream(dstream_);
  if (ZSTD_isError(ret)) {
    throw DL_ABORT_EX(
        fmt("ZSTD_initDStream failed: %s", ZSTD_getErrorName(ret)));
  }
}

void ZstdDecodingStreamFilter::release()
{
  if (dstream_) {
    ZSTD_freeDStream(dstream_);
    dstream_ = nullptr;
  }
}

ssize_t
ZstdDecodingStreamFilter::transform(const std::shared_ptr<BinaryStream>& out,
                                    const std::shared_ptr<Segment>& segment,
                                    const unsigned char* inbuf, size_t inlen)
{
  bytesProcessed_ = 0;
  ssize_t outlen = 0;
  if (inlen == 0) {
    return outlen;
  }

  ZSTD_inBuffer input = {inbuf, inlen, 0};

  unsigned char outbuf[OUTBUF_LENGTH];
  while (input.pos < input.size) {
    ZSTD_outBuffer output = {outbuf, OUTBUF_LENGTH, 0};

    size_t ret = ZSTD_decompressStream(dstream_, &output, &input);

    if (ZSTD_isError(ret)) {
      throw DL_ABORT_EX(
          fmt("ZSTD_decompressStream failed: %s", ZSTD_getErrorName(ret)));
    }

    if (ret == 0) {
      finished_ = true;
    }

    outlen += getDelegate()->transform(out, segment, outbuf, output.pos);
    if (output.pos < output.size) {
      break;
    }
  }
  bytesProcessed_ = input.pos;
  return outlen;
}

bool ZstdDecodingStreamFilter::finished()
{
  return finished_ && getDelegate()->finished();
}

const char* ZstdDecodingStreamFilter::getName() const { return NAME; }

} // namespace aria2
