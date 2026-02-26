/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
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
