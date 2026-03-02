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

#include <fstream>

#include <cppunit/extensions/HelperMacros.h>

#include "Exception.h"
#include "util.h"
#include "Segment.h"
#include "ByteArrayDiskWriter.h"
#include "SinkStreamFilter.h"
#include "MockSegment.h"
#include "MessageDigest.h"

namespace aria2 {

class ZstdDecodingStreamFilterTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(ZstdDecodingStreamFilterTest);
  CPPUNIT_TEST(testTransform);
  CPPUNIT_TEST_SUITE_END();

  class MockSegment2 : public MockSegment {
  private:
    int64_t positionToWrite_;

  public:
    MockSegment2() : positionToWrite_(0) {}

    virtual void updateWrittenLength(int64_t bytes) override
    {
      positionToWrite_ += bytes;
    }

    virtual int64_t getPositionToWrite() const override
    {
      return positionToWrite_;
    }
  };

  std::unique_ptr<ZstdDecodingStreamFilter> filter_;
  std::shared_ptr<ByteArrayDiskWriter> writer_;
  std::shared_ptr<MockSegment2> segment_;

public:
  void setUp()
  {
    writer_ = std::make_shared<ByteArrayDiskWriter>();
    auto sinkFilter = std::make_unique<SinkStreamFilter>();
    sinkFilter->init();
    filter_ = std::make_unique<ZstdDecodingStreamFilter>(
        std::move(sinkFilter));
    filter_->init();
    segment_ = std::make_shared<MockSegment2>();
  }

  void testTransform();
};

CPPUNIT_TEST_SUITE_REGISTRATION(ZstdDecodingStreamFilterTest);

void ZstdDecodingStreamFilterTest::testTransform()
{
  unsigned char buf[4_k];
  std::ifstream in(A2_TEST_DIR "/zstd_decode_test.zst",
                   std::ios::binary);
  while (in) {
    in.read(reinterpret_cast<char*>(buf), sizeof(buf));
    filter_->transform(writer_, segment_, buf, in.gcount());
  }
  CPPUNIT_ASSERT(filter_->finished());
  std::string data = writer_->getString();
  std::shared_ptr<MessageDigest> sha1(MessageDigest::sha1());
  sha1->update(data.data(), data.size());
  CPPUNIT_ASSERT_EQUAL(
      std::string("8b577b33c0411b2be9d4fa74c7402d54a8d21f96"),
      util::toHex(sha1->digest()));
}

} // namespace aria2
