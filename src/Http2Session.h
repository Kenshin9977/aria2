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
#ifndef D_HTTP2_SESSION_H
#define D_HTTP2_SESSION_H

#include "common.h"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <utility>

#include <nghttp2/nghttp2.h>

namespace aria2 {

class SocketCore;

// Per-stream state tracked by Http2Session.
struct Http2StreamData {
  int statusCode;
  std::vector<std::pair<std::string, std::string>> headers;
  // Buffered body data received via DATA frames
  std::string recvBuf;
  bool headersComplete;
  bool closed;
  uint32_t errorCode;

  Http2StreamData()
      : statusCode(0),
        headersComplete(false),
        closed(false),
        errorCode(0)
  {
  }
};

// Wraps a client-side nghttp2_session.  Provides submit/drive/query
// for HTTP/2 streams over an already-established TLS connection.
class Http2Session {
public:
  Http2Session(const std::shared_ptr<SocketCore>& socket);
  ~Http2Session();

  // Initializes the nghttp2 session and sends the connection preface
  // (client magic + initial SETTINGS).  Returns 0 on success.
  int init();

  // Submits an HTTP request.  Returns the stream ID (>0) on success,
  // or -1 on error.
  int32_t submitRequest(
      const std::string& method, const std::string& scheme,
      const std::string& authority, const std::string& path,
      const std::vector<std::pair<std::string, std::string>>& extraHeaders);

  // Sends pending session data to the remote peer.  Returns 0 on
  // success, -1 on fatal error.  May partially write if the socket
  // would block.
  int sendData();

  // Reads data from the socket and feeds it to the nghttp2 session.
  // Returns 0 on success, -1 on fatal error, 1 if EAGAIN/WOULDBLOCK.
  int recvData();

  // Returns the stream data for |streamId|, or nullptr.
  const Http2StreamData* getStreamData(int32_t streamId) const;

  // Consumes up to |len| bytes of body data from stream |streamId|.
  // Returns the number of bytes consumed.
  size_t readStreamData(int32_t streamId, void* buf, size_t len);

  // Returns true if the nghttp2 session wants to send data.
  bool wantWrite() const;

  // Returns true if the nghttp2 session wants to read data.
  bool wantRead() const;

  // Returns true if the session is alive (not terminated).
  bool isAlive() const;

private:
  // nghttp2 callbacks
  static ssize_t sendCallback(nghttp2_session* session,
                               const uint8_t* data, size_t length,
                               int flags, void* userData);
  static int onFrameRecvCallback(nghttp2_session* session,
                                  const nghttp2_frame* frame,
                                  void* userData);
  static int onDataChunkRecvCallback(nghttp2_session* session,
                                      uint8_t flags, int32_t streamId,
                                      const uint8_t* data, size_t len,
                                      void* userData);
  static int onStreamCloseCallback(nghttp2_session* session,
                                    int32_t streamId,
                                    uint32_t errorCode,
                                    void* userData);
  static int onHeaderCallback(nghttp2_session* session,
                               const nghttp2_frame* frame,
                               const uint8_t* name, size_t namelen,
                               const uint8_t* value, size_t valuelen,
                               uint8_t flags, void* userData);

  Http2StreamData& getOrCreateStream(int32_t streamId);

  nghttp2_session* session_;
  std::shared_ptr<SocketCore> socket_;
  std::map<int32_t, Http2StreamData> streams_;
  bool fatal_;

  Http2Session(const Http2Session&);
  Http2Session& operator=(const Http2Session&);
};

} // namespace aria2

#endif // D_HTTP2_SESSION_H
