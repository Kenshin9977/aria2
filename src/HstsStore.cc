/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2024 aria2 contributors
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
#include "HstsStore.h"

#include "util.h"
#include "LogFactory.h"
#include "fmt.h"

namespace aria2 {

void HstsStore::processHeader(const std::string& host, const std::string& value)
{
  int64_t maxAge = -1;
  bool includeSubdomains = false;

  // Parse HSTS directives: max-age=N; includeSubDomains
  std::string::size_type pos = 0;
  while (pos < value.size()) {
    auto semi = value.find(';', pos);
    auto directive = value.substr(
        pos, semi == std::string::npos ? std::string::npos : semi - pos);
    pos = (semi == std::string::npos) ? value.size() : semi + 1;

    // Trim whitespace
    auto start = directive.find_first_not_of(" \t");
    if (start == std::string::npos) {
      continue;
    }
    auto end = directive.find_last_not_of(" \t");
    directive = directive.substr(start, end - start + 1);

    if (util::istartsWith(directive, std::string_view("max-age="))) {
      auto val = directive.substr(8);
      auto r = util::parseLLInt(val);
      if (r) {
        maxAge = *r;
      }
    }
    else if (util::strieq(directive, std::string_view("includeSubDomains"))) {
      includeSubdomains = true;
    }
  }

  if (maxAge < 0) {
    return;
  }

  if (maxAge == 0) {
    entries_.erase(host);
    A2_LOG_DEBUG(fmt("HSTS: removed entry for %s", host.c_str()));
    return;
  }

  Entry entry;
  entry.expiry = std::time(nullptr) + maxAge;
  entry.includeSubdomains = includeSubdomains;
  entries_[host] = entry;
  A2_LOG_DEBUG(fmt("HSTS: added entry for %s (max-age=%" PRId64
                   ", includeSubDomains=%d)",
                   host.c_str(), maxAge, static_cast<int>(includeSubdomains)));
}

bool HstsStore::shouldUpgrade(const std::string& host) const
{
  time_t now = std::time(nullptr);

  // Direct match
  auto it = entries_.find(host);
  if (it != entries_.end() && it->second.expiry > now) {
    return true;
  }

  // Check parent domains for includeSubDomains
  std::string_view h = host;
  auto dot = h.find('.');
  while (dot != std::string_view::npos) {
    h = h.substr(dot + 1);
    auto parent = std::string(h);
    auto pit = entries_.find(parent);
    if (pit != entries_.end() && pit->second.includeSubdomains &&
        pit->second.expiry > now) {
      return true;
    }
    dot = h.find('.');
  }
  return false;
}

} // namespace aria2
