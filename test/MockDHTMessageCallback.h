#ifndef D_MOCK_DHT_MESSAGE_CALLBACK_H
#define D_MOCK_DHT_MESSAGE_CALLBACK_H

#include "DHTMessageCallback.h"

namespace aria2 {

class MockDHTMessageCallback : public DHTMessageCallback {
public:
  MockDHTMessageCallback() {}

  ~MockDHTMessageCallback() override {}

  void visit(const DHTAnnouncePeerReplyMessage* message) override {}

  void visit(const DHTFindNodeReplyMessage* message) override {}

  void visit(const DHTGetPeersReplyMessage* message) override {}

  void visit(const DHTPingReplyMessage* message) override {}

  void onTimeout(const std::shared_ptr<DHTNode>& remoteNode) override {}
};

} // namespace aria2

#endif // D_MOCK_DHT_MESSAGE_CALLBACK_H
