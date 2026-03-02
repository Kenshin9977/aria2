#ifndef D_MOCK_BT_MESSAGE_DISPATCHER_H
#define D_MOCK_BT_MESSAGE_DISPATCHER_H

#include "BtMessageDispatcher.h"

#include <algorithm>

#include "BtMessage.h"
#include "Piece.h"

namespace aria2 {

class MockBtMessageDispatcher : public BtMessageDispatcher {
public:
  std::deque<std::unique_ptr<BtMessage>> messageQueue;

  ~MockBtMessageDispatcher() override {}

  void addMessageToQueue(std::unique_ptr<BtMessage> btMessage) override
  {
    messageQueue.push_back(std::move(btMessage));
  }

  void sendMessages() override {}

  void doCancelSendingPieceAction(size_t index, int32_t begin,
                                  int32_t length) override
  {
  }

  void doCancelSendingPieceAction(const std::shared_ptr<Piece>& piece) override
  {
  }

  void
  doAbortOutstandingRequestAction(const std::shared_ptr<Piece>& piece) override
  {
  }

  void doChokedAction() override {}

  void doChokingAction() override {}

  void checkRequestSlotAndDoNecessaryThing() override {}

  bool isSendingInProgress() override { return false; }

  size_t countMessageInQueue() override { return messageQueue.size(); }

  size_t countOutstandingRequest() override { return 0; }

  bool isOutstandingRequest(size_t index, size_t blockIndex) override
  {
    return false;
  }

  const RequestSlot* getOutstandingRequest(size_t index, int32_t begin,
                                           int32_t length) override
  {
    return nullptr;
  }

  void removeOutstandingRequest(const RequestSlot* slot) override {}

  void addOutstandingRequest(std::unique_ptr<RequestSlot> slot) override {}

  size_t countOutstandingUpload() override { return 0; }
};

} // namespace aria2

#endif // D_MOCK_BT_MESSAGE_DISPATCHER_H
