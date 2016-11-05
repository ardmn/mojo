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

constexpr char kDefaultConfigPath[] =
    "/system/data/application_manager/startup.config";

static void LoadStartupConfig(mojo::StartupConfig* config,
                              const std::string& config_path) {
  std::string data;
  if (!files::ReadFileToString(config_path, &data)) {
    fprintf(stderr, "application_manager: Failed to read startup config: %s\n",
            config_path.c_str());
  } else if (!config->Parse(data)) {
    fprintf(stderr, "application_manager: Failed to parse startup config: %s\n",
            config_path.c_str());
  }
}

int main(int argc, char** argv) {
  auto command_line = ftl::CommandLineFromArgcArgv(argc, argv);

  std::string config_path;
  command_line.GetOptionValue("config", &config_path);

  const auto& positional_args = command_line.positional_args();
  if (config_path.empty() && positional_args.empty())
    config_path = kDefaultConfigPath;

  std::vector<std::string> initial_apps;
  mojo::ApplicationArgs args_for;
  if (!config_path.empty()) {
    mojo::StartupConfig config;
    LoadStartupConfig(&config, config_path);
    initial_apps = config.TakeInitialApps();
    args_for = config.TakeArgsFor();
  }

  if (!positional_args.empty()) {
    // TODO(alhaad): This implementation passes all the command-line arguments
    // to the initial application. Having an '--args-for' option is desirable.
    std::string initial_app = positional_args[0];
    initial_apps.push_back(initial_app);
    args_for[initial_app] = std::vector<std::string>(
        positional_args.begin() + 1, positional_args.end());
  }

  // TODO(jeffbrown): If there's already a running instance of
  // application_manager, it might be nice to pass the request over to
  // it instead of starting a whole new instance.  Alternately, we could create
  // a separate command-line program to act as an interface for modifying
  // configuration, starting / stopping applications, listing what's running,
  // printing debugging information, etc.  Having multiple instances of
  // application manager running is not what we want, in general.

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
