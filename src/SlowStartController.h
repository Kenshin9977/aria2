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
#ifndef D_SLOW_START_CONTROLLER_H
#define D_SLOW_START_CONTROLLER_H

#include "common.h"

#include <algorithm>

namespace aria2 {

// Controls per-host connection ramp-up using TCP-style AIMD
// (additive increase, multiplicative decrease). Only activates
// when ceiling exceeds THRESHOLD (default 4). Below that, all
// connections are allowed immediately (backward compatible).
//
// When active: starts at 1 connection, doubles when speed
// increases by >= 10%, halves on 403 or connection failure.
class SlowStartController {
public:
  // Slow-start only engages when ceiling > THRESHOLD
  static const int THRESHOLD = 4;

  explicit SlowStartController(int ceiling = 1)
      : ceiling_(std::max(1, ceiling)),
        allowed_(computeInitial(ceiling)),
        lastSpeed_(0)
  {
  }

  // Returns the current effective connection limit.
  // If ceiling <= THRESHOLD, returns ceiling (no slow-start).
  int getAllowedConnections() const { return allowed_; }

  int getCeiling() const { return ceiling_; }

  void setCeiling(int ceiling)
  {
    ceiling_ = std::max(1, ceiling);
    allowed_ = computeInitial(ceiling_);
    lastSpeed_ = 0;
  }

  bool isActive() const { return ceiling_ > THRESHOLD; }

  // Call periodically with aggregate speed (bytes/sec).
  // Only has effect when slow-start is active (ceiling > THRESHOLD).
  // Doubles allowed if speed increased by >= 10%.
  void update(int currentSpeed)
  {
    if (!isActive()) {
      return;
    }
    if (lastSpeed_ > 0 && currentSpeed > lastSpeed_) {
      double gain =
          static_cast<double>(currentSpeed - lastSpeed_) / lastSpeed_;
      if (gain >= 0.10) {
        allowed_ = std::min(allowed_ * 2, ceiling_);
      }
    }
    lastSpeed_ = currentSpeed;
  }

  // Halve on 403 or connection failure. Floor is 1.
  // Only has effect when slow-start is active.
  void backOff()
  {
    if (!isActive()) {
      return;
    }
    allowed_ = std::max(1, allowed_ / 2);
    lastSpeed_ = 0;
  }

  void reset()
  {
    allowed_ = computeInitial(ceiling_);
    lastSpeed_ = 0;
  }

private:
  static int computeInitial(int ceiling)
  {
    return (ceiling > THRESHOLD) ? 1 : ceiling;
  }

  int ceiling_;
  int allowed_;
  int lastSpeed_;
};

} // namespace aria2

#endif // D_SLOW_START_CONTROLLER_H
