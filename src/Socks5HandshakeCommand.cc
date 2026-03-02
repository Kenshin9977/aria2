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
#include "Socks5HandshakeCommand.h"

#include "Socks5ConnectCommand.h"
#include "ISocketCore.h"
#include "Request.h"
#include "DownloadEngine.h"
#include "Option.h"
#include "prefs.h"
#include "DlAbortEx.h"
#include "Logger.h"
#include "LogFactory.h"
#include "fmt.h"

namespace aria2 {

namespace {
const uint8_t SOCKS5_VERSION = 0x05;
const uint8_t AUTH_NONE = 0x00;
const uint8_t AUTH_USERPASS = 0x02;
const uint8_t AUTH_REJECT = 0xFF;
const uint8_t USERPASS_VERSION = 0x01;
const uint8_t USERPASS_SUCCESS = 0x00;
} // namespace

Socks5HandshakeCommand::Socks5HandshakeCommand(
    cuid_t cuid, const std::shared_ptr<Request>& req,
    const std::shared_ptr<FileEntry>& fileEntry, RequestGroup* requestGroup,
    DownloadEngine* e, const std::shared_ptr<Request>& proxyRequest,
    const std::shared_ptr<ISocketCore>& s)
    : AbstractCommand(cuid, req, fileEntry, requestGroup, e, s),
      state_(GREETING_WRITE),
      proxyRequest_(proxyRequest),
      hasAuth_(false)
{
  std::string user = getOption()->get(PREF_SOCKS5_PROXY_USER);
  std::string passwd = getOption()->get(PREF_SOCKS5_PROXY_PASSWD);
  hasAuth_ = !user.empty() && !passwd.empty();
  buildGreeting();
  disableReadCheckSocket();
  setWriteCheckSocket(getSocket());
}

Socks5HandshakeCommand::~Socks5HandshakeCommand() = default;

void Socks5HandshakeCommand::buildGreeting()
{
  writeBuf_.clear();
  writeBuf_ += static_cast<char>(SOCKS5_VERSION);
  if (hasAuth_) {
    writeBuf_ += static_cast<char>(2); // 2 methods
    writeBuf_ += static_cast<char>(AUTH_NONE);
    writeBuf_ += static_cast<char>(AUTH_USERPASS);
  }
  else {
    writeBuf_ += static_cast<char>(1); // 1 method
    writeBuf_ += static_cast<char>(AUTH_NONE);
  }
}

void Socks5HandshakeCommand::buildAuth()
{
  std::string user = getOption()->get(PREF_SOCKS5_PROXY_USER);
  std::string passwd = getOption()->get(PREF_SOCKS5_PROXY_PASSWD);
  writeBuf_.clear();
  writeBuf_ += static_cast<char>(USERPASS_VERSION);
  writeBuf_ += static_cast<char>(user.size());
  writeBuf_ += user;
  writeBuf_ += static_cast<char>(passwd.size());
  writeBuf_ += passwd;
}

bool Socks5HandshakeCommand::doWrite()
{
  if (writeBuf_.empty()) {
    return true;
  }
  ssize_t written = getSocket()->writeData(writeBuf_.data(), writeBuf_.size());
  if (written > 0) {
    writeBuf_.erase(0, written);
  }
  return writeBuf_.empty();
}

bool Socks5HandshakeCommand::doRead(size_t expectedLen)
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

bool Socks5HandshakeCommand::executeInternal()
{
  switch (state_) {
  case GREETING_WRITE:
    if (doWrite()) {
      state_ = GREETING_READ;
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
  case GREETING_READ:
    if (!doRead(2)) {
      addCommandSelf();
      return false;
    }
    {
      uint8_t ver = static_cast<uint8_t>(readBuf_[0]);
      uint8_t method = static_cast<uint8_t>(readBuf_[1]);
      if (ver != SOCKS5_VERSION) {
        throw DL_ABORT_EX(
            fmt("SOCKS5: unexpected version %d", static_cast<int>(ver)));
      }
      if (method == AUTH_REJECT) {
        throw DL_ABORT_EX("SOCKS5: no acceptable authentication method");
      }
      if (method == AUTH_USERPASS) {
        if (!hasAuth_) {
          throw DL_ABORT_EX(
              "SOCKS5: server requires auth but no credentials provided");
        }
        buildAuth();
        state_ = AUTH_WRITE;
        readBuf_.clear();
        disableReadCheckSocket();
        setWriteCheckSocket(getSocket());
        // fall through to AUTH_WRITE
      }
      else if (method == AUTH_NONE) {
        state_ = DONE;
      }
      else {
        throw DL_ABORT_EX(fmt("SOCKS5: unsupported auth method %d",
                              static_cast<int>(method)));
      }
    }
    if (state_ == DONE) {
      break;
    }
    if (state_ == AUTH_WRITE) {
      // fall through
    }
    else {
      addCommandSelf();
      return false;
    }
  case AUTH_WRITE:
    if (doWrite()) {
      state_ = AUTH_READ;
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
  case AUTH_READ:
    if (!doRead(2)) {
      addCommandSelf();
      return false;
    }
    {
      uint8_t ver = static_cast<uint8_t>(readBuf_[0]);
      uint8_t status = static_cast<uint8_t>(readBuf_[1]);
      if (ver != USERPASS_VERSION) {
        throw DL_ABORT_EX(
            fmt("SOCKS5: unexpected auth version %d", static_cast<int>(ver)));
      }
      if (status != USERPASS_SUCCESS) {
        throw DL_ABORT_EX("SOCKS5: authentication failed");
      }
      state_ = DONE;
    }
    break;
  case DONE:
    break;
  }

  // Handshake complete — transition to CONNECT
  A2_LOG_INFO(fmt("CUID#%" PRId64 " - SOCKS5 handshake complete, "
                  "connecting to %s:%d",
                  getCuid(), getRequest()->getHost().c_str(),
                  getRequest()->getPort()));
  auto connectCmd = std::make_unique<Socks5ConnectCommand>(
      getCuid(), getRequest(), getFileEntry(), getRequestGroup(),
      getDownloadEngine(), proxyRequest_, getSocket());
  connectCmd->setStatus(Command::STATUS_ONESHOT_REALTIME);
  getDownloadEngine()->setNoWait(true);
  getDownloadEngine()->addCommand(std::move(connectCmd));
  return true;
}

} // namespace aria2
