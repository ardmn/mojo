// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of functions declared in <mojo/system/wait.h>.

#include <stdio.h>
#include <stdlib.h>

#include <mojo/system/wait.h>

#include "mojo/system/mojo_export.h"

MOJO_EXPORT MojoResult MojoWait(MojoHandle handle,
                                MojoHandleSignals signals,
                                MojoDeadline deadline,
                                struct MojoHandleSignalsState* signals_state) {
  fputs("Please use mx_handle_wait_one() instead of MojoWait().\n", stderr);
  abort();
}

MOJO_EXPORT MojoResult
MojoWaitMany(const MojoHandle* handles,
             const MojoHandleSignals* signals,
             uint32_t num_handles,
             MojoDeadline deadline,
             uint32_t* result_index,
             struct MojoHandleSignalsState* signals_states) {
  fputs("Please use mx_handle_wait_many() instead of MojoWaitMany().\n",
        stderr);
  abort();
}
