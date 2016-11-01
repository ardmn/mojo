// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of functions declared in <mojo/system/wait.h>.

#include <mojo/system/wait.h>

#include <magenta/syscalls.h>
#include <mojo/system/handle.h>
#include <mojo/system/result.h>

#include "mojo/system/mojo_export.h"
#include "mojo/system/time_utils.h"

MOJO_EXPORT MojoResult MojoWait(MojoHandle handle,
                                MojoHandleSignals signals,
                                MojoDeadline deadline,
                                struct MojoHandleSignalsState* signals_state) {
  mx_time_t mx_deadline = MojoDeadlineToTime(deadline);

  mx_signals_t pending;
  mx_status_t status = mx_handle_wait_one((mx_handle_t)handle, signals,
                                          mx_deadline, &pending);
  if (signals_state) {
    signals_state->satisfied_signals = pending;
    // NOTE: This is wrong (and will break the tests) but no one should be
    // relying on this state anymore and we will be deleting all of this soon.
    signals_state->satisfiable_signals = 0;
  }

  switch (status) {
    case NO_ERROR:
      return MOJO_RESULT_OK;
    case ERR_HANDLE_CLOSED:
      return MOJO_SYSTEM_RESULT_CANCELLED;
    // TODO(vtl): This is currently not specified for MojoWait(), nor does
    // mx_handle_wait_one() currently return this.
    case ERR_NO_MEMORY:
      return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
    case ERR_BAD_HANDLE:
    case ERR_INVALID_ARGS:
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    case ERR_TIMED_OUT:
      return MOJO_SYSTEM_RESULT_DEADLINE_EXCEEDED;
    case ERR_BAD_STATE:
      return MOJO_SYSTEM_RESULT_FAILED_PRECONDITION;
    // TODO(vtl): The Mojo version doesn't require any rights to wait, whereas
    // Magenta requires MX_RIGHT_READ.
    case ERR_ACCESS_DENIED:
      return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}

MOJO_EXPORT MojoResult
MojoWaitMany(const MojoHandle* handles,
             const MojoHandleSignals* signals,
             uint32_t num_handles,
             MojoDeadline deadline,
             uint32_t* result_index,
             struct MojoHandleSignalsState* signals_states) {
  const mx_handle_t* mx_handles = (const mx_handle_t*)handles;
  const mx_signals_t* mx_signals = (const mx_signals_t*)signals;
  mx_time_t mx_deadline = MojoDeadlineToTime(deadline);
  mx_signals_state_t* mx_signals_state = (mx_signals_state_t*)signals_states;

  mx_status_t status =
      mx_handle_wait_many(num_handles, mx_handles, mx_signals, mx_deadline,
                          result_index, mx_signals_state);

  switch (status) {
    case NO_ERROR:
      return MOJO_RESULT_OK;
    case ERR_HANDLE_CLOSED:
      return MOJO_SYSTEM_RESULT_CANCELLED;
    case ERR_NO_MEMORY:
      return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
    case ERR_BAD_HANDLE:
    case ERR_INVALID_ARGS:
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    case ERR_TIMED_OUT:
      return MOJO_SYSTEM_RESULT_DEADLINE_EXCEEDED;
    case ERR_BAD_STATE:
      return MOJO_SYSTEM_RESULT_FAILED_PRECONDITION;
    // TODO(vtl): The Mojo version doesn't require any rights to wait, whereas
    // Magenta requires MX_RIGHT_READ.
    case ERR_ACCESS_DENIED:
      return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}
