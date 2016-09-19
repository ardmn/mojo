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
#include "mojo/system/options.h"

// TODO(vtl): Need assertion that |MojoCreateDataPipeOptions| flags and
// |mx_datapipe_create()| flags are the same (currently, there are no flags).

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
  struct MojoCreateDataPipeOptions validated_options = {
      sizeof(struct MojoCreateDataPipeOptions),  // struct_size
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,   // flags
      1u,                                        // element_num_bytes
      0u,                                        // capacity_num_bytes
  };
  if (options) {
    if (!IS_VALID_OPTIONS_STRUCT(MojoCreateDataPipeOptions, options))
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
    READ_OPTIONS_FIELD_IF_PRESENT(MojoCreateDataPipeOptions, flags,
                                  &validated_options, options);
    READ_OPTIONS_FIELD_IF_PRESENT(MojoCreateDataPipeOptions, element_num_bytes,
                                  &validated_options, options);
    READ_OPTIONS_FIELD_IF_PRESENT(MojoCreateDataPipeOptions, capacity_num_bytes,
                                  &validated_options, options);
  }
  mx_handle_t mx_consumer_handle = 0u;
  mx_handle_t mx_producer_handle = mx_datapipe_create(
      validated_options.flags, validated_options.element_num_bytes,
      validated_options.capacity_num_bytes, &mx_consumer_handle);
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
  // Note: Null |options| resets back to default.
  uint32_t threshold_u32 = 0u;
  if (options) {
    if (!IS_VALID_OPTIONS_STRUCT(MojoDataPipeProducerOptions, options))
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;

    // TODO(vtl): We treat |write_threshold_num_bytes| as optional for setting,
    // but required for getting, which is inconsistent. (This inconsistency
    // exists in the domokit EDK implementation.)
    if (!HAS_OPTIONS_FIELD(MojoDataPipeProducerOptions,
                           write_threshold_num_bytes, options))
      return MOJO_RESULT_OK;
    READ_OPTIONS_FIELD_TO(write_threshold_num_bytes, options, &threshold_u32);
  }

  const mx_size_t threshold = threshold_u32;
  mx_status_t result = mx_object_set_property(
      (mx_handle_t)data_pipe_producer_handle, MX_PROP_DATAPIPE_WRITE_THRESHOLD,
      &threshold, sizeof(threshold));
  if (result < 0) {
    switch (result) {
      case ERR_INVALID_ARGS:
      case ERR_BAD_HANDLE:
      case ERR_WRONG_TYPE:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      // TODO(vtl): This seems like a dubious choice of error code (on the
      // Magenta side), and also a dubious "translation" (though we should never
      // get this).
      case ERR_BUFFER_TOO_SMALL:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult
MojoGetDataPipeProducerOptions(MojoHandle data_pipe_producer_handle,
                               struct MojoDataPipeProducerOptions* options,
                               uint32_t options_num_bytes) {
  // Note: If/when |MojoDataPipeProducerOptions| is extended beyond its initial
  // definition, more work will be necessary. (See the definition of
  // |MojoGetDataPipeProducerOptions()| in <mojo/system/data_pipe.h>.)
  static_assert(sizeof(struct MojoDataPipeProducerOptions) == 8u,
                "MojoDataPipeProducerOptions has been extended!");

  // TODO(vtl): We treat |write_threshold_num_bytes| as optional for setting,
  // but required for getting, which is inconsistent. (This inconsistency exists
  // in the domokit EDK implementation.)
  if (options_num_bytes < sizeof(struct MojoDataPipeProducerOptions))
    return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;

  mx_size_t threshold = 0u;
  mx_status_t result = mx_object_get_property(
      (mx_handle_t)data_pipe_producer_handle, MX_PROP_DATAPIPE_WRITE_THRESHOLD,
      &threshold, sizeof(threshold));
  if (result < 0) {
    switch (result) {
      case ERR_INVALID_ARGS:
      case ERR_BAD_HANDLE:
      case ERR_WRONG_TYPE:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      // TODO(vtl): This seems like a dubious choice of error code (on the
      // Magenta side), and also a dubious "translation" (though we should never
      // get this).
      case ERR_BUFFER_TOO_SMALL:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  // TODO(vtl): Handle overflow?
  struct MojoDataPipeProducerOptions model_options = {
      sizeof(struct MojoDataPipeProducerOptions),  // |struct_size|.
      (uint32_t)threshold,  // |write_threshold_num_bytes|.
  };
  memcpy(options, &model_options, sizeof(model_options));
  return MOJO_RESULT_OK;
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
  // Note: Null |options| resets back to default.
  uint32_t threshold_u32 = 0u;
  if (options) {
    if (!IS_VALID_OPTIONS_STRUCT(MojoDataPipeConsumerOptions, options))
      return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;

    // TODO(vtl): We treat |read_threshold_num_bytes| as optional for setting,
    // but required for getting, which is inconsistent. (This inconsistency
    // exists in the domokit EDK implementation.)
    if (!HAS_OPTIONS_FIELD(MojoDataPipeConsumerOptions,
                           read_threshold_num_bytes, options))
      return MOJO_RESULT_OK;
    READ_OPTIONS_FIELD_TO(read_threshold_num_bytes, options, &threshold_u32);
  }

  const mx_size_t threshold = threshold_u32;
  mx_status_t result = mx_object_set_property(
      (mx_handle_t)data_pipe_consumer_handle, MX_PROP_DATAPIPE_READ_THRESHOLD,
      &threshold, sizeof(threshold));
  if (result < 0) {
    switch (result) {
      case ERR_INVALID_ARGS:
      case ERR_BAD_HANDLE:
      case ERR_WRONG_TYPE:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      // TODO(vtl): This seems like a dubious choice of error code (on the
      // Magenta side), and also a dubious "translation" (though we should never
      // get this).
      case ERR_BUFFER_TOO_SMALL:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  return MOJO_RESULT_OK;
}

MOJO_EXPORT MojoResult
MojoGetDataPipeConsumerOptions(MojoHandle data_pipe_consumer_handle,
                               struct MojoDataPipeConsumerOptions* options,
                               uint32_t options_num_bytes) {
  // Note: If/when |MojoDataPipeConsumerOptions| is extended beyond its initial
  // definition, more work will be necessary. (See the definition of
  // |MojoGetDataPipeConsumerOptions()| in <mojo/system/data_pipe.h>.)
  static_assert(sizeof(struct MojoDataPipeConsumerOptions) == 8u,
                "MojoDataPipeConsumerOptions has been extended!");

  // TODO(vtl): We treat |read_threshold_num_bytes| as optional for setting,
  // but required for getting, which is inconsistent. (This inconsistency exists
  // in the domokit EDK implementation.)
  if (options_num_bytes < sizeof(struct MojoDataPipeConsumerOptions))
    return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;

  mx_size_t threshold = 0u;
  mx_status_t result = mx_object_get_property(
      (mx_handle_t)data_pipe_consumer_handle, MX_PROP_DATAPIPE_READ_THRESHOLD,
      &threshold, sizeof(threshold));
  if (result < 0) {
    switch (result) {
      case ERR_INVALID_ARGS:
      case ERR_BAD_HANDLE:
      case ERR_WRONG_TYPE:
        return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
      case ERR_ACCESS_DENIED:
        return MOJO_SYSTEM_RESULT_PERMISSION_DENIED;
      // TODO(vtl): This seems like a dubious choice of error code (on the
      // Magenta side), and also a dubious "translation" (though we should never
      // get this).
      case ERR_BUFFER_TOO_SMALL:
        return MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED;
      default:
        return MOJO_SYSTEM_RESULT_UNKNOWN;
    }
  }
  // TODO(vtl): Handle overflow?
  struct MojoDataPipeConsumerOptions model_options = {
      sizeof(struct MojoDataPipeConsumerOptions),  // |struct_size|.
      (uint32_t)threshold,  // |read_threshold_num_bytes|.
  };
  memcpy(options, &model_options, sizeof(model_options));
  return MOJO_RESULT_OK;
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
