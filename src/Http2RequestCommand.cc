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
#include "Http2RequestCommand.h"

#include "Http2Session.h"
#include "Request.h"
#include "DownloadEngine.h"
#include "RequestGroup.h"
#include "DownloadContext.h"
#include "FileEntry.h"
#include "Segment.h"
#include "SegmentMan.h"
#include "PieceStorage.h"
#include "DiskAdaptor.h"
#include "DlAbortEx.h"
#include "Logger.h"
#include "LogFactory.h"
#include "fmt.h"
#include "SocketCore.h"
#include "Option.h"
#include "prefs.h"
#include "util.h"

namespace aria2 {

Http2RequestCommand::Http2RequestCommand(
    cuid_t cuid, const std::shared_ptr<Request>& req,
    const std::shared_ptr<FileEntry>& fileEntry, RequestGroup* requestGroup,
    DownloadEngine* e, const std::shared_ptr<ISocketCore>& socket,
    const std::shared_ptr<Http2Session>& h2session, int32_t streamId)
    : AbstractCommand(cuid, req, fileEntry, requestGroup, e, socket),
      h2session_(h2session),
      streamId_(streamId),
      state_(H2_SEND),
      totalReceived_(0)
{
  disableReadCheckSocket();
  setWriteCheckSocket(getSocket());
}

Http2RequestCommand::~Http2RequestCommand() = default;

bool Http2RequestCommand::executeInternal()
{
  switch (state_) {
  case H2_SEND: {
    // Send pending HTTP/2 frames (connection preface + request)
    int rv = h2session_->sendData();
    if (rv != 0) {
      throw DL_ABORT_EX("HTTP2: Failed to send data");
    }
    if (!h2session_->wantWrite()) {
      // All pending data sent — switch to receiving
      state_ = H2_RECV_HEADERS;
      disableWriteCheckSocket();
      setReadCheckSocket(getSocket());
    }
    else {
      setWriteCheckSocket(getSocket());
      addCommandSelf();
      return false;
    }
  }
  // fall through
  case H2_RECV_HEADERS: {
    // Receive and process incoming HTTP/2 frames
    int rv = h2session_->recvData();
    if (rv == -1) {
      throw DL_ABORT_EX("HTTP2: Failed to receive data");
    }
    // Send any pending responses (SETTINGS ACK, WINDOW_UPDATE, etc.)
    h2session_->sendData();

    const Http2StreamData* sd = h2session_->getStreamData(streamId_);
    if (!sd || !sd->headersComplete) {
      // Headers not yet complete — keep waiting
      setReadCheckSocket(getSocket());
      addCommandSelf();
      return false;
    }

    A2_LOG_INFO(fmt("CUID#%" PRId64 " - HTTP2: Response status=%d", getCuid(),
                    sd->statusCode));
    for (const auto& h : sd->headers) {
      A2_LOG_DEBUG(
          fmt("HTTP2: Header %s: %s", h.first.c_str(), h.second.c_str()));
    }

    if (sd->statusCode / 100 == 3) {
      // Redirect — find Location header
      for (const auto& h : sd->headers) {
        if (h.first == "location") {
          A2_LOG_INFO(fmt("HTTP2: Redirect to %s", h.second.c_str()));
          getRequest()->redirectUri(h.second);
          return prepareForRetry(0);
        }
      }
      throw DL_ABORT_EX("HTTP2: Redirect without Location header");
    }

    if (sd->statusCode != 200 && sd->statusCode != 206) {
      throw DL_ABORT_EX(
          fmt("HTTP2: Unexpected status code %d", sd->statusCode));
    }

    // Extract content-length if present
    for (const auto& h : sd->headers) {
      if (h.first == "content-length") {
        auto r = util::parseLLInt(h.second);
        if (r) {
          getFileEntry()->setLength(*r);
        }
        break;
      }
    }

    // Initialize storage
    if (!getPieceStorage()) {
      getRequestGroup()->initPieceStorage();
      if (getPieceStorage()) {
        getPieceStorage()->getDiskAdaptor()->openFile();
      }
    }

    // Transition to data reception
    state_ = H2_RECV_DATA;

    // Check if body data already arrived with headers
    if (sd->closed && sd->recvBuf.empty()) {
      // Zero-length body
      state_ = H2_DONE;
      break;
    }
  }
  // fall through
  case H2_RECV_DATA: {
    // Continue receiving DATA frames
    int rv = h2session_->recvData();
    if (rv == -1) {
      throw DL_ABORT_EX("HTTP2: Failed to receive data");
    }
    // Send WINDOW_UPDATE etc.
    h2session_->sendData();

    // Write buffered data to disk
    const Http2StreamData* sd = h2session_->getStreamData(streamId_);
    if (sd) {
      uint8_t buf[16384];
      size_t nread;
      while ((nread = h2session_->readStreamData(streamId_, buf, sizeof(buf))) >
             0) {
        if (getPieceStorage()) {
          getPieceStorage()->getDiskAdaptor()->writeData(buf, nread,
                                                         totalReceived_);
        }
        totalReceived_ += nread;
      }

      if (sd->closed) {
        state_ = H2_DONE;
        break;
      }
    }

    setReadCheckSocket(getSocket());
    addCommandSelf();
    return false;
  }
  case H2_DONE:
    break;
  }

  // Download complete
  const Http2StreamData* sd = h2session_->getStreamData(streamId_);
  if (sd && sd->errorCode != 0) {
    throw DL_ABORT_EX(fmt("HTTP2: Stream error (code=%u)", sd->errorCode));
  }

  A2_LOG_INFO(fmt("CUID#%" PRId64 " - HTTP2: Download complete (%" PRId64
                  " bytes)",
                  getCuid(), totalReceived_));

  // Mark pieces as complete
  if (getPieceStorage()) {
    getPieceStorage()->markAllPiecesDone();
  }

  return true;
}

} // namespace aria2
