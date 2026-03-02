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
#include "Http2Session.h"

#include <cstring>
#include <algorithm>

#include "ISocketCore.h"
#include "LogFactory.h"
#include "Logger.h"
#include "fmt.h"

namespace aria2 {

Http2Session::Http2Session(const std::shared_ptr<ISocketCore>& socket)
    : session_(nullptr), socket_(socket), fatal_(false)
{
}

Http2Session::~Http2Session()
{
  if (session_) {
    nghttp2_session_del(session_);
    session_ = nullptr;
  }
}

int Http2Session::init()
{
  nghttp2_session_callbacks* callbacks;
  int rv = nghttp2_session_callbacks_new(&callbacks);
  if (rv != 0) {
    A2_LOG_ERROR(fmt("HTTP2: nghttp2_session_callbacks_new failed: %s",
                     nghttp2_strerror(rv)));
    return -1;
  }

  nghttp2_session_callbacks_set_send_callback(callbacks, sendCallback);
  nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
                                                       onFrameRecvCallback);
  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
      callbacks, onDataChunkRecvCallback);
  nghttp2_session_callbacks_set_on_stream_close_callback(callbacks,
                                                         onStreamCloseCallback);
  nghttp2_session_callbacks_set_on_header_callback(callbacks, onHeaderCallback);

  rv = nghttp2_session_client_new(&session_, callbacks, this);
  nghttp2_session_callbacks_del(callbacks);
  if (rv != 0) {
    A2_LOG_ERROR(fmt("HTTP2: nghttp2_session_client_new failed: %s",
                     nghttp2_strerror(rv)));
    return -1;
  }

  // Send HTTP/2 connection preface (SETTINGS frame)
  nghttp2_settings_entry settings[] = {
      {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100},
      {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, 1048576}, // 1MB
  };
  rv = nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, settings,
                               sizeof(settings) / sizeof(settings[0]));
  if (rv != 0) {
    A2_LOG_ERROR(
        fmt("HTTP2: nghttp2_submit_settings failed: %s", nghttp2_strerror(rv)));
    return -1;
  }

  // Increase connection-level window size
  rv = nghttp2_submit_window_update(session_, NGHTTP2_FLAG_NONE, 0,
                                    1048576); // +1MB
  if (rv != 0) {
    A2_LOG_WARN(fmt("HTTP2: nghttp2_submit_window_update failed: %s",
                    nghttp2_strerror(rv)));
  }

  A2_LOG_INFO("HTTP2: Session initialized");
  return 0;
}

int32_t Http2Session::submitRequest(
    const std::string& method, const std::string& scheme,
    const std::string& authority, const std::string& path,
    const std::vector<std::pair<std::string, std::string>>& extraHeaders)
{
  // Build nghttp2 header array: pseudo-headers first, then extras
  std::vector<nghttp2_nv> nva;
  nva.reserve(4 + extraHeaders.size());

  auto makeNv = [](const std::string& name,
                   const std::string& value) -> nghttp2_nv {
    nghttp2_nv nv;
    nv.name =
        const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(name.c_str()));
    nv.namelen = name.size();
    nv.value =
        const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(value.c_str()));
    nv.valuelen = value.size();
    nv.flags = NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE;
    return nv;
  };

  // Pseudo-headers
  std::string methodHeader = ":method";
  std::string schemeHeader = ":scheme";
  std::string authorityHeader = ":authority";
  std::string pathHeader = ":path";

  nva.push_back(makeNv(methodHeader, method));
  nva.push_back(makeNv(schemeHeader, scheme));
  nva.push_back(makeNv(authorityHeader, authority));
  nva.push_back(makeNv(pathHeader, path));

  for (const auto& h : extraHeaders) {
    nva.push_back(makeNv(h.first, h.second));
  }

  int32_t streamId =
      nghttp2_submit_request(session_, nullptr, nva.data(), nva.size(),
                             nullptr, // no data provider (GET)
                             nullptr);
  if (streamId < 0) {
    A2_LOG_ERROR(fmt("HTTP2: nghttp2_submit_request failed: %s",
                     nghttp2_strerror(streamId)));
    return -1;
  }

  // Pre-create stream data
  getOrCreateStream(streamId);
  A2_LOG_INFO(fmt("HTTP2: Submitted %s %s%s (stream %d)", method.c_str(),
                  authority.c_str(), path.c_str(), streamId));
  return streamId;
}

int Http2Session::sendData()
{
  if (fatal_) {
    return -1;
  }
  // nghttp2_session_send() calls our sendCallback for each chunk
  int rv = nghttp2_session_send(session_);
  if (rv != 0) {
    A2_LOG_ERROR(
        fmt("HTTP2: nghttp2_session_send failed: %s", nghttp2_strerror(rv)));
    fatal_ = true;
    return -1;
  }
  return 0;
}

int Http2Session::recvData()
{
  if (fatal_) {
    return -1;
  }

  uint8_t buf[16384];
  size_t len = sizeof(buf);
  socket_->readData(buf, len);
  if (len == 0) {
    // No data available (EAGAIN)
    return 1;
  }

  ssize_t rv = nghttp2_session_mem_recv(session_, buf, len);
  if (rv < 0) {
    A2_LOG_ERROR(fmt("HTTP2: nghttp2_session_mem_recv failed: %s",
                     nghttp2_strerror(static_cast<int>(rv))));
    fatal_ = true;
    return -1;
  }

  return 0;
}

const Http2StreamData* Http2Session::getStreamData(int32_t streamId) const
{
  auto it = streams_.find(streamId);
  if (it != streams_.end()) {
    return &it->second;
  }
  return nullptr;
}

size_t Http2Session::readStreamData(int32_t streamId, void* buf, size_t len)
{
  auto it = streams_.find(streamId);
  if (it == streams_.end()) {
    return 0;
  }
  auto& data = it->second;
  size_t available = std::min(len, data.recvBuf.size());
  if (available > 0) {
    std::memcpy(buf, data.recvBuf.data(), available);
    data.recvBuf.erase(0, available);
    // Inform nghttp2 that we consumed the data, allowing it to
    // send WINDOW_UPDATE
    int rv = nghttp2_session_consume(session_, streamId, available);
    if (rv != 0) {
      A2_LOG_WARN(fmt("HTTP2: nghttp2_session_consume failed: %s",
                      nghttp2_strerror(rv)));
    }
  }
  return available;
}

bool Http2Session::wantWrite() const
{
  return session_ && nghttp2_session_want_write(session_);
}

bool Http2Session::wantRead() const
{
  return session_ && nghttp2_session_want_read(session_);
}

bool Http2Session::isAlive() const
{
  return session_ && !fatal_ &&
         (nghttp2_session_want_read(session_) ||
          nghttp2_session_want_write(session_));
}

Http2StreamData& Http2Session::getOrCreateStream(int32_t streamId)
{
  return streams_[streamId];
}

// --- nghttp2 callbacks ---

ssize_t Http2Session::sendCallback(nghttp2_session* /* session */,
                                   const uint8_t* data, size_t length,
                                   int /* flags */, void* userData)
{
  auto* self = static_cast<Http2Session*>(userData);
  ssize_t written = self->socket_->writeData(data, length);
  if (written < 0) {
    if (self->socket_->wantWrite() || self->socket_->wantRead()) {
      return NGHTTP2_ERR_WOULDBLOCK;
    }
    return NGHTTP2_ERR_CALLBACK_FAILURE;
  }
  if (written == 0) {
    return NGHTTP2_ERR_WOULDBLOCK;
  }
  return written;
}

int Http2Session::onFrameRecvCallback(nghttp2_session* /* session */,
                                      const nghttp2_frame* frame,
                                      void* userData)
{
  auto* self = static_cast<Http2Session*>(userData);
  switch (frame->hd.type) {
  case NGHTTP2_HEADERS: {
    if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
      auto it = self->streams_.find(frame->hd.stream_id);
      if (it != self->streams_.end()) {
        it->second.headersComplete = true;
        A2_LOG_INFO(fmt("HTTP2: Stream %d response headers complete "
                        "(status=%d)",
                        frame->hd.stream_id, it->second.statusCode));
      }
    }
    break;
  }
  case NGHTTP2_GOAWAY: {
    A2_LOG_INFO(fmt("HTTP2: Received GOAWAY (last_stream_id=%d, "
                    "error_code=%u)",
                    frame->goaway.last_stream_id, frame->goaway.error_code));
    break;
  }
  default:
    break;
  }
  return 0;
}

int Http2Session::onDataChunkRecvCallback(nghttp2_session* /* session */,
                                          uint8_t /* flags */, int32_t streamId,
                                          const uint8_t* data, size_t len,
                                          void* userData)
{
  auto* self = static_cast<Http2Session*>(userData);
  auto& stream = self->getOrCreateStream(streamId);
  stream.recvBuf.append(reinterpret_cast<const char*>(data), len);
  return 0;
}

int Http2Session::onStreamCloseCallback(nghttp2_session* /* session */,
                                        int32_t streamId, uint32_t errorCode,
                                        void* userData)
{
  auto* self = static_cast<Http2Session*>(userData);
  auto& stream = self->getOrCreateStream(streamId);
  stream.closed = true;
  stream.errorCode = errorCode;
  A2_LOG_INFO(
      fmt("HTTP2: Stream %d closed (error_code=%u)", streamId, errorCode));
  return 0;
}

int Http2Session::onHeaderCallback(nghttp2_session* /* session */,
                                   const nghttp2_frame* frame,
                                   const uint8_t* name, size_t namelen,
                                   const uint8_t* value, size_t valuelen,
                                   uint8_t /* flags */, void* userData)
{
  auto* self = static_cast<Http2Session*>(userData);
  if (frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_RESPONSE) {
    return 0;
  }

  auto& stream = self->getOrCreateStream(frame->hd.stream_id);
  std::string n(reinterpret_cast<const char*>(name), namelen);
  std::string v(reinterpret_cast<const char*>(value), valuelen);

  if (n == ":status") {
    stream.statusCode = std::stoi(v);
  }
  else {
    stream.headers.emplace_back(std::move(n), std::move(v));
  }
  return 0;
}

} // namespace aria2
