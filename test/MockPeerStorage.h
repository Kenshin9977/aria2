#ifndef D_MOCK_PEER_STORAGE_H
#  define D_MOCK_PEER_STORAGE_H

#  include "PeerStorage.h"

#  include <algorithm>

#  include "Peer.h"

namespace aria2 {

class MockPeerStorage : public PeerStorage {
private:
  std::deque<std::shared_ptr<Peer>> unusedPeers;
  PeerSet usedPeers;
  std::deque<std::shared_ptr<Peer>> droppedPeers;
  std::vector<std::shared_ptr<Peer>> activePeers;
  int numChokeExecuted_;

public:
  MockPeerStorage() : numChokeExecuted_(0) {}
  ~MockPeerStorage() override {}

  bool addPeer(const std::shared_ptr<Peer>& peer) override
  {
    unusedPeers.push_back(peer);
    return true;
  }

  void addPeer(const std::vector<std::shared_ptr<Peer>>& peers) override
  {
    unusedPeers.insert(unusedPeers.end(), peers.begin(), peers.end());
  }

  const std::deque<std::shared_ptr<Peer>>& getUnusedPeers()
  {
    return unusedPeers;
  }

  std::shared_ptr<Peer> addAndCheckoutPeer(const std::shared_ptr<Peer>& peer,
                                           cuid_t cuid) override
  {
    unusedPeers.push_back(peer);
    return nullptr;
  }

  size_t countAllPeer() const override
  {
    return unusedPeers.size() + usedPeers.size();
  }

  const std::deque<std::shared_ptr<Peer>>& getDroppedPeers() override
  {
    return droppedPeers;
  }

  void addDroppedPeer(const std::shared_ptr<Peer>& peer)
  {
    droppedPeers.push_back(peer);
  }

  bool isPeerAvailable() override { return false; }

  void setActivePeers(const std::vector<std::shared_ptr<Peer>>& activePeers)
  {
    this->activePeers = activePeers;
  }

  void getActivePeers(std::vector<std::shared_ptr<Peer>>& peers)
  {
    peers.insert(peers.end(), activePeers.begin(), activePeers.end());
  }

  const PeerSet& getUsedPeers() override { return usedPeers; }

  bool isBadPeer(const std::string& ipaddr) override { return false; }

  void addBadPeer(const std::string& ipaddr) override {}

  std::shared_ptr<Peer> checkoutPeer(cuid_t cuid) override { return nullptr; }

  void returnPeer(const std::shared_ptr<Peer>& peer) override {}

  bool chokeRoundIntervalElapsed() override { return false; }

  void executeChoke() override { ++numChokeExecuted_; }

  int getNumChokeExecuted() const { return numChokeExecuted_; }
};

#endif // D_MOCK_PEER_STORAGE_H

} // namespace aria2
