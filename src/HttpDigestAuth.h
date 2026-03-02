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
#ifndef D_HTTP_DIGEST_AUTH_H
#define D_HTTP_DIGEST_AUTH_H

#include "common.h"

#include <string>

namespace aria2 {

struct DigestChallenge {
  std::string realm;
  std::string nonce;
  std::string opaque;
  std::string qop;
  std::string algorithm;
};

namespace http_digest_auth {

// Parse a WWW-Authenticate: Digest ... header value (without the
// "Digest " prefix). Returns true on success.
bool parseChallenge(const std::string& header, DigestChallenge& result);

// Compute H(data) using the given algorithm ("md5" or "sha-256").
// Returns lowercase hex string. Empty string on unsupported algo.
std::string hashHex(const std::string& algorithm, const std::string& data);

// Generate a random cnonce (16 hex characters).
std::string generateCnonce();

// Compute the full Authorization header value for HTTP Digest auth
// (RFC 7616). Returns the complete "Digest username=..., ..." string.
std::string computeAuthHeader(const std::string& user,
                              const std::string& password,
                              const std::string& method, const std::string& uri,
                              const DigestChallenge& challenge, uint32_t nc);

// Map RFC 7616 algorithm names (e.g. "MD5", "SHA-256") to
// MessageDigest hash type strings (e.g. "md5", "sha-256").
std::string mapAlgorithm(const std::string& algorithm);

} // namespace http_digest_auth
} // namespace aria2

#endif // D_HTTP_DIGEST_AUTH_H
