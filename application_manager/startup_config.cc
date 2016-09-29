// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application_manager/startup_config.h"

#include <utility>

#include <rapidjson/document.h>

namespace mojo {
namespace {

constexpr char kInitialApps[] = "initial-apps";
constexpr char kArgsFor[] = "args-for";

}  // namespace

StartupConfig::StartupConfig() = default;

StartupConfig::~StartupConfig() = default;

bool StartupConfig::Parse(const std::string& string) {
  initial_apps_.clear();
  args_for_.clear();

  rapidjson::Document document;
  document.Parse(string.data(), string.size());
  if (!document.IsObject())
    return false;

  auto inital_apps_it = document.FindMember(kInitialApps);
  if (inital_apps_it != document.MemberEnd()) {
    const auto& value = inital_apps_it->value;
    if (!value.IsArray())
      return false;
    for (const auto& application_name : value.GetArray()) {
      if (!application_name.IsString())
        return false;
      initial_apps_.push_back(application_name.GetString());
    }
  }

  auto args_for_it = document.FindMember(kArgsFor);
  if (args_for_it != document.MemberEnd()) {
    const auto& value = args_for_it->value;
    if (!value.IsObject())
      return false;
    for (const auto& command : value.GetObject()) {
      if (!command.name.IsString() || !command.value.IsArray())
        return false;
      std::string application_name = command.name.GetString();
      std::vector<std::string> args;
      for (const auto& arg : command.value.GetArray()) {
        if (!arg.IsString())
          return false;
        args.push_back(arg.GetString());
      }
      args_for_.emplace(std::move(application_name), std::move(args));
    }
  }

  return true;
}

ApplicationArgs StartupConfig::TakeArgsFor() {
  return std::move(args_for_);
}

std::vector<std::string> StartupConfig::TakeInitialApps() {
  return std::move(initial_apps_);
}

}  // namespace mojo
