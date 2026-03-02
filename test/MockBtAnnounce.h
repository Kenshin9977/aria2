#ifndef D_MOCK_BT_ANNOUNCE_H
#define D_MOCK_BT_ANNOUNCE_H

#include "BtAnnounce.h"

namespace aria2 {

class MockBtAnnounce : public BtAnnounce {
private:
  bool announceReady;
  std::string announceUrl;
  std::string peerId;

public:
  MockBtAnnounce() {}
  ~MockBtAnnounce() override {}

  bool isAnnounceReady() override { return announceReady; }

  void setAnnounceReady(bool flag) { this->announceReady = flag; }

  std::string getAnnounceUrl() override { return announceUrl; }

  std::shared_ptr<UDPTrackerRequest>
  createUDPTrackerRequest(const std::string& remoteAddr, uint16_t remotePort,
                          uint16_t localPort) override
  {
    return nullptr;
  }

  void setAnnounceUrl(const std::string& url) { this->announceUrl = url; }

  void announceStart() override {}

  void announceSuccess() override {}

  void announceFailure() override {}

  bool isAllAnnounceFailed() override { return false; }

  void resetAnnounce() override {}

  void processAnnounceResponse(const unsigned char* trackerResponse,
                               size_t trackerResponseLength) override
  {
  }

  void processUDPTrackerResponse(
      const std::shared_ptr<UDPTrackerRequest>& req) override
  {
  }

  bool noMoreAnnounce() override { return false; }

  void shuffleAnnounce() override {}

  void overrideMinInterval(std::chrono::seconds interval) override {}

  void setTcpPort(uint16_t port) override {}

  void setPeerId(const std::string& peerId) { this->peerId = peerId; }
};

} // namespace aria2

#endif // D_MOCK_BT_ANNOUNCE_H
