// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of functions declared in <mojo/system/buffer.h>.

#include <mojo/system/buffer.h>

#include <magenta/process.h>
#include <magenta/syscalls.h>
#include <mojo/system/buffer.h>
#include <mojo/system/result.h>

#include "mojo/system/mojo_export.h"

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
