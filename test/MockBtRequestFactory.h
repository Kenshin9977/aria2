#ifndef D_MOCK_BT_REQUEST_FACTORY_H
#define D_MOCK_BT_REQUEST_FACTORY_H

#include "BtRequestFactory.h"
#include "BtRequestMessage.h"

namespace aria2 {

class MockBtRequestFactory : public BtRequestFactory {
public:
  ~MockBtRequestFactory() override {}

  void addTargetPiece(const std::shared_ptr<Piece>& piece) override {}

  void removeTargetPiece(const std::shared_ptr<Piece>& piece) override {}

  void removeAllTargetPiece() override {}

  size_t countTargetPiece() override { return 0; }

  size_t countMissingBlock() override { return 0; }

  void removeCompletedPiece() override {}

  void doChokedAction() override {}

  std::vector<std::unique_ptr<BtRequestMessage>>
  createRequestMessages(size_t max, bool endGame) override
  {
    return std::vector<std::unique_ptr<BtRequestMessage>>{};
  }

  std::vector<size_t> getTargetPieceIndexes() const override
  {
    return std::vector<size_t>{};
  }
};

} // namespace aria2

#endif // D_MOCK_BT_REQUEST_FACTORY_H
