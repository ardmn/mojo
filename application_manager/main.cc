// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <magenta/processargs.h>
#include <mxio/util.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "lib/ftl/command_line.h"
#include "lib/mtl/tasks/message_loop.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/application_manager/command_listener.h"

int main(int argc, char** argv) {
  auto args = ftl::CommandLineFromArgcArgv(argc, argv);

  std::string initial_app;
  const auto& positional_args = args.positional_args();
  if (positional_args.size() > 0)
    initial_app = positional_args[0];

  std::unordered_map<std::string, std::vector<std::string>> args_for;
  // TODO(alhaad): This implementation passes all the command-line arguments
  // to the initial application. Having an '--args-for' option is desirable.
  args_for[initial_app] = args.positional_args();
  mojo::ApplicationManager manager(std::move(args_for));
  mojo::CommandListener command_listener(&manager);

  mtl::MessageLoop message_loop;

  if (!initial_app.empty()) {
    message_loop.task_runner()->PostTask([&manager, &initial_app] {
      if (!manager.StartInitialApplication(initial_app))
        fprintf(stderr, "Unable to start initial application: %s\n",
                initial_app.c_str());
    });
  }

  message_loop.task_runner()->PostTask([&command_listener] {
    mojo::ScopedMessagePipeHandle launcher(mojo::MessagePipeHandle(
        mxio_get_startup_handle(MX_HND_TYPE_APPLICATION_LAUNCHER)));
    command_listener.StartListening(std::move(launcher));
  });

  message_loop.Run();
  return 0;
}
