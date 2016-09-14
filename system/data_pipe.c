// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of functions declared in <mojo/system/data_pipe.h>.

#include <mojo/system/data_pipe.h>

#include <assert.h>
#include <magenta/syscalls.h>
#include <mojo/system/data_pipe.h>
#include <mojo/system/result.h>

#include "mojo/system/mojo_export.h"

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
