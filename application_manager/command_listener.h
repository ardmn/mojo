// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPLICATION_MANAGER_COMMAND_LISTENER_H_
#define MOJO_APPLICATION_MANAGER_COMMAND_LISTENER_H_

#include <mojo/environment/async_waiter.h>

#include <memory>

#include "lib/ftl/macros.h"
#include "mojo/public/cpp/environment/async_waiter.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
class ApplicationManager;

// This class listens on the given handle for commands to drive the application
// manager. For example, mxsh sends commands the user types that begin with
// "mojo:" to this class to run the cooresponding applications.
class CommandListener {
 public:
  explicit CommandListener(ApplicationManager* manager);
  ~CommandListener();

  void StartListening(ScopedMessagePipeHandle handle);

 private:
  void WaitForCommand();
  void ExecuteCommand();

  ApplicationManager* const manager_;
  ScopedMessagePipeHandle handle_;
  std::unique_ptr<AsyncWaiter> waiter_;

  FTL_DISALLOW_COPY_AND_ASSIGN(CommandListener);
};

}  // namespace mojo

#endif  // MOJO_APPLICATION_MANAGER_COMMAND_LISTENER_H_
