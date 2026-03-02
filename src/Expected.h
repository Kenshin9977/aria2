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
#ifndef D_EXPECTED_H
#define D_EXPECTED_H

#include <type_traits>
#include <variant>
#include <utility>

namespace aria2 {

template <typename E> struct Unexpected {
  E error;
  explicit Unexpected(E e) : error(std::move(e)) {}
};

template <typename E> Unexpected<E> makeUnexpected(E e)
{
  return Unexpected<E>(std::move(e));
}

template <typename T, typename E> class Expected {
public:
  Expected(T val) : data_(std::move(val)) {}
  Expected(Unexpected<E> err) : data_(std::move(err.error)) {}

  template <typename U,
            typename = std::enable_if_t<std::is_constructible_v<E, U>>>
  Expected(Unexpected<U> err) : data_(E(std::move(err.error)))
  {
  }

  explicit operator bool() const noexcept
  {
    return std::holds_alternative<T>(data_);
  }
  bool has_value() const noexcept { return std::holds_alternative<T>(data_); }

  T& operator*() & { return std::get<T>(data_); }
  const T& operator*() const& { return std::get<T>(data_); }
  T&& operator*() && { return std::get<T>(std::move(data_)); }

  T* operator->() { return &std::get<T>(data_); }
  const T* operator->() const { return &std::get<T>(data_); }

  T& value() & { return std::get<T>(data_); }
  const T& value() const& { return std::get<T>(data_); }

  E& error() & { return std::get<E>(data_); }
  const E& error() const& { return std::get<E>(data_); }

private:
  std::variant<T, E> data_;
};

} // namespace aria2

#endif // D_EXPECTED_H
