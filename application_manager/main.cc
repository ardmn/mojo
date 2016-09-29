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
  mojo::StartupConfig config;
  LoadStartupConfig(&config);

  mojo::ApplicationManager manager(config.TakeArgsFor());
  mojo::CommandListener command_listener(&manager);

  mtl::MessageLoop message_loop;

  std::vector<std::string> initial_apps = config.TakeInitialApps();
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
