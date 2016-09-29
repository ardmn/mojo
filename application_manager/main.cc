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

#include "lib/ftl/files/file.h"
#include "lib/mtl/tasks/message_loop.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/application_manager/command_listener.h"
#include "mojo/application_manager/startup_config.h"

constexpr char kConfigPath[] = "/boot/data/application_manager/startup.config";

static void LoadStartupConfig(mojo::StartupConfig* config) {
  std::string data;
  if (!files::ReadFileToString(kConfigPath, &data)) {
    fprintf(stderr, "application_manager: Failed to read startup config: %s\n",
            kConfigPath);
  } else if (!config->Parse(data)) {
    fprintf(stderr, "application_manager: Failed to parse startup config: %s\n",
            kConfigPath);
  }
}

int main(int argc, char** argv) {
  auto args = ftl::CommandLineFromArgcArgv(argc, argv);

  std::vector<std::string> initial_apps;
  mojo::ApplicationArgs args_for;

  const auto& positional_args = args.positional_args();
  if (positional_args.size() > 0) {
    // TODO(alhaad): This implementation passes all the command-line arguments
    // to the initial application. Having an '--args-for' option is desirable.
    std::string initial_app = positional_args[0];
    initial_apps.push_back(initial_app);
    args_for[initial_app] = positional_args;
  } else {
    mojo::StartupConfig config;
    LoadStartupConfig(&config);
    initial_apps = config.TakeInitialApps();
    args_for = config.TakeArgsFor();
  }

  mtl::MessageLoop message_loop;
  mojo::ApplicationManager manager(std::move(args_for));
  mojo::CommandListener command_listener(&manager);

  if (!initial_apps.empty()) {
    message_loop.task_runner()->PostTask([&manager, &initial_apps] {
      for (const std::string& app : initial_apps)
        manager.GetOrStartApplicationInstance(app);
    });
  }

  message_loop.task_runner()->PostTask([&command_listener] {
    mojo::ScopedMessagePipeHandle launcher(mojo::MessagePipeHandle(
        mxio_get_startup_handle(MX_HND_TYPE_APPLICATION_LAUNCHER)));
    if (launcher.is_valid())
      command_listener.StartListening(std::move(launcher));
  });

  message_loop.Run();
  return 0;
}
