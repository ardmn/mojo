// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_TIME_UTILS_H_
#define MOJO_SYSTEM_TIME_UTILS_H_

#include <magenta/types.h>
#include <mojo/system/time.h>

// MojoTimeTicks is in microseconds.
// mx_time_t is in nanoseconds.

inline MojoTimeTicks TimeToMojoTicks(mx_time_t t) {
  return (MojoTimeTicks)t / 1000u;
}

inline mx_time_t MojoDeadlineToTime(MojoDeadline deadline) {
  if (deadline == MOJO_DEADLINE_INDEFINITE)
    return MX_TIME_INFINITE;
  // TODO(abarth): Handle overflow.
  return (mx_time_t)deadline * 1000u;
}

#endif  // MOJO_SYSTEM_TIME_UTILS_H_
