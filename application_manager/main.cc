// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "lib/ftl/command_line.h"
#include "lib/mtl/tasks/message_loop.h"
#include "mojo/application_manager/application_manager.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "error: Missing path to initial application\n");
    return 1;
  }

  auto args = ftl::CommandLineFromArgcArgv(argc, argv);
  const auto& initial_app = args.positional_args()[0];
  std::unordered_map<std::string, std::vector<std::string>> args_for;
  // TODO(alhaad): This implementation passes all the command-line arguments
  // to the initial application. Having an '--args-for' option is desirable.
  args_for[initial_app] = args.positional_args();
  mojo::ApplicationManager manager(std::move(args_for));

  mtl::MessageLoop message_loop;
  message_loop.task_runner()->PostTask([&]() {
    if (!manager.StartInitialApplication(initial_app)) {
      exit(1);
    }
  });

  message_loop.Run();
  return 0;
}
