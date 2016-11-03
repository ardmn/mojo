// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of functions declared in <mojo/system/wait_set.h>.

#include <mojo/system/wait_set.h>

#include <assert.h>
#include <magenta/syscalls.h>
#include <mojo/system/handle.h>
#include <mojo/system/result.h>
#include <mojo/system/wait_set.h>

#include "mojo/system/mojo_export.h"
#include "mojo/system/time_utils.h"

// Like |offsetof()|, but includes that data itself.
// TODO(vtl): This isn't quite right/safe: even if |member_name| is within
// |EXTENT_OF(struct_type, member_name)|, looking at
// |struct_instance->member_name| might not be safe.
#define EXTENT_OF(struct_type, member_name) \
  (offsetof(struct_type, member_name) + sizeof(((struct_type*)0)->member_name))

MOJO_EXPORT MojoResult
MojoCreateWaitSet(const struct MojoCreateWaitSetOptions* options,
                  MojoHandle* handle) {
  if (options) {
    if (options->struct_size <
        EXTENT_OF(struct MojoCreateWaitSetOptions, struct_size))
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    if (options->struct_size >=
        EXTENT_OF(struct MojoCreateWaitSetOptions, flags)) {
      // Currently no known flags.
      if (options->flags)
        return MOJO_SYSTEM_RESULT_UNIMPLEMENTED;
    }
  }

  mx_handle_t wait_set = MX_HANDLE_INVALID;
  mx_status_t status = mx_waitset_create(0u, &wait_set);
  if (status < 0) {
    switch (status) {
      case ERR_NO_MEMORY:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  *handle = (MojoHandle)wait_set;
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult
MojoWaitSetAdd(MojoHandle wait_set_handle,
               MojoHandle handle,
               MojoHandleSignals signals,
               uint64_t cookie,
               const struct MojoWaitSetAddOptions* options) {
  if (options) {
    if (options->struct_size <
        EXTENT_OF(struct MojoWaitSetAddOptions, struct_size))
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    if (options->struct_size >=
        EXTENT_OF(struct MojoWaitSetAddOptions, flags)) {
      // Currently no known flags.
      if (options->flags)
        return MOJO_SYSTEM_RESULT_UNIMPLEMENTED;
    }
  }

  mx_status_t status = mx_waitset_add((mx_handle_t)wait_set_handle, cookie,
                                      (mx_handle_t)handle, signals);
  switch (status) {
    case NO_ERROR:
      return MOJO_RESULT_OK;
    case ERR_NO_MEMORY:
      return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
    case ERR_INVALID_ARGS:
    case ERR_BAD_HANDLE:
    case ERR_WRONG_TYPE:
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    case ERR_ACCESS_DENIED:
      return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
    case ERR_NOT_SUPPORTED:
      // TODO(vtl): |mx_waitset_add()| returns this is |handle| is not
      // waitable. There's currently no Mojo/EDK equivalent. Maybe this should
      // be |MOJO_SYSTEM_RESULT_UNIMPLEMENTED| instead?
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    case ERR_ALREADY_EXISTS:
      return MOJO_SYSTEM_RESULT_ALREADY_EXISTS;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}

MOJO_EXPORT MojoResult MojoWaitSetRemove(MojoHandle wait_set_handle,
                                         uint64_t cookie) {
  mx_status_t status = mx_waitset_remove((mx_handle_t)wait_set_handle, cookie);
  switch (status) {
    case NO_ERROR:
      return MOJO_RESULT_OK;
    case ERR_INVALID_ARGS:
    case ERR_BAD_HANDLE:
    case ERR_WRONG_TYPE:
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    case ERR_ACCESS_DENIED:
      return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
    case ERR_NOT_FOUND:
      return MOJO_SYSTEM_RESULT_NOT_FOUND;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}

MOJO_EXPORT MojoResult MojoWaitSetWait(MojoHandle wait_set_handle,
                                       MojoDeadline deadline,
                                       uint32_t* num_results,
                                       struct MojoWaitSetResult* results,
                                       uint32_t* max_results) {
  uint32_t temp_results = *num_results;
  mx_status_t status = mx_waitset_wait(
      (mx_handle_t)wait_set_handle, MojoDeadlineToTime(deadline),
      (mx_waitset_result_t*)results, &temp_results);
  switch (status) {
    case NO_ERROR:
      *max_results = temp_results;
      return MOJO_RESULT_OK;
    case ERR_NO_MEMORY:
      return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
    case ERR_INVALID_ARGS:
    case ERR_BAD_HANDLE:
    case ERR_WRONG_TYPE:
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    case ERR_ACCESS_DENIED:
      return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
    case ERR_TIMED_OUT:
      return MOJO_SYSTEM_RESULT_DEADLINE_EXCEEDED;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}
