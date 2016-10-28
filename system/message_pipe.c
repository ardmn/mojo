// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of functions declared in <mojo/system/message_pipe.h>.

#include <mojo/system/message_pipe.h>

#include <magenta/syscalls.h>
#include <mojo/system/message_pipe.h>
#include <mojo/system/result.h>

#include "mojo/system/mojo_export.h"

MOJO_EXPORT MojoResult
MojoCreateMessagePipe(const struct MojoCreateMessagePipeOptions* options,
                      MojoHandle* message_pipe_handle0,
                      MojoHandle* message_pipe_handle1) {
  if (options && options->flags != MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE)
    return MOJO_SYSTEM_RESULT_UNIMPLEMENTED;
  mx_handle_t mx_handles[2];
  mx_status_t status = mx_channel_create(0u, &mx_handles[0], &mx_handles[1]);
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
  *message_pipe_handle0 = (MojoHandle)mx_handles[0];
  *message_pipe_handle1 = (MojoHandle)mx_handles[1];
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult MojoWriteMessage(MojoHandle message_pipe_handle,
                                        const void* bytes,
                                        uint32_t num_bytes,
                                        const MojoHandle* handles,
                                        uint32_t num_handles,
                                        MojoWriteMessageFlags flags) {
  mx_handle_t* mx_handles = (mx_handle_t*)handles;
  // TODO(abarth): Handle messages that are too big to fit.
  mx_status_t status =
      mx_channel_write((mx_handle_t)message_pipe_handle, flags, bytes,
                       num_bytes, mx_handles, num_handles);
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
    case ERR_NO_MEMORY:
      return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
    // TODO(abarth): Handle messages that are too big to fit.
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}

MOJO_EXPORT MojoResult MojoReadMessage(MojoHandle message_pipe_handle,
                                       void* bytes,
                                       uint32_t* num_bytes,
                                       MojoHandle* handles,
                                       uint32_t* num_handles,
                                       MojoReadMessageFlags flags) {
  mx_handle_t* mx_handles = (mx_handle_t*)handles;
  // TODO(abarth): Handle messages that were too big to fit.
  uint32_t nbytes = num_bytes ? *num_bytes : 0u;
  uint32_t nhandles = num_handles ? *num_handles : 0u;
  mx_status_t status =
      mx_channel_read((mx_handle_t)message_pipe_handle, flags, bytes,
                      nbytes, num_bytes, mx_handles, nhandles,
                      num_handles);
  switch (status) {
    case NO_ERROR:
      return MOJO_RESULT_OK;
    case ERR_INVALID_ARGS:
    case ERR_BAD_HANDLE:
    case ERR_WRONG_TYPE:
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    case ERR_ACCESS_DENIED:
      return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
    case ERR_SHOULD_WAIT:
      return MOJO_SYSTEM_RESULT_SHOULD_WAIT;
    case ERR_REMOTE_CLOSED:
      return MOJO_SYSTEM_RESULT_FAILED_PRECONDITION;
    case ERR_NO_MEMORY:
      // Notice the collision with ERR_NOT_ENOUGH_BUFFER.
      return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
    case ERR_BUFFER_TOO_SMALL:
      return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}
