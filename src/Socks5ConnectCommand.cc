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
#include "Socks5ConnectCommand.h"

#include "ISocketCore.h"
#include "Request.h"
#include "DownloadEngine.h"
#include "DlAbortEx.h"
#include "Logger.h"
#include "LogFactory.h"
#include "fmt.h"
#include "SocketRecvBuffer.h"
#include "HttpConnection.h"
#include "HttpRequestCommand.h"
#include "FtpNegotiationCommand.h"
#include "util.h"
#ifdef HAVE_LIBSSH2
#  include "SftpNegotiationCommand.h"
#endif // HAVE_LIBSSH2

namespace aria2 {

namespace {
const uint8_t SOCKS5_VERSION = 0x05;
const uint8_t CMD_CONNECT = 0x01;
const uint8_t ATYP_IPV4 = 0x01;
const uint8_t ATYP_DOMAIN = 0x03;
const uint8_t ATYP_IPV6 = 0x04;
const uint8_t REP_SUCCESS = 0x00;
} // namespace

Socks5ConnectCommand::Socks5ConnectCommand(
    cuid_t cuid, const std::shared_ptr<Request>& req,
    const std::shared_ptr<FileEntry>& fileEntry, RequestGroup* requestGroup,
    DownloadEngine* e, const std::shared_ptr<Request>& proxyRequest,
    const std::shared_ptr<ISocketCore>& s)
    : AbstractCommand(cuid, req, fileEntry, requestGroup, e, s),
      state_(CONNECT_WRITE),
      proxyRequest_(proxyRequest),
      addrLen_(0)
{
  buildConnectRequest();
  disableReadCheckSocket();
  setWriteCheckSocket(getSocket());
}

Socks5ConnectCommand::~Socks5ConnectCommand() = default;

void Socks5ConnectCommand::buildConnectRequest()
{
  const std::string& host = getRequest()->getHost();
  uint16_t port = getRequest()->getPort();

  writeBuf_.clear();
  writeBuf_ += static_cast<char>(SOCKS5_VERSION);
  writeBuf_ += static_cast<char>(CMD_CONNECT);
  writeBuf_ += static_cast<char>(0x00); // RSV

  // Use domain name type — let the SOCKS proxy resolve DNS
  writeBuf_ += static_cast<char>(ATYP_DOMAIN);
  writeBuf_ += static_cast<char>(host.size());
  writeBuf_ += host;

  // Port in network byte order (big endian)
  writeBuf_ += static_cast<char>((port >> 8) & 0xFF);
  writeBuf_ += static_cast<char>(port & 0xFF);
}

bool Socks5ConnectCommand::doWrite()
{
  if (writeBuf_.empty()) {
    return true;
  }
  ssize_t written =
      getSocket()->writeData(writeBuf_.data(), writeBuf_.size());
  if (written > 0) {
    writeBuf_.erase(0, written);
  }
  return writeBuf_.empty();
}

bool Socks5ConnectCommand::doRead(size_t expectedLen)
{
  unsigned char buf[256];
  size_t toRead = expectedLen - readBuf_.size();
  if (toRead == 0) {
    return true;
  }
  size_t len = toRead;
  getSocket()->readData(buf, len);
  if (len > 0) {
    readBuf_.append(reinterpret_cast<char*>(buf), len);
  }
  return readBuf_.size() >= expectedLen;
}

std::unique_ptr<Command> Socks5ConnectCommand::createNextCommand()
{
  Protocol protocol = getRequest()->getProtocol();

  if (protocol == Protocol::HTTP || protocol == Protocol::HTTPS) {
    auto b = std::make_shared<SocketRecvBuffer>(getSocket());
    auto k = std::make_shared<HttpConnection>(getCuid(), getSocket(), b);
    return make_unique<HttpRequestCommand>(
        getCuid(), getRequest(), getFileEntry(), getRequestGroup(), k,
        getDownloadEngine(), getSocket());
  }

  if (protocol == Protocol::FTP) {
    return make_unique<FtpNegotiationCommand>(
        getCuid(), getRequest(), getFileEntry(), getRequestGroup(),
        getDownloadEngine(), getSocket(),
        FtpNegotiationCommand::SEQ_RECV_GREETING);
  }

#ifdef HAVE_LIBSSH2
  if (protocol == Protocol::SFTP) {
    return make_unique<SftpNegotiationCommand>(
        getCuid(), getRequest(), getFileEntry(), getRequestGroup(),
        getDownloadEngine(), getSocket(),
        SftpNegotiationCommand::SEQ_HANDSHAKE);
  }
#endif // HAVE_LIBSSH2

  throw DL_ABORT_EX(fmt("SOCKS5: unsupported protocol for tunneling"));
}

bool Socks5ConnectCommand::executeInternal()
{
  switch (state_) {
  case CONNECT_WRITE:
    if (doWrite()) {
      state_ = CONNECT_READ_HEADER;
      readBuf_.clear();
      disableWriteCheckSocket();
      setReadCheckSocket(getSocket());
    }
    else {
      setWriteCheckSocket(getSocket());
      addCommandSelf();
      return false;
    }
    // fall through
  case CONNECT_READ_HEADER:
    // Read first 4 bytes: VER REP RSV ATYP
    if (!doRead(4)) {
      addCommandSelf();
      return false;
    }
    {
      uint8_t ver = static_cast<uint8_t>(readBuf_[0]);
      uint8_t rep = static_cast<uint8_t>(readBuf_[1]);
      uint8_t atyp = static_cast<uint8_t>(readBuf_[3]);

      if (ver != SOCKS5_VERSION) {
        throw DL_ABORT_EX(
            fmt("SOCKS5: unexpected version %d", static_cast<int>(ver)));
      }
      if (rep != REP_SUCCESS) {
        const char* msg;
        switch (rep) {
        case 0x01:
          msg = "general SOCKS server failure";
          break;
        case 0x02:
          msg = "connection not allowed by ruleset";
          break;
        case 0x03:
          msg = "network unreachable";
          break;
        case 0x04:
          msg = "host unreachable";
          break;
        case 0x05:
          msg = "connection refused";
          break;
        case 0x06:
          msg = "TTL expired";
          break;
        case 0x07:
          msg = "command not supported";
          break;
        case 0x08:
          msg = "address type not supported";
          break;
        default:
          msg = "unknown error";
          break;
        }
        throw DL_ABORT_EX(
            fmt("SOCKS5: CONNECT failed: %s (0x%02x)", msg, rep));
      }

      // Determine remaining bytes to read based on address type
      switch (atyp) {
      case ATYP_IPV4:
        addrLen_ = 4 + 2; // 4 bytes IPv4 + 2 bytes port
        break;
      case ATYP_IPV6:
        addrLen_ = 16 + 2; // 16 bytes IPv6 + 2 bytes port
        break;
      case ATYP_DOMAIN: {
        // Need to read 1 more byte for domain length, then domain + 2
        // bytes port. For now, set to read 1 byte, then we'll adjust.
        addrLen_ = 1;
        break;
      }
      default:
        throw DL_ABORT_EX(fmt("SOCKS5: unsupported address type %d",
                              static_cast<int>(atyp)));
      }
    }
    state_ = CONNECT_READ_ADDR;
    // fall through
  case CONNECT_READ_ADDR: {
    uint8_t atyp = static_cast<uint8_t>(readBuf_[3]);
    if (atyp == ATYP_DOMAIN && readBuf_.size() == 4) {
      // Need to read the domain length byte first
      if (!doRead(5)) {
        addCommandSelf();
        return false;
      }
      uint8_t dlen = static_cast<uint8_t>(readBuf_[4]);
      addrLen_ = 1 + dlen + 2; // length byte + domain + port
    }
    // Read remaining address + port bytes
    if (!doRead(4 + addrLen_)) {
      addCommandSelf();
      return false;
    }
    break;
  }
  }

  // SOCKS5 CONNECT succeeded — socket is now tunneled
  A2_LOG_INFO(fmt("CUID#%" PRId64 " - SOCKS5 tunnel established to %s:%d",
                  getCuid(), getRequest()->getHost().c_str(),
                  getRequest()->getPort()));

  auto nextCmd = createNextCommand();
  nextCmd->setStatus(Command::STATUS_ONESHOT_REALTIME);
  getDownloadEngine()->setNoWait(true);
  getDownloadEngine()->addCommand(std::move(nextCmd));
  return true;
}

} // namespace aria2
