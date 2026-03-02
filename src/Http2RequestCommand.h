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
#ifndef D_HTTP2_REQUEST_COMMAND_H
#define D_HTTP2_REQUEST_COMMAND_H

#include "AbstractCommand.h"

#include <memory>

namespace aria2 {

class Http2Session;

// Handles the HTTP/2 request/response cycle for a single stream.
// Created by HttpRequestCommand when ALPN negotiates "h2".
// Drives the nghttp2 session I/O, collects response headers, and
// streams body data to the PieceStorage via writeData callbacks.
class Http2RequestCommand : public AbstractCommand {
public:
  Http2RequestCommand(cuid_t cuid, const std::shared_ptr<Request>& req,
                      const std::shared_ptr<FileEntry>& fileEntry,
                      RequestGroup* requestGroup, DownloadEngine* e,
                      const std::shared_ptr<ISocketCore>& socket,
                      const std::shared_ptr<Http2Session>& h2session,
                      int32_t streamId);
  ~Http2RequestCommand() override;

protected:
  bool executeInternal() override;

private:
  enum State {
    // Sending frames (connection preface + request headers)
    H2_SEND,
    // Waiting for response headers
    H2_RECV_HEADERS,
    // Receiving body data
    H2_RECV_DATA,
    // Stream complete
    H2_DONE
  };

  std::shared_ptr<Http2Session> h2session_;
  int32_t streamId_;
  State state_;
  int64_t totalReceived_;
};

} // namespace aria2

#endif // D_HTTP2_REQUEST_COMMAND_H
