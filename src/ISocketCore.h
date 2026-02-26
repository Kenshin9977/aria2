#ifndef D_ISOCKET_CORE_H
#define D_ISOCKET_CORE_H

#include "common.h"

#include <string>

#include "a2netcompat.h"
#include "a2io.h"

namespace aria2 {

class ISocketCore {
public:
  virtual ~ISocketCore() = default;

  // I/O operations
  virtual ssize_t writeData(const void* data, size_t len) = 0;
  virtual ssize_t writeData(const void* data, size_t len,
                            const std::string& host, uint16_t port) = 0;
  virtual void readData(void* data, size_t& len) = 0;
  virtual ssize_t readDataFrom(void* data, size_t len, Endpoint& sender) = 0;
  virtual ssize_t writeVector(a2iovec* iov, size_t iovcnt) = 0;

  // I/O readiness
  virtual bool isReadable(time_t timeout) = 0;
  virtual bool isWritable(time_t timeout) = 0;
  virtual bool wantRead() const = 0;
  virtual bool wantWrite() const = 0;

  // Socket state
  virtual sock_t getSockfd() const = 0;
  virtual bool isOpen() const = 0;
  virtual Endpoint getAddrInfo() const = 0;
  virtual Endpoint getPeerInfo() const = 0;
  virtual std::string getSocketError() const = 0;
  virtual size_t getRecvBufferedLength() const = 0;
  virtual int getAddressFamily() const = 0;

  // Connection control
  virtual void closeConnection() = 0;
  virtual void setBlockingMode() = 0;
  virtual void setNonBlockingMode() = 0;
  virtual void setTcpNodelay(bool f) = 0;
};

} // namespace aria2

#endif // D_ISOCKET_CORE_H
