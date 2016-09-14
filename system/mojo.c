// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <magenta/process.h>
#include <magenta/syscalls.h>
#include <mojo/system/buffer.h>
#include <mojo/system/data_pipe.h>
#include <mojo/system/handle.h>
#include <mojo/system/message_pipe.h>
#include <mojo/system/result.h>
#include <mojo/system/wait.h>
#include <mojo/system/wait_set.h>
#include <stddef.h>
#include <string.h>

#include "mojo/system/mojo_export.h"

// MojoTimeTicks is in microseconds.
// mx_time_t is in nanoseconds.

// TODO(abarth): MojoTimeTicks probably shouldn't be signed.
static MojoTimeTicks TimeToMojoTicks(mx_time_t t) {
  return (MojoTimeTicks)t / 1000u;
}

static mx_time_t MojoDeadlineToTime(MojoDeadline deadline) {
  if (deadline == MOJO_DEADLINE_INDEFINITE)
    return MX_TIME_INFINITE;
  // TODO(abarth): Handle overflow.
  return (mx_time_t)deadline * 1000u;
}

// handle.h --------------------------------------------------------------------

static_assert(MOJO_HANDLE_RIGHT_NONE == MX_RIGHT_NONE, "RIGHT_NONE must match");
static_assert(MOJO_HANDLE_RIGHT_DUPLICATE == MX_RIGHT_DUPLICATE,
              "RIGHT_DUPLICATE must match");
static_assert(MOJO_HANDLE_RIGHT_TRANSFER == MX_RIGHT_TRANSFER,
              "RIGHT_TRANSFER must match");
static_assert(MOJO_HANDLE_RIGHT_READ == MX_RIGHT_READ, "RIGHT_READ must match");
static_assert(MOJO_HANDLE_RIGHT_WRITE == MX_RIGHT_WRITE,
              "RIGHT_WRITE must match");
static_assert(MOJO_HANDLE_RIGHT_EXECUTE == MX_RIGHT_EXECUTE,
              "RIGHT_EXECUTE must match");
// TODO(vtl): Add get/set options rights to Magenta, and debug to Mojo (and
// align their values).

MOJO_EXPORT MojoResult MojoClose(MojoHandle handle) {
  mx_status_t status = mx_handle_close((mx_handle_t)handle);
  switch (status) {
    case NO_ERROR:
      return MOJO_RESULT_OK;
    case ERR_BAD_HANDLE:
    case ERR_INVALID_ARGS:
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}

MOJO_EXPORT MojoResult MojoGetRights(MojoHandle handle,
                                     MojoHandleRights* rights) {
  mx_info_handle_basic_t handle_info;
  mx_ssize_t result = mx_object_get_info(
      (mx_handle_t)handle, MX_INFO_HANDLE_BASIC, sizeof(handle_info.rec),
      &handle_info, sizeof(handle_info));
  if (result < 0) {
    switch (result) {
      case ERR_BAD_HANDLE:
      case ERR_INVALID_ARGS:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      case ERR_NO_MEMORY:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  *rights = handle_info.rec.rights;
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult
MojoReplaceHandleWithReducedRights(MojoHandle handle,
                                   MojoHandleRights rights_to_remove,
                                   MojoHandle* replacement_handle) {
  MojoHandleRights original_rights;
  MojoResult result = MojoGetRights(handle, &original_rights);
  if (result != MOJO_RESULT_OK)
    return result;
  MojoHandleRights new_rights = original_rights & ~rights_to_remove;
  mx_handle_t new_mx_handle =
      mx_handle_replace((mx_handle_t)handle, new_rights);
  if (new_mx_handle < 0) {
    switch (new_mx_handle) {
      case ERR_BAD_HANDLE:
      case ERR_INVALID_ARGS:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      case ERR_NO_MEMORY:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  *replacement_handle = (MojoHandle)new_mx_handle;
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult
MojoDuplicateHandleWithReducedRights(MojoHandle handle,
                                     MojoHandleRights rights_to_remove,
                                     MojoHandle* new_handle) {
  MojoHandleRights original_rights;
  MojoResult result = MojoGetRights(handle, &original_rights);
  if (result != MOJO_RESULT_OK)
    return result;
  MojoHandleRights new_rights = original_rights & ~rights_to_remove;
  mx_handle_t new_mx_handle =
      mx_handle_duplicate((mx_handle_t)handle, new_rights);
  if (new_mx_handle < 0) {
    switch (new_mx_handle) {
      case ERR_BAD_HANDLE:
      case ERR_INVALID_ARGS:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      case ERR_NO_MEMORY:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  *new_handle = (MojoHandle)new_mx_handle;
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult MojoDuplicateHandle(MojoHandle handle,
                                           MojoHandle* new_handle) {
  mx_handle_t result =
      mx_handle_duplicate((mx_handle_t)handle, MX_RIGHT_SAME_RIGHTS);
  if (result < 0) {
    switch (result) {
      case ERR_BAD_HANDLE:
      case ERR_INVALID_ARGS:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      case ERR_NO_MEMORY:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  *new_handle = (MojoHandle)result;
  return MOJO_RESULT_OK;
}

// time.h ----------------------------------------------------------------------

MOJO_EXPORT MojoTimeTicks MojoGetTimeTicksNow() {
  return TimeToMojoTicks(mx_current_time());
}

// wait.h ----------------------------------------------------------------------

static_assert(MOJO_HANDLE_SIGNAL_NONE == MX_SIGNAL_NONE,
              "SIGNAL_NONE must match");
static_assert(MOJO_HANDLE_SIGNAL_READABLE == MX_SIGNAL_READABLE,
              "SIGNAL_READABLE must match");
static_assert(MOJO_HANDLE_SIGNAL_WRITABLE == MX_SIGNAL_WRITABLE,
              "SIGNAL_WRITABLE must match");
static_assert(MOJO_HANDLE_SIGNAL_PEER_CLOSED == MX_SIGNAL_PEER_CLOSED,
              "PEER_CLOSED must match");
static_assert(sizeof(struct MojoHandleSignalsState) ==
                  sizeof(mx_signals_state_t),
              "Signals state structs must match");
static_assert(offsetof(struct MojoHandleSignalsState, satisfied_signals) ==
                  offsetof(mx_signals_state_t, satisfied),
              "Signals state structs must match");
static_assert(offsetof(struct MojoHandleSignalsState, satisfiable_signals) ==
                  offsetof(mx_signals_state_t, satisfiable),
              "Signals state structs must match");

MOJO_EXPORT MojoResult MojoWait(MojoHandle handle,
                                MojoHandleSignals signals,
                                MojoDeadline deadline,
                                struct MojoHandleSignalsState* signals_state) {
  mx_time_t mx_deadline = MojoDeadlineToTime(deadline);
  mx_signals_state_t* mx_signals_state = (mx_signals_state_t*)signals_state;

  mx_status_t status = mx_handle_wait_one((mx_handle_t)handle, signals,
                                          mx_deadline, mx_signals_state);

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

// message_pipe.h --------------------------------------------------------------

MOJO_EXPORT MojoResult
MojoCreateMessagePipe(const struct MojoCreateMessagePipeOptions* options,
                      MojoHandle* message_pipe_handle0,
                      MojoHandle* message_pipe_handle1) {
  if (options && options->flags != MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE)
    return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
  mx_handle_t mx_handles[2];
  mx_status_t status = mx_msgpipe_create(mx_handles, 0);
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
      mx_msgpipe_write((mx_handle_t)message_pipe_handle, bytes, num_bytes,
                       mx_handles, num_handles, flags);
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
      // Notice the different semantics than mx_msgpipe_read.
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
  mx_status_t status =
      mx_msgpipe_read((mx_handle_t)message_pipe_handle, bytes, num_bytes,
                      mx_handles, num_handles, flags);
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
      // Notice the different semantics than mx_msgpipe_write.
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

// data_pipe.h -----------------------------------------------------------------

static_assert(MOJO_WRITE_DATA_FLAG_ALL_OR_NONE ==
                  MX_DATAPIPE_WRITE_FLAG_ALL_OR_NONE,
              "Data pipe write flags must match");
static_assert(MOJO_WRITE_DATA_FLAG_ALL_OR_NONE == MX_DATAPIPE_WRITE_FLAG_MASK,
              "Unknown Magenta data pipe write flag(s)");

static_assert(MOJO_READ_DATA_FLAG_ALL_OR_NONE ==
                  MX_DATAPIPE_READ_FLAG_ALL_OR_NONE,
              "Data pipe read flags must match");
static_assert(MOJO_READ_DATA_FLAG_DISCARD == MX_DATAPIPE_READ_FLAG_DISCARD,
              "Data pipe read flags must match");
static_assert(MOJO_READ_DATA_FLAG_QUERY == MX_DATAPIPE_READ_FLAG_QUERY,
              "Data pipe read flags must match");
static_assert(MOJO_READ_DATA_FLAG_PEEK == MX_DATAPIPE_READ_FLAG_PEEK,
              "Data pipe read flags must match");
static_assert(MOJO_READ_DATA_FLAG_ALL_OR_NONE | MOJO_READ_DATA_FLAG_DISCARD |
                  MOJO_READ_DATA_FLAG_QUERY |
                  MOJO_READ_DATA_FLAG_PEEK == MX_DATAPIPE_WRITE_FLAG_MASK,
              "Unknown Magenta data pipe read flag(s)");

MOJO_EXPORT MojoResult
MojoCreateDataPipe(const struct MojoCreateDataPipeOptions* options,
                   MojoHandle* data_pipe_producer_handle,
                   MojoHandle* data_pipe_consumer_handle) {
  uint32_t element_num_bytes = 1u;
  uint32_t capacity_num_bytes = 0u;
  if (options) {
    if (options->flags) {
      // TODO: Support flags
      return MOJO_SYSTEM_RESULT_UNIMPLEMENTED;
    }
    element_num_bytes = options->element_num_bytes;
    if (options->capacity_num_bytes)
      capacity_num_bytes = options->capacity_num_bytes;
  }
  mx_handle_t mx_consumer_handle = 0u;
  mx_handle_t mx_producer_handle = mx_datapipe_create(
      0u,  // TODO: Flags
      element_num_bytes, capacity_num_bytes, &mx_consumer_handle);
  if (mx_producer_handle < 0) {
    switch (mx_producer_handle) {
      case ERR_INVALID_ARGS:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_NO_MEMORY:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  *data_pipe_producer_handle = (MojoHandle)mx_producer_handle;
  *data_pipe_consumer_handle = (MojoHandle)mx_consumer_handle;
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult MojoSetDataPipeProducerOptions(
    MojoHandle data_pipe_producer_handle,
    const struct MojoDataPipeProducerOptions* options) {
  return MOJO_SYSTEM_RESULT_UNIMPLEMENTED;
}

MOJO_EXPORT MojoResult
MojoGetDataPipeProducerOptions(MojoHandle data_pipe_producer_handle,
                               struct MojoDataPipeProducerOptions* options,
                               uint32_t options_num_bytes) {
  return MOJO_SYSTEM_RESULT_UNIMPLEMENTED;
}

MOJO_EXPORT MojoResult MojoWriteData(MojoHandle data_pipe_producer_handle,
                                     const void* elements,
                                     uint32_t* num_bytes,
                                     MojoWriteDataFlags flags) {
  mx_ssize_t mx_bytes_written = mx_datapipe_write(
      (mx_handle_t)data_pipe_producer_handle, flags, *num_bytes, elements);
  if (mx_bytes_written < 0) {
    switch (mx_bytes_written) {
      case ERR_INVALID_ARGS:
      case ERR_BAD_HANDLE:
      case ERR_WRONG_TYPE:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      case ERR_BAD_STATE:
      case ERR_REMOTE_CLOSED:
        return MOJO_SYSTEM_RESULT_FAILED_PRECONDITION;
      case ERR_OUT_OF_RANGE:
        return MOJO_SYSTEM_RESULT_OUT_OF_RANGE;
      case ERR_SHOULD_WAIT:
        return MOJO_SYSTEM_RESULT_SHOULD_WAIT;
      case ERR_ALREADY_BOUND:
        return MOJO_SYSTEM_RESULT_BUSY;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  *num_bytes = mx_bytes_written;
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult MojoBeginWriteData(MojoHandle data_pipe_producer_handle,
                                          void** buffer,
                                          uint32_t* buffer_num_bytes,
                                          MojoWriteDataFlags flags) {
  mx_ssize_t result = mx_datapipe_begin_write(
      (mx_handle_t)data_pipe_producer_handle, flags, (uintptr_t*)buffer);
  if (result < 0) {
    switch (result) {
      case ERR_INVALID_ARGS:
      case ERR_BAD_HANDLE:
      case ERR_WRONG_TYPE:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      case ERR_BAD_STATE:
      case ERR_REMOTE_CLOSED:
        return MOJO_SYSTEM_RESULT_FAILED_PRECONDITION;
      case ERR_SHOULD_WAIT:
        return MOJO_SYSTEM_RESULT_SHOULD_WAIT;
      case ERR_ALREADY_BOUND:
        return MOJO_SYSTEM_RESULT_BUSY;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  *buffer_num_bytes = (uint32_t)result;
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult MojoEndWriteData(MojoHandle data_pipe_producer_handle,
                                        uint32_t num_bytes_written) {
  mx_status_t result = mx_datapipe_end_write(
      (mx_handle_t)data_pipe_producer_handle, num_bytes_written);
  switch (result) {
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
      return MOJO_SYSTEM_RESULT_INTERNAL;
  }
}

MOJO_EXPORT MojoResult MojoSetDataPipeConsumerOptions(
    MojoHandle data_pipe_consumer_handle,
    const struct MojoDataPipeConsumerOptions* options) {
  return MOJO_SYSTEM_RESULT_UNIMPLEMENTED;
}

MOJO_EXPORT MojoResult
MojoGetDataPipeConsumerOptions(MojoHandle data_pipe_consumer_handle,
                               struct MojoDataPipeConsumerOptions* options,
                               uint32_t options_num_bytes) {
  return MOJO_SYSTEM_RESULT_UNIMPLEMENTED;
}

MOJO_EXPORT MojoResult MojoReadData(MojoHandle data_pipe_consumer_handle,
                                    void* elements,
                                    uint32_t* num_bytes,
                                    MojoReadDataFlags flags) {
  mx_ssize_t bytes_read = mx_datapipe_read(
      (mx_handle_t)data_pipe_consumer_handle, flags, *num_bytes, elements);
  if (bytes_read < 0) {
    switch (bytes_read) {
      case ERR_INVALID_ARGS:
      case ERR_BAD_HANDLE:
      case ERR_WRONG_TYPE:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      case ERR_BAD_STATE:
      case ERR_REMOTE_CLOSED:
        return MOJO_SYSTEM_RESULT_FAILED_PRECONDITION;
      case ERR_OUT_OF_RANGE:
        return MOJO_SYSTEM_RESULT_OUT_OF_RANGE;
      case ERR_SHOULD_WAIT:
        return MOJO_SYSTEM_RESULT_SHOULD_WAIT;
      case ERR_ALREADY_BOUND:
        return MOJO_SYSTEM_RESULT_BUSY;
      default:
        return MOJO_SYSTEM_RESULT_INTERNAL;
    }
  }
  if (bytes_read < 0) {
    return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
  *num_bytes = bytes_read;
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult MojoBeginReadData(MojoHandle data_pipe_consumer_handle,
                                         const void** buffer,
                                         uint32_t* buffer_num_bytes,
                                         MojoReadDataFlags flags) {
  mx_ssize_t result = mx_datapipe_begin_read(
      (mx_handle_t)data_pipe_consumer_handle, flags, (uintptr_t*)buffer);
  if (result < 0) {
    switch (result) {
      case ERR_INVALID_ARGS:
      case ERR_BAD_HANDLE:
      case ERR_WRONG_TYPE:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      case ERR_BAD_STATE:
      case ERR_REMOTE_CLOSED:
        return MOJO_SYSTEM_RESULT_FAILED_PRECONDITION;
      case ERR_SHOULD_WAIT:
        return MOJO_SYSTEM_RESULT_SHOULD_WAIT;
      case ERR_ALREADY_BOUND:
        return MOJO_SYSTEM_RESULT_BUSY;
      default:
        return MOJO_SYSTEM_RESULT_INTERNAL;
    }
  }
  *buffer_num_bytes = (uint32_t)result;
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult MojoEndReadData(MojoHandle data_pipe_consumer_handle,
                                       uint32_t num_bytes_read) {
  mx_status_t result = mx_datapipe_end_read(
      (mx_handle_t)data_pipe_consumer_handle, num_bytes_read);
  switch (result) {
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
      return MOJO_SYSTEM_RESULT_INTERNAL;
  }
}

// buffer.h --------------------------------------------------------------------

MOJO_EXPORT MojoResult
MojoCreateSharedBuffer(const struct MojoCreateSharedBufferOptions* options,
                       uint64_t num_bytes,
                       MojoHandle* shared_buffer_handle) {
  if (options && options->flags != MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE)
    return MOJO_SYSTEM_RESULT_UNIMPLEMENTED;
  mx_handle_t result = mx_vmo_create(num_bytes);
  if (result < 0) {
    switch (result) {
      case ERR_INVALID_ARGS:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_NO_MEMORY:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  *shared_buffer_handle = (MojoHandle)result;
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult MojoDuplicateBufferHandle(
    MojoHandle buffer_handle,
    const struct MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  if (options &&
      options->flags != MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE)
    return MOJO_SYSTEM_RESULT_UNIMPLEMENTED;
  return MojoDuplicateHandle(buffer_handle, new_buffer_handle);
}

MOJO_EXPORT MojoResult
MojoGetBufferInformation(MojoHandle buffer_handle,
                         struct MojoBufferInformation* info,
                         uint32_t info_num_bytes) {
  if (!info || info_num_bytes < 16)
    return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
  mx_handle_t vmo_handle = (mx_handle_t)buffer_handle;
  uint64_t num_bytes = 0;
  mx_status_t status = mx_vmo_get_size(vmo_handle, &num_bytes);
  switch (status) {
    case NO_ERROR:
      info->struct_size = sizeof(struct MojoBufferInformation);
      info->flags = MOJO_BUFFER_INFORMATION_FLAG_NONE;
      info->num_bytes = num_bytes;
      return MOJO_RESULT_OK;
    case ERR_INVALID_ARGS:
    case ERR_BAD_HANDLE:
    case ERR_WRONG_TYPE:
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    case ERR_ACCESS_DENIED:
      return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}

MOJO_EXPORT MojoResult MojoMapBuffer(MojoHandle buffer_handle,
                                     uint64_t offset,
                                     uint64_t num_bytes,
                                     void** buffer,
                                     MojoMapBufferFlags flags) {
  if (flags != MOJO_MAP_BUFFER_FLAG_NONE)
    return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
  mx_handle_t vmo_handle = (mx_handle_t)buffer_handle;
  uintptr_t* mx_pointer = (uintptr_t*)buffer;
  // TODO(abarth): Mojo doesn't let you specify any flags. It's unclear whether
  // this is a reasonable default.
  uint32_t mx_flags = MX_VM_FLAG_PERM_READ | MX_VM_FLAG_PERM_WRITE;

  mx_status_t status = mx_process_map_vm(mx_process_self(), vmo_handle, offset,
                                         num_bytes, mx_pointer, mx_flags);
  switch (status) {
    case NO_ERROR:
      return MOJO_RESULT_OK;
    case ERR_INVALID_ARGS:
    case ERR_BAD_HANDLE:
    case ERR_WRONG_TYPE:
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    case ERR_ACCESS_DENIED:
      return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
    case ERR_NO_MEMORY:
      return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}

MOJO_EXPORT MojoResult MojoUnmapBuffer(void* buffer) {
  uintptr_t address = (uintptr_t)buffer;
  // TODO(abarth): mx_process_unmap_vm needs the length to unmap, but Mojo
  // doesn't give us the length.
  mx_size_t length = 0;
  mx_status_t status = mx_process_unmap_vm(mx_process_self(), address, length);
  switch (status) {
    case NO_ERROR:
      return MOJO_RESULT_OK;
    case ERR_INVALID_ARGS:
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}

// wait_set.h ------------------------------------------------------------------

static_assert(sizeof(struct MojoWaitSetResult) == sizeof(mx_waitset_result_t),
              "Wait set result types must match");
static_assert(offsetof(struct MojoWaitSetResult, cookie) ==
                  offsetof(mx_waitset_result_t, cookie),
              "Wait set result types must match");
static_assert(offsetof(struct MojoWaitSetResult, wait_result) ==
                  offsetof(mx_waitset_result_t, wait_result),
              "Wait set result types must match");
static_assert(offsetof(struct MojoWaitSetResult, reserved) ==
                  offsetof(mx_waitset_result_t, reserved),
              "Wait set result types must match");
static_assert(offsetof(struct MojoWaitSetResult, signals_state) ==
                  offsetof(mx_waitset_result_t, signals_state),
              "Wait set result types must match");

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

  mx_status_t result = mx_waitset_create();
  if (result < 0) {
    switch (result) {
      case ERR_NO_MEMORY:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  *handle = (MojoHandle)result;
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

  mx_status_t status = mx_waitset_add((mx_handle_t)wait_set_handle,
                                      (mx_handle_t)handle, signals, cookie);
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
  mx_status_t status = mx_waitset_wait(
      (mx_handle_t)wait_set_handle, MojoDeadlineToTime(deadline), num_results,
      (mx_waitset_result_t*)results, max_results);
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
    case ERR_TIMED_OUT:
      return MOJO_SYSTEM_RESULT_DEADLINE_EXCEEDED;
    default:
      return MOJO_SYSTEM_RESULT_UNKNOWN;
  }
}
