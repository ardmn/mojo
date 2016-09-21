// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application_manager/command_listener.h"

#include <utility>

#include "lib/ftl/logging.h"
#include "mojo/application_manager/application_manager.h"

namespace mojo {

CommandListener::CommandListener(ApplicationManager* manager)
    : manager_(manager) {}

CommandListener::~CommandListener() = default;

void CommandListener::StartListening(ScopedMessagePipeHandle handle) {
  FTL_DCHECK(!handle_.is_valid());
  handle_ = std::move(handle);
  WaitForCommand();
}

void CommandListener::WaitForCommand() {
  waiter_.reset(new AsyncWaiter(handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                                [this](MojoResult result) {
                                  if (result == MOJO_RESULT_OK)
                                    ExecuteCommand();
                                }));
}

void CommandListener::ExecuteCommand() {
  uint32_t num_bytes = 0;
  MojoResult result = ReadMessageRaw(handle_.get(), nullptr, &num_bytes,
                                     nullptr, 0, MOJO_READ_MESSAGE_FLAG_NONE);
  if (result == MOJO_SYSTEM_RESULT_RESOURCE_EXHAUSTED) {
    std::vector<char> bytes(num_bytes);
    result = ReadMessageRaw(handle_.get(), &bytes[0], &num_bytes, nullptr, 0,
                            MOJO_READ_MESSAGE_FLAG_NONE);
    FTL_CHECK(result == MOJO_RESULT_OK);
    manager_->GetOrStartApplicationInstance(
        std::string(bytes.data(), bytes.size()));
  }

  WaitForCommand();
}

}  // namespace mojo
