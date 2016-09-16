// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helpers for dealing with Mojo (variable-size/extensible) options structs.
//
// A Mojo options struct looks like:
//
//   struct MOJO_ALIGNAS(8) MojoFooOptions {
//     uint32_t struct_size;
//     // Fields follow; typically all are optional. E.g.:
//     uint32_t flags;
//     int32_t bar;
//   };
//
// * They are 8-byte-aligned on the assumption that types like |int64_t| or
//   |double| may require 8-byte alignment, and the struct may be extended to
//   have such a field in the future even if it does not presently have one.
// * The minimal requirement for "validity" is that |struct_size >=
//   sizeof(uint32_t)|.
// * An "instance" of the (extensible) struct has a given field if |struct_size|
//   indicates that it accommodates it. If it does not have a given field, a
//   default value should be used.
// * IMPORTANT NOTE: Even if an "instance" (of a possibly-earlier version) of a
//   struct has a size that indicates a field is present, it is not safe to
//   "directly" read it. E.g., if |foo| is a user-provided |const struct
//   MojoFooOptions*|, it is NOT safe to directly access |foo->flags|, even if
//   |foo->struct_size >= 8| (for that matter, it is not safe to directly
//   examine |foo->struct_size| either). The reason for this is that the
//   compiler may read (or write) beyond |foo->flags|, believing that the |bar|
//   field is present.
//
// Example code to process a user-provided Mojo options struct:
//
//   MojoResult MojoFoo(const MojoFooOptions* options) {
//     // Populated with defaults:
//     struct MojoFooOptions validated_options = {
//         (uint32_t)sizeof(struct MojoFooOptions),  // struct_size
//         MOJO_FOO_OPTIONS_FLAG_NONE,               // flags
//         42                                        // bar
//     };
//     if (options) {
//       if (!IS_VALID_OPTIONS_STRUCT(options))
//         return MOJO_SYSTEM_RESULT_INVALID_ARGUMENT;
//       READ_OPTIONS_FIELD_IF_PRESENT(MojoFooOptions, flags,
//                                     &validated_options, options);
//       READ_OPTIONS_FIELD_IF_PRESENT(MojoFooOptions, bar, &validated_options,
//                                     options);
//     }
//     // Do stuff (using |validated_options|)....
//   }

#ifndef MOJO_SYSTEM_OPTIONS_H_
#define MOJO_SYSTEM_OPTIONS_H_

#include <stdalign.h>
#include <stdint.h>
#include <string.h>

// Gets the value of the |struct_size| field of the Mojo options struct pointed
// to by |ptr|. (|struct_size| must be its first member.)
// TODO(vtl): The cast of |ptr| to |const uint32_t*| here is a bit dodgy
// (really, I should |memcpy()|).
#define OPTIONS_STRUCT_SIZE(ptr) (*(const uint32_t*)(ptr))

// Checks if the Mojo options struct pointed to by |ptr| is valid (according to
// the criteria above. |struct_size| must be the first member of |struct
// struct_name|. |ptr| should be a valid pointer (and non-null).
#define IS_VALID_OPTIONS_STRUCT(struct_name, ptr)       \
  (!((uintptr_t)(ptr) % alignof(struct struct_name)) && \
   (OPTIONS_STRUCT_SIZE(ptr) >= sizeof(uint32_t)))

// Checks if (the instance of) the Mojo options struct pointed to by |ptr| has
// the field |field_name|. (|ptr| must point to a valid options struct.)
#define HAS_OPTIONS_FIELD(struct_name, field_name, ptr) \
  (OPTIONS_STRUCT_SIZE(ptr) >=                          \
   offsetof(struct struct_name, field_name) + sizeof((ptr)->field_name))

// Read the given field |field_name| from the Mojo options struct pointed to by
// |ptr| to |target|.
#define READ_OPTIONS_FIELD_TO(field_name, ptr, target) \
  memcpy(target, &(ptr)->field_name, sizeof((ptr)->field_name))

// Reads a field from the Mojo options struct pointed to by |ptr| into an
// existing Mojo options struct (pointed to by |target_ptr|), which should
// typically be prepopulated with default values.
#define READ_OPTIONS_FIELD_IF_PRESENT(struct_name, field_name, target_ptr, \
                                      ptr)                                 \
  do {                                                                     \
    if (HAS_OPTIONS_FIELD(struct_name, field_name, (ptr)))                 \
      READ_OPTIONS_FIELD_TO(field_name, ptr, &(target_ptr)->field_name);   \
  } while (0)

#endif  // MOJO_SYSTEM_OPTIONS_H_
