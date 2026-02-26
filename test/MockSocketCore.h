#ifndef D_MOCK_SOCKET_CORE_H
#define D_MOCK_SOCKET_CORE_H

#include "ISocketCore.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace aria2 {

class MockSocketCore : public ISocketCore {
public:
  // --- Controllable state ---

  bool readable = false;
  bool writable = false;
  bool open_ = true;
  bool wantRead_ = false;
  bool wantWrite_ = false;

  std::string readBuffer;
  size_t readPos = 0;
  std::vector<std::string> writtenData;

  Endpoint addrInfo = {"127.0.0.1", AF_INET, 0};
  Endpoint peerInfo = {"127.0.0.1", AF_INET, 80};

  sock_t sockfd = 100;
  std::string socketError;

  // --- ISocketCore I/O ---

  ssize_t writeData(const void* data, size_t len) override
  {
    writtenData.emplace_back(static_cast<const char*>(data), len);
    return static_cast<ssize_t>(len);
  }

  ssize_t writeData(const void* data, size_t len, const std::string& host,
                    uint16_t port) override
  {
    return writeData(data, len);
  }

  void readData(void* data, size_t& len) override
  {
    size_t available = readBuffer.size() - readPos;
    size_t toRead = std::min(len, available);
    if (toRead > 0) {
      std::memcpy(data, readBuffer.data() + readPos, toRead);
      readPos += toRead;
    }
    len = toRead;
  }

  ssize_t readDataFrom(void* data, size_t len, Endpoint& sender) override
  {
    readData(data, len);
    sender = peerInfo;
    return static_cast<ssize_t>(len);
  }

  ssize_t writeVector(a2iovec* iov, size_t iovcnt) override
  {
    ssize_t total = 0;
    for (size_t i = 0; i < iovcnt; ++i) {
      writtenData.emplace_back(static_cast<const char*>(iov[i].A2IOVEC_BASE),
                               iov[i].A2IOVEC_LEN);
      total += static_cast<ssize_t>(iov[i].A2IOVEC_LEN);
    }
    return total;
  }

  // --- I/O readiness ---

  bool isReadable(time_t timeout) override { return readable; }
  bool isWritable(time_t timeout) override { return writable; }
  bool wantRead() const override { return wantRead_; }
  bool wantWrite() const override { return wantWrite_; }

  // --- Socket state ---

  sock_t getSockfd() const override { return sockfd; }
  bool isOpen() const override { return open_; }
  Endpoint getAddrInfo() const override { return addrInfo; }
  Endpoint getPeerInfo() const override { return peerInfo; }
  std::string getSocketError() const override { return socketError; }
  size_t getRecvBufferedLength() const override
  {
    return readBuffer.size() - readPos;
  }
  int getAddressFamily() const override { return addrInfo.family; }

  // --- Connection control ---

  void closeConnection() override { open_ = false; }
  void setBlockingMode() override {}
  void setNonBlockingMode() override {}
  void setTcpNodelay(bool f) override {}
};

} // namespace aria2

#endif // D_MOCK_SOCKET_CORE_H
