// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <magenta/processargs.h>
#include <mojo/system/main.h>
#include <mxio/util.h>

int main(int argc, char** argv) {
  return MojoMain(mxio_get_startup_handle(MX_HND_TYPE_APPLICATION_REQUEST));
}
