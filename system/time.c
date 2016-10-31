// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of functions declared in <mojo/system/time.h>.

#include <mojo/system/time.h>

#include <magenta/syscalls.h>

#include "mojo/system/mojo_export.h"
#include "mojo/system/time_utils.h"

MOJO_EXPORT MojoTimeTicks MojoGetTimeTicksNow() {
  return TimeToMojoTicks(mx_time_get(MX_CLOCK_MONOTONIC));
}
