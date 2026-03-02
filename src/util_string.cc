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
#include "util_string.h"

#include <algorithm>
#include <ranges>

namespace aria2 {

namespace util {

constexpr char DEFAULT_STRIP_CHARSET[] = "\r\n\t ";

std::string strip(const std::string& str, const char* chars)
{
  std::pair<std::string::const_iterator, std::string::const_iterator> p =
      stripIter(str.begin(), str.end(), chars);
  return std::string(p.first, p.second);
}

std::string replace(const std::string& target, const std::string& oldstr,
                    const std::string& newstr)
{
  if (target.empty() || oldstr.empty()) {
    return target;
  }
  std::string result;
  std::string::size_type p = 0;
  std::string::size_type np = target.find(oldstr);
  while (np != std::string::npos) {
    result.append(target.begin() + p, target.begin() + np);
    result += newstr;
    p = np + oldstr.size();
    np = target.find(oldstr, p);
  }
  result.append(target.begin() + p, target.end());
  return result;
}

bool isAlpha(const char c)
{
  return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

bool isDigit(const char c) { return '0' <= c && c <= '9'; }

bool isHexDigit(const char c)
{
  return isDigit(c) || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
}

bool isHexDigit(std::string_view s)
{
  return std::ranges::all_of(
      s, [](char c) { return isHexDigit(c); });
}

bool isLws(const char c) { return c == ' ' || c == '\t'; }
bool isCRLF(const char c) { return c == '\r' || c == '\n'; }

char toUpperChar(char c)
{
  if ('a' <= c && c <= 'z') {
    c += 'A' - 'a';
  }
  return c;
}

char toLowerChar(char c)
{
  if ('A' <= c && c <= 'Z') {
    c += 'a' - 'A';
  }
  return c;
}

std::string toUpper(std::string src)
{
  uppercase(src);
  return src;
}

std::string toLower(std::string src)
{
  lowercase(src);
  return src;
}

void uppercase(std::string& s)
{
  std::ranges::transform(s, s.begin(), toUpperChar);
}

void lowercase(std::string& s)
{
  std::ranges::transform(s, s.begin(), toLowerChar);
}

bool strieq(std::string_view a, std::string_view b)
{
  return strieq(a.begin(), a.end(), b.begin(), b.end());
}

bool startsWith(std::string_view a, std::string_view b)
{
  return startsWith(a.begin(), a.end(), b.begin(), b.end());
}

bool istartsWith(std::string_view a, std::string_view b)
{
  return istartsWith(a.begin(), a.end(), b.begin(), b.end());
}

bool endsWith(std::string_view a, std::string_view b)
{
  return endsWith(a.begin(), a.end(), b.begin(), b.end());
}

bool iendsWith(std::string_view a, std::string_view b)
{
  return iendsWith(a.begin(), a.end(), b.begin(), b.end());
}

} // namespace util

} // namespace aria2
