// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPLICATION_MANAGER_APPLICATION_MANAGER_H_
#define MOJO_APPLICATION_MANAGER_APPLICATION_MANAGER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "lib/ftl/command_line.h"
#include "mojo/application_manager/application_table.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/public/interfaces/network/url_response.mojom.h"

namespace mojo {

using ApplicationArgs =
    std::unordered_map<std::string, std::vector<std::string>>;

class ApplicationManager {
 public:
  explicit ApplicationManager(ApplicationArgs args_for);
  ~ApplicationManager();

  void ConnectToApplication(const std::string& application_name,
                            const std::string& requestor_name,
                            InterfaceRequest<ServiceProvider> services);

  void StartApplicationUsingContentHandler(
      const std::string& content_handler_name,
      URLResponsePtr response,
      InterfaceRequest<Application> application_request);

  ApplicationInstance* GetOrStartApplicationInstance(
      std::string name,
      std::vector<std::string>* override_args = nullptr);

 private:
  ApplicationTable table_;
  std::unordered_map<std::string, std::vector<std::string>> args_for_;

  FTL_DISALLOW_COPY_AND_ASSIGN(ApplicationManager);
};

}  // namespace mojo

#endif  // MOJO_APPLICATION_MANAGER_APPLICATION_MANAGER_H_
