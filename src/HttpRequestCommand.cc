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
#include "HttpRequestCommand.h"

#include <algorithm>

#include "Request.h"
#include "DownloadEngine.h"
#include "RequestGroup.h"
#include "HttpResponseCommand.h"
#include "HttpConnection.h"
#include "HttpRequest.h"
#include "SegmentMan.h"
#include "Segment.h"
#include "Option.h"
#include "SocketCore.h"
#include "prefs.h"
#include "a2functional.h"
#include "util.h"
#include "CookieStorage.h"
#include "AuthConfigFactory.h"
#include "AuthConfig.h"
#include "DownloadContext.h"
#include "PieceStorage.h"
#include "DefaultBtProgressInfoFile.h"
#include "Logger.h"
#include "LogFactory.h"
#include "fmt.h"
#include "SocketRecvBuffer.h"
#ifdef HAVE_LIBNGHTTP2
#  include "Http2Session.h"
#  include "Http2RequestCommand.h"
#endif // HAVE_LIBNGHTTP2

namespace aria2 {

HttpRequestCommand::HttpRequestCommand(
    cuid_t cuid, const std::shared_ptr<Request>& req,
    const std::shared_ptr<FileEntry>& fileEntry, RequestGroup* requestGroup,
    const std::shared_ptr<HttpConnection>& httpConnection, DownloadEngine* e,
    const std::shared_ptr<ISocketCore>& s)
    : AbstractCommand(cuid, req, fileEntry, requestGroup, e, s,
                      httpConnection->getSocketRecvBuffer()),
      httpConnection_(httpConnection)
{
  initConnectTimeout();
}

HttpRequestCommand::~HttpRequestCommand() = default;

namespace {
std::unique_ptr<HttpRequest>
createHttpRequest(const std::shared_ptr<Request>& req,
                  const std::shared_ptr<FileEntry>& fileEntry,
                  const std::shared_ptr<Segment>& segment,
                  const std::shared_ptr<Option>& option, const RequestGroup* rg,
                  const DownloadEngine* e,
                  const std::shared_ptr<Request>& proxyRequest,
                  int64_t endOffset = 0)
{
  auto httpRequest = std::make_unique<HttpRequest>();
  httpRequest->setUserAgent(option->get(PREF_USER_AGENT));
  httpRequest->setRequest(req);
  httpRequest->setFileEntry(fileEntry);
  httpRequest->setSegment(segment);
  httpRequest->addHeader(option->get(PREF_HEADER));
  httpRequest->setCookieStorage(e->getCookieStorage().get());
  httpRequest->setAuthConfigFactory(e->getAuthConfigFactory().get());
  httpRequest->setOption(option.get());
  httpRequest->setProxyRequest(proxyRequest);
  httpRequest->setAcceptMetalink(rg->getDownloadContext()->getAcceptMetalink());
  httpRequest->setNoWantDigest(option->getAsBool(PREF_NO_WANT_DIGEST_HEADER));

  if (option->getAsBool(PREF_HTTP_ACCEPT_GZIP)) {
    httpRequest->enableAcceptGZip();
  }
  else {
    httpRequest->disableAcceptGZip();
  }
  if (option->getAsBool(PREF_HTTP_NO_CACHE)) {
    httpRequest->enableNoCache();
  }
  else {
    httpRequest->disableNoCache();
  }
  if (endOffset > 0) {
    httpRequest->setEndOffsetOverride(endOffset);
  }
  return httpRequest;
}
} // namespace

bool HttpRequestCommand::executeInternal()
{
  using enum Protocol;
  // socket->setBlockingMode();
  if (httpConnection_->sendBufferIsEmpty()) {
#ifdef ENABLE_SSL
    if (getRequest()->getProtocol() == HTTPS) {
      auto sc = std::static_pointer_cast<SocketCore>(getSocket());
#  ifdef HAVE_LIBNGHTTP2
      // Offer h2 + http/1.1 via ALPN (harmless if called repeatedly;
      // only applied once when TLS session is first created)
      sc->setALPNProtocols({"h2", "http/1.1"});
#  endif // HAVE_LIBNGHTTP2
      if (!sc->tlsConnect(getRequest()->getHost())) {
        setReadCheckSocketIf(getSocket(), getSocket()->wantRead());
        setWriteCheckSocketIf(getSocket(), getSocket()->wantWrite());
        addCommandSelf();
        return false;
      }
#  ifdef HAVE_LIBNGHTTP2
      if (sc->getNegotiatedProtocol() == "h2") {
        return createHttp2Command();
      }
#  endif // HAVE_LIBNGHTTP2
    }
#endif // ENABLE_SSL
    if (getSegments().empty()) {
      auto httpRequest = createHttpRequest(
          getRequest(), getFileEntry(), std::shared_ptr<Segment>(), getOption(),
          getRequestGroup(), getDownloadEngine(), proxyRequest_);
      if (getOption()->getAsBool(PREF_CONDITIONAL_GET) &&
          (getRequest()->getProtocol() == HTTP ||
           getRequest()->getProtocol() == HTTPS)) {

        std::string path;

        if (getFileEntry()->getPath().empty()) {
          auto& file = getRequest()->getFile();

          // If filename part of URI is empty, we just use
          // Request::DEFAULT_FILE, since it is the name we use to
          // store file in disk.

          path = util::createSafePath(
              getOption()->get(PREF_DIR),
              (getRequest()->getFile().empty()
                   ? Request::DEFAULT_FILE
                   : util::percentDecode(std::begin(file), std::end(file))));
        }
        else {
          path = getFileEntry()->getPath();
        }

        File ctrlfile(path + DefaultBtProgressInfoFile::getSuffix());
        File file(path);

        if (!ctrlfile.exists() && file.exists()) {
          httpRequest->setIfModifiedSinceHeader(
              file.getModifiedTime().toHTTPDate());
        }
      }
      httpConnection_->sendRequest(std::move(httpRequest));
    }
    else {
      for (auto& segment : getSegments()) {
        if (!httpConnection_->isIssued(segment)) {
          int64_t endOffset = 0;
          // FTP via HTTP proxy does not support end byte marker
          if (getRequest()->getProtocol() != FTP &&
              getRequest()->getProtocol() != FTPS &&
              getRequestGroup()->getTotalLength() > 0 && getPieceStorage()) {
            size_t nextIndex =
                getPieceStorage()->getNextUsedIndex(segment->getIndex());
            endOffset =
                std::min(getFileEntry()->getLength(),
                         getFileEntry()->gtoloff(
                             static_cast<int64_t>(segment->getSegmentLength()) *
                             nextIndex));
          }
          httpConnection_->sendRequest(
              createHttpRequest(getRequest(), getFileEntry(), segment,
                                getOption(), getRequestGroup(),
                                getDownloadEngine(), proxyRequest_, endOffset));
        }
      }
    }
  }
  else {
    httpConnection_->sendPendingData();
  }
  if (httpConnection_->sendBufferIsEmpty()) {
    getDownloadEngine()->addCommand(std::make_unique<HttpResponseCommand>(
        getCuid(), getRequest(), getFileEntry(), getRequestGroup(),
        httpConnection_, getDownloadEngine(), getSocket()));
    return true;
  }
  else {
    setReadCheckSocketIf(getSocket(), getSocket()->wantRead());
    setWriteCheckSocketIf(getSocket(), getSocket()->wantWrite());
    addCommandSelf();
    return false;
  }
}

bool HttpRequestCommand::createHttp2Command()
{
#ifdef HAVE_LIBNGHTTP2
  A2_LOG_INFO(fmt("CUID#%" PRId64 " - ALPN negotiated h2, switching to HTTP/2",
                  getCuid()));

  auto h2session = std::make_shared<Http2Session>(
      std::static_pointer_cast<SocketCore>(getSocket()));
  if (h2session->init() != 0) {
    throw DL_ABORT_EX("HTTP2: Failed to initialize session");
  }

  // Build request headers
  std::string method = "GET";
  std::string scheme = "https";
  std::string authority = getRequest()->getHost();
  if (getRequest()->getPort() != 443) {
    authority += ":" + util::uitos(getRequest()->getPort());
  }
  std::string path = getRequest()->getCurrentUri();
  // Extract path portion from full URI
  auto pathStart = path.find(authority);
  if (pathStart != std::string::npos) {
    path = path.substr(pathStart + authority.size());
  }
  if (path.empty()) {
    path = "/";
  }
  // If path still contains scheme://host, parse it properly
  if (path.find("://") != std::string::npos) {
    // Full URI — extract path component
    auto schemeEnd = path.find("://");
    auto hostStart = schemeEnd + 3;
    auto pathPos = path.find('/', hostStart);
    if (pathPos != std::string::npos) {
      path = path.substr(pathPos);
    }
    else {
      path = "/";
    }
  }

  std::vector<std::pair<std::string, std::string>> headers;
  headers.emplace_back("user-agent", getOption()->get(PREF_USER_AGENT));
  headers.emplace_back("accept", "*/*");

  int32_t streamId =
      h2session->submitRequest(method, scheme, authority, path, headers);
  if (streamId < 0) {
    throw DL_ABORT_EX("HTTP2: Failed to submit request");
  }

  auto cmd = std::make_unique<Http2RequestCommand>(
      getCuid(), getRequest(), getFileEntry(), getRequestGroup(),
      getDownloadEngine(), getSocket(), h2session, streamId);
  cmd->setStatus(Command::STATUS_ONESHOT_REALTIME);
  getDownloadEngine()->setNoWait(true);
  getDownloadEngine()->addCommand(std::move(cmd));
  return true;
#else
  return false;
#endif // HAVE_LIBNGHTTP2
}

void HttpRequestCommand::setProxyRequest(
    const std::shared_ptr<Request>& proxyRequest)
{
  proxyRequest_ = proxyRequest;
}

} // namespace aria2
