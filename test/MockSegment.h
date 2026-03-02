#ifndef D_MOCK_SEGMENT_H
#define D_MOCK_SEGMENT_H

#include "Segment.h"
#include "Piece.h"
#include "A2STR.h"

namespace aria2 {

class MockSegment : public Segment {
public:
  bool complete() const override { return false; }

  size_t getIndex() const override { return 0; }

  int64_t getPosition() const override { return 0; }

  int64_t getPositionToWrite() const override { return 0; }

  int64_t getLength() const override { return 0; }

  int64_t getSegmentLength() const override { return 0; }

  int64_t getWrittenLength() const override { return 0; }

  void updateWrittenLength(int64_t bytes) override {}

  // `begin' is a offset inside this segment.
  bool updateHash(int64_t begin, const unsigned char* data,
                  size_t dataLength) override
  {
    return false;
  }

  bool isHashCalculated() const override { return false; }

  std::string getDigest() override { return A2STR::NIL; }

  void clear(WrDiskCache* diskCache) override {}

  std::shared_ptr<Piece> getPiece() const override
  {
    return std::shared_ptr<Piece>(new Piece());
  }
};

} // namespace aria2

#endif // D_MOCK_SEGMENT_H
