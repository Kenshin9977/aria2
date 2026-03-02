/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2024 Tatsuhiro Tsujikawa
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
#include "HttpDigestAuth.h"

#include <algorithm>
#include <cctype>

#include "MessageDigest.h"
#include "SimpleRandomizer.h"
#include "util.h"
#include "fmt.h"

namespace aria2 {
namespace http_digest_auth {

namespace {
// Skip whitespace
void skipWs(const std::string& s, size_t& pos)
{
  while (pos < s.size() && std::isspace(s[pos])) {
    ++pos;
  }
}

// Parse a token (RFC 7230 token characters)
std::string parseToken(const std::string& s, size_t& pos)
{
  size_t start = pos;
  while (pos < s.size() && s[pos] != '=' && s[pos] != ',' &&
         !std::isspace(s[pos])) {
    ++pos;
  }
  return s.substr(start, pos - start);
}

// Parse a quoted string or token value
std::string parseValue(const std::string& s, size_t& pos)
{
  if (pos < s.size() && s[pos] == '"') {
    ++pos; // skip opening quote
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
      if (s[pos] == '\\' && pos + 1 < s.size()) {
        ++pos;
      }
      result += s[pos];
      ++pos;
    }
    if (pos < s.size()) {
      ++pos; // skip closing quote
    }
    return result;
  }
  return parseToken(s, pos);
}
} // namespace

bool parseChallenge(const std::string& header, DigestChallenge& result)
{
  result = DigestChallenge{};

  size_t pos = 0;
  skipWs(header, pos);

  bool foundAny = false;
  while (pos < header.size()) {
    skipWs(header, pos);
    if (pos >= header.size()) {
      break;
    }

    auto key = parseToken(header, pos);
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    skipWs(header, pos);
    if (pos >= header.size() || header[pos] != '=') {
      break;
    }
    ++pos; // skip '='
    skipWs(header, pos);

    auto value = parseValue(header, pos);

    if (key == "realm") {
      result.realm = value;
      foundAny = true;
    }
    else if (key == "nonce") {
      result.nonce = value;
      foundAny = true;
    }
    else if (key == "opaque") {
      result.opaque = value;
      foundAny = true;
    }
    else if (key == "qop") {
      result.qop = value;
      foundAny = true;
    }
    else if (key == "algorithm") {
      result.algorithm = value;
      foundAny = true;
    }

    skipWs(header, pos);
    if (pos < header.size() && header[pos] == ',') {
      ++pos;
    }
  }

  return foundAny && !result.nonce.empty();
}

std::string mapAlgorithm(const std::string& algorithm)
{
  std::string lower = algorithm;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (lower.empty() || lower == "md5" || lower == "md5-sess") {
    return "md5";
  }
  if (lower == "sha-256" || lower == "sha-256-sess") {
    return "sha-256";
  }
  if (lower == "sha-512-256" || lower == "sha-512-256-sess") {
    return "sha-512";
  }
  return "";
}

std::string hashHex(const std::string& algorithm, const std::string& data)
{
  auto hashType = mapAlgorithm(algorithm);
  if (hashType.empty() || !MessageDigest::supports(hashType)) {
    return "";
  }
  auto md = MessageDigest::create(hashType);
  md->update(data.data(), data.size());
  auto raw = md->digest();
  return util::toHex(raw);
}

std::string generateCnonce()
{
  unsigned char buf[8];
  SimpleRandomizer::getInstance()->getRandomBytes(buf, sizeof(buf));
  return util::toHex(buf, sizeof(buf));
}

std::string computeAuthHeader(const std::string& user,
                              const std::string& password,
                              const std::string& method, const std::string& uri,
                              const DigestChallenge& challenge, uint32_t nc)
{
  auto algo = challenge.algorithm.empty() ? "MD5" : challenge.algorithm;

  // HA1 = H(username:realm:password)
  std::string a1 = user + ":" + challenge.realm + ":" + password;
  auto ha1 = hashHex(algo, a1);

  // For -sess variants, HA1 = H(H(username:realm:password):nonce:cnonce)
  std::string cnonce = generateCnonce();
  std::string algoLower = algo;
  std::transform(algoLower.begin(), algoLower.end(), algoLower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (algoLower.find("-sess") != std::string::npos) {
    std::string sessA1 = ha1 + ":" + challenge.nonce + ":" + cnonce;
    ha1 = hashHex(algo, sessA1);
  }

  // HA2 = H(method:uri)
  std::string a2 = method + ":" + uri;
  auto ha2 = hashHex(algo, a2);

  std::string ncStr = fmt("%08x", nc);

  // response = H(HA1:nonce:nc:cnonce:qop:HA2) when qop is specified
  std::string response;
  bool useQop = !challenge.qop.empty();
  std::string qopUsed = "auth";
  if (useQop) {
    std::string rdata = ha1 + ":" + challenge.nonce + ":" + ncStr + ":" +
                        cnonce + ":" + qopUsed + ":" + ha2;
    response = hashHex(algo, rdata);
  }
  else {
    // Legacy: response = H(HA1:nonce:HA2)
    std::string rdata = ha1 + ":" + challenge.nonce + ":" + ha2;
    response = hashHex(algo, rdata);
  }

  // Build the Authorization header value
  std::string result = "Digest username=\"" + user + "\"";
  result += ", realm=\"" + challenge.realm + "\"";
  result += ", nonce=\"" + challenge.nonce + "\"";
  result += ", uri=\"" + uri + "\"";
  result += ", response=\"" + response + "\"";
  result += ", algorithm=" + algo;
  if (!challenge.opaque.empty()) {
    result += ", opaque=\"" + challenge.opaque + "\"";
  }
  if (useQop) {
    result += ", qop=" + qopUsed;
    result += ", nc=" + ncStr;
    result += ", cnonce=\"" + cnonce + "\"";
  }

  return result;
}

} // namespace http_digest_auth
} // namespace aria2
