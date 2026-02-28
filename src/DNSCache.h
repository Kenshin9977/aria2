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
#ifndef D_DNS_CACHE_H
#define D_DNS_CACHE_H

#include "common.h"

#include <string>
#include <map>
#include <algorithm>
#include <vector>
#include <utility>
#include <chrono>

namespace aria2 {

class DNSCache {
private:
  struct AddrEntry {
    std::string addr_;
    bool good_;

    AddrEntry(const std::string& addr);
    AddrEntry(const AddrEntry& c);
    ~AddrEntry();

    AddrEntry& operator=(const AddrEntry& c);
  };

  struct CacheEntry {
    std::string hostname_;
    uint16_t port_;
    std::vector<AddrEntry> addrEntries_;
    std::chrono::steady_clock::time_point createdAt_;

    CacheEntry(const std::string& hostname, uint16_t port);
    CacheEntry(const CacheEntry& c);
    ~CacheEntry();

    CacheEntry& operator=(const CacheEntry& c);

    bool add(const std::string& addr);

    std::vector<AddrEntry>::iterator find(const std::string& addr);

    std::vector<AddrEntry>::const_iterator find(const std::string& addr) const;

    bool contains(const std::string& addr) const;

    const std::string& getGoodAddr() const;

    template <typename OutputIterator>
    void getAllGoodAddrs(OutputIterator out) const
    {
      for (auto& elem : addrEntries_) {
        if (elem.good_) {
          *out++ = elem.addr_;
        }
      }
    }

    void markBad(const std::string& addr);

  };

  using CacheKey = std::pair<std::string, uint16_t>;
  std::map<CacheKey, CacheEntry> entries_;
  std::chrono::seconds ttl_;

  bool isExpired(const CacheEntry& entry) const;

public:
  // Default TTL: 5 minutes. Set to 0 to disable expiration.
  DNSCache(std::chrono::seconds ttl = std::chrono::seconds(300));
  DNSCache(const DNSCache& c);
  ~DNSCache();

  DNSCache& operator=(const DNSCache& c);

  const std::string& find(const std::string& hostname, uint16_t port);

  template <typename OutputIterator>
  void findAll(OutputIterator out, const std::string& hostname,
               uint16_t port)
  {
    auto i = entries_.find({hostname, port});
    if (i != entries_.end()) {
      if (isExpired(i->second)) {
        entries_.erase(i);
        return;
      }
      i->second.getAllGoodAddrs(out);
    }
  }

  void put(const std::string& hostname, const std::string& ipaddr,
           uint16_t port);

  void markBad(const std::string& hostname, const std::string& ipaddr,
               uint16_t port);

  void remove(const std::string& hostname, uint16_t port);

  void setTTL(std::chrono::seconds ttl) { ttl_ = ttl; }

  std::chrono::seconds getTTL() const { return ttl_; }
};

} // namespace aria2

#endif // D_DNS_CACHE_H
