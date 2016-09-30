// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application_manager/application_manager.h"

#include <stdio.h>
#include <stdlib.h>

#include <utility>

#include "lib/ftl/command_line.h"
#include "lib/ftl/logging.h"
#include "mojo/application_manager/application_instance.h"
#include "mojo/application_manager/shell_impl.h"
#include "mojo/public/cpp/bindings/formatting.h"

namespace mojo {

ApplicationManager::ApplicationManager(ApplicationArgs args_for)
    : args_for_(std::move(args_for)) {}

ApplicationManager::~ApplicationManager() {}

void ApplicationManager::ConnectToApplication(
    const std::string& application_name,
    const std::string& requestor_name,
    InterfaceRequest<ServiceProvider> services) {
  ApplicationInstance* instance =
      GetOrStartApplicationInstance(std::move(application_name));
  if (!instance)
    return;
  instance->application()->AcceptConnection(requestor_name, application_name,
                                            std::move(services));
}

void ApplicationManager::StartApplicationUsingContentHandler(
    const std::string& content_handler_name,
    URLResponsePtr response,
    InterfaceRequest<Application> application_request) {
  ApplicationInstance* instance =
      GetOrStartApplicationInstance(content_handler_name);
  if (!instance)
    return;
  ContentHandler* content_handler = instance->GetOrCreateContentHandler();
  content_handler->StartApplication(std::move(application_request),
                                    std::move(response));
}

ApplicationInstance* ApplicationManager::GetOrStartApplicationInstance(
    std::string name,
    std::vector<std::string>* override_args) {
  ApplicationInstance* instance = table_.GetOrStartApplication(this, name);
  if (!instance) {
    fprintf(stderr, "application_manager: Failed to start application %s",
            name.c_str());
    return nullptr;
  }
  if (!instance->is_initialized()) {
    Array<String> args;
    if (override_args) {
      args = Array<String>::From(*override_args);
    } else {
      const auto& it = args_for_.find(name);
      if (it != args_for_.end())
        args = Array<String>::From(it->second);
    }
    FTL_DLOG(INFO) << "Starting application: \"" << name
                   << "\", with args: " << args;
    instance->Initialize(std::make_unique<ShellImpl>(name, this),
                         std::move(args), name);
    instance->set_connection_error_handler([this, instance]() {
      FTL_DLOG(INFO) << "Application terminated: \"" << instance->name()
                     << "\"";
      table_.StopApplication(instance->name());
    });
  }
  return instance;
}

}  // namespace mojo
