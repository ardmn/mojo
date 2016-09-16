// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of functions declared in <mojo/system/event_pair.h>.

#include <mojo/system/event_pair.h>

#include <magenta/syscalls.h>

#include "mojo/system/mojo_export.h"

MOJO_EXPORT MojoResult
MojoCreateEventPair(const struct MojoCreateEventPairOptions* options,
                    MojoHandle* event_pair_handle0,
                    MojoHandle* event_pair_handle1) {
  if (options && options->flags != MOJO_CREATE_EVENT_PAIR_OPTIONS_FLAG_NONE)
    return MOJO_SYSTEM_RESULT_UNIMPLEMENTED;
  mx_handle_t mx_handles[2];
  mx_status_t status = mx_eventpair_create(mx_handles, 0u);
  if (status != NO_ERROR) {
    switch (status) {
      case ERR_INVALID_ARGS:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_NO_MEMORY:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  *event_pair_handle0 = (MojoHandle)mx_handles[0];
  *event_pair_handle1 = (MojoHandle)mx_handles[1];
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult MojoSignal(MojoHandle handle,
                                  MojoHandleSignals clear_signals,
                                  MojoHandleSignals set_signals) {
  mx_status_t status =
      mx_object_signal((mx_handle_t)handle, (mx_signals_t)clear_signals,
                       (mx_signals_t)set_signals);
  switch (status) {
    case NO_ERROR:
      return MOJO_RESULT_OK;
    case ERR_INVALID_ARGS:
    case ERR_BAD_HANDLE:
    case ERR_WRONG_TYPE:
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    case ERR_ACCESS_DENIED:
      return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
    case ERR_BAD_STATE:
      return MOJO_SYSTEM_RESULT_FAILED_PRECONDITION;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}
