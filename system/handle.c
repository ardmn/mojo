// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of functions declared in <mojo/system/handle.h>.

#include <mojo/system/handle.h>

#include <assert.h>
#include <magenta/syscalls.h>
#include <mojo/system/result.h>

#include "mojo/system/mojo_export.h"

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
static_assert(MOJO_HANDLE_RIGHT_MAP == MX_RIGHT_MAP, "RIGHT_MAP must match");
static_assert(MOJO_HANDLE_RIGHT_GET_OPTIONS == MX_RIGHT_GET_PROPERTY,
              "RIGHT_GET_OPTIONS/PROPERTY must match");
static_assert(MOJO_HANDLE_RIGHT_SET_OPTIONS == MX_RIGHT_SET_PROPERTY,
              "RIGHT_SET_OPTIONS/PROPERTY must match");
// TODO(vtl): Add debug to Mojo.

static_assert(MOJO_HANDLE_SIGNAL_NONE == MX_SIGNAL_NONE,
              "SIGNAL_NONE must match");
static_assert(MOJO_HANDLE_SIGNAL_READABLE == MX_SIGNAL_READABLE,
              "SIGNAL_READABLE must match");
static_assert(MOJO_HANDLE_SIGNAL_WRITABLE == MX_SIGNAL_WRITABLE,
              "SIGNAL_WRITABLE must match");
static_assert(MOJO_HANDLE_SIGNAL_PEER_CLOSED == MX_SIGNAL_PEER_CLOSED,
              "PEER_CLOSED must match");
static_assert(MOJO_HANDLE_SIGNAL_SIGNAL0 == MX_SIGNAL_SIGNAL0,
              "SIGNAL0 must match");
static_assert(MOJO_HANDLE_SIGNAL_SIGNAL1 == MX_SIGNAL_SIGNAL1,
              "SIGNAL1 must match");
static_assert(MOJO_HANDLE_SIGNAL_SIGNAL2 == MX_SIGNAL_SIGNAL2,
              "SIGNAL2 must match");
static_assert(MOJO_HANDLE_SIGNAL_SIGNAL3 == MX_SIGNAL_SIGNAL3,
              "SIGNAL3 must match");
static_assert(MOJO_HANDLE_SIGNAL_SIGNAL4 == MX_SIGNAL_SIGNAL4,
              "SIGNAL4 must match");
// TODO(vtl): Add {READ,WRITE}_THRESHOLD (once Magenta has them).

static_assert(sizeof(struct MojoHandleSignalsState) ==
                  sizeof(mx_signals_state_t),
              "Signals state structs must match");
static_assert(offsetof(struct MojoHandleSignalsState, satisfied_signals) ==
                  offsetof(mx_signals_state_t, satisfied),
              "Signals state structs must match");
static_assert(offsetof(struct MojoHandleSignalsState, satisfiable_signals) ==
                  offsetof(mx_signals_state_t, satisfiable),
              "Signals state structs must match");

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
