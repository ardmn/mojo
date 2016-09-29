// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPLICATION_MANAGER_STARTUP_CONFIG_H_
#define MOJO_APPLICATION_MANAGER_STARTUP_CONFIG_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "lib/ftl/macros.h"
#include "mojo/application_manager/application_manager.h"

namespace mojo {

// The configuration file should be specified as:
// {
//   "initial-apps": [
//     "mojo:device_runner"
//   ],
//   "args-for": {
//     "mojo:example_app": ["user1"]
//   }
// }

class StartupConfig {
 public:
  StartupConfig();
  ~StartupConfig();

  bool Parse(const std::string& string);

  ApplicationArgs TakeArgsFor();
  std::vector<std::string> TakeInitialApps();

 private:
  ApplicationArgs args_for_;
  std::vector<std::string> initial_apps_;

  FTL_DISALLOW_COPY_AND_ASSIGN(StartupConfig);
};

}  // namespace mojo

#endif  // MOJO_APPLICATION_MANAGER_STARTUP_CONFIG_H_
