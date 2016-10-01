// Copyright 2015 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_ICU_DATA_CPP_ICU_DATA_H_
#define MOJO_SERVICES_ICU_DATA_CPP_ICU_DATA_H_

namespace mojo {
class ApplicationConnector;
}

namespace icu_data {

bool Initialize(mojo::ApplicationConnector* application_connector);
bool Release();

}  // namespace icu_data

#endif  // MOJO_SERVICES_ICU_DATA_CPP_ICU_DATA_H_
