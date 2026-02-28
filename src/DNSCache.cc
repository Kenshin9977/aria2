/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2010 Tatsuhiro Tsujikawa
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
#include "DNSCache.h"

#include <algorithm>

#include "A2STR.h"

namespace aria2 {

DNSCache::AddrEntry::AddrEntry(const std::string& addr)
    : addr_(addr), good_(true)
{
}

DNSCache::AddrEntry::AddrEntry(const AddrEntry& c) = default;

DNSCache::AddrEntry::~AddrEntry() = default;

DNSCache::AddrEntry& DNSCache::AddrEntry::operator=(const AddrEntry& c)
{
  if (this != &c) {
    addr_ = c.addr_;
    good_ = c.good_;
  }
  return *this;
}

DNSCache::CacheEntry::CacheEntry(const std::string& hostname, uint16_t port)
    : hostname_(hostname),
      port_(port),
      createdAt_(std::chrono::steady_clock::now())
{
}

DNSCache::CacheEntry::CacheEntry(const CacheEntry& c) = default;

DNSCache::CacheEntry::~CacheEntry() = default;

DNSCache::CacheEntry& DNSCache::CacheEntry::operator=(const CacheEntry& c)
{
  if (this != &c) {
    hostname_ = c.hostname_;
    port_ = c.port_;
    addrEntries_ = c.addrEntries_;
  }
  return *this;
}

bool DNSCache::CacheEntry::add(const std::string& addr)
{
  if (std::ranges::any_of(addrEntries_, [&addr](const auto& entry) {
        return entry.addr_ == addr;
      })) {
    return false;
  }
  addrEntries_.emplace_back(addr);
  return true;
}

std::vector<DNSCache::AddrEntry>::iterator
DNSCache::CacheEntry::find(const std::string& addr)
{
  return std::ranges::find_if(addrEntries_, [&addr](const auto& entry) {
    return entry.addr_ == addr;
  });
}

std::vector<DNSCache::AddrEntry>::const_iterator
DNSCache::CacheEntry::find(const std::string& addr) const
{
  return std::ranges::find_if(addrEntries_, [&addr](const auto& entry) {
    return entry.addr_ == addr;
  });
}

bool DNSCache::CacheEntry::contains(const std::string& addr) const
{
  return find(addr) != addrEntries_.end();
}

const std::string& DNSCache::CacheEntry::getGoodAddr() const
{
  auto it = std::ranges::find_if(
      addrEntries_, [](const auto& elem) { return elem.good_; });
  return it != addrEntries_.end() ? it->addr_ : A2STR::NIL;
}

void DNSCache::CacheEntry::markBad(const std::string& addr)
{
  auto i = find(addr);
  if (i != addrEntries_.end()) {
    i->good_ = false;
  }
}

DNSCache::DNSCache(std::chrono::seconds ttl) : ttl_(ttl) {}

DNSCache::DNSCache(const DNSCache& c) = default;

DNSCache::~DNSCache() = default;

DNSCache& DNSCache::operator=(const DNSCache& c)
{
  if (this != &c) {
    entries_ = c.entries_;
    ttl_ = c.ttl_;
  }
  return *this;
}

bool DNSCache::isExpired(const CacheEntry& entry) const
{
  if (ttl_.count() == 0) {
    return false;
  }
  auto now = std::chrono::steady_clock::now();
  return now - entry.createdAt_ > ttl_;
}

const std::string& DNSCache::find(const std::string& hostname,
                                  uint16_t port)
{
  auto i = entries_.find({hostname, port});
  if (i == entries_.end()) {
    return A2STR::NIL;
  }
  if (isExpired(i->second)) {
    entries_.erase(i);
    return A2STR::NIL;
  }
  return i->second.getGoodAddr();
}

void DNSCache::put(const std::string& hostname, const std::string& ipaddr,
                   uint16_t port)
{
  auto key = CacheKey{hostname, port};
  auto i = entries_.find(key);
  if (i != entries_.end()) {
    i->second.add(ipaddr);
    i->second.createdAt_ = std::chrono::steady_clock::now();
  }
  else {
    CacheEntry entry(hostname, port);
    entry.add(ipaddr);
    entries_.emplace(std::move(key), std::move(entry));
  }
}

void DNSCache::markBad(const std::string& hostname, const std::string& ipaddr,
                       uint16_t port)
{
  auto i = entries_.find({hostname, port});
  if (i != entries_.end()) {
    i->second.markBad(ipaddr);
  }
}

void DNSCache::remove(const std::string& hostname, uint16_t port)
{
  entries_.erase({hostname, port});
}

} // namespace aria2
