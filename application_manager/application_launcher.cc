// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application_manager/application_launcher.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <launchpad/launchpad.h>
#include <magenta/processargs.h>
#include <magenta/syscalls.h>
#include <mxio/util.h>

#include "lib/ftl/files/unique_fd.h"
#include "lib/ftl/logging.h"
#include "lib/mtl/data_pipe/files.h"
#include "lib/mtl/tasks/message_loop.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace mojo {
namespace {

constexpr char kFileUriPrefix[] = "file://";
constexpr char kMojoAppDir[] = "/boot/apps/";
constexpr char kMojoScheme[] = "mojo:";
constexpr size_t kMojoSchemeLength = sizeof(kMojoScheme) - 1;

constexpr char kMojoMagic[] = "#!mojo ";
constexpr size_t kMojoMagicLength = sizeof(kMojoMagic) - 1;
constexpr size_t kMaxShebangLength = 2048;

void Ignored(bool success) {
  // There's not much we can do with this success value. If we didn't succeed in
  // filling the data pipe with content, that could just mean the content
  // handler wasn't interested in the data and closed its end of the pipe before
  // reading all the data.
}

std::string GetPathFromApplicationName(const std::string& name) {
  if (name.find(kMojoScheme) == 0)
    return kMojoAppDir + name.substr(kMojoSchemeLength);
  return std::string();
}

std::string GetUriFromPath(const std::string& path) {
  return kFileUriPrefix + path;
}

mojo::InterfaceRequest<mojo::Application> LaunchWithContentHandler(
    ApplicationManager* manager,
    const std::string& path,
    mojo::InterfaceRequest<mojo::Application> request) {
  ftl::UniqueFD fd(open(path.c_str(), O_RDONLY));
  if (!fd.is_valid())
    return request;
  std::string shebang(kMaxShebangLength, '\0');
  ssize_t count = read(fd.get(), &shebang[0], shebang.length());
  if (count == -1)
    return request;
  if (shebang.find(kMojoMagic) != 0)
    return request;
  size_t newline = shebang.find('\n', kMojoMagicLength);
  if (newline == std::string::npos)
    return request;
  if (lseek(fd.get(), 0, SEEK_SET) == -1)
    return request;
  std::string handler =
      shebang.substr(kMojoMagicLength, newline - kMojoMagicLength);
  URLResponsePtr response = URLResponse::New();
  response->status_code = 200;
  response->url = GetUriFromPath(path);
  mojo::DataPipe data_pipe;
  response->body = std::move(data_pipe.consumer_handle);
  mtl::CopyFromFileDescriptor(
      std::move(fd), std::move(data_pipe.producer_handle),
      // TODO(abarth): Move file tasks to a background thread.
      mtl::MessageLoop::GetCurrent()->task_runner(), Ignored);
  manager->StartApplicationUsingContentHandler(handler, std::move(response),
                                               std::move(request));
  return nullptr;
}

// TODO(abarth): We should use the fd we opened in LaunchWithContentHandler
// rather than using the path again.
mtl::UniqueHandle LaunchWithProcess(
    const std::string& path,
    mojo::InterfaceRequest<mojo::Application> request) {
  const char* path_arg = path.c_str();
  mx_handle_t request_handle =
      static_cast<mx_handle_t>(request.PassMessagePipe().release().value());
  uint32_t request_id = MX_HND_TYPE_APPLICATION_REQUEST;
  // TODO(abarth): We shouldn't pass stdin, stdout, stderr, or the file system
  // when launching Mojo applications. We probably shouldn't pass environ, but
  // currently this is very useful as a way to tell the loader in the child
  // process to print out load addresses so we can understand crashes.
  mx_handle_t result = launchpad_launch_mxio_etc(
      path_arg, 1, &path_arg, environ, 1, &request_handle, &request_id);
  if (result < 0)
    return mtl::UniqueHandle();
  return mtl::UniqueHandle(result);
}

}  // namespace

std::pair<bool, mtl::UniqueHandle> LaunchApplication(
    ApplicationManager* manager,
    const std::string& name,
    mojo::InterfaceRequest<mojo::Application> request) {
  std::string path = GetPathFromApplicationName(name);
  if (path.empty())
    return std::make_pair(false, mtl::UniqueHandle());

  request = LaunchWithContentHandler(manager, path, std::move(request));
  if (!request.is_pending()) {
    // LaunchWithContentHandler has consumed the interface request, which
    // means we succeeded in kicking off the load process.
    return std::make_pair(true, mtl::UniqueHandle());
  }

  mtl::UniqueHandle handle = LaunchWithProcess(path, std::move(request));
  bool success = handle.is_valid();
  return std::make_pair(success, std::move(handle));
}

}  // namespace mojo
