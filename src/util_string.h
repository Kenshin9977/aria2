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
#ifndef D_UTIL_STRING_H
#define D_UTIL_STRING_H

#include "common.h"

#include <cstring>
#include <string>
#include <string_view>
#include <algorithm>
#include <iterator>
#include <utility>

namespace aria2 {

namespace util {

extern const char DEFAULT_STRIP_CHARSET[];

template <typename InputIterator>
std::pair<InputIterator, InputIterator>
stripIter(InputIterator first, InputIterator last,
          const char* chars = DEFAULT_STRIP_CHARSET)
{
  for (; first != last && strchr(chars, *first) != 0; ++first)
    ;
  if (first == last) {
    return std::make_pair(first, last);
  }
  InputIterator left = last - 1;
  for (; left != first && strchr(chars, *left) != 0; --left)
    ;
  return std::make_pair(first, left + 1);
}

template <typename InputIterator>
InputIterator lstripIter(InputIterator first, InputIterator last, char ch)
{
  for (; first != last && *first == ch; ++first)
    ;
  return first;
}

template <typename InputIterator>
InputIterator lstripIter(InputIterator first, InputIterator last,
                         const char* chars)
{
  for (; first != last && strchr(chars, *first) != 0; ++first)
    ;
  return first;
}

template <typename InputIterator>
InputIterator lstripIter(InputIterator first, InputIterator last)
{
  return lstripIter(first, last, DEFAULT_STRIP_CHARSET);
}

std::string strip(const std::string& str,
                  const char* chars = DEFAULT_STRIP_CHARSET);

std::string replace(const std::string& target, const std::string& oldstr,
                    const std::string& newstr);

template <typename InputIterator>
std::pair<std::pair<InputIterator, InputIterator>,
          std::pair<InputIterator, InputIterator>>
divide(InputIterator first, InputIterator last, char delim, bool strip = true)
{
  auto dpos = std::find(first, last, delim);
  if (dpos == last) {
    if (strip) {
      return {stripIter(first, last), {last, last}};
    }
    return {{first, last}, {last, last}};
  }

  if (strip) {
    return {stripIter(first, dpos), stripIter(dpos + 1, last)};
  }
  return {{first, dpos}, {dpos + 1, last}};
}

template <typename InputIterator, typename OutputIterator>
OutputIterator splitIter(InputIterator first, InputIterator last,
                         OutputIterator out, char delim, bool doStrip = false,
                         bool allowEmpty = false)
{
  for (InputIterator i = first; i != last;) {
    InputIterator j = std::find(i, last, delim);
    std::pair<InputIterator, InputIterator> p(i, j);
    if (doStrip) {
      p = stripIter(i, j);
    }
    if (allowEmpty || p.first != p.second) {
      *out++ = p;
    }
    i = j;
    if (j != last) {
      ++i;
    }
  }
  if (allowEmpty && (first == last || *(last - 1) == delim)) {
    *out++ = std::make_pair(last, last);
  }
  return out;
}

template <typename InputIterator, typename OutputIterator>
OutputIterator splitIterM(InputIterator first, InputIterator last,
                          OutputIterator out, const char* delims,
                          bool doStrip = false, bool allowEmpty = false)
{
  size_t numDelims = strlen(delims);
  const char* dlast = delims + numDelims;
  for (InputIterator i = first; i != last;) {
    InputIterator j = i;
    for (; j != last && std::find(delims, dlast, *j) == dlast; ++j)
      ;
    std::pair<InputIterator, InputIterator> p(i, j);
    if (doStrip) {
      p = stripIter(i, j);
    }
    if (allowEmpty || p.first != p.second) {
      *out++ = p;
    }
    i = j;
    if (j != last) {
      ++i;
    }
  }
  if (allowEmpty &&
      (first == last || std::find(delims, dlast, *(last - 1)) != dlast)) {
    *out++ = std::make_pair(last, last);
  }
  return out;
}

template <typename InputIterator, typename OutputIterator>
OutputIterator split(InputIterator first, InputIterator last,
                     OutputIterator out, char delim, bool doStrip = false,
                     bool allowEmpty = false)
{
  for (InputIterator i = first; i != last;) {
    InputIterator j = std::find(i, last, delim);
    std::pair<InputIterator, InputIterator> p(i, j);
    if (doStrip) {
      p = stripIter(i, j);
    }
    if (allowEmpty || p.first != p.second) {
      *out++ = std::string(p.first, p.second);
    }
    i = j;
    if (j != last) {
      ++i;
    }
  }
  if (allowEmpty && (first == last || *(last - 1) == delim)) {
    *out++ = std::string(last, last);
  }
  return out;
}

template <typename InputIterator1, typename InputIterator2>
bool streq(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2,
           InputIterator2 last2)
{
  if (last1 - first1 != last2 - first2) {
    return false;
  }
  return std::equal(first1, last1, first2);
}

template <typename InputIterator>
bool streq(InputIterator first, InputIterator last, const char* b)
{
  for (; first != last && *b != '\0'; ++first, ++b) {
    if (*first != *b) {
      return false;
    }
  }
  return first == last && *b == '\0';
}

struct CaseCmp {
  bool operator()(char lhs, char rhs) const
  {
    if ('A' <= lhs && lhs <= 'Z') {
      lhs += 'a' - 'A';
    }
    if ('A' <= rhs && rhs <= 'Z') {
      rhs += 'a' - 'A';
    }
    return lhs == rhs;
  }
};

template <typename InputIterator1, typename InputIterator2>
InputIterator1 strifind(InputIterator1 first1, InputIterator1 last1,
                        InputIterator2 first2, InputIterator2 last2)
{
  return std::search(first1, last1, first2, last2, CaseCmp());
}

template <typename InputIterator1, typename InputIterator2>
bool strieq(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2,
            InputIterator2 last2)
{
  if (last1 - first1 != last2 - first2) {
    return false;
  }
  return std::equal(first1, last1, first2, CaseCmp());
}

template <typename InputIterator>
bool strieq(InputIterator first, InputIterator last, const char* b)
{
  CaseCmp cmp;
  for (; first != last && *b != '\0'; ++first, ++b) {
    if (!cmp(*first, *b)) {
      return false;
    }
  }
  return first == last && *b == '\0';
}

bool strieq(std::string_view a, std::string_view b);

template <typename InputIterator1, typename InputIterator2>
bool startsWith(InputIterator1 first1, InputIterator1 last1,
                InputIterator2 first2, InputIterator2 last2)
{
  if (last1 - first1 < last2 - first2) {
    return false;
  }
  return std::equal(first2, last2, first1);
}

template <typename InputIterator>
bool startsWith(InputIterator first, InputIterator last, const char* b)
{
  for (; first != last && *b != '\0'; ++first, ++b) {
    if (*first != *b) {
      return false;
    }
  }
  return *b == '\0';
}

bool startsWith(std::string_view a, std::string_view b);

template <typename InputIterator1, typename InputIterator2>
bool istartsWith(InputIterator1 first1, InputIterator1 last1,
                 InputIterator2 first2, InputIterator2 last2)
{
  if (last1 - first1 < last2 - first2) {
    return false;
  }
  return std::equal(first2, last2, first1, CaseCmp());
}

template <typename InputIterator>
bool istartsWith(InputIterator first, InputIterator last, const char* b)
{
  CaseCmp cmp;
  for (; first != last && *b != '\0'; ++first, ++b) {
    if (!cmp(*first, *b)) {
      return false;
    }
  }
  return *b == '\0';
}

bool istartsWith(std::string_view a, std::string_view b);

template <typename InputIterator1, typename InputIterator2>
bool endsWith(InputIterator1 first1, InputIterator1 last1,
              InputIterator2 first2, InputIterator2 last2)
{
  if (last1 - first1 < last2 - first2) {
    return false;
  }
  return std::equal(first2, last2, last1 - (last2 - first2));
}

bool endsWith(std::string_view a, std::string_view b);

template <typename InputIterator1, typename InputIterator2>
bool iendsWith(InputIterator1 first1, InputIterator1 last1,
               InputIterator2 first2, InputIterator2 last2)
{
  if (last1 - first1 < last2 - first2) {
    return false;
  }
  return std::equal(first2, last2, last1 - (last2 - first2), CaseCmp());
}

bool iendsWith(std::string_view a, std::string_view b);

template <typename InputIterator>
bool isNumber(InputIterator first, InputIterator last)
{
  if (first == last) {
    return false;
  }
  for (; first != last; ++first) {
    if ('0' > *first || *first > '9') {
      return false;
    }
  }
  return true;
}

template <typename InputIterator>
bool isLowercase(InputIterator first, InputIterator last)
{
  if (first == last) {
    return false;
  }
  for (; first != last; ++first) {
    if ('a' > *first || *first > 'z') {
      return false;
    }
  }
  return true;
}

template <typename InputIterator>
bool isUppercase(InputIterator first, InputIterator last)
{
  if (first == last) {
    return false;
  }
  for (; first != last; ++first) {
    if ('A' > *first || *first > 'Z') {
      return false;
    }
  }
  return true;
}

std::string toUpper(std::string src);

std::string toLower(std::string src);

void uppercase(std::string& s);

void lowercase(std::string& s);

char toUpperChar(char c);

char toLowerChar(char c);

bool isAlpha(const char c);

bool isDigit(const char c);

bool isHexDigit(const char c);

bool isHexDigit(std::string_view s);

bool isLws(const char c);

bool isCRLF(const char c);

} // namespace util

} // namespace aria2

#endif // D_UTIL_STRING_H
