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
#include "IouringEventPoll.h"

#include <cerrno>
#include <cstring>
#include <algorithm>
#include <numeric>

#include "Command.h"
#include "LogFactory.h"
#include "Logger.h"
#include "util.h"
#include "a2functional.h"
#include "fmt.h"

namespace aria2 {

IouringEventPoll::KSocketEntry::KSocketEntry(sock_t s)
    : SocketEntry<KCommandEvent, KADNSEvent>(s)
{
}

namespace {
int accumulateEvent(int events, const IouringEventPoll::KEvent& event)
{
  return events | event.getEvents();
}
} // namespace

int IouringEventPoll::KSocketEntry::getEvents()
{
#ifdef ENABLE_ASYNC_DNS
  return std::accumulate(
      adnsEvents_.begin(), adnsEvents_.end(),
      std::accumulate(commandEvents_.begin(), commandEvents_.end(), 0,
                      aria2::accumulateEvent),
      aria2::accumulateEvent);
#else  // !ENABLE_ASYNC_DNS
  return std::accumulate(commandEvents_.begin(), commandEvents_.end(), 0,
                         aria2::accumulateEvent);
#endif // !ENABLE_ASYNC_DNS
}

IouringEventPoll::IouringEventPoll() : ringInitialized_(false)
{
  memset(&ring_, 0, sizeof(ring_));
  int ret = io_uring_queue_init(RING_SIZE, &ring_, 0);
  if (ret == 0) {
    ringInitialized_ = true;
  }
  else {
    A2_LOG_ERROR(
        fmt("io_uring_queue_init failed: %s", util::safeStrerror(-ret).c_str()));
  }
}

IouringEventPoll::~IouringEventPoll()
{
  if (ringInitialized_) {
    io_uring_queue_exit(&ring_);
  }
}

bool IouringEventPoll::good() const { return ringInitialized_; }

void IouringEventPoll::submitPollAdd(KSocketEntry& entry)
{
  int events = entry.getEvents();
  if (events == 0) {
    return;
  }

  struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
  if (!sqe) {
    A2_LOG_DEBUG("io_uring: no SQE available for poll_add");
    return;
  }

  io_uring_prep_poll_add(sqe, entry.getSocket(), events);
  // Use IORING_POLL_ADD_MULTI so the poll stays armed across
  // multiple events (kernel 5.13+). If not supported, we re-arm
  // after each completion.
  sqe->len |= IORING_POLL_ADD_MULTI;
  io_uring_sqe_set_data(sqe, &entry);
}

void IouringEventPoll::cancelPoll(sock_t socket)
{
  struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
  if (!sqe) {
    return;
  }
  io_uring_prep_poll_remove(sqe, 0);
  // Cancel all poll operations matching this socket's user_data
  auto i = socketEntries_.find(socket);
  if (i != socketEntries_.end()) {
    io_uring_sqe_set_data(sqe, nullptr);
    // Use POLL_REMOVE with the original user_data
    sqe->addr = reinterpret_cast<__u64>(&i->second);
  }
}

void IouringEventPoll::poll(const struct timeval& tv)
{
  // Submit any pending SQEs
  io_uring_submit(&ring_);

  // Wait for completions with timeout
  struct __kernel_timespec ts;
  ts.tv_sec = tv.tv_sec;
  ts.tv_nsec = tv.tv_usec * 1000;

  struct io_uring_cqe* cqe;
  int ret = io_uring_wait_cqe_timeout(&ring_, &cqe, &ts);

  if (ret == 0) {
    // Process all available CQEs
    unsigned head;
    unsigned count = 0;
    io_uring_for_each_cqe(&ring_, head, cqe)
    {
      ++count;
      auto* entry =
          reinterpret_cast<KSocketEntry*>(io_uring_cqe_get_data(cqe));
      if (!entry) {
        // This is a cancel completion or internal event
        continue;
      }

      if (cqe->res < 0) {
        // Error on poll — might be cancelled or socket closed
        if (cqe->res != -ECANCELED) {
          A2_LOG_DEBUG(
              fmt("io_uring poll error on fd %d: %s", entry->getSocket(),
                  util::safeStrerror(-cqe->res).c_str()));
        }
        continue;
      }

      // cqe->res contains the poll events that fired
      int events = cqe->res;
      entry->processEvents(events);

      // If multi-shot poll is not supported (indicated by
      // IORING_CQE_F_MORE not being set), re-arm the poll
      if (!(cqe->flags & IORING_CQE_F_MORE)) {
        submitPollAdd(*entry);
      }
    }
    io_uring_cq_advance(&ring_, count);
  }
  else if (ret != -ETIME && ret != -EINTR) {
    A2_LOG_INFO(
        fmt("io_uring_wait_cqe_timeout error: %s",
            util::safeStrerror(-ret).c_str()));
  }

#ifdef ENABLE_ASYNC_DNS
  for (auto& [key, ent] : nameResolverEntries_) {
    ent.processTimeout();
    ent.removeSocketEvents(this);
    ent.addSocketEvents(this);
  }
#endif // ENABLE_ASYNC_DNS
}

namespace {
int translateEvents(EventPoll::EventType events)
{
  int newEvents = 0;
  if (EventPoll::EVENT_READ & events) {
    newEvents |= POLLIN;
  }
  if (EventPoll::EVENT_WRITE & events) {
    newEvents |= POLLOUT;
  }
  if (EventPoll::EVENT_ERROR & events) {
    newEvents |= POLLERR;
  }
  if (EventPoll::EVENT_HUP & events) {
    newEvents |= POLLHUP;
  }
  return newEvents;
}
} // namespace

bool IouringEventPoll::addEvents(sock_t socket,
                                 const IouringEventPoll::KEvent& event)
{
  auto i = socketEntries_.find(socket);
  if (i != socketEntries_.end()) {
    auto& socketEntry = i->second;
    int oldEvents = socketEntry.getEvents();

    event.addSelf(&socketEntry);

    int newEvents = socketEntry.getEvents();
    if (oldEvents != newEvents) {
      // Cancel old poll and submit new one with updated mask
      cancelPoll(socket);
      submitPollAdd(socketEntry);
      io_uring_submit(&ring_);
    }
  }
  else {
    auto p = socketEntries_.emplace(socket, KSocketEntry(socket));
    auto& socketEntry = p.first->second;

    event.addSelf(&socketEntry);

    submitPollAdd(socketEntry);
    io_uring_submit(&ring_);
  }
  return true;
}

bool IouringEventPoll::addEvents(sock_t socket, Command* command,
                                 EventPoll::EventType events)
{
  int pollEvents = translateEvents(events);
  return addEvents(socket, KCommandEvent(command, pollEvents));
}

#ifdef ENABLE_ASYNC_DNS
bool IouringEventPoll::addEvents(sock_t socket, Command* command, int events,
                                 const std::shared_ptr<AsyncNameResolver>& rs)
{
  return addEvents(socket, KADNSEvent(rs, command, socket, events));
}
#endif // ENABLE_ASYNC_DNS

bool IouringEventPoll::deleteEvents(sock_t socket,
                                    const IouringEventPoll::KEvent& event)
{
  auto i = socketEntries_.find(socket);
  if (i == std::end(socketEntries_)) {
    A2_LOG_DEBUG(fmt("Socket %d is not found in SocketEntries.", socket));
    return false;
  }

  auto& socketEntry = (*i).second;
  event.removeSelf(&socketEntry);

  if (socketEntry.eventEmpty()) {
    cancelPoll(socket);
    io_uring_submit(&ring_);
    socketEntries_.erase(i);
  }
  else {
    // Update poll mask
    cancelPoll(socket);
    submitPollAdd(socketEntry);
    io_uring_submit(&ring_);
  }
  return true;
}

bool IouringEventPoll::deleteEvents(sock_t socket, Command* command,
                                    EventPoll::EventType events)
{
  int pollEvents = translateEvents(events);
  return deleteEvents(socket, KCommandEvent(command, pollEvents));
}

#ifdef ENABLE_ASYNC_DNS
bool IouringEventPoll::deleteEvents(
    sock_t socket, Command* command,
    const std::shared_ptr<AsyncNameResolver>& rs)
{
  return deleteEvents(socket, KADNSEvent(rs, command, socket, 0));
}
#endif // ENABLE_ASYNC_DNS

#ifdef ENABLE_ASYNC_DNS
bool IouringEventPoll::addNameResolver(
    const std::shared_ptr<AsyncNameResolver>& resolver, Command* command)
{
  auto key = std::make_pair(resolver.get(), command);
  auto itr = nameResolverEntries_.lower_bound(key);

  if (itr != std::end(nameResolverEntries_) && (*itr).first == key) {
    return false;
  }

  itr = nameResolverEntries_.insert(
      itr, std::make_pair(key, KAsyncNameResolverEntry(resolver, command)));
  (*itr).second.addSocketEvents(this);
  return true;
}

bool IouringEventPoll::deleteNameResolver(
    const std::shared_ptr<AsyncNameResolver>& resolver, Command* command)
{
  auto key = std::make_pair(resolver.get(), command);
  auto itr = nameResolverEntries_.find(key);
  if (itr == std::end(nameResolverEntries_)) {
    return false;
  }

  (*itr).second.removeSocketEvents(this);
  nameResolverEntries_.erase(itr);
  return true;
}
#endif // ENABLE_ASYNC_DNS

} // namespace aria2
